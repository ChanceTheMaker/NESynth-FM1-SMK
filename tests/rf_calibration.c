#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../experiments/bluetooth/rf_calibration.h"
static unsigned char flash[0x23000];
static int read_fault;
static unsigned reads;
static uint32_t read_base=0x9d000,read_size=0x20000;
static int read_data(void *c,uint32_t off,uint8_t *p,uint32_t n){
    (void)c;assert(off>=read_base&&off<read_base+read_size&&n<=read_base+read_size-off);reads++;
    if(read_fault)return -1;
    memcpy(p,flash+off-read_base,n);return 0;
}
static unsigned crc(const unsigned char *p,unsigned n){
    unsigned c=0;while(n--){c^=(unsigned)*p++<<8;for(unsigned k=0;k<8;k++)c=((c&32768)?(c<<1)^0x1021:c<<1)&65535;}return c;
}
static unsigned put(unsigned at,unsigned id,const unsigned char *p,unsigned n){
    flash[at]=(unsigned char)crc(p,n);flash[at+1]=(unsigned char)id;
    flash[at+2]=(unsigned char)(n<<4|(id>>8));flash[at+3]=(unsigned char)(n>>4);
    memcpy(flash+at+4,p,n);return at+4+n;
}
static unsigned bank(unsigned base,unsigned seed){
    static const unsigned char magic[]={0x55,0xaa,0xaa,0x55},xosc[]={11,11},pa[]={1,7,4,7,11,1,7};
    unsigned char rf[68]={0};for(unsigned i=0;i<64;i++)rf[i]=(unsigned char)(i+seed);
    unsigned c=crc(rf,64);rf[64]=(unsigned char)c;rf[65]=(unsigned char)(c>>8);
    memcpy(flash+base,magic,4);unsigned at=put(base+4,106,xosc,2);at=put(at,107,pa,7);
    return put(at,187,rf,68);
}
int main(int argc,char **argv){
    if(argc>=2){
        int fm1=argc==3&&!strcmp(argv[2],"fm1");if(fm1){read_base=0xe8000;read_size=4096;}
        FILE *f=fopen(argv[1],"rb");assert(f);assert(!fseek(f,read_base,SEEK_SET));
        assert(fread(flash,1,read_size,f)==read_size);fclose(f);
        np_rf_calibration actual;int result=fm1?np_rf_load_fm1(read_data,0,&actual):np_rf_load_smk(read_data,0,&actual);
        printf("private backup: result=%d bank=0x%x RF record=0x%x records=%u (payload not exported)\n",result,actual.bank,actual.record,actual.records);
        return result!=0;
    }
    np_rf_calibration out;memset(flash,255,sizeof flash);
    assert(np_rf_load_smk(read_data,0,&out)==NP_RF_LAYOUT&&!out.record);
    unsigned end=bank(0,3);bank(0x21000,17); /* stale high-address copy must be ignored */
    assert(np_rf_load_smk(read_data,0,&out)==0&&out.bank==0x9d000&&out.record==0x9d015&&out.records==3);
    assert(out.trim[0]==3&&out.xosc[0]==11&&out.pa[0]==1);
    bank(0x10000,7);assert(np_rf_load_smk(read_data,0,&out)==NP_RF_LAYOUT&&!out.record);
    memset(flash,255,0x10000);assert(np_rf_load_smk(read_data,0,&out)==0&&out.bank==0xad000&&out.trim[0]==7);
    memset(flash,255,sizeof flash);end=bank(0,3);flash[0x15+4]^=1;
    assert(np_rf_load_smk(read_data,0,&out)==NP_RF_CORRUPT&&!out.record);
    bank(0,3);flash[0x15+4+64]^=1;flash[0x15]=(unsigned char)crc(flash+0x19,68);
    assert(np_rf_load_smk(read_data,0,&out)==NP_RF_CORRUPT);
    bank(0,3);flash[end]=0;assert(np_rf_load_smk(read_data,0,&out)==NP_RF_CORRUPT);
    memset(flash+end,255,4);read_fault=1;assert(np_rf_load_smk(read_data,0,&out)==NP_RF_IO&&!out.record);
    read_fault=0;memset(flash+0x15,255,72);assert(np_rf_load_smk(read_data,0,&out)==NP_RF_MISSING);
    read_base=0xe8000;read_size=4096;memset(flash,255,sizeof flash);bank(0,19);
    assert(np_rf_load_fm1(read_data,0,&out)==0&&out.bank==0xe8000&&out.record==0xe8015&&out.trim[0]==19);
    flash[0x19]^=1;assert(np_rf_load_fm1(read_data,0,&out)==NP_RF_CORRUPT&&!out.record);
    bank(0,19);flash[0x15+2]=0xf0;flash[0x15+3]=0xff;
    assert(np_rf_load_fm1(read_data,0,&out)==NP_RF_CORRUPT&&!out.record);
    memset(flash,255,sizeof flash);bank(0x1000,19);
    assert(np_rf_load_fm1(read_data,0,&out)==NP_RF_LAYOUT&&!out.record);
    read_fault=1;assert(np_rf_load_fm1(read_data,0,&out)==NP_RF_IO&&!out.record);
    assert(reads>0);puts("PASS: read-only RF bank selection, stale copies, ambiguous banks, CRCs and failed reads");
}
