#include "psram.h"

#if PICO_RP2350

#include "pico/stdlib.h"
#include "hardware/clocks.h"
#include "hardware/sync.h"
#include "hardware/structs/qmi.h"
#include "hardware/structs/sysinfo.h"
#include "hardware/structs/xip_ctrl.h"

#ifndef PSRAM_CS1_GPIO_RP2350A
#error "PSRAM_CS1_GPIO_RP2350A must be defined by the board header"
#endif
#ifndef PSRAM_CS1_GPIO_RP2350B
#define PSRAM_CS1_GPIO_RP2350B 47
#endif

static bool psram_available;
static size_t psram_size;
static int psram_cs_gpio = -1;

#define NES_PSRAM_UNCACHED_BASE ((uintptr_t)0x15000000u)

static bool __no_inline_not_in_flash_func(psram_direct_probe)(void) {
    for (int attempt = 0; attempt < 3; ++attempt) {
        qmi_hw->direct_csr |= QMI_DIRECT_CSR_ASSERT_CS1N_BITS;
        qmi_hw->direct_tx = QMI_DIRECT_TX_OE_BITS |
                            (QMI_DIRECT_TX_IWIDTH_VALUE_Q << QMI_DIRECT_TX_IWIDTH_LSB) |
                            0xF5u;
        while (qmi_hw->direct_csr & QMI_DIRECT_CSR_BUSY_BITS)
            ;
        (void)qmi_hw->direct_rx;
        qmi_hw->direct_csr &= ~QMI_DIRECT_CSR_ASSERT_CS1N_BITS;
        for (volatile int d = 0; d < 64; ++d)
            ;

        qmi_hw->direct_csr |= QMI_DIRECT_CSR_ASSERT_CS1N_BITS;
        uint8_t mfid = 0, kgd = 0;
        for (int i = 0; i < 6; ++i) {
            qmi_hw->direct_tx = (i == 0) ? 0x9Fu : 0xFFu;
            while (!(qmi_hw->direct_csr & QMI_DIRECT_CSR_TXEMPTY_BITS))
                ;
            while (qmi_hw->direct_csr & QMI_DIRECT_CSR_BUSY_BITS)
                ;
            const uint8_t v = (uint8_t)qmi_hw->direct_rx;
            if (i == 4) mfid = v;
            if (i == 5) kgd = v;
        }
        qmi_hw->direct_csr &= ~QMI_DIRECT_CSR_ASSERT_CS1N_BITS;
        for (volatile int d = 0; d < 64; ++d)
            ;

        if (kgd == 0x5Du || mfid == 0x5Du)
            return true;
    }
    return false;
}

static size_t psram_detect_size(void) {
    volatile uint32_t *base = (volatile uint32_t *)NES_PSRAM_UNCACHED_BASE;
    static const size_t boundaries[] = {
        1u << 20, 2u << 20, 4u << 20, 8u << 20
    };
    const uint32_t mark0 = 0x13579BDFu;
    const uint32_t mark1 = 0x2468ACE0u;
    uint32_t old0 = base[0];

    base[0] = mark0;
    __dmb();
    if (base[0] != mark0) {
        base[0] = old0;
        __dmb();
        return 0;
    }
    base[0] = old0;
    __dmb();

    for (unsigned i = 0; i < sizeof(boundaries) / sizeof(boundaries[0]); ++i) {
        const size_t bytes = boundaries[i];
        volatile uint32_t *probe =
            (volatile uint32_t *)(NES_PSRAM_UNCACHED_BASE + bytes);
        old0 = base[0];
        const uint32_t oldp = probe[0];

        base[0] = mark0;
        __dmb();
        probe[0] = mark1;
        __dmb();

        if (base[0] == mark1) {
            base[0] = old0;
            __dmb();
            return bytes;
        }

        probe[0] = oldp;
        base[0] = old0;
        __dmb();
    }

    return NES_PSRAM_MAX_SIZE;
}

static void __no_inline_not_in_flash_func(psram_set_timing)(void) {
    const int clock_hz = (int)clock_get_hz(clk_sys);
    const int max_psram_freq = 133 * 1000000;
    int divisor = (clock_hz + max_psram_freq - 1) / max_psram_freq;
    if (divisor == 1 && clock_hz > 100000000)
        divisor = 2;
    int rxdelay = divisor;
    if (clock_hz / divisor > 100000000)
        rxdelay += 1;

    const int64_t clock_period_fs = 1000000000000000ll / clock_hz;
    const int max_select = (125 * 1000000) / clock_period_fs;
    const int min_deselect =
        (18 * 1000000 + (clock_period_fs - 1)) / clock_period_fs - (divisor + 1) / 2;

    qmi_hw->m[1].timing =
        1u << QMI_M1_TIMING_COOLDOWN_LSB |
        QMI_M1_TIMING_PAGEBREAK_VALUE_1024 << QMI_M1_TIMING_PAGEBREAK_LSB |
        (uint32_t)max_select << QMI_M1_TIMING_MAX_SELECT_LSB |
        (uint32_t)min_deselect << QMI_M1_TIMING_MIN_DESELECT_LSB |
        (uint32_t)rxdelay << QMI_M1_TIMING_RXDELAY_LSB |
        (uint32_t)divisor << QMI_M1_TIMING_CLKDIV_LSB;
}

bool __no_inline_not_in_flash_func(nes_psram_init)(void) {
    const bool rp2350a =
        (*((io_ro_32 *)(SYSINFO_BASE + SYSINFO_PACKAGE_SEL_OFFSET)) & 1u) != 0;
    const uint cs_pin = rp2350a ? PSRAM_CS1_GPIO_RP2350A : PSRAM_CS1_GPIO_RP2350B;

    psram_available = false;
    psram_size = 0;
    psram_cs_gpio = -1;
    gpio_set_function(cs_pin, GPIO_FUNC_XIP_CS1);

    const uint32_t ints = save_and_disable_interrupts();
    qmi_hw->direct_csr = 30u << QMI_DIRECT_CSR_CLKDIV_LSB |
                         QMI_DIRECT_CSR_EN_BITS;
    while (qmi_hw->direct_csr & QMI_DIRECT_CSR_BUSY_BITS)
        ;

    if (!psram_direct_probe()) {
        qmi_hw->direct_csr = 0;
        restore_interrupts(ints);
        /* QMI CS1 can be a normal GPIO on boards without PSRAM (for
         * example NESpad CLK on Olimex).  Do not leave it claimed. */
        gpio_deinit(cs_pin);
        return false;
    }

    qmi_hw->direct_csr = 10u << QMI_DIRECT_CSR_CLKDIV_LSB |
                         QMI_DIRECT_CSR_EN_BITS |
                         QMI_DIRECT_CSR_AUTO_CS1N_BITS;
    while (qmi_hw->direct_csr & QMI_DIRECT_CSR_BUSY_BITS)
        ;
    qmi_hw->direct_tx = QMI_DIRECT_TX_NOPUSH_BITS | 0x35u;
    while (qmi_hw->direct_csr & QMI_DIRECT_CSR_BUSY_BITS)
        ;

    /* QMI direct mode also blocks XIP flash.  Exit before calling the
     * clock helpers used by psram_set_timing(). */
    qmi_hw->direct_csr = 0;
    psram_set_timing();

    qmi_hw->m[1].rfmt =
        QMI_M0_RFMT_PREFIX_WIDTH_VALUE_Q << QMI_M0_RFMT_PREFIX_WIDTH_LSB |
        QMI_M0_RFMT_ADDR_WIDTH_VALUE_Q << QMI_M0_RFMT_ADDR_WIDTH_LSB |
        QMI_M0_RFMT_SUFFIX_WIDTH_VALUE_Q << QMI_M0_RFMT_SUFFIX_WIDTH_LSB |
        QMI_M0_RFMT_DUMMY_WIDTH_VALUE_Q << QMI_M0_RFMT_DUMMY_WIDTH_LSB |
        QMI_M0_RFMT_DATA_WIDTH_VALUE_Q << QMI_M0_RFMT_DATA_WIDTH_LSB |
        QMI_M0_RFMT_PREFIX_LEN_VALUE_8 << QMI_M0_RFMT_PREFIX_LEN_LSB |
        6u << QMI_M0_RFMT_DUMMY_LEN_LSB;
    qmi_hw->m[1].rcmd = 0xEBu;

    qmi_hw->m[1].wfmt =
        QMI_M0_WFMT_PREFIX_WIDTH_VALUE_Q << QMI_M0_WFMT_PREFIX_WIDTH_LSB |
        QMI_M0_WFMT_ADDR_WIDTH_VALUE_Q << QMI_M0_WFMT_ADDR_WIDTH_LSB |
        QMI_M0_WFMT_SUFFIX_WIDTH_VALUE_Q << QMI_M0_WFMT_SUFFIX_WIDTH_LSB |
        QMI_M0_WFMT_DUMMY_WIDTH_VALUE_Q << QMI_M0_WFMT_DUMMY_WIDTH_LSB |
        QMI_M0_WFMT_DATA_WIDTH_VALUE_Q << QMI_M0_WFMT_DATA_WIDTH_LSB |
        QMI_M0_WFMT_PREFIX_LEN_VALUE_8 << QMI_M0_WFMT_PREFIX_LEN_LSB;
    qmi_hw->m[1].wcmd = 0x38u;

    hw_set_bits(&xip_ctrl_hw->ctrl, XIP_CTRL_WRITABLE_M1_BITS);
    restore_interrupts(ints);

    psram_size = psram_detect_size();
    psram_available = psram_size != 0;
    if (psram_available)
        psram_cs_gpio = (int)cs_pin;
    return psram_available;
}

bool nes_psram_available(void) {
    return psram_available;
}

size_t nes_psram_size(void) {
    return psram_size;
}

int nes_psram_cs1_gpio(void) {
    return psram_available ? psram_cs_gpio : -1;
}

void nes_psram_reclock(void) {
    if (psram_available)
        psram_set_timing();
}

void __no_inline_not_in_flash_func(nes_psram_prepare_reset)(void) {
    if (!psram_available)
        return;

    /* Stop mapped PSRAM transfers before watchdog resets QMI.  The PSRAM
     * must leave QPI/quad mode and CS1 must be released.  This code
     * executes from SRAM; interrupts are disabled by the caller. */
    qmi_hw->direct_csr = 30u << QMI_DIRECT_CSR_CLKDIV_LSB |
                         QMI_DIRECT_CSR_EN_BITS;
    while (qmi_hw->direct_csr & QMI_DIRECT_CSR_BUSY_BITS)
        ;
    qmi_hw->direct_csr |= QMI_DIRECT_CSR_ASSERT_CS1N_BITS;
    qmi_hw->direct_tx = QMI_DIRECT_TX_OE_BITS |
                        (QMI_DIRECT_TX_IWIDTH_VALUE_Q << QMI_DIRECT_TX_IWIDTH_LSB) |
                        0xF5u;
    while (qmi_hw->direct_csr & QMI_DIRECT_CSR_BUSY_BITS)
        ;
    (void)qmi_hw->direct_rx;
    qmi_hw->direct_csr &= ~QMI_DIRECT_CSR_ASSERT_CS1N_BITS;
    while (qmi_hw->direct_csr & QMI_DIRECT_CSR_BUSY_BITS)
        ;
    qmi_hw->direct_csr = 0;
    __dmb();
}

#else

bool nes_psram_init(void) {
    return false;
}

bool nes_psram_available(void) {
    return false;
}

size_t nes_psram_size(void) {
    return 0;
}

int nes_psram_cs1_gpio(void) {
    return -1;
}

void nes_psram_reclock(void) {
}

void nes_psram_prepare_reset(void) {
}

#endif
