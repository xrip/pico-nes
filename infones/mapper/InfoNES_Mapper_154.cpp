/*===================================================================*/
/*                                                                   */
/*       Mapper 154 (Namco 108 + one-screen mirroring)              */
/*                                                                   */
/*===================================================================*/

BYTE Map154_Reg;

/*-------------------------------------------------------------------*/
/*  Initialize Mapper 154                                            */
/*-------------------------------------------------------------------*/
void Map154_Init()
{
  /* Initialize Mapper */
  MapperInit = Map154_Init;

  /* Write to Mapper */
  MapperWrite = Map154_Write;

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
  if (NesHeader.byVRomSize > 0)
  {
    for (int nPage = 0; nPage < 8; ++nPage)
      PPUBANK[nPage] = VROMPAGE(nPage);
    InfoNES_SetupChr();
  }

  Map154_Reg = 0;
  InfoNES_Mirroring(3);

  /* Set up wiring of the interrupt pin */
  K6502_Set_Int_Wiring(1, 1);
}

/*-------------------------------------------------------------------*/
/*  Mapper 154 Write Function                                        */
/*-------------------------------------------------------------------*/
void Map154_Write(WORD wAddr, BYTE byData)
{
  /* bit 6 of every write selects the nametable (one-screen) */
  InfoNES_Mirroring((byData & 0x40) ? 2 : 3);

  if (wAddr >= 0xa000)
    return;

  if (!(wAddr & 1))
  {
    Map154_Reg = byData;
    return;
  }

  DWORD dwVRom = NesHeader.byVRomSize << 3;
  switch (Map154_Reg & 0x07)
  {
  case 0x00: /* 2 KB at $0000 */
    PPUBANK[0] = VROMPAGE(((byData & 0x3e) + 0) % dwVRom);
    PPUBANK[1] = VROMPAGE(((byData & 0x3e) + 1) % dwVRom);
    InfoNES_SetupChr();
    break;
  case 0x01: /* 2 KB at $0800 */
    PPUBANK[2] = VROMPAGE(((byData & 0x3e) + 0) % dwVRom);
    PPUBANK[3] = VROMPAGE(((byData & 0x3e) + 1) % dwVRom);
    InfoNES_SetupChr();
    break;
  case 0x02: /* 1 KB at $1000-$1FFF, CHR A16 forced high */
  case 0x03:
  case 0x04:
  case 0x05:
    PPUBANK[(Map154_Reg & 0x07) + 2] = VROMPAGE(((byData & 0x3f) | 0x40) % dwVRom);
    InfoNES_SetupChr();
    break;
  case 0x06:
    ROMBANK0 = ROMPAGE((byData & 0x0f) % (NesHeader.byRomSize << 1));
    break;
  case 0x07:
    ROMBANK1 = ROMPAGE((byData & 0x0f) % (NesHeader.byRomSize << 1));
    break;
  }
}
