/*===================================================================*/
/*                                                                   */
/*       Mapper 190 (Magic Kid GooGoo)                              */
/*                                                                   */
/*===================================================================*/

/*-------------------------------------------------------------------*/
/*  Initialize Mapper 190                                            */
/*-------------------------------------------------------------------*/
void Map190_Init()
{
  /* Initialize Mapper */
  MapperInit = Map190_Init;

  /* Write to Mapper */
  MapperWrite = Map190_Write;

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

  /* Set ROM Banks: 16 KB switchable at $8000, first 16 KB fixed at $C000 */
  ROMBANK0 = ROMPAGE(0);
  ROMBANK1 = ROMPAGE(1);
  ROMBANK2 = ROMPAGE(0);
  ROMBANK3 = ROMPAGE(1);

  /* Set PPU Banks */
  if (NesHeader.byVRomSize > 0)
  {
    for (int nPage = 0; nPage < 8; ++nPage)
      PPUBANK[nPage] = VROMPAGE(nPage);
    InfoNES_SetupChr();
  }

  InfoNES_Mirroring(1);

  /* Set up wiring of the interrupt pin */
  K6502_Set_Int_Wiring(1, 1);
}

/*-------------------------------------------------------------------*/
/*  Mapper 190 Write Function                                        */
/*-------------------------------------------------------------------*/
void Map190_Write(WORD wAddr, BYTE byData)
{
  switch (wAddr >> 12)
  {
  case 0x8: case 0x9: /* PRG bank 0-7 */
  case 0xc: case 0xd: /* PRG bank 8-15 */
  {
    BYTE byPrg = ((((wAddr >> 11) & 8) | (byData & 7)) << 1) % (NesHeader.byRomSize << 1);
    ROMBANK0 = ROMPAGE(byPrg);
    ROMBANK1 = ROMPAGE(byPrg + 1);
    break;
  }
  case 0xa: case 0xb: /* 2 KB CHR banks */
  {
    DWORD dwVRom = NesHeader.byVRomSize << 3;
    if (dwVRom)
    {
      int nPage = (wAddr & 3) << 1;
      DWORD dwBank = ((DWORD)byData << 1) % dwVRom;
      PPUBANK[nPage] = VROMPAGE(dwBank);
      PPUBANK[nPage + 1] = VROMPAGE(dwBank + 1);
      InfoNES_SetupChr();
    }
    break;
  }
  }
}
