/* SPDX-License-Identifier: GPL-3.0-only
 * AxROM / mapper 7: 32 KiB PRG banks, fixed 8 KiB CHR RAM and one-screen VRAM.
 * https://www.nesdev.org/wiki/AxROM
 * Standard iNES behavior is conflict-free (needed by ANROM/AN1ROM games).
 */
void Map7_Write(WORD,BYTE data){
    unsigned bank=(data&7)&(NesHeader.byRomSize/2-1);
    ROMBANK0=ROMPAGE(bank*4);ROMBANK1=ROMPAGE(bank*4+1);
    ROMBANK2=ROMPAGE(bank*4+2);ROMBANK3=ROMPAGE(bank*4+3);
    InfoNES_Mirroring((data&16)?2:3);
}
void Map7_Init(){
    Map0_Init();MapperInit=Map7_Init;MapperWrite=Map7_Write;
    np_prg_ram_enabled=np_prg_ram_writable=0;Map7_Write(0,0);
}
