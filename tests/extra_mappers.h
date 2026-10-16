extern BYTE np_prg_ram_enabled,np_prg_ram_writable,np_mapper_irq_pending,IRQ_State;
static std::vector<uint8_t> bank_cart(unsigned mapper,unsigned prg,unsigned chr){
    std::vector<uint8_t> c(16+prg*16384+chr*8192);
    memcpy(c.data(),"NES\x1a",4);c[4]=prg;c[5]=chr;c[6]=mapper<<4;
    for(unsigned i=0;i<prg*2;i++)memset(c.data()+16+i*8192,i,8192);
    for(unsigned i=0;i<chr*8;i++)memset(c.data()+16+prg*16384+i*1024,i,1024);
    return c;
}
static void test_extra_mappers(){
    auto ux=bank_cart(2,16,0);assert(!gp_load(ux.data(),ux.size()));
    for(unsigned v=0;v<256;v++){
        MapperWrite(0x8000,v);assert(ROMBANK0[0]==(v&15)*2&&ROMBANK1[0]==(v&15)*2+1);
        assert(ROMBANK2[0]==30&&ROMBANK3[0]==31);
    }
    gp_reset();assert(ROMBANK0[0]==0&&ROMBANK2[0]==30);
    auto cn=bank_cart(3,2,4);assert(!gp_load(cn.data(),cn.size()));
    for(unsigned v=0;v<256;v++){
        MapperWrite(0xffff,v);
        for(unsigned i=0;i<8;i++)assert(PPUBANK[i][0]==(v&3)*8+i);
        assert(ROMBANK0[0]==0&&ROMBANK3[0]==3);
    }
    gp_reset();assert(PPUBANK[0][0]==0);
    auto mm=bank_cart(4,32,32);assert(!gp_load(mm.data(),mm.size()));
    assert(ROMBANK0[0]==0&&ROMBANK1[0]==1&&ROMBANK2[0]==62&&ROMBANK3[0]==63);
    MapperWrite(0x8000,6);MapperWrite(0x8001,5);
    MapperWrite(0x8000,7);MapperWrite(0x8001,7);
    assert(ROMBANK0[0]==5&&ROMBANK1[0]==7);
    MapperWrite(0x8000,64);assert(ROMBANK0[0]==62&&ROMBANK2[0]==5);
    for(unsigned mode: {0u,128u})for(unsigned reg=0;reg<8;reg++)for(unsigned v=0;v<256;v++){
        MapperWrite(0x8000,mode|reg);MapperWrite(0x8001,v);
        for(unsigned i=0;i<8;i++)assert(PPUBANK[i]>=VROM&&PPUBANK[i]+1024<=mm.data()+mm.size());
        assert(ROMBANK0>=ROM&&ROMBANK0+8192<=VROM);
        if(reg==0){assert(PPUBANK[mode?4:0][0]==(v&254));assert(PPUBANK[mode?5:1][0]==(v|1));}
        if(reg==2)assert(PPUBANK[mode?0:4][0]==v);
    }
    MapperWrite(0xa000,0);assert(PPUBANK[8]==PPUBANK[10]&&PPUBANK[9]==PPUBANK[11]);
    MapperWrite(0xa000,1);assert(PPUBANK[8]==PPUBANK[9]&&PPUBANK[10]==PPUBANK[11]);
    // Exercise protection through actual CPU read/write instructions.
    for(unsigned mode: {0u,128u,192u}){
        MapperWrite(0xa001,mode);SRAM[0]=0x5a;
        const BYTE code[]={0xa9,0x7b,0x8d,0,0x60,0xad,0,0x60,0x85,0x20};
        memcpy(RAM,code,sizeof code);PC=0;F=0x24;g_wPassedClocks=0;K6502_Step(13);
        assert(SRAM[0]==(mode==128?0x7b:0x5a));assert(RAM[0x20]==(mode==0?0xff:SRAM[0]));
    }
    // MMC3B/C reload is deferred; $E001 does not clear pending/reload state.
    PPU_R1=R1_SHOW_SCR;PPU_Scanline=0;MapperWrite(0xc000,2);MapperWrite(0xc001,0);MapperWrite(0xe001,0);
    MapperHSync();assert(!np_mapper_irq_pending);MapperHSync();assert(!np_mapper_irq_pending);
    MapperHSync();assert(np_mapper_irq_pending&&IRQ_State==0);
    MapperWrite(0xe001,0);assert(np_mapper_irq_pending);
    MapperWrite(0xe000,0);assert(!np_mapper_irq_pending&&IRQ_State==1);
    MapperWrite(0xc000,0);MapperWrite(0xc001,0);MapperWrite(0xe001,0);
    PPU_R1=0;MapperHSync();assert(!np_mapper_irq_pending);
    PPU_R1=R1_SHOW_SP;PPU_Scanline=241;MapperHSync();assert(!np_mapper_irq_pending);
    PPU_Scanline=261;MapperHSync();assert(np_mapper_irq_pending);
    gp_reset();assert(!np_mapper_irq_pending&&np_prg_ram_writable&&np_prg_ram_enabled);
    mm[6]|=8;assert(!gp_load(mm.data(),mm.size()));
    for(unsigned v=0;v<2;v++){MapperWrite(0xa000,v);for(unsigned i=0;i<4;i++)assert(PPUBANK[8+i]==PPURAM+0x2000+i*1024);}
    mm=bank_cart(4,2,0);assert(!gp_load(mm.data(),mm.size()));
    MapperWrite(0x8000,2);MapperWrite(0x8001,255);assert(PPUBANK[4]==PPURAM+7*1024);
    // CNROM/MMC3 rows must switch immediately, and CHR RAM must never cache.
    PPU_R1=R1_SHOW_SCR|R1_CLIP_BG;PPU_BG_Base=ChrBuf;PPU_NameTableBank=8;PPU_Scanline=0;
    memset(PPUBANK[8],0,1024);for(unsigned i=0;i<32;i++)PalTable[i]=i;
    PPUBANK[0][0]=255;PPUBANK[0][8]=0;InfoNES_DrawLine();assert(WorkFrame[0]==1);
    PPUBANK[0][0]=0;PPUBANK[0][8]=255;InfoNES_DrawLine();assert(WorkFrame[0]==2);
    auto bad=bank_cart(2,2,1);assert(gp_load(bad.data(),bad.size())==GP_ROM_SIZE);
    bad=bank_cart(3,2,0);assert(gp_load(bad.data(),bad.size())==GP_ROM_SIZE);
    bad=bank_cart(4,3,1);assert(gp_load(bad.data(),bad.size())==GP_ROM_SIZE);
    puts("PASS: UxROM/CNROM/MMC3 banking, bank bounds, CHR RAM, mirroring, RAM protection, IRQ reload/ack/disable, reset and invalid boards.");
}
