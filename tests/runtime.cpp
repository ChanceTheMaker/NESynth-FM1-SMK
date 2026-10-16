/* SPDX-License-Identifier: GPL-3.0-only */
#include "nes_port.h"
#include "InfoNES.h"
#include "InfoNES_Mapper.h"
#include "video_job.h"
#include <cassert>
#include <cstdio>
#include <vector>
#include <chrono>
#include <cstring>
#include <cstdlib>
#undef assert
#define assert(x) do { if(!(x)){fprintf(stderr,"FAIL %s:%d: %s\n",__FILE__,__LINE__,#x);exit(1);} } while(0)
#include "apu_release.h"
#include "mmc1.h"
#include "extra_mappers.h"
#include "vblank.h"
#include "ppu_mask.h"
static int16_t audio[4096*2];
static void advance(unsigned count){for(unsigned i=0;i<count;i++){assert(gp_frame());assert(!gp_error());memset(audio,0,sizeof audio);gp_audio_read(audio,gp_audio_queued());}}
static void test_mmc2(){
    std::vector<uint8_t> cart(16+128*1024+128*1024,0);
    memcpy(cart.data(),"NES\x1a",4);cart[4]=8;cart[5]=16;cart[6]=0x90;
    for(unsigned bank=0;bank<16;bank++)memset(cart.data()+16+bank*8192,bank,8192);
    for(unsigned bank=0;bank<32;bank++)memset(cart.data()+16+128*1024+bank*4096,bank,4096);
    assert(!gp_load(cart.data(),cart.size()));
    assert(ROMBANK0[0]==0&&ROMBANK1[0]==13&&ROMBANK2[0]==14&&ROMBANK3[0]==15);
    MapperWrite(0xafff,0xff);assert(ROMBANK0[0]==15);
    MapperWrite(0xa000,0x12);assert(ROMBANK0[0]==2&&ROMBANK1[0]==13);
    MapperWrite(0xb000,3);MapperWrite(0xc000,5);MapperWrite(0xd000,7);MapperWrite(0xe000,9);
    assert(PPUBANK[0][0]==5&&PPUBANK[4][0]==9);
    MapperPPU(0x0fd9);assert(PPUBANK[0][0]==5); // Only row zero triggers left latch.
    MapperPPU(0x0fd8);assert(PPUBANK[0][0]==3);
    MapperWrite(0xb000,0xff);assert(PPUBANK[0][0]==31);
    MapperPPU(0x0fe8);assert(PPUBANK[0][0]==5);
    MapperPPU(0x1fd7);assert(PPUBANK[4][0]==9);
    MapperPPU(0x1fdf);assert(PPUBANK[4][0]==7); // All eight rows trigger right latch.
    MapperPPU(0x1fe8);assert(PPUBANK[4][0]==9);
    MapperWrite(0xf000,1);assert(PPUBANK[8]==PPUBANK[9]&&PPUBANK[10]==PPUBANK[11]);
    MapperWrite(0xf000,0);assert(PPUBANK[8]==PPUBANK[10]&&PPUBANK[9]==PPUBANK[11]);
    gp_reset();assert(ROMBANK0[0]==0&&PPUBANK[0][0]==0&&PPUBANK[4][0]==0);
    // Tile FD/FE must render with the OLD bank, then switch the next tile.
    memset(VROM,0,128*1024);
    for(unsigned tile=0;tile<256;tile++){
        memset(VROM+tile*16,0xff,8); // Bank 0: color index 1.
        memset(VROM+4096+tile*16+8,0xff,8); // Bank 1: color index 2.
    }
    ChrBufUpdate=0xff;
    MapperWrite(0xd000,0);MapperWrite(0xe000,1);
    PPU_R1=R1_SHOW_SCR|R1_CLIP_BG;PPU_BG_Base=ChrBuf+256*64;PPU_NameTableBank=8;
    PPU_Scr_H_Byte=PPU_Scr_H_Bit=PPU_Scr_V_Byte=PPU_Scr_V_Bit=0;PPU_Scanline=0;
    memset(PPUBANK[8],0,1024);PPUBANK[8][0]=0xfd;PPUBANK[8][1]=1;PPUBANK[8][2]=0xfe;PPUBANK[8][3]=1;
    for(unsigned i=0;i<32;i++)PalTable[i]=i;
    InfoNES_DrawLine();assert(WorkFrame[0]==2&&WorkFrame[8]==1&&WorkFrame[16]==1&&WorkFrame[24]==2);
    MapperWrite(0xb000,0);MapperWrite(0xc000,1);PPU_BG_Base=ChrBuf;PPU_Scanline=1;
    InfoNES_DrawLine();assert(WorkFrame[0]==2&&WorkFrame[8]==2); // Left latch ignores row 1.
    PPU_Scanline=0;InfoNES_DrawLine();assert(WorkFrame[0]==2&&WorkFrame[8]==1);
    // Sprite fetches are in OAM order, with the same post-fetch latch behavior.
    MapperPPU(0x0fe8);memset(SPRRAM,0xff,256);PPU_SP_Base=ChrBuf;PPU_SP_Height=8;PPU_R0=0;
    PPU_R1=R1_SHOW_SP|R1_CLIP_SP;PPU_Scanline=1;
    SPRRAM[0]=SPRRAM[2]=SPRRAM[3]=0;SPRRAM[1]=0xfd;
    SPRRAM[4]=SPRRAM[6]=0;SPRRAM[5]=1;SPRRAM[7]=8;
    InfoNES_DrawLine();assert(WorkFrame[0]==18&&WorkFrame[8]==17);
    // Bound every register value to cartridge storage, including smaller boards.
    cart[4]=2;cart[5]=1;cart.resize(16+32768+8192);assert(!gp_load(cart.data(),cart.size()));
    for(unsigned data=0;data<256;data++){
        for(unsigned reg=0xa000;reg<0xf000;reg+=0x1000)MapperWrite(reg,(BYTE)data);
        for(unsigned addr: {0x0fd8u,0x0fe8u,0x1fd8u,0x1fefu}){
            MapperPPU(addr);
            assert(ROMBANK0>=ROM&&ROMBANK0+8192<=VROM);
            for(unsigned bank=0;bank<8;bank++)assert(PPUBANK[bank]>=VROM&&PPUBANK[bank]+1024<=cart.data()+cart.size());
        }
    }
    auto bad=cart;bad[5]=0;bad.resize(16+32768);assert(gp_load(bad.data(),bad.size())==GP_ROM_SIZE);
    bad=cart;bad[6]|=8;assert(gp_load(bad.data(),bad.size())==GP_FORMAT);
    assert(gp_load(cart.data(),cart.size()-1)==GP_ROM_SIZE);
}
int main(int argc,char **argv){
    assert(argc==2);FILE*f=fopen(argv[1],"rb");assert(f);std::vector<uint8_t> rom(24592);assert(fread(rom.data(),1,rom.size(),f)==rom.size());fclose(f);
    assert(gp_load(nullptr,0)==GP_SHORT);
    auto bad=rom;bad[0]=0;assert(gp_load(bad.data(),bad.size())==GP_FORMAT);
    bad=rom;bad[7]=8;assert(gp_load(bad.data(),bad.size())==GP_FORMAT);
    bad=rom;bad[6]=0x50;assert(gp_load(bad.data(),bad.size())==GP_MAPPER);
    bad=rom;bad[9]=1;assert(gp_load(bad.data(),bad.size())==GP_REGION);
    bad=rom;bad[4]=0;assert(gp_load(bad.data(),bad.size())==GP_ROM_SIZE);
    assert(gp_load(rom.data(),rom.size()-1)==GP_ROM_SIZE);
    assert(gp_load(rom.data(),rom.size())==0);advance(10);
    uint32_t frame_hash=gb_video_hash(gp_pixels());
    unsigned lit=0;for(unsigned i=0;i<256*240;i++){assert(gp_pixels()[i]<64);if(gp_pixels()[i]==0x30)lit++;}
    fprintf(stderr,"Diagnostic: lit=%u, PPU ctrl=%u mask=%u, sprite=(%u,%u), RAM0=%u, palette=%u,%u\n",lit,PPU_R0,PPU_R1,SPRRAM[3],SPRRAM[0],RAM[0],PalTable[0],PalTable[1]);assert(lit>400);
    unsigned x=SPRRAM[3];gp_buttons(GP_RIGHT);advance(12);gp_buttons(0);advance(2);assert(SPRRAM[3]>x);
    unsigned y=SPRRAM[0];gp_buttons(GP_UP);advance(12);gp_buttons(0);advance(2);assert(SPRRAM[0]<y);
    gp_buttons(GP_B);advance(3);assert(gp_pixels()[239*256+240]==0x11);gp_buttons(0);advance(3);assert(gp_pixels()[239*256+240]==0x0f);
    gp_volume(100);advance(3);bool sounded=false;for(auto v:audio)if(v)sounded=true;assert(sounded);
    auto crossings=[](){unsigned n=0;for(unsigned i=2;i<1460;i+=2)if((audio[i]>=0)!=(audio[i-2]>=0))n++;return n;};
    unsigned lowTone=crossings();gp_buttons(GP_A);advance(3);assert(crossings()>lowTone);gp_buttons(0);
    gp_volume(0);advance(3);for(auto v:audio)assert(v==0);gp_volume(25);
    gp_pause(1);unsigned before=gp_frame_count();assert(!gp_frame());assert(gp_frame_count()==before);gp_pause(0);
    gp_reset();unsigned full=0;while(gp_frame()){assert(++full<10);}assert(gp_audio_queued()<=4096);gp_audio_read(audio,gp_audio_queued());assert(gp_frame());gp_audio_read(audio,gp_audio_queued());
    gp_reset();auto start=std::chrono::steady_clock::now();advance(1800);double seconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
    assert(np_audio_samples()==(uint64_t)1800*44100*59561/3579545);
    assert(gp_frame_count()==1800);
    // Battery RAM survives reset; reloading is a new cartridge session.
    bad=rom;bad[6]|=2;assert(!gp_load(bad.data(),bad.size()));assert(gp_save_size()==8192);gp_save_data()[5]=123;assert(gp_save_dirty());gp_save_clean();assert(!gp_save_dirty());gp_reset();assert(gp_save_data()[5]==123);
    gb_video_palette_init();std::vector<uint8_t> input(256*240);std::vector<uint16_t> output(240*225+2,0xbeef);
    for(unsigned i=0;i<input.size();i++)input[i]=(uint8_t)((i/256+i%256)%64);
    gb_video_job_t job={1,~1u,0,0,0,gb_video_hash(input.data())};
    assert(gb_video_work(&job,input.data(),output.data()+1)==1);assert(output.front()==0xbeef&&output.back()==0xbeef);
    for(unsigned row=0;row<225;row++)for(unsigned col=0;col<240;col++)assert(output[1+row*240+col]==video_palette[input[(row+row/15)*256+col+col/15]]);
    assert(gb_video_poll(&job,1,999,1,0)==1); // completed frames remain valid after a late poll
    assert(job.dirty_rows==225); // first frame differs throughout
    assert(gb_video_scale(input.data(),output.data()+1)==(225u<<16)); // unchanged
    input[16*256]^=1; // source row 16 is display row 15
    assert(gb_video_scale(input.data(),output.data()+1)==((15u<<16)|16u));
    input[0]^=1;input[238*256+254]^=1;
    assert(gb_video_scale(input.data(),output.data()+1)==225u); // first and last rows
    input[15*256]^=1; // dropped source row must not trigger a transfer
    assert(gb_video_scale(input.data(),output.data()+1)==(225u<<16));
    job.request=2;job.request_inverse=~2u;input[0]^=1;assert(gb_video_work(&job,input.data(),output.data()+1)==-1);
    assert(gb_video_poll(&job,2,0,240000,0)==-1);
    // Cartridge RAM/trainer variants and 32 KiB PRG mapping.
    bad=rom;bad[6]=4;bad.insert(bad.begin()+16,512,0x5a);assert(!gp_load(bad.data(),bad.size()));assert(gp_save_data()[0x1000]==0x5a);advance(3);
    bad=rom;bad.insert(bad.begin()+16+16384,rom.begin()+16,rom.begin()+16+16384);bad[4]=2;assert(!gp_load(bad.data(),bad.size()));advance(10);assert(gb_video_hash(gp_pixels())==frame_hash);
    bad=rom;bad.resize(16+16384);bad[5]=0;assert(!gp_load(bad.data(),bad.size()));advance(10);
    test_mmc2();
    test_mmc1();
    test_extra_mappers();
    test_apu_release();
    test_vblank_polling();
    test_ppu_mask();
    FILE *report=fopen("build/host-benchmark.json","wb");assert(report);
    fprintf(report,"{\"frames\":1800,\"seconds\":%.6f,\"fps\":%.2f,\"diagnostic_frame10_fnv1a\":%u,\"hardware_fps\":null}\n",seconds,1800/seconds,frame_hash);fclose(report);
    printf("PASS: loader rejection, rendering, controller, audio, backpressure, reset, save RAM; 1800 frames in %.3fs (%.1f host fps)\n",seconds,1800/seconds);
}
