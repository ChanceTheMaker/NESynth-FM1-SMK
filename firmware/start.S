/* SPDX-License-Identifier: GPL-3.0-only
 * Entry and interrupt convention from Felucca by Leo Kuroshita / Hugelton. */
.section .text.crt0,"ax",@progbits
.globl _start
_start:
    goto 1f
    rts
1:
    cli
    sp = _ustack_top
    ssp = _sstack_top
    call gb_cstart
    goto .
.macro GB_ISR name, target
.section .text.\name,"ax",@progbits
.globl \name
\name:
    [--sp] = {psr, rets, reti}
    [--sp] = {r3-r0}
    call \target
    {r3-r0} = [sp++]
    {psr, rets, reti} = [sp++]
    csync
    rti
.endm
GB_ISR gb_isr_audio, gb_audio_irq
GB_ISR gb_isr_scan, gb_scan_irq

/* Private IRQ-disabled second-core worker, matching the AC79 reset ABI. */
.section .text.core1,"ax",@progbits
.globl gb_core1_start
gb_core1_start:
    r0 = video_stage
    r1 = 1
    [r0] = r1
    csync
    cli
    icfg = 0
    usp = _cpu1_ustack_top
    sp = _cpu1_sstack_top
    ssp = _cpu1_sstack_top
    r1 = 2
    [r0] = r1
    csync
    reti = gb_core1_main
    rti
