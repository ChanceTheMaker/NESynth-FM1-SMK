/* SPDX-License-Identifier: GPL-3.0-only */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../experiments/bluetooth/library_store.h"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static uint8_t flash[0x100000],saved[0x100000],rom[NP_CART_LIMIT];
static unsigned calls,fail_at,corrupt;
static int allowed(uint32_t o,uint32_t n){return o>=NP_LIBRARY_META&&o<=NP_LIBRARY_END&&n<=NP_LIBRARY_END-o;}
static int rd(void*c,uint32_t o,void*p,uint32_t n){(void)c;CHECK(allowed(o,n));if(++calls==fail_at)return -1;memcpy(p,flash+o,n);if(corrupt&&o>=NP_LIBRARY_DATA)((uint8_t*)p)[0]^=1;return 0;}
static int er(void*c,uint32_t o){(void)c;CHECK(allowed(o,4096)&&!(o&4095));if(++calls==fail_at)return -1;memset(flash+o,255,4096);return 0;}
static int wr(void*c,uint32_t o,const void*p,uint32_t n){(void)c;CHECK(allowed(o,n)&&n&&n<=256&&(o&255)+n<=256);if(++calls==fail_at)return -1;for(unsigned i=0;i<n;i++){uint8_t b=((const uint8_t*)p)[i];CHECK((flash[o+i]&b)==b);flash[o+i]&=b;}return 0;}
static const np_store_io io={0,rd,er,wr};
static void load(np_store*s){calls=fail_at=0;CHECK(!np_store_load(s,&io));}
static void protect(void){for(unsigned i=0;i<NP_LIBRARY_META;i++)CHECK(flash[i]==0xa5);for(unsigned i=NP_LIBRARY_END;i<sizeof flash;i++)CHECK(flash[i]==0xa5);}
static void init(np_store*s){memset(flash,0xa5,sizeof flash);memset(flash+NP_LIBRARY_META,255,NP_LIBRARY_END-NP_LIBRARY_META);load(s);CHECK(!s->catalog.count);}
static uint32_t make(unsigned n,unsigned seed){for(unsigned i=0;i<n;i++)rom[i]=(uint8_t)(i*31+seed);return np_rx_crc32(rom,n);}
int main(void){
 np_store s;init(&s);CHECK(np_store_free(&s)==NP_LIBRARY_BYTES);CHECK(np_store_largest(&s)==NP_CART_LIMIT);
 uint32_t crc=make(40976,7);CHECK(np_store_add(&s,rom,40976,crc,"MARIO")==0);load(&s);CHECK(s.catalog.count==1);np_game first=s.catalog.games[0];
 CHECK(!memcmp(flash+first.offset,rom,40976));CHECK(np_store_add(&s,rom,40976,crc,"DUPLICATE")==0);CHECK(s.catalog.count==1);
 memcpy(saved,flash,sizeof flash);crc=make(NP_CART_LIMIT,11);calls=0;CHECK(np_store_add(&s,rom,NP_CART_LIMIT,crc,"LARGE GAME")==1);unsigned total=calls;CHECK(s.catalog.count==2);CHECK(!memcmp(flash+first.offset,saved+first.offset,first.length));protect();
 for(unsigned cut=1;cut<=total;cut++){
  memcpy(flash,saved,sizeof flash);load(&s);calls=0;fail_at=cut;(void)np_store_add(&s,rom,NP_CART_LIMIT,crc,"LARGE GAME");load(&s);
  CHECK(s.catalog.count==1||s.catalog.count==2);CHECK(!memcmp(flash+first.offset,saved+first.offset,first.length));
  if(s.catalog.count==2)CHECK(!memcmp(flash+s.catalog.games[1].offset,rom,NP_CART_LIMIT));protect();
 }
 memcpy(flash,saved,sizeof flash);load(&s);corrupt=1;CHECK(np_store_add(&s,rom,NP_CART_LIMIT,crc,"BAD")<0);corrupt=0;load(&s);CHECK(s.catalog.count==1);
 CHECK(np_store_add(&s,rom,NP_CART_LIMIT,crc,"LARGE")==1);memcpy(saved,flash,sizeof flash);calls=0;CHECK(!np_store_remove(&s,0,first.crc));total=calls;
 for(unsigned cut=1;cut<=total;cut++){
  memcpy(flash,saved,sizeof flash);load(&s);calls=0;fail_at=cut;(void)np_store_remove(&s,0,first.crc);load(&s);CHECK(s.catalog.count==1||s.catalog.count==2);
  unsigned large=s.catalog.count-1;CHECK(!memcmp(flash+s.catalog.games[large].offset,rom,NP_CART_LIMIT));protect();
 }
 memcpy(flash,saved,sizeof flash);load(&s);CHECK(np_store_remove(&s,0,first.crc^1)<0);CHECK(!np_store_remove(&s,0,first.crc));CHECK(np_store_find(&s,40976)==first.offset);
 crc=make(40976,23);CHECK(np_store_add(&s,rom,40976,crc,"REUSE")==1);CHECK(s.catalog.games[1].offset==first.offset);
 CHECK(np_store_add(&s,rom,40976,crc^1,"BAD CRC")<0);CHECK(!np_store_find(&s,NP_CART_LIMIT));
 init(&s);for(unsigned i=0;i<NP_LIBRARY_MAX;i++){crc=make(16,i);CHECK(np_store_add(&s,rom,16,crc,"SMALL")== (int)i);}CHECK(!np_store_find(&s,16));CHECK(!np_store_largest(&s));
 np_catalog bad=s.catalog;bad.count=33;bad.crc=np_rx_crc32((uint8_t*)&bad,offsetof(np_catalog,crc));CHECK(!np_catalog_valid(&bad));
 bad=s.catalog;bad.games[0].offset=0;bad.crc=np_rx_crc32((uint8_t*)&bad,offsetof(np_catalog,crc));CHECK(!np_catalog_valid(&bad));
 bad=s.catalog;bad.games[1].offset=bad.games[0].offset;bad.crc=np_rx_crc32((uint8_t*)&bad,offsetof(np_catalog,crc));CHECK(!np_catalog_valid(&bad));
 puts("PASS: library append/delete power interruption at every I/O, protected flash, readback corruption, duplicate upload, hole reuse, capacity and malformed catalogs");
}
