/*===================================================================*/
/*                                                                   */
/*       Mapper 184 (Sunsoft-1)                                     */
/*                                                                   */
/*===================================================================*/

/*-------------------------------------------------------------------*/
/*  Initialize Mapper 184                                            */
/*-------------------------------------------------------------------*/
void Map184_Init()
{
  /* Initialize Mapper */
  MapperInit = Map184_Init;

  /* Write to Mapper */
  MapperWrite = Map0_Write;

  /* Write to SRAM */
  MapperSram = Map184_Sram;

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

  /* Set ROM Banks: 32 KB fixed */
  ROMBANK0 = ROMPAGE(0);
  ROMBANK1 = ROMPAGE(1);
  ROMBANK2 = ROMPAGE(2 % (NesHeader.byRomSize << 1));
  ROMBANK3 = ROMPAGE(3 % (NesHeader.byRomSize << 1));

  /* Set PPU Banks */
  Map184_Sram(0x6000, 0x00);

  /* Set up wiring of the interrupt pin */
  K6502_Set_Int_Wiring(1, 1);
}

/*-------------------------------------------------------------------*/
/*  Mapper 184 Write to SRAM Function                                */
/*-------------------------------------------------------------------*/
void Map184_Sram(WORD wAddr, BYTE byData)
{
  /* $6000-$7FFF: [.HHH .LLL] 4 KB CHR banks at $1000 (H) and $0000 (L) */
  DWORD dwVRom = NesHeader.byVRomSize << 3;
  if (!dwVRom)
    return;
  DWORD dwLo = ((byData & 0x07) << 2) % dwVRom;
  DWORD dwHi = (((byData >> 4) & 0x07) << 2) % dwVRom;
  for (int i = 0; i < 4; ++i)
  {
    PPUBANK[i] = VROMPAGE(dwLo + i);
    PPUBANK[i + 4] = VROMPAGE(dwHi + i);
  }
  InfoNES_SetupChr();
}
