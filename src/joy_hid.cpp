#include <host/usbh.h>
#include "xinput_host.h"

struct input_bits_t {
    bool a: true;
    bool b: true;
    bool select: true;
    bool start: true;
    bool right: true;
    bool left: true;
    bool up: true;
    bool down: true;
};

#include "main.h"
extern SETTINGS settings;

extern input_bits_t gamepad1_bits;

//Since https://github.com/hathach/tinyusb/pull/2222, we can add in custom vendor drivers easily
usbh_class_driver_t const* usbh_app_driver_get_cb(uint8_t* driver_count) {
    *driver_count = 1;
    return &usbh_xinput_driver;
}

void tuh_xinput_report_received_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* report, uint16_t len) {
    auto xid_itf = (xinputh_interface_t *)report;
    const xinput_gamepad_t* p = &xid_itf->pad;

    if (settings.swap_ab) {
        gamepad1_bits.b = p->wButtons & XINPUT_GAMEPAD_A;
        gamepad1_bits.a = p->wButtons & XINPUT_GAMEPAD_B;
    } else {
        gamepad1_bits.a = p->wButtons & XINPUT_GAMEPAD_A;
        gamepad1_bits.b = p->wButtons & XINPUT_GAMEPAD_B;
    }


    if (p->wButtons & XINPUT_GAMEPAD_GUIDE) {
        gamepad1_bits.start = true;
        gamepad1_bits.select = true;
    }
    else {
        gamepad1_bits.start = p->wButtons & XINPUT_GAMEPAD_START;
        gamepad1_bits.select = p->wButtons & XINPUT_GAMEPAD_BACK;
    }
    const uint8_t dpad = p->wButtons & 0xf;

    if (!dpad) {
        gamepad1_bits.up = p->sThumbLY > 0 || p->sThumbRY > 0;
        gamepad1_bits.down = p->sThumbLY < 0 || p->sThumbRY < 0;

        gamepad1_bits.right = p->sThumbLX > 0 || p->sThumbRX > 0;
        gamepad1_bits.left = p->sThumbLX < 0 || p->sThumbRX < 0;
    }
    else {
        gamepad1_bits.down = dpad & XINPUT_GAMEPAD_DPAD_DOWN;
        gamepad1_bits.up = dpad & XINPUT_GAMEPAD_DPAD_UP;
        gamepad1_bits.left = dpad & XINPUT_GAMEPAD_DPAD_LEFT;
        gamepad1_bits.right = dpad & XINPUT_GAMEPAD_DPAD_RIGHT;
    }

    /*char tmp[128];
    sprintf(tmp, "[%02x, %02x], Type: %s, Buttons %04x, LT: %02x RT: %02x, LX: %d, LY: %d, RX: %d, RY: %d\n",
                 dev_addr, instance, type_str, p->wButtons, p->bLeftTrigger, p->bRightTrigger, p->sThumbLX, p->sThumbLY, p->sThumbRX, p->sThumbRY);
    draw_text(tmp, 0,0, 15,0);*/

    tuh_xinput_receive_report(dev_addr, instance);
}

void tuh_xinput_mount_cb(uint8_t dev_addr, uint8_t instance, const xinputh_interface_t* xinput_itf) {
    TU_LOG1("XINPUT MOUNTED %02x %d\n", dev_addr, instance);
    // If this is a Xbox 360 Wireless controller we need to wait for a connection packet
    // on the in pipe before setting LEDs etc. So just start getting data until a controller is connected.
    if (xinput_itf->type == XBOX360_WIRELESS && xinput_itf->connected == false) {
        tuh_xinput_receive_report(dev_addr, instance);
        return;
    }

    tuh_xinput_set_led(dev_addr, instance, 0, true);
    tuh_xinput_set_led(dev_addr, instance, 1, true);
    tuh_xinput_set_rumble(dev_addr, instance, 0, 0, true);
    tuh_xinput_receive_report(dev_addr, instance);
}

void tuh_xinput_umount_cb(uint8_t dev_addr, uint8_t instance) {
    TU_LOG1("XINPUT UNMOUNTED %02x %d\n", dev_addr, instance);
}
