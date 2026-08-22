/* Render one frame of the current screen RAM to a PGM file. Test aid. */
#include <stdio.h>
#include <string.h>
#include "../core/mem.h"
#include "../core/vicke.h"
static int load_file(const char *p, uint8_t *b, size_t m){FILE*f=fopen(p,"rb");if(!f)return -1;int n=fread(b,1,m,f);fclose(f);return n;}
int main(void){
    uint8_t chargen[4096], font[256*8]; load_file("data/chargen",chargen,4096);
    memset(font,0,sizeof font);
    for(int a=0x20;a<0x80;a++){int sc=(a<0x40)?a:(a&0x1F); if(a>=0x40&&a<0x60) sc=a&0x3F; memcpy(&font[a*8],&chargen[sc*8],8);}
    mem_init(); vicke_init(); vicke_set_font(font);
    const char *m="BMC-K4510  --  VICKe text layer, 80x60"; mem_load(VICKE_SCREEN_BASE+1*80+2,(const uint8_t*)m,strlen(m));
    for(int c=0;c<80;c++) mem_poke(VICKE_SCREEN_BASE+5*80+c,0x20+(c%0x60));
    static uint8_t fb[640*480]; vicke_render(fb,640);
    FILE*o=fopen("/tmp/k4510-shot.pgm","wb"); fprintf(o,"P5 640 480 255\n");
    for(int i=0;i<640*480;i++) fputc(fb[i]?230:40,o); fclose(o); return 0; }
