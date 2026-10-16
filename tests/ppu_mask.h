/* Left-edge PPUMASK bits independently hide backgrounds and sprites. */
static void test_ppu_mask(){
    std::vector<uint8_t> cart(16+16384+8192,0);
    memcpy(cart.data(),"NES\x1a",4);cart[4]=cart[5]=1;
    assert(!gp_load(cart.data(),cart.size()));
    memset(ChrBuf,1,256*64);memset(ChrBuf+256*64,2,256*64);
    PPU_BG_Base=ChrBuf;PPU_SP_Base=ChrBuf+256*64;
    PPU_NameTableBank=8;memset(PPUBANK[8],0,1024);
    PPU_Scr_H_Byte=PPU_Scr_H_Bit=PPU_Scr_V_Byte=PPU_Scr_V_Bit=0;
    PPU_Scanline=1;PPU_SP_Height=8;PPU_R0=0;PPU_UpDown_Clip=0;
    for(unsigned i=0;i<32;i++)PalTable[i]=i;
    // Non-gray backdrop and distinct opaque background / sprite colors.
    PalTable[0]=0x8000|0x0f;PalTable[1]=0x21;PalTable[18]=0x30;
    for(unsigned backdrop: {0x0fu,0x12u}){
      PalTable[0]=0x8000|backdrop;
      for(unsigned bg=0;bg<2;bg++)for(unsigned sp=0;sp<2;sp++)
      for(unsigned bgleft=0;bgleft<2;bgleft++)for(unsigned spleft=0;spleft<2;spleft++)
      for(unsigned behind=0;behind<2;behind++){
        memset(SPRRAM,0xff,256);
        for(unsigned i=0;i<2;i++){
          SPRRAM[i*4]=0;SPRRAM[i*4+1]=0;
          SPRRAM[i*4+2]=behind?0x20:0;SPRRAM[i*4+3]=i*8;
        }
        PPU_R1=(bg?R1_SHOW_SCR:0)|(sp?R1_SHOW_SP:0)|
          (bgleft?R1_CLIP_BG:0)|(spleft?R1_CLIP_SP:0);
        InfoNES_DrawLine();
        for(unsigned x=0;x<256;x++){
          bool background=bg&&(x>=8||bgleft);
          bool sprite=sp&&x<16&&(x>=8||spleft);
          unsigned expected=sprite&&(!behind||!background)?0x30:background?0x21:backdrop;
          assert((WorkFrame[x]&63)==expected);
        }
      }
    }
    puts("PASS: backdrop and independent left-edge background/sprite masks, all layer and priority combinations");
}
