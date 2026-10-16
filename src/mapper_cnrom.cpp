/* SPDX-License-Identifier: GPL-3.0-only
 * Standard CNROM: fixed 16/32 KiB PRG, up to four 8 KiB CHR banks.
 * iNES cannot distinguish bus-conflict variants; use the written bank value.
 */
void Map3_Write(WORD,BYTE data){
    unsigned bank=data&(NesHeader.byVRomSize-1);
    for(unsigned i=0;i<8;i++)PPUBANK[i]=VROMPAGE(bank*8+i);
}
void Map3_Init(){
    Map0_Init();MapperInit=Map3_Init;MapperWrite=Map3_Write;Map3_Write(0,0);
}
