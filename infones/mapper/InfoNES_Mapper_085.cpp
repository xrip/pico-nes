/*===================================================================*/
/*                                                                   */
/*                    Mapper 85 (Konami VRC7)                        */
/*                                                                   */
/*===================================================================*/

/* VRC7 boards with CHR-RAM (Lagrange Point) carry 8 KB; CHR-ROM boards (Tiny Toon Adventures 2) use VROM */
BYTE Map85_Chr_Ram[0x2000];
BYTE Map85_Regs[1];
BYTE Map85_IRQ_Enable;
BYTE Map85_IRQ_Cnt;
BYTE Map85_IRQ_Latch;

/* The address of 1Kbytes unit of CHR: VROM if present, otherwise the 8 KB CHR-RAM */
#define Map85_VROMPAGE(a) (NesHeader.byVRomSize ? VROMPAGE((a) % (NesHeader.byVRomSize << 3)) : &Map85_Chr_Ram[((a)&0x07) * 0x400])

/*-------------------------------------------------------------------*/
/*  Initialize Mapper 85                                             */
/*-------------------------------------------------------------------*/
void Map85_Init()
{
  /* Initialize Mapper */
  MapperInit = Map85_Init;

  /* Write to Mapper */
  MapperWrite = Map85_Write;

  /* Write to SRAM */
  MapperSram = Map0_Sram;

  /* Write to APU */
  MapperApu = Map0_Apu;

  /* Read from APU */
  MapperReadApu = Map0_ReadApu;

  /* Callback at VSync */
  MapperVSync = Map0_VSync;

  /* Callback at HSync */
  MapperHSync = Map85_HSync;

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
  InfoNES_MemorySet(Map85_Chr_Ram, 0x00, sizeof(Map85_Chr_Ram));
  for (int nPage = 0; nPage < 8; ++nPage)
    PPUBANK[nPage] = Map85_VROMPAGE(nPage);
  InfoNES_SetupChr();

  /* Initialize State Registers */
  Map85_Regs[0] = 0;
  Map85_IRQ_Enable = 0;
  Map85_IRQ_Cnt = 0;
  Map85_IRQ_Latch = 0;

  /* Set up wiring of the interrupt pin */
  K6502_Set_Int_Wiring(1, 1);
}

/*-------------------------------------------------------------------*/
/*  Mapper 85 Write Function                                         */
/*-------------------------------------------------------------------*/
void Map85_Write(WORD wAddr, BYTE byData)
{
  /* VRC7a decodes the second register of each pair on A4 ($x010), VRC7b on A3 ($x008) */
  switch ((wAddr & 0xf000) | ((wAddr & 0x18) ? 0x10 : 0))
  {
  case 0x8000:
    byData %= (NesHeader.byRomSize << 1);
    ROMBANK0 = ROMPAGE(byData);
    break;

  case 0x8010:
    byData %= (NesHeader.byRomSize << 1);
    ROMBANK1 = ROMPAGE(byData);
    break;

  case 0x9000:
    byData %= (NesHeader.byRomSize << 1);
    ROMBANK2 = ROMPAGE(byData);
    break;

  case 0x9010: /* $9010 / $9030: FM sound registers (not emulated) */
    break;

  case 0xa000:
    PPUBANK[0] = Map85_VROMPAGE(byData);
    InfoNES_SetupChr();
    break;

  case 0xa010:
    PPUBANK[1] = Map85_VROMPAGE(byData);
    InfoNES_SetupChr();
    break;

  case 0xb000:
    PPUBANK[2] = Map85_VROMPAGE(byData);
    InfoNES_SetupChr();
    break;

  case 0xb010:
    PPUBANK[3] = Map85_VROMPAGE(byData);
    InfoNES_SetupChr();
    break;

  case 0xc000:
    PPUBANK[4] = Map85_VROMPAGE(byData);
    InfoNES_SetupChr();
    break;

  case 0xc010:
    PPUBANK[5] = Map85_VROMPAGE(byData);
    InfoNES_SetupChr();
    break;

  case 0xd000:
    PPUBANK[6] = Map85_VROMPAGE(byData);
    InfoNES_SetupChr();
    break;

  case 0xd010:
    PPUBANK[7] = Map85_VROMPAGE(byData);
    InfoNES_SetupChr();
    break;

  /* Name Table Mirroring */
  case 0xe000:
    switch (byData & 0x03)
    {
    case 0x00:
      InfoNES_Mirroring(1);
      break;
    case 0x01:
      InfoNES_Mirroring(0);
      break;
    case 0x02:
      InfoNES_Mirroring(3);
      break;
    case 0x03:
      InfoNES_Mirroring(2);
      break;
    }
    break;

  case 0xe010:
    Map85_IRQ_Latch = 0xff - byData;
    break;

  case 0xf000:
    Map85_Regs[0] = byData & 0x01;
    Map85_IRQ_Enable = (byData & 0x02) >> 1;
    Map85_IRQ_Cnt = Map85_IRQ_Latch;
    break;

  case 0xf010:
    Map85_IRQ_Enable = Map85_Regs[0];
    Map85_IRQ_Cnt = Map85_IRQ_Latch;
    break;
  }
}

/*-------------------------------------------------------------------*/
/*  Mapper 85 H-Sync Function                                        */
/*-------------------------------------------------------------------*/
void Map85_HSync()
{
  if (Map85_IRQ_Enable)
  {
    if (Map85_IRQ_Cnt == 0)
    {
      /* VRC IRQ counter reloads and keeps counting; $F010 acknowledges */
      IRQ_REQ;
      Map85_IRQ_Cnt = Map85_IRQ_Latch;
    }
    else
    {
      Map85_IRQ_Cnt--;
    }
  }
}
