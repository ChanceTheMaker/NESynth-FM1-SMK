/* SPDX-License-Identifier: GPL-3.0-only
 * Standard MMC3B/C banking and RAM protection. IRQs use the core's scanline
 * clock, not cycle-exact PPU A12 edges. MMC3A, MMC6 and clone variants are
 * not modeled. https://www.nesdev.org/wiki/MMC3
 */
static BYTE mmc3_select,mmc3_banks[8],mmc3_latch,mmc3_counter;
static bool mmc3_reload,mmc3_irq_enabled;
extern BYTE IRQ_Wiring;
BYTE np_mapper_irq_pending=0;
static void mmc3_sync(){
    unsigned last=NesHeader.byRomSize*2-1;
    unsigned a=(mmc3_banks[6]&63)&last,b=(mmc3_banks[7]&63)&last;
    ROMBANK0=ROMPAGE((mmc3_select&64)?last-1:a);
    ROMBANK1=ROMPAGE(b);
    ROMBANK2=ROMPAGE((mmc3_select&64)?a:last-1);
    ROMBANK3=ROMPAGE(last);
    unsigned mask=NesHeader.byVRomSize?NesHeader.byVRomSize*8-1:7;
    BYTE *base=NesHeader.byVRomSize?VROM:PPURAM;
    unsigned banks[8]={unsigned(mmc3_banks[0]&254),unsigned(mmc3_banks[0]|1),
        unsigned(mmc3_banks[1]&254),unsigned(mmc3_banks[1]|1),
        mmc3_banks[2],mmc3_banks[3],mmc3_banks[4],mmc3_banks[5]};
    for(unsigned i=0;i<8;i++)PPUBANK[i^((mmc3_select&128)?4:0)]=base+(banks[i]&mask)*1024;
}
void Map4_Write(WORD address,BYTE data){
    switch(address&0xe001){
    case 0x8000:mmc3_select=data;mmc3_sync();break;
    case 0x8001:mmc3_banks[mmc3_select&7]=data;mmc3_sync();break;
    case 0xa000:if(!ROM_FourScr)InfoNES_Mirroring((data&1)?0:1);break;
    case 0xa001:np_prg_ram_enabled=!!(data&128);np_prg_ram_writable=!(data&64);break;
    case 0xc000:mmc3_latch=data;break;
    case 0xc001:mmc3_reload=true;break;
    case 0xe000:
        mmc3_irq_enabled=false;np_mapper_irq_pending=0;
        if(!(APU_Reg[0x15]&0x40))IRQ_State=IRQ_Wiring;
        break;
    case 0xe001:mmc3_irq_enabled=true;break;
    }
}
void Map4_HSync(){
    // One qualified rendering clock per visible/pre-render scanline. This
    // remains active when pixel drawing is skipped to keep hardware IRQ pace.
    if((PPU_Scanline<240||PPU_Scanline==261)&&(PPU_R1&(R1_SHOW_SCR|R1_SHOW_SP))){
        if(!mmc3_counter||mmc3_reload)mmc3_counter=mmc3_latch;
        else --mmc3_counter;
        mmc3_reload=false;
        if(!mmc3_counter&&mmc3_irq_enabled)np_mapper_irq_pending=1;
    }
    if(np_mapper_irq_pending){IRQ_REQ;}
}
void Map4_Init(){
    Map0_Init();MapperInit=Map4_Init;MapperWrite=Map4_Write;MapperHSync=Map4_HSync;
    mmc3_select=mmc3_latch=mmc3_counter=0;
    mmc3_reload=mmc3_irq_enabled=false;np_mapper_irq_pending=0;
    for(unsigned i=0;i<8;i++)mmc3_banks[i]=0;
    mmc3_banks[7]=1;np_prg_ram_enabled=np_prg_ram_writable=1;mmc3_sync();
}
