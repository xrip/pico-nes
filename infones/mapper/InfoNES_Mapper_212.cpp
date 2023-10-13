/*===================================================================*/
/*                                                                   */
/*             Mapper 212 : BMC Super HiK 300-in-1                   */
/*                                                                   */
/*===================================================================*/

/*-------------------------------------------------------------------*/
/*  Initialize Mapper 212                                            */
/*-------------------------------------------------------------------*/
void Map212_Init() {
    /* Initialize Mapper */
    MapperInit = Map212_Init;

    /* Write to Mapper */
    MapperWrite = Map212_Write;

    /* Write to SRAM */
    MapperSram = Map0_Sram;

    /* Write to APU */
    MapperApu = Map0_Apu;

    /* Read from APU */
    MapperReadApu = Map0_ReadApu;

    /* Callback at VSync */
    MapperVSync = Map0_VSync;

    /* Callback at HSync */
    MapperHSync = Map0_HSync;

    /* Callback at PPU */
    MapperPPU = Map0_PPU;

    /* Callback at Rendering Screen ( 1:BG, 0:Sprite ) */
    MapperRenderScreen = Map0_RenderScreen;

    /* Set SRAM Banks */
    SRAMBANK = SRAM;

    /* Set ROM Banks */
    ROMBANK0 = ROMPAGE(0);
    ROMBANK1 = ROMPAGE(1);
    ROMBANK2 = ROMLASTPAGE(1);
    ROMBANK3 = ROMLASTPAGE(0);

    /* Set PPU Banks */
    if (NesHeader.byVRomSize > 0) {
        for (int nPage = 0; nPage < 8; ++nPage)
            PPUBANK[nPage] = VROMPAGE(nPage);
        InfoNES_SetupChr();
    }

    /* Set up wiring of the interrupt pin */
    K6502_Set_Int_Wiring(1, 1);
}

/*-------------------------------------------------------------------*/
/*  Mapper 212 Write Function                                        */
/*-------------------------------------------------------------------*/
void Map212_Write(WORD wAddr, BYTE byData) {
    /* Set ROM Banks */
    if ((wAddr & 0x4000) == 0x4000) {
        WORD value = ((wAddr & 6) >> 1) << 2;
        // When it's 1, BB is 32 KiB PRG bank at CPU $8000.
        ROMBANK0 = ROMPAGE(value % (NesHeader.byRomSize << 1));
        ROMBANK1 = ROMPAGE((value + 1) % (NesHeader.byRomSize << 1));
        ROMBANK2 = ROMPAGE((value + 2) % (NesHeader.byRomSize << 1));
        ROMBANK3 = ROMPAGE((value + 3) % (NesHeader.byRomSize << 1));
    } else {
        // 0, BBb specifies a 16 KiB PRG bank at both CPU $8000 and $C000
        WORD value = (wAddr & 7) << 1;
        ROMBANK0 = ROMPAGE(value % (NesHeader.byRomSize << 1));
        ROMBANK1 = ROMPAGE((value + 1) % (NesHeader.byRomSize << 1));
        ROMBANK2 = ROMPAGE((value) % (NesHeader.byRomSize << 1));
        ROMBANK3 = ROMPAGE((value + 1) % (NesHeader.byRomSize << 1));
    }

    /* Set PPU Banks */
    WORD value = (wAddr & 7) << 3;
    PPUBANK[0] = VROMPAGE(value % (NesHeader.byVRomSize<<3));
    PPUBANK[1] = VROMPAGE((value + 1) % (NesHeader.byVRomSize<<3));
    PPUBANK[2] = VROMPAGE((value + 2) % (NesHeader.byVRomSize<<3));
    PPUBANK[3] = VROMPAGE((value + 3) % (NesHeader.byVRomSize<<3));
    PPUBANK[4] = VROMPAGE((value + 4) % (NesHeader.byVRomSize<<3));
    PPUBANK[5] = VROMPAGE((value + 5) % (NesHeader.byVRomSize<<3));
    PPUBANK[6] = VROMPAGE((value + 6) % (NesHeader.byVRomSize<<3));
    PPUBANK[7] = VROMPAGE((value + 7) % (NesHeader.byVRomSize<<3));
    InfoNES_SetupChr();

    if (wAddr & 0x0008) {
        InfoNES_Mirroring(0);
    } else {
        InfoNES_Mirroring(1);
    }
}
