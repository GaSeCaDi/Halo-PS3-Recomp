#include "../cache/cache_index.h"
#include "texture.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <zlib.h>
#include <sys/process.h>
#include <cell/cell_fs.h>
#include <cell/sysutil.h>
#include <cell/msg_dialog.h>
SYS_PROCESS_PARAM(1001,0x100000);
static uint8_t header[H3_MAP_HEADER_SIZE];


struct input {int fd;uint64_t fault_at;int fault;};
struct writer {int fd,error;size_t used;char buf[8192];};
static int failure(int);
static int read_at(void *arg,uint64_t offset,void *buffer,size_t size){
 struct input *in=arg;uint64_t pos,n;size_t total=0;
 if(in->fault==1)return failure(__LINE__);
 if(cellFsLseek(in->fd,(int64_t)offset,CELL_FS_SEEK_SET,&pos)||pos!=offset)return failure(__LINE__);
 while(total<size){n=0;if(cellFsRead(in->fd,(uint8_t *)buffer+total,size-total,&n)||!n||n>size-total)return failure(__LINE__);total+=(size_t)n;}
 if(offset==in->fault_at){
  if(in->fault==2&&size>=40){uint8_t *p=buffer;p[0]=0x7f;p[1]=p[2]=p[3]=0xff;}
  if(in->fault==3&&size>=40)((uint8_t *)buffer)[36]^=1;
  if(in->fault==4&&size>=2){((uint8_t *)buffer)[0]=0x7f;((uint8_t *)buffer)[1]=0xff;}
  if(in->fault==5)memset(buffer,'A',size);
 }
 return 0;
}
static int flush(struct writer *w){
 size_t total=0;
 while(total<w->used){uint64_t n=0;if(cellFsWrite(w->fd,w->buf+total,w->used-total,&n)||!n||n>w->used-total){w->error=1;return failure(__LINE__);}total+=(size_t)n;}
 w->used=0;return 0;
}
static int emit(struct writer *w,const char *s){
 size_t n=strlen(s);
 while(n){size_t room=sizeof(w->buf)-w->used,take=n<room?n:room;memcpy(w->buf+w->used,s,take);w->used+=take;s+=take;n-=take;if(w->used==sizeof(w->buf)&&flush(w))return failure(__LINE__);}
 return 0;
}
static int create_writer(struct writer *w,const char *path){memset(w,0,sizeof(*w));w->fd=-1;return cellFsOpen(path,CELL_FS_O_CREAT|CELL_FS_O_TRUNC|CELL_FS_O_WRONLY,&w->fd,NULL,0);}
static int finish_writer(struct writer *w){int rc=flush(w);if(cellFsClose(w->fd))rc=-1;return rc||w->error;}

static int tag_read(struct h3_cache *c,uint32_t va,uint32_t n,void *out){
 uint32_t off;return h3_tag_offset(c,va,n,&off)||c->io.read_at(c->io.context,off,out,n);
}
static unsigned be16(const uint8_t *p){return ((unsigned)p[0]<<8)|p[1];}
static int failure(int line){printf("H3TEXTURE_ERROR line=%d\n",line);fflush(stdout);return -1;}
static int resource_probe(struct h3_cache *c){
 struct h3_tag zone={0},play={0},bitmap={0},t;uint8_t z[532],layout[60],b[164],handle[8],resource[64],section[16],page[88];
 uint32_t off,n,va;int result=-1;uint8_t *raw=NULL,*decoded=NULL;
 for(uint32_t i=0;i<c->tag_count;i++){
  if(h3_cache_tag(c,i,&t)||!t.valid)continue;
  if(t.group==0x7a6f6e65)zone=t;
  if(t.group==0x706c6179)play=t;
  if(t.group==0x6269746d&&!bitmap.valid&&t.name&&strstr(t.name,"locked_ui"))bitmap=t;
 }
 if(!zone.valid||!play.valid||!bitmap.valid)return failure(__LINE__);
 if(tag_read(c,zone.address,sizeof(z),z)||tag_read(c,bitmap.address,sizeof(b),b))return failure(__LINE__);
 if(!h3_be32(b+0x8c)||h3_be32(b+0x98)||tag_read(c,h3_be32(b+0x90),8,handle))return failure(__LINE__);
 uint32_t id=h3_be32(handle),ri=id&65535u;
 n=h3_be32(z+0x58);va=h3_be32(z+0x5c);
 if(n>65535||ri>=n||h3_tag_offset(c,va,n*64,&off)||tag_read(c,va+ri*64,64,resource))return failure(__LINE__);
 if(be16(resource+0x10)!=(id>>16)||resource[0x12]!=0)return failure(__LINE__);
 if(h3_be32(z+0x1c+0x30))memcpy(layout,z+0x1c,sizeof(layout));
 else if(tag_read(c,play.address,sizeof(layout),layout))return failure(__LINE__);
 uint32_t defsize=h3_be32(resource+0x18),defstart=h3_be32(resource+0x14),allsize=h3_be32(z+0x13c),defva=h3_be32(z+0x148);uint8_t def[64],image[48];
 if(defsize!=64||defstart>allsize||defsize>allsize-defstart||h3_tag_offset(c,defva,allsize,&off)||tag_read(c,defva+defstart,64,def))return failure(__LINE__);
 if(tag_read(c,h3_be32(b+0x64),48,image))return failure(__LINE__);
 unsigned width=be16(def+40),height=be16(def+42),highres=def[47];
 if(!width||!height||width>4096||height>4096||def[46]!=0||def[44]!=1||h3_be32(def+48)!=0x1a207f73||be16(image+12)!=14||width!=be16(image+4)||height!=be16(image+6))return failure(__LINE__);
 /* Validate supported definition layout and resource fixups, including primary data offset. */
 uint8_t fix[128];uint32_t fixcount=h3_be32(resource+0x28),pointer=UINT32_MAX;int root_valid=0;
 if(!fixcount||fixcount>16||tag_read(c,h3_be32(resource+0x2c),fixcount*8,fix))return failure(__LINE__);
 for(uint32_t fi=0;fi<fixcount;fi++){
  uint32_t field=h3_be32(fix+fi*8),address=h3_be32(fix+fi*8+4);
  if(field==(highres?32u:12u))pointer=address;
  if(field==52&&address==0x20000000)root_valid=1;
 }
 if(!root_valid||pointer==UINT32_MAX)return failure(__LINE__);
 uint32_t datasize=h3_be32(def+(highres?20:0));
 if((pointer>>29)!=(highres?3u:2u))return failure(__LINE__);
 uint32_t dataoffset=pointer&0x1fffffffu;
 unsigned segment=be16(resource+0x22);n=h3_be32(layout+0x30);va=h3_be32(layout+0x34);
 if(n>65535||segment>=n||h3_tag_offset(c,va,n*16,&off)||tag_read(c,va+segment*16,16,section))return failure(__LINE__);
 unsigned pi=be16(section+(highres?2:0));uint32_t segmentoff=h3_be32(section+(highres?8:4));
 n=h3_be32(layout+0x18);va=h3_be32(layout+0x1c);
 if(n>65535||pi>=n||h3_tag_offset(c,va,n*88,&off)||tag_read(c,va+pi*88,88,page))return failure(__LINE__);
 int shared=(int16_t)be16(page+4);
 if(shared>=0){
  uint8_t sharedfile[264];n=h3_be32(layout+12);va=h3_be32(layout+16);
  if(n>4096||(unsigned)shared>=n||h3_tag_offset(c,va,n*264,&off)||tag_read(c,va+(unsigned)shared*264,264,sharedfile))return failure(__LINE__);
  if(memcmp(sharedfile,"maps\\mainmenu.map",18)||!(be16(sharedfile+258)&1))return failure(__LINE__);
 }
 uint32_t compressed=h3_be32(page+12),uncompressed=h3_be32(page+16);
 if(page[3]!=0||!compressed||!uncompressed||compressed>32*1024*1024u||uncompressed>32*1024*1024u||segmentoff>uncompressed||uncompressed-segmentoff<64)return failure(__LINE__);
 uint32_t physical=h3_be32(page+8)+c->sections[1].delta;
 if(physical<c->sections[1].start||(uint64_t)physical+compressed>(uint64_t)c->sections[1].start+c->sections[1].size)return failure(__LINE__);
 raw=malloc(compressed);decoded=malloc(uncompressed);if(!raw||!decoded)goto finish;
 if(c->io.read_at(c->io.context,physical,raw,compressed))goto finish;
 z_stream stream;memset(&stream,0,sizeof(stream));stream.next_in=raw;stream.avail_in=compressed;stream.next_out=decoded;stream.avail_out=uncompressed;
 if(inflateInit2(&stream,-15)!=Z_OK)goto finish;
 int rc=inflate(&stream,Z_FINISH);int valid=rc==Z_STREAM_END&&stream.total_in==compressed&&stream.total_out==uncompressed;inflateEnd(&stream);
 if(!valid)goto finish;
 char text[512];snprintf(text,sizeof(text),"resource_id\tpage_index\tcompressed_bytes\tuncompressed_bytes\tsegment_offset\tcompressed_crc32\tpage_crc32\tsegment_prefix_crc32\n%08x\t%u\t%u\t%u\t%u\t%08x\t%08x\t%08x\n",id,pi,compressed,uncompressed,segmentoff,h3_crc32_update(0,raw,compressed),h3_crc32_update(0,decoded,uncompressed),h3_crc32_update(0,decoded+segmentoff,64));
 struct writer report;if(create_writer(&report,"/app_home/resource-page.tsv"))goto finish;
 emit(&report,text);if(finish_writer(&report))goto finish;
 printf("H3TEXTURE_PAGE %s",text);
 if(segmentoff>uncompressed||dataoffset>uncompressed-segmentoff||datasize>uncompressed-segmentoff-dataoffset)goto finish;
 uint32_t *pixels=malloc((size_t)width*height*4);if(!pixels)goto finish;
 if(h3_bc1_xenos(decoded+segmentoff+dataoffset,datasize,width,height,pixels,(size_t)width*height)){free(pixels);goto finish;}
 printf("H3TEXTURE_DECODE width=%u height=%u pixels_crc32=%08x\n",width,height,h3_crc32_update(0,pixels,(size_t)width*height*4));fflush(stdout);
 struct writer imageout;
 if(create_writer(&imageout,"/app_home/texture-argb.bin")){free(pixels);goto finish;}
 size_t bytes=(size_t)width*height*4;const uint8_t *src=(const uint8_t *)pixels;
 while(bytes){size_t take=bytes<sizeof(imageout.buf)?bytes:sizeof(imageout.buf);memcpy(imageout.buf,src,take);imageout.used=take;if(flush(&imageout))break;src+=take;bytes-=take;}
 int write_error=finish_writer(&imageout);if(bytes||write_error){free(pixels);goto finish;}
 result=h3_preview(pixels,width,height);free(pixels);
 if(!result)puts("H3TEXTURE_PASS");
finish:free(raw);free(decoded);return result;
}

int main(void){
 unsigned tests=h3_texture_tests();printf("H3TEXTURE_TESTS %u/8\n",tests);fflush(stdout);if(tests!=8)return 20;
 struct input input={.fd=-1};CellFsStat st;struct h3_cache cache;memset(&cache,0,sizeof(cache));
 if(cellFsOpen("/dev_usb000/mainmenu.map",CELL_FS_O_RDONLY,&input.fd,NULL,0))return 10;
 int rc=1;
 if(!cellFsFstat(input.fd,&st)&&!read_at(&input,0,header,sizeof(header))){
  struct h3_io io={read_at,&input,st.st_size};
  if(h3_cache_open(&cache,header,sizeof(header),io)==H3_INDEX_OK)rc=resource_probe(&cache)?1:0;
 }
 h3_cache_close(&cache);cellFsClose(input.fd);printf("H3TEXTURE_EXIT %d\n",rc);fflush(stdout);return rc;
}
