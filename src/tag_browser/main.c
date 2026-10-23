#include "../cache/cache_index.h"
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
static uint8_t header[H3_MAP_HEADER_SIZE],bad_header[H3_MAP_HEADER_SIZE];
static char summary[8192],manifest[4096];
static volatile int done;
struct input {int fd;uint64_t fault_at;int fault;};
struct writer {int fd,error;size_t used;char buf[8192];};
static void button(int value,void *arg){(void)value;(void)arg;done=1;}
static void event(uint64_t status,uint64_t param,void *arg){(void)param;(void)arg;if(status==CELL_SYSUTIL_REQUEST_EXITGAME)done=1;}
static int read_at(void *arg,uint64_t offset,void *buffer,size_t size){
 struct input *in=arg;uint64_t pos,n;size_t total=0;
 if(in->fault==1)return -1;
 if(cellFsLseek(in->fd,(int64_t)offset,CELL_FS_SEEK_SET,&pos)||pos!=offset)return -1;
 while(total<size){n=0;if(cellFsRead(in->fd,(uint8_t *)buffer+total,size-total,&n)||!n||n>size-total)return -1;total+=(size_t)n;}
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
 while(total<w->used){uint64_t n=0;if(cellFsWrite(w->fd,w->buf+total,w->used-total,&n)||!n||n>w->used-total){w->error=1;return -1;}total+=(size_t)n;}
 w->used=0;return 0;
}
static int emit(struct writer *w,const char *s){
 size_t n=strlen(s);
 while(n){size_t room=sizeof(w->buf)-w->used,take=n<room?n:room;memcpy(w->buf+w->used,s,take);w->used+=take;s+=take;n-=take;if(w->used==sizeof(w->buf)&&flush(w))return -1;}
 return 0;
}
static int create_writer(struct writer *w,const char *path){memset(w,0,sizeof(*w));w->fd=-1;return cellFsOpen(path,CELL_FS_O_CREAT|CELL_FS_O_TRUNC|CELL_FS_O_WRONLY,&w->fd,NULL,0);}
static int finish_writer(struct writer *w){int rc=flush(w);if(cellFsClose(w->fd))rc=-1;return rc||w->error;}

static int tag_read(struct h3_cache *c,uint32_t va,uint32_t n,void *out){
 uint32_t off;return h3_tag_offset(c,va,n,&off)||c->io.read_at(c->io.context,off,out,n);
}
static unsigned be16(const uint8_t *p){return ((unsigned)p[0]<<8)|p[1];}
static int bitmap_export(struct h3_cache *c,const struct h3_tag *t,struct writer *w){
 uint8_t root[164],image[48],handle[8];char line[1024];
 if(tag_read(c,t->address,sizeof(root),root))return -1;
 uint32_t normal=h3_be32(root+0x8c),handles=h3_be32(root+0x90),inter=h3_be32(root+0x98),off;
 if(normal>4096||inter>4096)return -1;
 if(normal&&h3_tag_offset(c,handles,normal*8,&off))return -1;
 if(inter&&h3_tag_offset(c,h3_be32(root+0x9c),inter*8,&off))return -1;
 for(unsigned bank=0;bank<2;bank++){
  unsigned b=bank?0x80:0x60;uint32_t count=h3_be32(root+b),va=h3_be32(root+b+4);
  if(count>4096||(count&&h3_tag_offset(c,va,count*48,&off)))return -1;
  for(uint32_t i=0;i<count;i++){
   uint32_t id=UINT32_MAX;
   if(tag_read(c,va+i*48,sizeof(image),image)||h3_be32(image)!=0x6269746d||!be16(image+4)||!be16(image+6))return -1;
   if(i<normal){if(tag_read(c,handles+i*8,sizeof(handle),handle))return -1;id=h3_be32(handle);}
   int n=snprintf(line,sizeof(line),"%u\t%08x\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%08x\t%s\n",t->index,t->id,bank,i,be16(image+4),be16(image+6),image[8],image[9],be16(image+10),be16(image+12),be16(image+14),image[20],normal,inter,id,t->name?t->name:"");
   if(n<0||(size_t)n>=sizeof(line)||emit(w,line))return -1;
  }
 }
 return 0;
}

/* Read one required local bitmap page. Shared/optional and interleaved pages
   are explicitly outside this first decompressor's supported subset. */
static int resource_probe(struct h3_cache *c){
 struct h3_tag zone={0},play={0},bitmap={0},t;uint8_t z[100],layout[60],b[164],handle[8],resource[64],section[16],page[88];
 uint32_t off,n,va;int result=-1;uint8_t *raw=NULL,*decoded=NULL;
 for(uint32_t i=0;i<c->tag_count;i++){
  if(h3_cache_tag(c,i,&t)||!t.valid)continue;
  if(t.group==0x7a6f6e65)zone=t;
  if(t.group==0x706c6179)play=t;
  if(t.group==0x6269746d&&!bitmap.valid&&t.name&&strstr(t.name,"random4_warp"))bitmap=t;
 }
 if(!zone.valid||!play.valid||!bitmap.valid)return -1;
 if(tag_read(c,zone.address,sizeof(z),z)||tag_read(c,bitmap.address,sizeof(b),b))return -1;
 if(!h3_be32(b+0x8c)||h3_be32(b+0x98)||tag_read(c,h3_be32(b+0x90),8,handle))return -1;
 uint32_t id=h3_be32(handle),ri=id&65535u;
 n=h3_be32(z+0x58);va=h3_be32(z+0x5c);
 if(n>65535||ri>=n||h3_tag_offset(c,va,n*64,&off)||tag_read(c,va+ri*64,64,resource))return -1;
 if(be16(resource+0x10)!=(id>>16)||resource[0x12]!=0)return -1;
 if(h3_be32(z+0x1c+0x30))memcpy(layout,z+0x1c,sizeof(layout));
 else if(tag_read(c,play.address,sizeof(layout),layout))return -1;
 unsigned segment=be16(resource+0x22);n=h3_be32(layout+0x30);va=h3_be32(layout+0x34);
 if(n>65535||segment>=n||h3_tag_offset(c,va,n*16,&off)||tag_read(c,va+segment*16,16,section))return -1;
 unsigned pi=be16(section);uint32_t segmentoff=h3_be32(section+4);
 n=h3_be32(layout+0x18);va=h3_be32(layout+0x1c);
 if(n>65535||pi>=n||h3_tag_offset(c,va,n*88,&off)||tag_read(c,va+pi*88,88,page))return -1;
 int shared=(int16_t)be16(page+4);
 if(shared>=0){
  uint8_t sharedfile[264];n=h3_be32(layout+12);va=h3_be32(layout+16);
  if(n>4096||(unsigned)shared>=n||h3_tag_offset(c,va,n*264,&off)||tag_read(c,va+(unsigned)shared*264,264,sharedfile))return -1;
  if(memcmp(sharedfile,"maps\\mainmenu.map",18)||!(be16(sharedfile+258)&1))return -1;
 }
 uint32_t compressed=h3_be32(page+12),uncompressed=h3_be32(page+16);
 if(page[3]!=0||!compressed||!uncompressed||compressed>32*1024*1024u||uncompressed>32*1024*1024u||segmentoff>uncompressed||uncompressed-segmentoff<64)return -1;
 uint32_t physical=h3_be32(page+8)+c->sections[1].delta;
 if(physical<c->sections[1].start||(uint64_t)physical+compressed>(uint64_t)c->sections[1].start+c->sections[1].size)return -1;
 raw=malloc(compressed);decoded=malloc(uncompressed);if(!raw||!decoded)goto finish;
 if(c->io.read_at(c->io.context,physical,raw,compressed))goto finish;
 z_stream stream;memset(&stream,0,sizeof(stream));stream.next_in=raw;stream.avail_in=compressed;stream.next_out=decoded;stream.avail_out=uncompressed;
 if(inflateInit2(&stream,-15)!=Z_OK)goto finish;
 int rc=inflate(&stream,Z_FINISH);int valid=rc==Z_STREAM_END&&stream.total_in==compressed&&stream.total_out==uncompressed;inflateEnd(&stream);
 if(!valid)goto finish;
 char text[512];snprintf(text,sizeof(text),"resource_id\tpage_index\tcompressed_bytes\tuncompressed_bytes\tsegment_offset\tcompressed_crc32\tpage_crc32\tsegment_prefix_crc32\n%08x\t%u\t%u\t%u\t%u\t%08x\t%08x\t%08x\n",id,pi,compressed,uncompressed,segmentoff,h3_crc32_update(0,raw,compressed),h3_crc32_update(0,decoded,uncompressed),h3_crc32_update(0,decoded+segmentoff,64));
 struct writer report;if(create_writer(&report,"/app_home/resource-page.tsv"))goto finish;
 emit(&report,text);if(finish_writer(&report))goto finish;
 printf("H3RESOURCE_PASS %s",text);result=0;
finish:free(raw);free(decoded);return result;
}
static unsigned checks(struct h3_cache *c,const uint8_t *h,struct h3_io io){
 struct h3_tag tag,first;uint32_t off;unsigned passed=0;
 passed+=h3_cache_tag(c,0,&first)==H3_INDEX_OK&&first.valid;
 passed+=h3_cache_lookup(c,first.id,&tag)==H3_INDEX_OK;
 passed+=h3_cache_lookup(c,first.id^0x10000u,&tag)==H3_INDEX_NOT_FOUND;
 passed+=h3_cache_lookup(c,UINT32_MAX,&tag)==H3_INDEX_NOT_FOUND;
 passed+=h3_cache_tag(c,c->tag_count,&tag)==H3_INDEX_NOT_FOUND;
 passed+=h3_tag_offset(c,c->virtual_base-1,1,&off)==H3_INDEX_BOUNDS;
 passed+=h3_tag_offset(c,c->virtual_base,1,&off)==H3_INDEX_OK&&off==c->sections[2].start;
 passed+=h3_tag_offset(c,c->virtual_base+c->sections[2].size,1,&off)==H3_INDEX_BOUNDS;
 passed+=h3_tag_offset(c,c->virtual_base,UINT32_MAX,&off)==H3_INDEX_BOUNDS;
 struct input fault=*(struct input *)io.context;struct h3_io fake=io;fake.context=&fault;struct h3_cache test;
 fault.fault=1;
 passed+=h3_cache_open(&test,h,H3_MAP_HEADER_SIZE,fake)==H3_INDEX_IO;h3_cache_close(&test);
 fault.fault=2;fault.fault_at=c->header_offset;
 passed+=h3_cache_open(&test,h,H3_MAP_HEADER_SIZE,fake)==H3_INDEX_LIMIT;h3_cache_close(&test);
 fault.fault=3;
 passed+=h3_cache_open(&test,h,H3_MAP_HEADER_SIZE,fake)==H3_INDEX_FORMAT;h3_cache_close(&test);
 fault.fault=4;
 uint8_t th[40];struct input *original=io.context;
 if(read_at(original,c->header_offset,th,sizeof(th)))return passed;
 h3_tag_offset(c,h3_be32(th+12),c->tag_count*8,&off);fault.fault_at=off;
 passed+=h3_cache_open(&test,h,H3_MAP_HEADER_SIZE,fake)==H3_INDEX_FORMAT;h3_cache_close(&test);
 fault.fault=5;fault.fault_at=h3_be32(h+0x2b8)+c->sections[0].delta;
 passed+=h3_cache_open(&test,h,H3_MAP_HEADER_SIZE,fake)==H3_INDEX_FORMAT;h3_cache_close(&test);
 memcpy(bad_header,h,H3_MAP_HEADER_SIZE);memset(bad_header+0x490,0xff,4);
 passed+=h3_cache_open(&test,bad_header,sizeof(bad_header),io)==H3_INDEX_BOUNDS;h3_cache_close(&test);
 return passed;
}
static int safe_name(const char *s){
 size_t n=strlen(s);if(n<5||n>100||strcmp(s+n-4,".map"))return 0;
 for(size_t i=0;i<n-4;i++)if(!((s[i]>='a'&&s[i]<='z')||(s[i]>='0'&&s[i]<='9')||s[i]=='_'))return 0;
 return 1;
}
static int inspect(const char *file,struct writer *results,uint32_t *total_tags,unsigned *check_result,char *mainmenu_info,size_t info_size){
 char path[192],line[1024],outpath[192];struct input input={.fd=-1};CellFsStat st;
 snprintf(path,sizeof(path),"/dev_usb000/%s",file);
 int rc=cellFsOpen(path,CELL_FS_O_RDONLY,&input.fd,NULL,0);
 if(rc){snprintf(line,sizeof(line),"%s\tFAIL_OPEN\t%08x\n",file,(unsigned)rc);emit(results,line);return -1;}
 int result=-1;struct h3_cache cache;memset(&cache,0,sizeof(cache));
 if(cellFsFstat(input.fd,&st))goto finish;
 if(read_at(&input,0,header,sizeof(header)))goto finish;
 struct h3_io io={read_at,&input,st.st_size};
 enum h3_index_status status=h3_cache_open(&cache,header,sizeof(header),io);
 if(status==H3_INDEX_EMPTY){snprintf(line,sizeof(line),"%s\tEMPTY\t0\t0\t0\t0\t00000000\tffffffff\t00000000\n",file);emit(results,line);result=0;goto finish;}
 if(status!=H3_INDEX_OK){snprintf(line,sizeof(line),"%s\tFAIL_%s\n",file,h3_index_status_text(status));emit(results,line);goto finish;}
 if(!safe_name(file))goto finish;
 /* output filename comes from the validated manifest, never from file contents */
 snprintf(outpath,sizeof(outpath),"/app_home/%s-tags.tsv",file);
 struct writer tags,images;
 if(create_writer(&tags,outpath))goto finish;
 snprintf(outpath,sizeof(outpath),"/app_home/%s-bitmaps.tsv",file);
 if(create_writer(&images,outpath)){finish_writer(&tags);goto finish;}
 emit(&images,"tag_index\ttag_id\timage_set\timage_index\twidth\theight\tdepth\txbox_flags\ttype\tformat\tflags\tmips\tresources\tinterleaved\tresource_id\tname\n");
 emit(&tags,"index\tid\tgroup_hex\tfile_offset\tvalid\tname\n");
 uint32_t scenario_index=UINT32_MAX,scenario_crc=0,bitmaps=0,models=0,sounds=0,shaders=0;
 for(uint32_t i=0;i<cache.tag_count;i++){
  struct h3_tag t;if(h3_cache_tag(&cache,i,&t)!=H3_INDEX_OK){tags.error=1;break;}
  if(t.valid){
   if(t.group==0x6269746du&&bitmap_export(&cache,&t,&images)){tags.error=1;break;}
   bitmaps+=t.group==0x6269746du;models+=t.group==0x6d6f6465u;sounds+=t.group==0x736e6421u;
   shaders+=t.group==0x7069786cu||t.group==0x76747368u;
   if(t.group==0x73636e72u){uint8_t prefix[64];uint32_t off;if(h3_tag_offset(&cache,t.address,sizeof(prefix),&off)||read_at(&input,off,prefix,sizeof(prefix))){tags.error=1;break;}scenario_index=i;scenario_crc=h3_crc32_update(0,prefix,sizeof(prefix));}
  }
  int n=snprintf(line,sizeof(line),"%u\t%08x\t%08x\t%08x\t%d\t%s\n",i,(unsigned)t.id,(unsigned)t.group,(unsigned)t.offset,t.valid,t.name?t.name:"");
  if(n<0||(size_t)n>=sizeof(line)||emit(&tags,line)){tags.error=1;break;}
 }
 int image_error=finish_writer(&images);
 if(finish_writer(&tags)||image_error)goto finish;
 if(strcmp(file,"mainmenu.map")==0){
  if(resource_probe(&cache))goto finish;
  *check_result=checks(&cache,header,io);
  snprintf(mainmenu_info,info_size,"mainmenu: %u tags / %u groups\nbitm: %u | mode: %u | snd!: %u | shaders: %u\nscnr: index %u | CRC32 %08x\nIndex CRC32: %08x | checks: %u/15",cache.tag_count,cache.group_count,bitmaps,models,sounds,shaders,scenario_index,scenario_crc,cache.index_crc32,*check_result);
  if(*check_result!=15)goto finish;
 }
 snprintf(line,sizeof(line),"%s\tPASS\t%u\t%u\t%u\t%u\t%08x\t%08x\t%08x\n",file,cache.tag_count,cache.group_count,cache.null_count,cache.global_count,cache.index_crc32,scenario_index,scenario_crc);
 emit(results,line);*total_tags+=cache.tag_count;result=0;
finish:
 h3_cache_close(&cache);if(cellFsClose(input.fd))result=-1;
 if(result)printf("H3INDEX_FILE_FAILED %s\n",file);
 return result;
}
int main(void){
 struct input m={.fd=-1};CellFsStat st;unsigned passed=0,failed=0,test_count=0;uint32_t total_tags=0;char details[512]="mainmenu not processed";
 if(cellFsOpen("/app_home/maps.txt",CELL_FS_O_RDONLY,&m.fd,NULL,0)||cellFsFstat(m.fd,&st))return 10;
 if(st.st_size>=sizeof(manifest)||read_at(&m,0,manifest,(size_t)st.st_size)){cellFsClose(m.fd);return 11;}
 manifest[st.st_size]=0;cellFsClose(m.fd);
 struct writer results;if(create_writer(&results,"/app_home/tag-index-results.tsv"))return 12;
 emit(&results,"map\tstatus\ttags\tgroups\tnulls\tglobals\tindex_crc32\tscenario_index\tscenario_crc32\n");
 for(char *file=strtok(manifest,"\r\n");file;file=strtok(NULL,"\r\n")){
  if(!safe_name(file)){failed++;continue;}
  if(inspect(file,&results,&total_tags,&test_count,details,sizeof(details)))failed++;else passed++;
  printf("H3INDEX_PROGRESS %s passed=%u failed=%u tags=%u\n",file,passed,failed,total_tags);fflush(stdout);
 }
 if(finish_writer(&results))failed++;
 int ok=passed>0&&failed==0&&test_count==15;
 snprintf(summary,sizeof(summary),"H3INDEX_%s\nMaps: %u passed / %u failed\nTotal tag entries: %u\n%s\nIndice y datos del tag scnr leidos. Cierre automatico en 30 s.",ok?"PASS":"FAIL",passed,failed,total_tags,details);
 puts(summary);fflush(stdout);
 struct writer text;if(!create_writer(&text,"/app_home/tag-browser-result.txt")){emit(&text,summary);finish_writer(&text);}
 if(!cellSysutilRegisterCallback(0,event,NULL)){
  unsigned type=CELL_MSGDIALOG_TYPE_SE_TYPE_NORMAL|CELL_MSGDIALOG_TYPE_BG_VISIBLE|CELL_MSGDIALOG_TYPE_BUTTON_TYPE_YESNO|CELL_MSGDIALOG_TYPE_DEFAULT_CURSOR_YES;
  if(!cellMsgDialogOpen2(type,summary,button,NULL,NULL)){
   for(unsigned i=0;i<300&&!done;i++){cellSysutilCheckCallback();usleep(100000);}if(!done)cellMsgDialogAbort();
  }
  cellSysutilUnregisterCallback(0);
 }
 printf("H3INDEX_EXIT %d\n",ok?0:1);fflush(stdout);return ok?0:1;
}
