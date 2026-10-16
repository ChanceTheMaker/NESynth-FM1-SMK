/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef NP_LIBRARY_STORE_H
#define NP_LIBRARY_STORE_H
#include <stdint.h>
#include <stddef.h>
#include "rom_transfer.h"
#ifndef NP_LIBRARY_META
#define NP_LIBRARY_META 0x39000u
#endif
#define NP_LIBRARY_DATA (NP_LIBRARY_META+8192u)
#ifndef NP_LIBRARY_END
#define NP_LIBRARY_END 0x9d000u
#endif
#define NP_LIBRARY_BYTES (NP_LIBRARY_END-NP_LIBRARY_DATA)
#define NP_LIBRARY_MAX 32u
#define NP_CART_LIMIT 262672u
#define NP_LIBRARY_MAGIC 0x324c504eu
#define NP_LIBRARY_COMMIT 0x54494d43u
typedef struct {uint32_t offset,length,crc;char name[32];} np_game;
typedef struct {uint32_t magic,sequence,count;np_game games[NP_LIBRARY_MAX];uint32_t crc,commit;} np_catalog;
typedef struct {void *ctx;int(*read)(void*,uint32_t,void*,uint32_t);int(*erase)(void*,uint32_t);int(*write)(void*,uint32_t,const void*,uint32_t);} np_store_io;
typedef struct {np_catalog catalog;np_store_io io;uint32_t bank;} np_store;
static uint32_t np_game_span(uint32_t n){return (n+4095u)&~4095u;}
static int np_catalog_valid(const np_catalog *c){
    if(c->magic!=NP_LIBRARY_MAGIC||c->commit!=NP_LIBRARY_COMMIT||c->count>NP_LIBRARY_MAX||c->crc!=np_rx_crc32((const uint8_t*)c,offsetof(np_catalog,crc)))return 0;
    for(unsigned i=0;i<c->count;i++){
        const np_game *g=&c->games[i];uint32_t span=np_game_span(g->length);
        if(g->length<16||g->length>NP_CART_LIMIT||g->offset<NP_LIBRARY_DATA||(g->offset&4095)||g->offset>NP_LIBRARY_END||span>NP_LIBRARY_END-g->offset||g->name[31])return 0;
        for(unsigned j=0;j<i;j++)if(g->offset<c->games[j].offset+np_game_span(c->games[j].length)&&c->games[j].offset<g->offset+span)return 0;
    }return 1;
}
static int np_store_load(np_store *s,const np_store_io *io){
    np_catalog other;s->io=*io;s->bank=0;
    if(io->read(io->ctx,NP_LIBRARY_META,&s->catalog,sizeof s->catalog)||io->read(io->ctx,NP_LIBRARY_META+4096,&other,sizeof other))return -1;
    int a=np_catalog_valid(&s->catalog),b=np_catalog_valid(&other);
    if(b&&(!a||(int32_t)(other.sequence-s->catalog.sequence)>0)){s->catalog=other;s->bank=1;}
    else if(!a){s->catalog=(np_catalog){0};s->catalog.magic=NP_LIBRARY_MAGIC;}
    return 0;
}
static uint32_t np_store_free(const np_store *s){
    uint32_t n=NP_LIBRARY_BYTES;for(unsigned i=0;i<s->catalog.count;i++)n-=np_game_span(s->catalog.games[i].length);return n;
}
static uint32_t np_store_find(const np_store *s,uint32_t n){
    if(n<16||n>NP_CART_LIMIT||s->catalog.count>=NP_LIBRARY_MAX)return 0;
    uint32_t span=np_game_span(n),off=NP_LIBRARY_DATA;
    for(unsigned tries=0;tries<=s->catalog.count;tries++){
        if(span>NP_LIBRARY_END-off)return 0;
        unsigned i;for(i=0;i<s->catalog.count;i++){
            const np_game *g=&s->catalog.games[i];uint32_t end=g->offset+np_game_span(g->length);
            if(off<end&&g->offset<off+span){off=end;break;}
        }if(i==s->catalog.count)return off;
    }return 0;
}
static uint32_t np_store_largest(const np_store *s){
    uint32_t n=NP_CART_LIMIT;
    while(n>=16){if(np_store_find(s,n))return n;n=(n-1)&~4095u;}
    return 0;
}
/* The old catalog and every live game remain intact until the new commit word. */
static int np_store_publish(np_store *s,np_catalog *next){
    uint32_t off=NP_LIBRARY_META+(s->bank^1u)*4096;
    next->magic=NP_LIBRARY_MAGIC;next->sequence=s->catalog.sequence+1;
    next->crc=np_rx_crc32((const uint8_t*)next,offsetof(np_catalog,crc));next->commit=NP_LIBRARY_COMMIT;
    if(!np_catalog_valid(next)||s->io.erase(s->io.ctx,off))return -1;
    const uint8_t *p=(const uint8_t*)next;uint8_t verify[256];
    for(unsigned pos=0;pos<offsetof(np_catalog,commit);){
        unsigned n=offsetof(np_catalog,commit)-pos;if(n>256)n=256;
        if(s->io.write(s->io.ctx,off+pos,p+pos,n)||s->io.read(s->io.ctx,off+pos,verify,n))return -1;
        for(unsigned j=0;j<n;j++)if(verify[j]!=p[pos+j])return -1;
        pos+=n;
    }
    if(s->io.write(s->io.ctx,off+offsetof(np_catalog,commit),&next->commit,4)||s->io.read(s->io.ctx,off+offsetof(np_catalog,commit),verify,4))return -1;
    for(unsigned j=0;j<4;j++)if(verify[j]!=p[offsetof(np_catalog,commit)+j])return -1;
    s->catalog=*next;s->bank^=1u;return 0;
}
static int np_store_add(np_store *s,const uint8_t *rom,uint32_t n,uint32_t crc,const char *name){
    if(n<16||n>NP_CART_LIMIT||np_rx_crc32(rom,n)!=crc)return -1;
    for(unsigned i=0;i<s->catalog.count;i++)if(s->catalog.games[i].length==n&&s->catalog.games[i].crc==crc)return (int)i;
    uint32_t off=np_store_find(s,n);if(!off)return -2;
    for(unsigned pos=0;pos<np_game_span(n);pos+=4096)if(s->io.erase(s->io.ctx,off+pos))return -3;
    uint8_t verify[256];
    for(unsigned pos=0;pos<n;){unsigned size=n-pos;if(size>256)size=256;
        if(s->io.write(s->io.ctx,off+pos,rom+pos,size)||s->io.read(s->io.ctx,off+pos,verify,size))return -3;
        for(unsigned j=0;j<size;j++)if(verify[j]!=rom[pos+j])return -3;
        pos+=size;
    }
    np_catalog next=s->catalog;unsigned index=next.count++;np_game *g=&next.games[index];
    *g=(np_game){0};g->offset=off;g->length=n;g->crc=crc;
    for(unsigned i=0;i<31&&name[i];i++){unsigned char c=(unsigned char)name[i];g->name[i]=c>=32&&c<=126?(char)c:' ';}
    if(!g->name[0]){g->name[0]='G';g->name[1]='A';g->name[2]='M';g->name[3]='E';}
    return np_store_publish(s,&next)?-3:(int)index;
}
static int np_store_remove(np_store *s,unsigned index,uint32_t crc){
    if(index>=s->catalog.count||s->catalog.games[index].crc!=crc)return -1;
    np_catalog next=s->catalog;for(unsigned i=index+1;i<next.count;i++)next.games[i-1]=next.games[i];
    next.games[--next.count]=(np_game){0};return np_store_publish(s,&next);
}
#endif
