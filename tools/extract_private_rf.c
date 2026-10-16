/* SPDX-License-Identifier: GPL-3.0-only */
/* Local-only extraction. Payload goes directly to an ignored generated header. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../experiments/bluetooth/rf_calibration.h"
#include "../vendor/smk-updater/smk37_trim.h"
static unsigned char first[1048576],second[1048576],factory[8192],factory_check[8192];
static int read_flash(void *ctx,uint32_t off,uint8_t *p,uint32_t n){
    (void)ctx;if(off>=sizeof first||n>sizeof first-off)return -1;
    memcpy(p,first+off,n);return 0;
}
static int load(const char *path,unsigned char *data,unsigned size){
    FILE *f=fopen(path,"rb");if(!f)return -1;
    int good=fread(data,1,size,f)==size&&fgetc(f)==EOF;fclose(f);return good?0:-1;
}
static void array(FILE *f,const unsigned char *p,unsigned n){
    fputc('{',f);for(unsigned i=0;i<n;i++)fprintf(f,"%s%u",i?",":"",p[i]);fputc('}',f);
}
int main(int argc,char **argv){
    np_rf_calibration rf;
    if((argc!=4&&argc!=6)||load(argv[1],first,sizeof first)||load(argv[2],second,sizeof second)||memcmp(first,second,sizeof first)){
        fputs("Private calibration requires two identical complete backups\n",stderr);return 1;
    }
    if(np_rf_load_smk(read_flash,0,&rf)!=NP_RF_OK){fputs("Backup has no unambiguous valid RF records\n",stderr);return 1;}
    memcpy(factory,first+0xc0000,sizeof factory);
    if(argc==6&&(load(argv[4],factory,sizeof factory)||load(argv[5],factory_check,sizeof factory_check)||memcmp(factory,factory_check,sizeof factory))){
        fputs("Current factory reference requires two identical 8192-byte backups\n",stderr);return 1;
    }
    if(!smk37_trim_keys_valid(factory+4096,556)){
        fputs("Invalid usrtrim identity record\n",stderr);return 1;
    }
    FILE *f=fopen(argv[3],"wb");if(!f)return 1;
    fputs("/* PRIVATE device calibration: never commit or publish this header or its firmware. */\n",f);
    fputs("static const unsigned char np_private_factory[8192]=",f);array(f,factory,sizeof factory);fputs(";\n",f);
    fputs("static const np_rf_calibration np_private_rf={.trim=",f);array(f,rf.trim,sizeof rf.trim);
    fputs(",.xosc=",f);array(f,rf.xosc,sizeof rf.xosc);fputs(",.pa=",f);array(f,rf.pa,sizeof rf.pa);
    fprintf(f,",.bank=%u,.record=%u,.records=%u};\n",rf.bank,rf.record,rf.records);
    int failed=ferror(f);if(fclose(f))failed=1;if(failed)return 1;
    puts("PASS: identical backups, strict RF parser, private header written; payload not printed");return 0;
}
