/*===================================================================*/
/*                                                                   */
/*     Mapper 153 (Bandai LZ93D50 + 8 KB WRAM, 512 KB PRG)           */
/*                                                                   */
/*===================================================================*/

BYTE  Map153_Outer;   /* PRG A18, taken from bit 0 of the CHR registers */
BYTE  Map153_Prg;
BYTE  Map153_IRQ_Enable;
DWORD Map153_IRQ_Cnt;
DWORD Map153_IRQ_Latch;

/*-------------------------------------------------------------------*/
/*  Mapper 153 helpers                                               */
/*-------------------------------------------------------------------*/
static void Map153_Sync_Prg()
{
  DWORD dwPages = NesHeader.byRomSize << 1;
  DWORD dwBank = ((Map153_Outer << 4) | (Map153_Prg & 0x0f)) << 1;
  DWORD dwLast = ((Map153_Outer << 4) | 0x0f) << 1;
  ROMBANK0 = ROMPAGE(dwBank % dwPages);
  ROMBANK1 = ROMPAGE((dwBank + 1) % dwPages);
  ROMBANK2 = ROMPAGE(dwLast % dwPages);
  ROMBANK3 = ROMPAGE((dwLast + 1) % dwPages);
}

/*-------------------------------------------------------------------*/
/*  Initialize Mapper 153                                            */
/*-------------------------------------------------------------------*/
void Map153_Init()
{
  /* Initialize Mapper */
  MapperInit = Map153_Init;

  /* Write to Mapper */
  MapperWrite = Map153_Write;

  /* Write to SRAM */
  MapperSram = Map0_Sram;

  /* Write to APU */
  MapperApu = Map0_Apu;

  /* Read from APU */
  MapperReadApu = Map0_ReadApu;

  /* Callback at VSync */
  MapperVSync = Map0_VSync;

  /* Callback at HSync */
  MapperHSync = Map153_HSync;

  /* Callback at PPU */
  MapperPPU = Map0_PPU;

  /* Callback at Rendering Screen ( 1:BG, 0:Sprite ) */
  MapperRenderScreen = Map0_RenderScreen;

  /* Set SRAM Banks */
  SRAMBANK = SRAM;

  /* Set ROM Banks */
  Map153_Outer = 0;
  Map153_Prg = 0;
  Map153_Sync_Prg();

  /* CHR is 8 KB of RAM (the default PPU banks) */

  Map153_IRQ_Enable = 0;
  Map153_IRQ_Cnt = 0;
  Map153_IRQ_Latch = 0;

  /* Set up wiring of the interrupt pin */
  K6502_Set_Int_Wiring(1, 1);
}

/*-------------------------------------------------------------------*/
/*  Mapper 153 Write Function                                        */
/*-------------------------------------------------------------------*/
void Map153_Write(WORD wAddr, BYTE byData)
{
  switch (wAddr & 0x000f)
  {
  case 0x0000: case 0x0001: case 0x0002: case 0x0003:
  case 0x0004: case 0x0005: case 0x0006: case 0x0007:
    Map153_Outer = byData & 0x01;
    Map153_Sync_Prg();
    break;

  case 0x0008:
    Map153_Prg = byData;
    Map153_Sync_Prg();
    break;

  case 0x0009:
    switch (byData & 0x03)
    {
    case 0x00: InfoNES_Mirroring(1); break;
    case 0x01: InfoNES_Mirroring(0); break;
    case 0x02: InfoNES_Mirroring(3); break;
    case 0x03: InfoNES_Mirroring(2); break;
    }
    break;

  case 0x000a:
    Map153_IRQ_Enable = byData & 0x01;
    Map153_IRQ_Cnt = Map153_IRQ_Latch;
    break;

  case 0x000b:
    Map153_IRQ_Latch = (Map153_IRQ_Latch & 0xff00) | byData;
    break;

  case 0x000c:
    Map153_IRQ_Latch = ((DWORD)byData << 8) | (Map153_IRQ_Latch & 0x00ff);
    break;

  case 0x000d:
    /* bit 5: WRAM enable (always readable here) */
    break;
  }
}

/*-------------------------------------------------------------------*/
/*  Mapper 153 H-Sync Function                                       */
/*-------------------------------------------------------------------*/
void Map153_HSync()
{
  if (Map153_IRQ_Enable)
  {
    if (Map153_IRQ_Cnt <= 114)
    {
      IRQ_REQ;
      Map153_IRQ_Cnt = 0;
      Map153_IRQ_Enable = 0;
    }
    else
    {
      Map153_IRQ_Cnt -= 114;
    }
  }
}
