/*===================================================================*/
/*                                                                   */
/*       Mapper 156 (Daou DIS23C01)                                 */
/*                                                                   */
/*===================================================================*/

BYTE Map156_Chr[8][2]; /* low / high byte of each 1 KB CHR bank */

/*-------------------------------------------------------------------*/
/*  Mapper 156 helpers                                               */
/*-------------------------------------------------------------------*/
static void Map156_SetChr(int nPage)
{
  DWORD dwVRom = NesHeader.byVRomSize << 3;
  if (dwVRom)
  {
    PPUBANK[nPage] = VROMPAGE((Map156_Chr[nPage][0] | (Map156_Chr[nPage][1] << 8)) % dwVRom);
    InfoNES_SetupChr();
  }
}

/*-------------------------------------------------------------------*/
/*  Initialize Mapper 156                                            */
/*-------------------------------------------------------------------*/
void Map156_Init()
{
  /* Initialize Mapper */
  MapperInit = Map156_Init;

  /* Write to Mapper */
  MapperWrite = Map156_Write;

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

  /* Set ROM Banks: 16 KB switchable at $8000, last 16 KB fixed */
  ROMBANK0 = ROMPAGE(0);
  ROMBANK1 = ROMPAGE(1);
  ROMBANK2 = ROMLASTPAGE(1);
  ROMBANK3 = ROMLASTPAGE(0);

  /* Set PPU Banks */
  for (int nPage = 0; nPage < 8; ++nPage)
  {
    Map156_Chr[nPage][0] = nPage;
    Map156_Chr[nPage][1] = 0;
    Map156_SetChr(nPage);
  }

  /* Single-screen until the game selects otherwise */
  InfoNES_Mirroring(3);

  /* Set up wiring of the interrupt pin */
  K6502_Set_Int_Wiring(1, 1);
}

/*-------------------------------------------------------------------*/
/*  Mapper 156 Write Function                                        */
/*-------------------------------------------------------------------*/
void Map156_Write(WORD wAddr, BYTE byData)
{
  switch (wAddr)
  {
  /* $C000-$C003 / $C004-$C007: low / high bytes of CHR banks 0-3 */
  case 0xc000: case 0xc001: case 0xc002: case 0xc003:
  case 0xc004: case 0xc005: case 0xc006: case 0xc007:
    Map156_Chr[wAddr & 0x03][(wAddr >> 2) & 1] = byData;
    Map156_SetChr(wAddr & 0x03);
    break;

  /* $C008-$C00B / $C00C-$C00F: low / high bytes of CHR banks 4-7 */
  case 0xc008: case 0xc009: case 0xc00a: case 0xc00b:
  case 0xc00c: case 0xc00d: case 0xc00e: case 0xc00f:
    Map156_Chr[(wAddr & 0x03) + 4][(wAddr >> 2) & 1] = byData;
    Map156_SetChr((wAddr & 0x03) + 4);
    break;

  case 0xc010: /* 16 KB PRG bank at $8000 */
    byData = (byData << 1) % (NesHeader.byRomSize << 1);
    ROMBANK0 = ROMPAGE(byData);
    ROMBANK1 = ROMPAGE(byData + 1);
    break;

  case 0xc014: /* mirroring */
    InfoNES_Mirroring((byData & 0x01) ? 0 : 1);
    break;
  }
}
