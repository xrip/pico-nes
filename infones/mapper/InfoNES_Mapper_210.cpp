/*===================================================================*/
/*                                                                   */
/*     Mapper 210 (Namco 175 / Namco 340)                            */
/*                                                                   */
/*===================================================================*/

/* 0: not known yet, 1: Namco 175 (header mirroring), 2: Namco 340 ($E000 mirroring) */
BYTE Map210_Type;

/*-------------------------------------------------------------------*/
/*  Initialize Mapper 210                                            */
/*-------------------------------------------------------------------*/
void Map210_Init()
{
  /* Initialize Mapper */
  MapperInit = Map210_Init;

  /* Write to Mapper */
  MapperWrite = Map210_Write;

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

  /* NES 2.0 submapper 1 = Namco 175, 2 = Namco 340 */
  Map210_Type = 0;
  if ((NesHeader.byInfo2 & 0x0c) == 0x08)
  {
    BYTE bySub = NesHeader.byReserve[0] >> 4;
    if (bySub == 1 || bySub == 2)
      Map210_Type = bySub;
  }

  /* Set up wiring of the interrupt pin */
  K6502_Set_Int_Wiring(1, 1);
}

/*-------------------------------------------------------------------*/
/*  Mapper 210 Write Function                                        */
/*-------------------------------------------------------------------*/
void Map210_Write(WORD wAddr, BYTE byData)
{
  switch (wAddr & 0xf800)
  {
  /* $8000-$BFFF: eight 1 KB CHR banks, one per 2 KB of address space */
  case 0x8000: case 0x8800: case 0x9000: case 0x9800:
  case 0xa000: case 0xa800: case 0xb000: case 0xb800:
    if (NesHeader.byVRomSize > 0)
    {
      PPUBANK[(wAddr - 0x8000) >> 11] = VROMPAGE(byData % (NesHeader.byVRomSize << 3));
      InfoNES_SetupChr();
    }
    break;

  case 0xe000: /* [MMPP PPPP] PRG $8000, M: mirroring (Namco 340 only) */
    ROMBANK0 = ROMPAGE((byData & 0x3f) % (NesHeader.byRomSize << 1));
    /* Without a submapper: switch to 340 behaviour once a game sets the mirroring bits */
    if (Map210_Type == 0 && (byData & 0xc0))
      Map210_Type = 2;
    if (Map210_Type == 2)
    {
      switch (byData >> 6)
      {
      case 0: InfoNES_Mirroring(3); break; /* one-screen $2000 */
      case 1: InfoNES_Mirroring(1); break; /* vertical */
      case 2: InfoNES_Mirroring(2); break; /* one-screen $2400 */
      case 3: InfoNES_Mirroring(0); break; /* horizontal */
      }
    }
    break;

  case 0xe800: /* PRG $A000 */
    ROMBANK1 = ROMPAGE((byData & 0x3f) % (NesHeader.byRomSize << 1));
    break;

  case 0xf000: /* PRG $C000 */
    ROMBANK2 = ROMPAGE((byData & 0x3f) % (NesHeader.byRomSize << 1));
    break;
  }
}
