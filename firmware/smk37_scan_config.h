/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once
/* NES controls do not need the stock velocity scanner's ~200k IRQ/s.
 * Slow SPI2 before its first transfer, preserving the entire latch/ADC
 * pipeline. Divider 95 versus stock 5 increases wire time 16-fold; the
 * expected full digital scan remains around 1 kHz, analog sweep ~6 ms.
 * Physical rates and controls must be verified on the target. */
#define SMK37_SCAN_BAUD 95u
