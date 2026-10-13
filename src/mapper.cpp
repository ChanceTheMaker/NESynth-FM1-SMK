/* SPDX-License-Identifier: GPL-3.0-only */
#include "InfoNES.h"
#include "InfoNES_Mapper.h"
#include "K6502.h"
#include "mapper/InfoNES_Mapper_000.cpp"
#include "mapper_mmc1.cpp"
#include "mapper_mmc2.cpp"
MapperTable_tag MapperTable[]={{0,Map0_Init},{1,Map1_Init},{9,Map9_Init},{-1,0}};

