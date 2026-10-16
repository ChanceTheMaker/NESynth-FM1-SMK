/* SPDX-License-Identifier: GPL-3.0-only
 * Color Dreams / mapper 11: bits 0-1 select 32 KiB PRG, bits 4-7 select
 * 8 KiB CHR. Conflict-free writes; no mapper-controlled RAM or mirroring.
 */
void Map11_Write(WORD,BYTE data){
    unsigned prg=(data&3)&(NesHeader.byRomSize/2-1);
    unsigned chr=(data>>4)&(NesHeader.byVRomSize-1);
    ROMBANK0=ROMPAGE(prg*4);ROMBANK1=ROMPAGE(prg*4+1);
    ROMBANK2=ROMPAGE(prg*4+2);ROMBANK3=ROMPAGE(prg*4+3);
    for(unsigned i=0;i<8;i++)PPUBANK[i]=VROMPAGE(chr*8+i);
}
void Map11_Init(){
    Map0_Init();MapperInit=Map11_Init;MapperWrite=Map11_Write;Map11_Write(0,0);
}
