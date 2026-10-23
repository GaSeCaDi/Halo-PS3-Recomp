#include "texture.h"
#include "../cache/cache_index.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <unistd.h>
#include <sysutil/video.h>
#include <rsx/gcm_sys.h>
#include <rsx/rsx.h>
int h3_preview(const uint32_t *pixels,unsigned width,unsigned height){
 void *host=memalign(1024*1024,32*1024*1024);gcmContextData *ctx=NULL;
 videoState state;videoResolution resolution;videoConfiguration config;uint32_t offset;
 if(!host)return -1;
 rsxInit(&ctx,1024*1024,32*1024*1024,host);
 if(!ctx||videoGetState(0,0,&state)||state.state||videoGetResolution(state.displayMode.resolution,&resolution))return -1;
 memset(&config,0,sizeof(config));config.resolution=state.displayMode.resolution;config.format=VIDEO_BUFFER_FORMAT_XRGB;config.pitch=resolution.width*4;config.aspect=state.displayMode.aspect;
 if(videoConfigure(0,&config,NULL,0))return -1;
 uint32_t *screen=rsxMemalign(64,(size_t)resolution.width*resolution.height*4);
 if(!screen||rsxAddressToOffset(screen,&offset)||gcmSetDisplayBuffer(0,offset,config.pitch,resolution.width,resolution.height))return -1;
 unsigned scale=resolution.width/width;if(resolution.height/height<scale)scale=resolution.height/height;if(scale>3)scale=3;if(!scale)scale=1;
 for(unsigned y=0;y<resolution.height;y++)for(unsigned x=0;x<resolution.width;x++){
  int sx=(int)x-((int)resolution.width-(int)(width*scale))/2,sy=(int)y-((int)resolution.height-(int)(height*scale))/2;
  uint32_t color=0xff101820;
  if(sx>=0&&sy>=0&&(unsigned)sx<width*scale&&(unsigned)sy<height*scale){
   uint32_t p=pixels[(size_t)(sy/(int)scale)*width+sx/(int)scale];unsigned a=p>>24;
   unsigned bg=(((x/16)^(y/16))&1)?64:96;unsigned r=(((p>>16)&255)*a+bg*(255-a))/255,g=(((p>>8)&255)*a+bg*(255-a))/255,b=((p&255)*a+bg*(255-a))/255;
   color=0xff000000|(r<<16)|(g<<8)|b;
  }
  screen[(size_t)y*resolution.width+x]=color;
 }
 __sync_synchronize();gcmSetFlipMode(GCM_FLIP_VSYNC);gcmResetFlipStatus();
 printf("H3TEXTURE_DISPLAY %ux%u framebuffer_crc32=%08x\n",resolution.width,resolution.height,h3_crc32_update(0,screen,(size_t)resolution.width*resolution.height*4));fflush(stdout);
 for(unsigned i=0;i<1800;i++){
  if(gcmSetFlip(ctx,0))return -1;
  rsxFlushBuffer(ctx);unsigned wait;
  for(wait=0;wait<50000&&gcmGetFlipStatus();wait++)usleep(100);
  if(wait==50000)return -1;
  gcmResetFlipStatus();gcmSetWaitFlip(ctx);
 }
 rsxFinish(ctx,1);free(host);return 0;
}
