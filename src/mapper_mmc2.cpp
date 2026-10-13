/* SPDX-License-Identifier: GPL-3.0-only
 * MMC2 / mapper 9 (PxROM). Register and latch behavior:
 * https://www.nesdev.org/wiki/MMC2
 * Fetch callbacks run AFTER the triggering pattern byte has been read.
 */
static BYTE mmc2_chr[4],mmc2_latch[2];
#if defined(GB_HARDWARE)
#define MMC2_RAM __attribute__((section(".dsp_text")))
#else
#define MMC2_RAM
#endif
static void MMC2_RAM mmc2_sync_ram(unsigned half){
    unsigned bank=mmc2_chr[half*2+mmc2_latch[half]]&(NesHeader.byVRomSize*2-1);
    for(unsigned i=0;i<4;i++)PPUBANK[half*4+i]=VROMPAGE(bank*4+i);
    // MMC2 rows are decoded at the actual fetch, after choosing the current bank.
}
static void mmc2_sync(unsigned half){void (*volatile fn)(unsigned)=mmc2_sync_ram;fn(half);}
void Map9_Write(WORD address,BYTE data){
    unsigned reg=address>>12;
    if(reg==0xa)ROMBANK0=ROMPAGE((data&15)%(NesHeader.byRomSize*2));
    else if(reg>=0xb&&reg<=0xe){
        unsigned index=reg-0xb;mmc2_chr[index]=data&31;
        if(mmc2_latch[index/2]==(index&1))mmc2_sync(index/2);
    }else if(reg==0xf)InfoNES_Mirroring((data&1)?0:1);
}
void MMC2_RAM Map9_PPU(WORD address){
    unsigned half,value;
    if(address==0x0fd8){half=0;value=0;}
    else if(address==0x0fe8){half=0;value=1;}
    else if((address&0x1ff8)==0x1fd8&&address<0x2000){half=1;value=0;}
    else if((address&0x1ff8)==0x1fe8&&address<0x2000){half=1;value=1;}
    else return;
    if(mmc2_latch[half]!=value){mmc2_latch[half]=value;mmc2_sync_ram(half);}
}
void Map9_Init(){
    Map0_Init();MapperInit=Map9_Init;MapperWrite=Map9_Write;MapperPPU=Map9_PPU;
    ROMBANK0=ROMPAGE(0);ROMBANK1=ROMLASTPAGE(2);ROMBANK2=ROMLASTPAGE(1);ROMBANK3=ROMLASTPAGE(0);
    for(unsigned i=0;i<4;i++)mmc2_chr[i]=0;
    mmc2_latch[0]=mmc2_latch[1]=1;mmc2_sync(0);mmc2_sync(1);
}
