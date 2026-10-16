/* SPDX-License-Identifier: GPL-3.0-only */
#include "rom_transfer.h"
static uint32_t crc_step(uint32_t crc,const uint8_t *p,uint32_t n){
    while(n--){crc^=*p++;for(unsigned i=0;i<8;i++)crc=(crc>>1)^(0xedb88320u& (0u-(crc&1u)));}
    return crc;
}
uint32_t np_rx_crc32(const uint8_t *p,uint32_t n){return ~crc_step(~0u,p,n);}
void np_rx_init(np_rx *s,const np_rx_io *io,uint32_t capacity){
    *s=(np_rx){0};s->io=*io;s->capacity=capacity;
}
static int failed(np_rx *s,int why){s->state=NP_RX_FAILED;return why;}
int np_rx_begin(np_rx *s,uint32_t session,uint32_t length,uint32_t crc){
    if(!session||length<16||length>s->capacity)return NP_RX_RANGE;
    if(!s->io.begin||!s->io.write||!s->io.read||!s->io.validate||!s->io.publish)return NP_RX_IO;
    if(s->state==NP_RX_RECEIVING){
        /* Retried BEGIN acknowledges progress; another transfer needs ABORT. */
        return s->session==session&&s->length==length&&s->expected_crc==crc?NP_RX_OK:NP_RX_STATE;
    }
    if(s->session==session)return NP_RX_STATE; /* closed sessions cannot restart */
    if(s->io.begin(s->io.ctx,length))return failed(s,NP_RX_IO);
    s->session=session;s->length=length;s->expected_crc=crc;s->received=0;s->state=NP_RX_RECEIVING;
    return NP_RX_OK;
}
int np_rx_data(np_rx *s,uint32_t session,uint32_t offset,const uint8_t *data,uint32_t n,uint32_t crc){
    uint8_t check[NP_TRANSFER_CHUNK];
    if(s->state!=NP_RX_RECEIVING||session!=s->session)return NP_RX_STATE;
    if(!data||!n||n>NP_TRANSFER_CHUNK||offset>s->length||n>s->length-offset)return NP_RX_RANGE;
    if(np_rx_crc32(data,n)!=crc)return NP_RX_CRC;
    if(offset<s->received){
        if(n>s->received-offset)return NP_RX_RANGE;
        if(s->io.read(s->io.ctx,offset,check,n))return failed(s,NP_RX_IO);
        for(uint32_t i=0;i<n;i++)if(check[i]!=data[i])return NP_RX_CRC;
        return NP_RX_OK; /* Exact replay: acknowledge without another write. */
    }
    if(offset!=s->received)return NP_RX_RANGE;
    if(s->io.write(s->io.ctx,offset,data,n))return failed(s,NP_RX_IO);
    s->received+=n;return NP_RX_OK;
}
int np_rx_commit(np_rx *s,uint32_t session){
    uint8_t block[NP_TRANSFER_CHUNK];uint32_t crc=~0u;
    if(session!=s->session)return NP_RX_STATE;
    if(s->state==NP_RX_VERIFIED)return NP_RX_OK; /* lost final ACK */
    if(s->state!=NP_RX_RECEIVING||s->received!=s->length)return NP_RX_STATE;
    for(uint32_t off=0;off<s->length;){
        uint32_t n=s->length-off;if(n>sizeof block)n=sizeof block;
        if(s->io.read(s->io.ctx,off,block,n))return failed(s,NP_RX_IO);
        crc=crc_step(crc,block,n);off+=n;
    }
    if(~crc!=s->expected_crc)return failed(s,NP_RX_CRC);
    if(s->io.validate(s->io.ctx,s->length))return failed(s,NP_RX_ROM);
    if(s->io.publish(s->io.ctx,s->length,s->expected_crc))return failed(s,NP_RX_IO);
    s->state=NP_RX_VERIFIED;return NP_RX_OK;
}
void np_rx_abort(np_rx *s){s->state=NP_RX_IDLE;s->received=0;}
