/* SPDX-License-Identifier: GPL-3.0-only
 * Standard UxROM: switchable 16 KiB PRG, fixed last 16 KiB, 8 KiB CHR RAM.
 * iNES cannot distinguish bus-conflict variants; use the written bank value.
 */
void Map2_Write(WORD,BYTE data){
    unsigned bank=data&(NesHeader.byRomSize-1);
    ROMBANK0=ROMPAGE(bank*2);ROMBANK1=ROMPAGE(bank*2+1);
}
void Map2_Init(){
    Map0_Init();MapperInit=Map2_Init;MapperWrite=Map2_Write;
    ROMBANK2=ROMLASTPAGE(1);ROMBANK3=ROMLASTPAGE(0);Map2_Write(0,0);
}
