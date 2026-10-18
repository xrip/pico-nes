/*===================================================================*/
/*                                                                   */
/*       Mapper 30 (UNROM 512, homebrew; 32 KB CHR-RAM)            */
/*                                                                   */
/*===================================================================*/

/* 4 x 8 KB of CHR-RAM, selected by bits 5-6 */
BYTE Map30_Chr_Ram[0x8000];
BYTE Map30_OneScreen;

/*-------------------------------------------------------------------*/
/*  Initialize Mapper 30                                             */
/*-------------------------------------------------------------------*/
void Map30_Init()
{
  /* Initialize Mapper */
  MapperInit = Map30_Init;

  /* Write to Mapper */
  MapperWrite = Map30_Write;

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
  InfoNES_MemorySet(Map30_Chr_Ram, 0x00, sizeof(Map30_Chr_Ram));
  for (int nPage = 0; nPage < 8; ++nPage)
    PPUBANK[nPage] = &Map30_Chr_Ram[nPage * 0x400];
  InfoNES_SetupChr();

  /* Header flags 6 bits 3,0 = %10: one-screen mirroring selected by bit 7 */
  Map30_OneScreen = ((NesHeader.byInfo1 & 0x09) == 0x08);
  if (Map30_OneScreen)
    InfoNES_Mirroring(3);

  /* Set up wiring of the interrupt pin */
  K6502_Set_Int_Wiring(1, 1);
}

/*-------------------------------------------------------------------*/
/*  Mapper 30 Write Function                                         */
/*-------------------------------------------------------------------*/
void Map30_Write(WORD wAddr, BYTE byData)
{
  /* [MCCP PPPP] M: one-screen select, C: 8 KB CHR-RAM bank, P: 16 KB PRG bank */
  BYTE byPrg = ((byData & 0x1f) << 1) % (NesHeader.byRomSize << 1);
  ROMBANK0 = ROMPAGE(byPrg);
  ROMBANK1 = ROMPAGE(byPrg + 1);

  BYTE *pChr = &Map30_Chr_Ram[((byData >> 5) & 0x03) * 0x2000];
  for (int nPage = 0; nPage < 8; ++nPage)
    PPUBANK[nPage] = pChr + nPage * 0x400;
  InfoNES_SetupChr();

  if (Map30_OneScreen)
    InfoNES_Mirroring((byData & 0x80) ? 2 : 3);
}
