/* SPDX-License-Identifier: GPL-3.0-only
 * AC79 CPU1 launch follows the retained Felucca fm1_multicore.h and crt0.S.
 * IRQs, USB, audio, NES/APU state and LCD DMA remain exclusively on CPU0. */
#define GB_VIDEO_RAM __attribute__((section(".dsp_text"),noinline))
#define GB_VIDEO_BARRIER() __asm__ volatile("csync" ::: "memory")
#include "video_job.h"
static gb_video_job_t video_job;
static uint8_t video_source[GB_VIDEO_PIXELS] __attribute__((section(".pool"),aligned(32)));
static uint32_t video_stacks[2048] __attribute__((section(".cpu1_stacks"),aligned(32),used));
static volatile uint32_t video_ready;
volatile uint32_t video_stage;
static volatile uint32_t video_core,video_worker_jobs,video_work_max_ticks;
static uint32_t video_launch_ready,video_retire_reason,video_retire_ticks;
static uint32_t video_launch_con,video_launch_pc,video_launch_emu,video_launch_dbg;
static uint32_t video_online,video_pending,video_started,video_faults,video_jobs,video_fallbacks;
volatile struct {uint32_t magic,barred,running;} video_guard __attribute__((section(".noinit")));
extern void gb_core1_start(void);
extern uint8_t _cpu1_stacks_lo[],_cpu1_stacks_hi[];
static void video_barrier(void){GB_VIDEO_BARRIER();}
/* XIP-to-SRAM calls exceed the pi32v2 short branch range. */
static void video_scale_local(const uint8_t *src,uint16_t *out){
 uint32_t (*volatile fn)(const uint8_t*,uint16_t*)=gb_video_scale;
 uint32_t rows=fn(src,out);video_dirty_top=rows>>16;video_dirty_bottom=rows&65535;
}
static uint32_t video_hash_local(const uint8_t *src){
 uint32_t (*volatile fn)(const uint8_t*)=gb_video_hash;return fn(src);
}
void __attribute__((section(".dsp_text"),noreturn,used)) gb_core1_main(void){
 video_stage=3;GB_VIDEO_BARRIER();
 uint32_t lo=(uint32_t)(uintptr_t)_cpu1_stacks_lo+256u,hi=(uint32_t)(uintptr_t)_cpu1_stacks_hi-1u;
 *(volatile uint32_t*)0x1EEF2D8u=hi;*(volatile uint32_t*)0x1EEF2DCu=lo;
 *(volatile uint32_t*)0x1EEF2E0u=hi;*(volatile uint32_t*)0x1EEF2E4u=lo;
 *(volatile uint32_t*)0x1EEF2D0u=(*(volatile uint32_t*)0x1EEF2D0u&~((1u<<2)|(0x1fu<<16)))|(1u<<3);
 uint32_t core;__asm__ volatile("%0 = cnum" : "=r"(core));
 video_core=core;video_stage=4;GB_VIDEO_BARRIER();video_ready=1;GB_VIDEO_BARRIER();
 for(;;){
  if(!video_job.request||video_job.request==video_job.complete)continue;
  uint32_t start=*(volatile uint32_t*)0x10804u;
  int done=gb_video_work(&video_job,video_source,screen);
  if(done>0){
   uint32_t ticks=*(volatile uint32_t*)0x10804u-start;
   if(ticks>video_work_max_ticks)video_work_max_ticks=ticks;
   video_worker_jobs++;GB_VIDEO_BARRIER();
  }
 }
}
static void video_stop(void){
 fm1_core1_stop();video_barrier();video_online=0;video_pending=0;video_guard.running=0;
}
static void video_retire(void){
 video_stop();video_guard.barred=1;video_fallbacks++;
}
static void video_start(uint32_t cold){
 if(cold||video_guard.magic!=0x47445634u){video_guard.magic=0x47445634u;video_guard.barred=video_guard.running=0;}
 if(video_guard.running)video_guard.barred=1;
 if(video_guard.barred)return;
 fm1_core1_stop();video_stage=video_core=0;video_ready=0;video_job.request=video_job.complete=video_job.error=0;
 video_guard.running=1;video_faults=fm1_core1_fault.count;
 for(uint32_t i=0;i<32;i++)*(volatile uint32_t*)(0x1EEF300u+4*i)=0;
 *(volatile uint32_t*)0x1EEF3C0u|=1u; /* CPU1 branch trace for startup diagnostics. */
 *(volatile uint32_t*)0x01C7FFF8u=(uint32_t)(uintptr_t)gb_core1_start;
 uint32_t div=*(volatile uint32_t*)0x10008u;
 *(volatile uint32_t*)0x10008u=div|8u;video_barrier();
 *(volatile uint32_t*)0x1EEE004u|=8u;*(volatile uint32_t*)0x1EEE004u&=~2u;
 uint32_t start=fm1_ticks();
 while(!video_ready&&(uint32_t)(fm1_ticks()-start)<240000u)video_barrier();
 video_barrier();video_online=video_ready!=0&&video_core==1u;video_launch_ready=video_online;
 video_launch_con=*(volatile uint32_t*)0x1EEE004u;
 video_launch_pc=*(volatile uint32_t*)0x1EEF3C4u;
 video_launch_emu=*(volatile uint32_t*)0x1EEF2D4u;video_launch_dbg=FM1_DBG_MSG;
 *(volatile uint32_t*)0x10008u=div;
 if(!video_online){video_retire_reason=1;video_retire();}
}
static void video_submit(const uint8_t *pixels){
 if(!video_online){video_scale_local(pixels,screen);return;}
 for(unsigned i=0;i<GB_VIDEO_PIXELS;i++)video_source[i]=pixels[i];
 video_job.hash=video_hash_local(video_source);
 uint32_t next=video_job.request+1u;if(!next)next=1;
 video_job.request_inverse=~next;video_job.error=0;video_started=fm1_ticks();video_pending=next;
 video_barrier();video_job.request=next;video_barrier();
}
/* Returns a complete, stable buffer; a bad worker is held before fallback. */
static int video_complete(void){
 if(!video_pending)return 0;
 video_barrier();
 uint32_t elapsed=(uint32_t)(fm1_ticks()-video_started);
 int fault=fm1_core1_fault.count!=video_faults;
 int result=gb_video_poll(&video_job,video_pending,elapsed,240000u,fault);
 if(result<0){
  video_retire_reason=fault?2:video_job.error?3:4;video_retire_ticks=elapsed;
  video_retire();video_scale_local(video_source,screen);
  /* A fault could interrupt writes; refresh everything after retirement. */
  video_dirty_top=0;video_dirty_bottom=225;return 1;
 }
 if(result>0){video_barrier();video_pending=0;video_jobs++;
  video_dirty_top=video_job.dirty_rows>>16;video_dirty_bottom=video_job.dirty_rows&65535;return 1;}
 return 0;
}
