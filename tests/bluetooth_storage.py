"""Fault-inject the actual firmware publication function, extracted to avoid MCU assembly."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[1]
source=(root/'experiments/bluetooth/loader_board.h').read_text()
body=source[source.index('static int np_program_cartridge('):source.index('static void np_loader_boot(')]
preamble=r'''
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define NP_CART_META 0x5b000u
#define NP_CART_DATA 0x5c000u
#define NP_CART_LIMIT 262672u
#define NP_CART_MAGIC 0x3143504eu
#define FL_FAR(f) f
static uint8_t flash[0x100000],stage[NP_CART_LIMIT];
#define NP_STAGE stage
typedef struct {uint32_t magic,length,crc,check;} np_cart_header;
static uint32_t np_header_check(uint32_t m,uint32_t n,uint32_t c){return ~(m^n^c);}
static void fm1_wdt_feed(void){}
static unsigned calls,fail_at,corrupt_read;
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static int allowed(uint32_t o,uint32_t n){return o>=NP_CART_META&&o<=0x9d000u&&n<=0x9d000u-o;}
static int fl_erase4k_ram(uint32_t off,uint32_t *took){
 CHECK(allowed(off,4096)&&!(off&4095));*took=1;if(++calls==fail_at)return -1;
 memset(flash+off,255,4096);return 0;
}
static int fl_prog_ram(uint32_t off,const uint8_t *p,uint32_t n,uint32_t *took){
 CHECK(allowed(off,n)&&n&&n<=256&&(off&255)+n<=256);*took=1;if(++calls==fail_at)return -1;
 for(unsigned i=0;i<n;i++){CHECK((flash[off+i]&p[i])==p[i]);flash[off+i]&=p[i];}return 0;
}
static int fl_read_ram(uint32_t off,uint8_t *p,uint32_t n){
 CHECK(allowed(off,n));if(++calls==fail_at)return -1;memcpy(p,flash+off,n);
 if(corrupt_read&&off==NP_CART_DATA)p[0]^=1;return 0;
}
static void reset(unsigned fail){memset(flash,0xa5,sizeof flash);memset(flash+NP_CART_META,255,0x9d000-NP_CART_META);calls=0;fail_at=fail;corrupt_read=0;}
static void protected(void){for(unsigned i=0;i<NP_CART_META;i++)CHECK(flash[i]==0xa5);for(unsigned i=0x9d000;i<sizeof flash;i++)CHECK(flash[i]==0xa5);}
'''
tail=r'''
int main(void){
 for(unsigned i=0;i<sizeof stage;i++)stage[i]=(uint8_t)(i*17u+31u);
 const unsigned sizes[]={16,40976,262672};
 for(unsigned s=0;s<3;s++){
  unsigned n=sizes[s];reset(0);CHECK(!np_program_cartridge(n,0x12345678));unsigned total=calls;
  CHECK(!memcmp(flash+NP_CART_DATA,stage,n));np_cart_header *h=(void*)(flash+NP_CART_META);
  CHECK(h->magic==NP_CART_MAGIC&&h->length==n&&h->crc==0x12345678&&h->check==np_header_check(h->magic,n,h->crc));protected();
  for(unsigned cut=1;cut<=total;cut++){
   reset(cut);CHECK(np_program_cartridge(n,0x12345678));protected();
   if(cut<total)CHECK(h->magic!=NP_CART_MAGIC);
   else CHECK(!memcmp(flash+NP_CART_DATA,stage,n));
  }
  reset(0);corrupt_read=1;CHECK(np_program_cartridge(n,0x12345678)==5);CHECK(h->magic!=NP_CART_MAGIC);protected();
 }
 puts("PASS: actual flash writer, every I/O interruption, readback corruption, metadata-last publication, maximum ROM, and protected flash bounds.");
}
'''
with tempfile.TemporaryDirectory() as directory:
    c=Path(directory)/'storage.c';exe=Path(directory)/'storage';c.write_text(preamble+body+tail)
    subprocess.run(['cc','-O1','-g','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-fsanitize=address,undefined','-fno-sanitize-recover=all',str(c),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)