"""One-time adaptation of the copied GameDude platform; source stays reviewable."""
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def main():
    p=ROOT/'firmware/board.c';s=p.read_text()
    if 'np_rom_size' in s:
        raise SystemExit('Platform already adapted; refusing to apply bootstrap twice.')
    s=s.replace('#include "boot_brand.h"','')
    start=s.index('static void read_samples(void)');end=s.index('\n}',start)+2
    s=s[:start]+'static void read_samples(void){gp_audio_read(samples,256);}'+s[end:]
    start=s.index('static void boot_splash(');end=s.index('static void fm1_fault',start)
    s=s[:start]+s[end:]
    s=s.replace('screen[240*216]','screen[240*225]').replace('0,12,240,216','0,7,240,225')
    s=s.replace('uint32_t cart_size=cart_header[0x148]<=4?(32768u<<cart_header[0x148]):0;',
                'uint32_t cart_size=np_rom_size(cart_header,sizeof cartridge);')
    s=s.replace('splash_active=1;ding_position=GD_DING_FRAMES;','splash_active=0;')
    s=s.replace('    boot_splash(display_ok);','    gb_video_palette_init();')
    s=s.replace('gp_frame_count()-drawn>=6','gp_frame_count()-drawn>=2')
    s=s.replace('static uint32_t display_count;',
        'static uint32_t display_count;\nstatic uint32_t emu_last_ticks,emu_max_ticks,emu_total_ticks,emu_measured_frames;\nstatic uint32_t lcd_started,lcd_max_ticks,lcd_measuring;')
    s=s.replace('            gp_frame();',
        '            uint32_t begun=fm1_ticks();\n            int advanced=gp_frame();\n            emu_last_ticks=fm1_ticks()-begun;\n            if(advanced){emu_measured_frames++;emu_total_ticks+=emu_last_ticks;\n                if(emu_last_ticks>emu_max_ticks)emu_max_ticks=emu_last_ticks;}')
    s=s.replace('    lcd_sync();return 1;',
        '    lcd_sync();\n    if(lcd_measuring){uint32_t dt=fm1_ticks()-lcd_started;if(dt>lcd_max_ticks)lcd_max_ticks=dt;lcd_measuring=0;}\n    return 1;')
    s=s.replace('static void display_frame(void){lcd_blit(0,7,240,225,screen);}',
        'static void display_frame(void){lcd_started=fm1_ticks();lcd_measuring=1;lcd_blit(0,7,240,225,screen);}')
    p.write_text(s)
    p=ROOT/'firmware/multicore_video.h';s=p.read_text().replace('uint16_t video_source','uint8_t video_source').replace('const uint16_t *src','const uint8_t *src').replace('const uint16_t*,uint16_t*','const uint8_t*,uint16_t*').replace('const uint16_t*)','const uint8_t*)').replace('const uint16_t *pixels','const uint8_t *pixels');p.write_text(s)
    p=ROOT/'firmware/fm1_recovery.h';s=p.read_text().replace('FM-1_937','FM-1_950').replace("'G'","'N'").replace("'D'","'S'")
    s=s.replace('video_work_max_ticks};','video_work_max_ticks,\n            emu_last_ticks,emu_max_ticks,emu_total_ticks,emu_measured_frames,lcd_max_ticks};')
    s=s.replace('uint8_t response[192]','uint8_t response[224]');p.write_text(s)
if __name__=='__main__':main()
