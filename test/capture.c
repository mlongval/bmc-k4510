/* Headless capture: run a ROM for N frames, write frame as PNG (no zlib: stored deflate). */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../core/xemu/emutools_basicdefs.h"
#include "../core/xemu/cpu65.h"
#include "../core/mem.h"
#include "../core/io.h"
#include "../core/vicke.h"
static uint32_t crc_table[256];
static void crc_init(void){for(uint32_t n=0;n<256;n++){uint32_t c=n;for(int k=0;k<8;k++)c=(c&1)?0xEDB88320u^(c>>1):c>>1;crc_table[n]=c;}}
static uint32_t crc(uint32_t c,const uint8_t*b,size_t n){c=~c;for(size_t i=0;i<n;i++)c=crc_table[(c^b[i])&0xFF]^(c>>8);return ~c;}
static uint32_t adler(const uint8_t*b,size_t n){uint32_t a=1,s=0;for(size_t i=0;i<n;i++){a=(a+b[i])%65521;s=(s+a)%65521;}return (s<<16)|a;}
static void be32(FILE*f,uint32_t v){fputc(v>>24,f);fputc(v>>16,f);fputc(v>>8,f);fputc(v,f);}
static void chunk(FILE*f,const char*t,const uint8_t*d,size_t n){be32(f,n);uint8_t*b=malloc(n+4);memcpy(b,t,4);if(n)memcpy(b+4,d,n);fwrite(b,1,n+4,f);be32(f,crc(0,b,n+4));free(b);}
int main(int argc,char**argv){
    const char*rom=argc>1?argv[1]:"rom/wozmon.bin"; int frames=argc>2?atoi(argv[2]):60; const char*out=argc>3?argv[3]:"/tmp/k4510.png";
    const char*keys=argc>4?argv[4]:"";
    uint8_t font[2048]; FILE*ff=fopen("data/font8.bin","rb"); if(!ff||fread(font,1,2048,ff)!=2048){fprintf(stderr,"font\n");return 1;} fclose(ff);
    if(mem_init())return 1; mem_load(K4510_FONT8_PHYS,font,2048); if(mem_load_rom(rom)!=4096){fprintf(stderr,"rom\n");return 1;}
    cpu65_reset();
    static uint8_t fb[640*480];
    for(int fr=0;fr<frames;fr++){
        if(fr==5) for(const char*k=keys;*k;k++) kbd_push(*k=='\\n'?0x0D:(uint8_t)*k);
        vicke_begin_frame(fb,640);
        for(int y=0;y<480;y++){cpu65.irqLevel=vicke_irq()?1:0;cpu65_step(40500000/60/480);vicke_line(y);}
        vicke_end_frame();
    }
    /* PNG, RGB, stored deflate blocks */
    size_t rowb=1+640*3, raw_n=rowb*480; uint8_t*raw=malloc(raw_n);
    for(int y=0;y<480;y++){raw[y*rowb]=0;for(int x=0;x<640;x++){uint32_t c=vicke_palette_rgb(fb[y*640+x]);uint8_t*p=&raw[y*rowb+1+x*3];p[0]=c>>16;p[1]=c>>8;p[2]=c;}}
    size_t nblk=(raw_n+65534)/65535; size_t z_n=2+raw_n+nblk*5+4; uint8_t*z=malloc(z_n); size_t zi=0; z[zi++]=0x78;z[zi++]=0x01;
    for(size_t off=0;off<raw_n;off+=65535){size_t len=raw_n-off>65535?65535:raw_n-off;z[zi++]=(off+len>=raw_n);z[zi++]=len&0xFF;z[zi++]=len>>8;z[zi++]=(~len)&0xFF;z[zi++]=((~len)>>8)&0xFF;memcpy(z+zi,raw+off,len);zi+=len;}
    uint32_t ad=adler(raw,raw_n); z[zi++]=ad>>24;z[zi++]=ad>>16;z[zi++]=ad>>8;z[zi++]=ad;
    crc_init(); FILE*o=fopen(out,"wb"); fwrite("\x89PNG\r\n\x1a\n",1,8,o);
    uint8_t ih[13]={0,0,2,128,0,0,1,224,8,2,0,0,0}; chunk(o,"IHDR",ih,13); chunk(o,"IDAT",z,zi); chunk(o,"IEND",NULL,0); fclose(o);
    printf("%s: %d frames -> %s\n",rom,frames,out); return 0; }
