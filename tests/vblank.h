// Original synthetic NROM regression: foreground and NMI both read PPUSTATUS.
// Immediate NMI dispatch permanently starves the foreground vblank polling loop.
static void test_vblank_polling(){
    std::vector<uint8_t> rom(16+16384+8192,0);
    memcpy(rom.data(),"NES\x1a",4);rom[4]=1;rom[5]=1;
    const uint8_t main[]={
        0x78,0xd8,0xa2,0xff,0x9a,0xa9,0x80,0x8d,0x00,0x20,
        0xad,0x02,0x20,0x10,0xfb,0xe6,0x10,
        0xad,0x02,0x20,0x10,0xfb,0xe6,0x10,0x4c,0x0a,0x80};
    const uint8_t nmi[]={0x48,0xad,0x02,0x20,0xe6,0x11,0x68,0x40};
    memcpy(rom.data()+16,main,sizeof main);memcpy(rom.data()+16+0x100,nmi,sizeof nmi);
    const uint8_t vectors[]={0x00,0x81,0x00,0x80,0x00,0x81};
    memcpy(rom.data()+16+16384-6,vectors,sizeof vectors);
    for(unsigned reset=0;reset<2;reset++){
        assert(!gp_load(rom.data(),rom.size()));
        if(reset)gp_reset();
        int16_t audio[8192];
        // Initial load starts at scanline zero and yields at the first vblank.
        assert(gp_frame());gp_audio_read(audio,gp_audio_queued());
        const uint32_t clockStart=np_cpu_clock_base;
        for(unsigned frame=0;frame<120;frame++){
            assert(gp_frame());gp_audio_read(audio,gp_audio_queued());assert(!gp_error());
        }
        assert(RAM[0x10]>0); // foreground can observe vblank
        assert(RAM[0x11]>0); // normal NMIs continue to reach their handler
        assert(np_cpu_clock_base-clockStart==120u*262u*341u/3u); // delay borrows, never adds cycles
    }
    printf("PASS: foreground vblank polling and NMI status reads coexist; reset and exact CPU budget preserved.\n");
}
