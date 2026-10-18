/*===================================================================*/
/*                                                                   */
/*       Mapper 207 (Taito X1-005, alternate nametable wiring)      */
/*                                                                   */
/*===================================================================*/

/*-------------------------------------------------------------------*/
/*  Initialize Mapper 207                                            */
/*-------------------------------------------------------------------*/
void Map207_Init()
{
  /* Initialize Mapper */
  MapperInit = Map207_Init;

  /* Write to Mapper */
  MapperWrite = Map0_Write;

  /* Write to SRAM */
  MapperSram = Map207_Sram;

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

  /* Set up wiring of the interrupt pin */
  K6502_Set_Int_Wiring(1, 1);
}

/*-------------------------------------------------------------------*/
/*  Mapper 207 Write to SRAM Function                                */
/*-------------------------------------------------------------------*/
void Map207_Sram(WORD wAddr, BYTE byData)
{
  DWORD dwVRom = NesHeader.byVRomSize << 3;

  switch (wAddr)
  {
  case 0x7ef0: /* 2 KB CHR at $0000, bit 7: nametable for $2000/$2400 */
  case 0x7ef1: /* 2 KB CHR at $0800, bit 7: nametable for $2800/$2C00 */
  {
    int nPage = (wAddr & 1) << 1;
    if (dwVRom)
    {
      PPUBANK[nPage] = VROMPAGE(((byData & 0x7e) + 0) % dwVRom);
      PPUBANK[nPage + 1] = VROMPAGE(((byData & 0x7e) + 1) % dwVRom);
      InfoNES_SetupChr();
    }
    PPUBANK[NAME_TABLE0 + nPage] = VRAMPAGE(byData >> 7);
    PPUBANK[NAME_TABLE0 + nPage + 1] = VRAMPAGE(byData >> 7);
    break;
  }
  case 0x7ef2: case 0x7ef3: case 0x7ef4: case 0x7ef5: /* 1 KB CHR at $1000-$1FFF */
    if (dwVRom)
    {
      PPUBANK[(wAddr - 0x7ef2) + 4] = VROMPAGE(byData % dwVRom);
      InfoNES_SetupChr();
    }
    break;
  case 0x7efa: case 0x7efb: /* 8 KB PRG at $8000 */
    ROMBANK0 = ROMPAGE(byData % (NesHeader.byRomSize << 1));
    break;
  case 0x7efc: case 0x7efd: /* 8 KB PRG at $A000 */
    ROMBANK1 = ROMPAGE(byData % (NesHeader.byRomSize << 1));
    break;
  case 0x7efe: case 0x7eff: /* 8 KB PRG at $C000 */
    ROMBANK2 = ROMPAGE(byData % (NesHeader.byRomSize << 1));
    break;
  }
}
