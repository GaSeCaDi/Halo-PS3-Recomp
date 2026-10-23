/* SPDX-License-Identifier: GPL-3.0-or-later
 * Xenos address equation adapted from public TagTool XboxGraphics.cs.
 * BC1 decoder and bounded surface conversion implemented for H3-PS3.
 */
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "texture.h"
static uint32_t tiled(uint32_t x,uint32_t y,uint32_t blocks){
 uint32_t macro=((x/32)+(y/32)*((blocks+31)/32))<<10;
 uint32_t micro=((x&7)+((y&6)<<2))<<3;
 uint32_t a=macro+((micro&~15u)<<1)+(micro&15)+((y&8)<<6)+((y&1)<<4);
 return ((a&~511u)<<3)+((a&448)<<2)+(a&63)+((y&16)<<7)+(((((y&8)>>2)+(x>>3))&3)<<6);
}
static uint32_t rgb565(unsigned v){
 unsigned r=(v>>11)&31,g=(v>>5)&63,b=v&31;
 return 0xff000000u|(((r<<3)|(r>>2))<<16)|(((g<<2)|(g>>4))<<8)|((b<<3)|(b>>2));
}
int h3_bc1_xenos(const uint8_t *src,size_t size,unsigned width,unsigned height,uint32_t *out,size_t pixels){
 if(!src||!out||!width||!height||width>4096||height>4096||pixels<(size_t)width*height)return -1;
 unsigned bw=(width+3)/4,bh=(height+3)/4;
 for(unsigned by=0;by<bh;by++)for(unsigned bx=0;bx<bw;bx++){
  size_t off=tiled(bx,by,bw);uint8_t b[8];uint32_t colors[4];
  if(off>size||size-off<8)return -1;
  for(unsigned j=0;j<8;j++)b[j]=src[off+(j^1)];
  unsigned c0=b[0]|((unsigned)b[1]<<8),c1=b[2]|((unsigned)b[3]<<8);
  colors[0]=rgb565(c0);colors[1]=rgb565(c1);colors[2]=colors[3]=0;
  for(unsigned channel=0;channel<3;channel++){
   unsigned shift=channel*8,a=(colors[0]>>shift)&255,c=(colors[1]>>shift)&255;
   colors[2]|=((c0>c1?(2*a+c)/3:(a+c)/2)<<shift);
   if(c0>c1)colors[3]|=((a+2*c)/3)<<shift;
  }
  colors[2]|=0xff000000u;if(c0>c1)colors[3]|=0xff000000u;
  uint32_t bits=(uint32_t)b[4]|((uint32_t)b[5]<<8)|((uint32_t)b[6]<<16)|((uint32_t)b[7]<<24);
  for(unsigned py=0;py<4;py++)for(unsigned px=0;px<4;px++){
   unsigned x=bx*4+px,y=by*4+py;
   if(x<width&&y<height)out[(size_t)y*width+x]=colors[(bits>>(2*(py*4+px)))&3];
  }
 }
 return 0;
}
unsigned h3_texture_tests(void){
 uint32_t pixels[16];unsigned passed=0;
 const uint8_t red_green[8]={0xf8,0x00,0x07,0xe0,0,0,0,0};
 uint8_t block[8];memcpy(block,red_green,8);
 passed+=h3_bc1_xenos(block,8,4,4,pixels,16)==0&&pixels[0]==0xffff0000u&&pixels[15]==0xffff0000u;
 memset(block+4,0x55,4);passed+=h3_bc1_xenos(block,8,4,4,pixels,16)==0&&pixels[0]==0xff00ff00u;
 memset(block+4,0xaa,4);passed+=h3_bc1_xenos(block,8,4,4,pixels,16)==0&&pixels[0]==0xffaa5500u;
 memset(block,0,4);memset(block+4,0xff,4);passed+=h3_bc1_xenos(block,8,4,4,pixels,16)==0&&pixels[0]==0;
 passed+=h3_bc1_xenos(block,7,4,4,pixels,16)!=0;
 passed+=h3_bc1_xenos(block,8,4,4,pixels,15)!=0;
 passed+=h3_bc1_xenos(block,8,0,4,pixels,16)!=0;
 passed+=h3_bc1_xenos(red_green,8,1,1,pixels,1)==0&&pixels[0]==0xffff0000u;
 return passed;
}
