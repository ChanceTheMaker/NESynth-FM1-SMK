/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once
#include <stdint.h>
/* Pro016 0x2006170: UART1 TX mux nibble 20..23 = 1, PC7 output.
 * 0x202daf4 writes TXADR/TXCNT; 0x202de54 tests pending bit 15 and clears
 * it with bit 13. Poll DMA completion instead of enabling its TX interrupt.
 * This reproduces the stock pin setup; electrical validation is still needed. */
#ifndef SMK37_UART_REG
#define SMK37_UART_REG(a) (*(volatile uint32_t *)(uintptr_t)(a))
#endif
#ifndef SMK37_UART_SYNC
#define SMK37_UART_SYNC() __asm__ volatile("csync" ::: "memory")
#endif
static void smk37_uart_tx_start(void){
    SMK37_UART_REG(0x12100)=0x3400;
    SMK37_UART_REG(0x12104)=0;
    SMK37_UART_REG(0x10010)=(SMK37_UART_REG(0x10010)&~(3u<<10))|(1u<<10); /* PLL48M */
    SMK37_UART_REG(0x51028)=(SMK37_UART_REG(0x51028)&~(0xf00000u|0x80u))|0x100000u;
    SMK37_UART_REG(0x50090)|=1u<<7; /* PC7 PU */
    SMK37_UART_REG(0x50094)|=1u<<7; /* PC7 PD, stock setup */
    SMK37_UART_REG(0x50080)&=~(1u<<7);
    SMK37_UART_REG(0x50088)&=~(1u<<7);
    SMK37_UART_REG(0x5008c)|=1u<<7;
    SMK37_UART_REG(0x12108)=48000000u/31250u/4u-1u;
    SMK37_UART_SYNC();SMK37_UART_REG(0x12100)=0x3421u; /* no RX or IRQ */
}
static int smk37_uart_tx_done(void){return !!(SMK37_UART_REG(0x12100)&0x8000u);}
static void smk37_uart_tx_send(const uint8_t *bytes,unsigned n){
    SMK37_UART_REG(0x12100)|=0x2000u;
    SMK37_UART_REG(0x12114)=(uint32_t)(uintptr_t)bytes;
    SMK37_UART_SYNC();SMK37_UART_REG(0x12118)=n;
}
static void smk37_uart_tx_stop(void){
    SMK37_UART_REG(0x12100)=0x3400;
    SMK37_UART_REG(0x50080)|=1u<<7; /* idle high */
}
