/* SPDX-License-Identifier: GPL-3.0-only */
/* Original NESynth pixel artwork. Render clipped strips into the existing LCD
 * workspace; no bitmap, new framebuffer, allocation, or cartridge space. */
#ifndef NP_LOADER_ART_H
#define NP_LOADER_ART_H
#include <stdint.h>
typedef struct {uint16_t *pixels;unsigned top,height;} np_art;
static void np_art_box(np_art *a,unsigned x,unsigned y,unsigned w,unsigned h,uint16_t rgb){
    unsigned right=x+w,bottom=y+h;
    if(right>240)right=240;
    if(bottom>a->top+a->height)bottom=a->top+a->height;
    if(y<a->top)y=a->top;
    uint16_t wire=(uint16_t)((rgb>>8)|(rgb<<8));
    for(;y<bottom;y++)for(unsigned col=x;col<right;col++)a->pixels[(y-a->top)*240+col]=wire;
}
static void np_art_text(np_art *a,unsigned x,unsigned y,const char *s,unsigned scale,uint16_t color){
    for(;*s;s++,x+=4*scale){
        unsigned c=(unsigned char)*s,k=c>='0'&&c<='9'?c-'0':c>='A'&&c<='Z'?c-'A'+10:99;
        if(k>=36)continue;
        for(unsigned r=0;r<5;r++)for(unsigned col=0;col<3;col++)
            if(np_font[k][r]&(4u>>col))np_art_box(a,x+col*scale,y+r*scale,scale,scale,color);
    }
}
static void np_art_center(np_art *a,unsigned y,const char *s,unsigned scale,uint16_t color){
    unsigned n=0;while(s[n])n++;
    unsigned width=n?((n*4-1)*scale):0;
    np_art_text(a,width<240?(240-width)/2:0,y,s,scale,color);
}
static void np_loader_art(uint16_t *pixels,unsigned top,unsigned height,const char *status,unsigned percent,unsigned state){
    np_art a={pixels,top,height};
    const uint16_t bg=0x1083,red=0xe9e8,white=0xef5c,gray=0x9cf3,dark=0x2945,blue=0x45bf;
    np_art_box(&a,0,top,240,height,bg);
    /* Inset badge and two-tone wordmark. */
    np_art_box(&a,18,16,204,3,red);np_art_box(&a,18,59,204,3,red);
    np_art_box(&a,18,19,3,40,red);np_art_box(&a,219,19,3,40,red);
    np_art_text(&a,39,24,"NES",6,white);np_art_text(&a,111,24,"YNTH",6,red);
    /* Cartridge entering the front-loading console. */
    np_art_box(&a,77,77,86,48,dark);np_art_box(&a,81,73,78,49,gray);
    for(unsigned x=85;x<155;x+=8)np_art_box(&a,x,77,3,5,dark);
    np_art_box(&a,92,87,56,27,red);np_art_text(&a,104,94,"NES",3,white);
    np_art_box(&a,40,122,160,33,dark);np_art_box(&a,34,117,172,30,white);
    np_art_box(&a,34,147,172,9,gray);np_art_box(&a,42,156,24,4,dark);np_art_box(&a,174,156,24,4,dark);
    np_art_box(&a,70,122,99,13,gray);np_art_box(&a,76,125,87,5,dark);
    for(unsigned x=179;x<201;x+=5)np_art_box(&a,x,121,2,18,dark);
    np_art_box(&a,44,141,5,4,red);np_art_box(&a,55,140,15,5,dark);np_art_box(&a,75,140,15,5,dark);
    /* Radio waves frame the cartridge. */
    for(unsigned i=0;i<3;i++){
        np_art_box(&a,47-i*8,88+i*5,3,18-i*5,blue);
        np_art_box(&a,190+i*8,88+i*5,3,18-i*5,blue);
    }
    np_art_center(&a,168,"GAME LOADER",2,gray);
    np_art_center(&a,231,"PLAY LIBRARY",1,gray);
    /* Only this lower 64-row strip is refreshed while the radio runs. */
    uint16_t accent=state==5?red:state==4?0x5e8e:blue;
    np_art_center(&a,190,status,3,state==5?red:white);
    np_art_box(&a,20,216,200,8,dark);
    if(percent>100)percent=100;
    if(percent)np_art_box(&a,20,216,percent*2,8,accent);
    else for(unsigned x=108;x<132;x+=9)np_art_box(&a,x,218,5,4,accent);
}
#endif
