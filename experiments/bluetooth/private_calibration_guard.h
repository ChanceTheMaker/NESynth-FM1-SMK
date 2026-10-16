/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once
#include <stdint.h>
#include "../../vendor/smk-updater/smk37_trim.h"
#define NP_PRIVATE_FACTORY_BYTES 8192u
/* The updater and Melodee fingerprint the validated 556-byte usrtrim record.
 * The surrounding sectors are compared for diagnostics, not unit identity. */
typedef int (*np_factory_read)(void *,uint32_t,uint8_t *,uint32_t);
typedef struct {uint32_t full_match,unit_match,first_difference,differences,read_failed;} np_factory_report;
static inline int np_private_unit_matches(np_factory_read read,void *ctx,const uint8_t *expected,unsigned n,np_factory_report *report){
    uint8_t buffer[128];np_factory_report result={0,0,UINT32_MAX,0,0};
    if(report)*report=result;
    if(!read||!expected||n!=NP_PRIVATE_FACTORY_BYTES||!smk37_trim_keys_valid(expected+4096,556))return 0;
    result.unit_match=1;
    for(unsigned off=0;off<n;off+=sizeof buffer){
        if(read(ctx,0xc0000u+off,buffer,sizeof buffer)){
            result.unit_match=0;result.read_failed=1;if(report)*report=result;return 0;
        }
        for(unsigned j=0;j<sizeof buffer;j++)if(buffer[j]!=expected[off+j]){
            unsigned pos=off+j;
            if(!result.differences)result.first_difference=0xc0000u+pos;
            result.differences++;
            if(pos>=4096u&&pos<4096u+556u)result.unit_match=0;
        }
    }
    result.full_match=result.differences==0;if(report)*report=result;return (int)result.unit_match;
}
