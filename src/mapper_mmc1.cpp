/* SPDX-License-Identifier: GPL-3.0-only
 * Standard MMC1B: up to 256 KiB PRG, 128 KiB CHR ROM or 8 KiB CHR RAM,
 * and one 8 KiB PRG-RAM bank. Extended SxROM boards are not supported.
 * Register reference: https://www.nesdev.org/wiki/MMC1 */
static BYTE mmc1_shift,mmc1_control,mmc1_chr[2],mmc1_prg;
static DWORD mmc1_last_write;
static bool mmc1_wrote;
BYTE np_prg_ram_enabled=1;
BYTE np_prg_ram_writable=1;
static void mmc1_sync(){
    static const BYTE mirror[4]={3,2,1,0};
    InfoNES_Mirroring(mirror[mmc1_control&3]);
    unsigned mask=NesHeader.byRomSize-1,bank=mmc1_prg&mask;
    unsigned mode=(mmc1_control>>2)&3,lo,hi;
    if(mode<2){lo=bank&~1u;hi=(lo+1)&mask;}
    else if(mode==2){lo=0;hi=bank;}
    else {lo=bank;hi=mask;}
    ROMBANK0=ROMPAGE(lo*2);ROMBANK1=ROMPAGE(lo*2+1);
    ROMBANK2=ROMPAGE(hi*2);ROMBANK3=ROMPAGE(hi*2+1);
    unsigned chrMask=NesHeader.byVRomSize?NesHeader.byVRomSize*2-1:1;
    unsigned a=mmc1_chr[0],b=mmc1_chr[1];
    if(!(mmc1_control&16)){a&=~1u;b=a+1;}
    a&=chrMask;b&=chrMask;
    BYTE *base=NesHeader.byVRomSize?VROM:PPURAM;
    for(unsigned i=0;i<4;i++){PPUBANK[i]=base+a*4096+i*1024;PPUBANK[4+i]=base+b*4096+i*1024;}
    np_prg_ram_enabled=!(mmc1_prg&16);
}
void Map1_Write(WORD address,BYTE data){
    DWORD now=np_cpu_clock_base+g_wPassedClocks;
    bool adjacent=mmc1_wrote&&(DWORD)(now-mmc1_last_write)==1;
    mmc1_last_write=now;mmc1_wrote=true;
    if(data&128){mmc1_shift=16;mmc1_control|=12;mmc1_sync();return;}
    if(adjacent)return;
    bool complete=mmc1_shift&1;
    mmc1_shift=(mmc1_shift>>1)|((data&1)<<4);
    if(!complete)return;
    unsigned reg=(address>>13)&3;
    if(reg==0)mmc1_control=mmc1_shift;
    else if(reg<3)mmc1_chr[reg-1]=mmc1_shift;
    else mmc1_prg=mmc1_shift;
    mmc1_shift=16;mmc1_sync();
}
void Map1_Init(){
    Map0_Init();MapperInit=Map1_Init;MapperWrite=Map1_Write;
    mmc1_shift=16;mmc1_control=12;mmc1_chr[0]=mmc1_chr[1]=mmc1_prg=0;
    mmc1_wrote=false;mmc1_sync();
}
