#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../experiments/bluetooth/rom_transfer.h"
static uint8_t staging[2048],game[1025];
static unsigned writes,published,starts;
static int bad_write,bad_read,bad_rom,bad_publish;
static int begin(void *c,uint32_t n){(void)c;assert(n<=sizeof staging);starts++;published=0;memset(staging,0,sizeof staging);return 0;}
static int write_data(void *c,uint32_t off,const uint8_t *p,uint32_t n){(void)c;assert(off+n<=sizeof staging);writes++;if(bad_write)return -1;memcpy(staging+off,p,n);return 0;}
static int read_data(void *c,uint32_t off,uint8_t *p,uint32_t n){(void)c;assert(off+n<=sizeof staging);if(bad_read)return -1;memcpy(p,staging+off,n);return 0;}
static int validate(void *c,uint32_t n){(void)c;assert(n==sizeof game);return bad_rom;}
static int publish(void *c,uint32_t n,uint32_t crc){(void)c;assert(n==sizeof game&&crc==np_rx_crc32(game,n));if(bad_publish)return -1;published++;return 0;}
static void feed(np_rx *s,unsigned id){
    for(unsigned off=0;off<sizeof game;){unsigned n=sizeof game-off;if(n>512)n=512;
        assert(np_rx_data(s,id,off,game+off,n,np_rx_crc32(game+off,n))==0);off+=n;}
}
int main(void){
    for(unsigned i=0;i<sizeof game;i++)game[i]=(uint8_t)(i*43);
    assert(np_rx_crc32((const uint8_t *)"123456789",9)==0xcbf43926);
    np_rx s;np_rx_io io={0,begin,write_data,read_data,validate,publish};np_rx_init(&s,&io,sizeof staging);
    unsigned crc=np_rx_crc32(game,sizeof game);
    assert(np_rx_begin(&s,0,sizeof game,crc)==NP_RX_RANGE);
    assert(np_rx_begin(&s,1,UINT32_MAX,crc)==NP_RX_RANGE&&starts==0);
    assert(np_rx_begin(&s,1,sizeof game,crc)==0);
    assert(np_rx_begin(&s,2,sizeof game,crc)==NP_RX_STATE&&starts==1);
    assert(np_rx_commit(&s,1)==NP_RX_STATE&&!published);
    assert(np_rx_data(&s,2,0,game,512,np_rx_crc32(game,512))==NP_RX_STATE);
    assert(np_rx_data(&s,1,UINT32_MAX,game,512,0)==NP_RX_RANGE);
    assert(np_rx_data(&s,1,512,game,512,np_rx_crc32(game,512))==NP_RX_RANGE);
    assert(np_rx_data(&s,1,0,game,512,0)==NP_RX_CRC&&writes==0);
    assert(np_rx_data(&s,1,0,game,512,np_rx_crc32(game,512))==0);
    assert(np_rx_begin(&s,1,sizeof game,crc)==0&&starts==1&&s.received==512);
    assert(np_rx_data(&s,1,0,game,512,np_rx_crc32(game,512))==0&&writes==1);
    uint8_t wrong[512];memcpy(wrong,game,512);wrong[3]^=1;
    assert(np_rx_data(&s,1,0,wrong,512,np_rx_crc32(wrong,512))==NP_RX_CRC);
    feed(&s,1);assert(np_rx_commit(&s,1)==0&&published==1);
    assert(np_rx_commit(&s,1)==0&&published==1);
    assert(np_rx_begin(&s,1,sizeof game,crc)==NP_RX_STATE);
    assert(!memcmp(game,staging,sizeof game));
    assert(np_rx_begin(&s,2,sizeof game,crc)==0);feed(&s,2);staging[600]^=1;
    assert(np_rx_commit(&s,2)==NP_RX_CRC&&!published&&s.state==NP_RX_FAILED);
    assert(np_rx_begin(&s,3,sizeof game,crc)==0);bad_write=1;
    assert(np_rx_data(&s,3,0,game,512,np_rx_crc32(game,512))==NP_RX_IO);bad_write=0;
    assert(np_rx_begin(&s,4,sizeof game,crc)==0);feed(&s,4);bad_read=1;
    assert(np_rx_commit(&s,4)==NP_RX_IO&&!published);bad_read=0;
    assert(np_rx_begin(&s,5,sizeof game,crc)==0);feed(&s,5);bad_rom=1;
    assert(np_rx_commit(&s,5)==NP_RX_ROM&&!published);bad_rom=0;
    assert(np_rx_begin(&s,6,sizeof game,crc)==0);feed(&s,6);bad_publish=1;
    assert(np_rx_commit(&s,6)==NP_RX_IO&&!published);bad_publish=0;
    assert(np_rx_begin(&s,7,sizeof game,crc)==0);np_rx_abort(&s);
    assert(np_rx_commit(&s,7)==NP_RX_STATE&&np_rx_begin(&s,7,sizeof game,crc)==NP_RX_STATE);
    assert(np_rx_begin(&s,8,sizeof game,crc)==0);feed(&s,8);assert(np_rx_commit(&s,8)==0);
    puts("PASS: bounded cartridge transfer, retries, gaps, CRCs, interruption, backend failures and publish gate.");
}
