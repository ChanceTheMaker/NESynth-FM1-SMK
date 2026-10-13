#include "K6502.h"
extern BYTE np_prg_ram_enabled;
extern WORD PC;
extern BYTE F;
static void mmc1_bit(WORD address,BYTE value){np_cpu_clock_base+=3;MapperWrite(address,value);}
static void mmc1_reg(WORD address,BYTE value){for(unsigned i=0;i<5;i++)mmc1_bit(address,(value>>i)&1);}
static void test_mmc1(){
    std::vector<uint8_t> cart(16+256*1024+128*1024,0);
    memcpy(cart.data(),"NES\x1a",4);cart[4]=16;cart[5]=16;cart[6]=0x12;
    for(unsigned bank=0;bank<32;bank++)memset(cart.data()+16+bank*8192,bank,8192);
    for(unsigned bank=0;bank<32;bank++)memset(cart.data()+16+256*1024+bank*4096,bank,4096);
    assert(!gp_load(cart.data(),cart.size()));
    assert(ROMBANK0[0]==0&&ROMBANK1[0]==1&&ROMBANK2[0]==30&&ROMBANK3[0]==31);
    mmc1_reg(0xe000,3);assert(ROMBANK0[0]==6&&ROMBANK2[0]==30);
    mmc1_reg(0x8000,8);assert(ROMBANK0[0]==0&&ROMBANK2[0]==6);
    mmc1_reg(0x8000,0);assert(ROMBANK0[0]==4&&ROMBANK3[0]==7);
    mmc1_reg(0x8000,28);mmc1_reg(0xa000,5);mmc1_reg(0xc000,7);
    assert(PPUBANK[0][0]==5&&PPUBANK[4][0]==7);
    mmc1_reg(0x8000,12);assert(PPUBANK[0][0]==4&&PPUBANK[4][0]==5);
    const unsigned mirrors[4][4]={{0,0,0,0},{1,1,1,1},{0,1,0,1},{0,0,1,1}};
    for(unsigned mode=0;mode<4;mode++){
        mmc1_reg(0x8000,12|mode);
        for(unsigned i=0;i<4;i++)assert(PPUBANK[8+i]==PPURAM+0x2000+mirrors[mode][i]*1024);
    }
    mmc1_reg(0xe000,16);assert(!np_prg_ram_enabled);
    SRAM[0]=0x5a;const BYTE ramTest[]={0xa9,0x7b,0x8d,0,0x60,0xad,0,0x60,0x85,0x20};
    memcpy(RAM,ramTest,sizeof ramTest);PC=0;F=0x24;g_wPassedClocks=0;K6502_Step(13);
    assert(SRAM[0]==0x5a&&RAM[0x20]==0xff);
    mmc1_reg(0xe000,0);assert(np_prg_ram_enabled);
    // Any address can supply the first four bits; the fifth selects the register.
    mmc1_bit(0x8000,128);for(unsigned i=0;i<4;i++)mmc1_bit(0xa000,(2>>i)&1);
    mmc1_bit(0xe000,0);assert(ROMBANK0[0]==4);
    // Adjacent data writes are ignored, while reset is accepted on either cycle.
    mmc1_bit(0x8000,128);mmc1_bit(0xe000,1);
    np_cpu_clock_base++;MapperWrite(0xe000,0);
    for(unsigned i=0;i<4;i++)mmc1_bit(0xe000,0);
    assert(ROMBANK0[0]==2);
    mmc1_reg(0x8000,0);mmc1_bit(0x8000,0);np_cpu_clock_base++;MapperWrite(0x8000,128);
    assert(ROMBANK2[0]==30);
    // INC $FFFF must reset on its original $FF write and ignore the new $00.
    mmc1_reg(0x8000,0);ROMBANK3[8191]=0xff;
    RAM[0]=0xee;RAM[1]=RAM[2]=0xff;PC=0;F=0x24;g_wPassedClocks=0;K6502_Step(6);
    assert(ROMBANK2[0]==30);
    // Every register value must stay within the cartridge's bank storage.
    for(unsigned value=0;value<32;value++){
        mmc1_reg(0x8000,value);mmc1_reg(0xa000,value);mmc1_reg(0xc000,value);mmc1_reg(0xe000,value);
        assert(ROMBANK0>=ROM&&ROMBANK0+8192<=VROM);
        assert(ROMBANK3>=ROM&&ROMBANK3+8192<=VROM);
        for(unsigned i=0;i<8;i++)assert(PPUBANK[i]>=VROM&&PPUBANK[i]+1024<=cart.data()+cart.size());
    }
    gp_reset();assert(np_prg_ram_enabled&&ROMBANK0[0]==0&&ROMBANK2[0]==30);
    cart[5]=0;cart.resize(16+256*1024);assert(!gp_load(cart.data(),cart.size()));
    mmc1_reg(0x8000,28);mmc1_reg(0xa000,1);mmc1_reg(0xc000,0);
    assert(PPUBANK[0]==PPURAM+4096&&PPUBANK[4]==PPURAM);
    // CHR RAM changes must be visible on the very next row, not cached as ROM.
    PPU_R1=R1_SHOW_SCR|R1_CLIP_BG;PPU_BG_Base=ChrBuf;PPU_NameTableBank=8;PPU_Scanline=0;
    memset(PPUBANK[8],0,1024);for(unsigned i=0;i<32;i++)PalTable[i]=i;
    PPUBANK[0][0]=0xff;PPUBANK[0][8]=0;InfoNES_DrawLine();assert(WorkFrame[0]==1);
    PPUBANK[0][0]=0;PPUBANK[0][8]=0xff;InfoNES_DrawLine();assert(WorkFrame[0]==2);
    puts("PASS: MMC1 serial/reset, PRG/CHR modes, mirroring, RAM control, adjacent writes, bounds and uncached CHR RAM.");
}
