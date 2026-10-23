/* Original read-only Halo 3 retail index reader. All offsets checked before I/O. */
#include "cache_index.h"
#include <stdlib.h>
#include <string.h>
uint32_t h3_be32(const uint8_t *p){return ((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|((uint32_t)p[2]<<8)|p[3];}
static uint16_t be16(const uint8_t *p){return (uint16_t)((uint16_t)p[0]<<8)|p[1];}
uint32_t h3_crc32_update(uint32_t previous,const void *data,size_t n){
 const uint8_t *p=data;uint32_t c=~previous;
 for(size_t i=0;i<n;i++){c^=p[i];for(unsigned j=0;j<8;j++)c=(c>>1)^(0xedb88320u&(0u-(c&1u)));}
 return ~c;
}
static int range(uint64_t start,uint64_t length,uint64_t total){return start<=total&&length<=total-start;}
static int section_range(const struct h3_section *s,uint32_t off,uint32_t n){
 return off>=s->start&&range((uint64_t)off-s->start,n,s->size);
}
enum h3_index_status h3_tag_offset(const struct h3_cache *c,uint32_t va,uint32_t n,uint32_t *out){
 if(!c||!out||va<c->virtual_base)return H3_INDEX_BOUNDS;
 uint64_t relative=(uint64_t)va-c->virtual_base;
 if(!range(relative,n,c->sections[2].size))return H3_INDEX_BOUNDS;
 uint64_t offset=(uint64_t)c->sections[2].start+relative;
 if(offset>UINT32_MAX||!range(offset,n,c->io.size))return H3_INDEX_BOUNDS;
 *out=(uint32_t)offset;return H3_INDEX_OK;
}
static enum h3_index_status string_offset(const struct h3_cache *c,uint32_t va,uint32_t n,uint32_t *out){
 uint32_t offset=va+c->sections[0].delta; /* format-defined modulo-2^32 section relocation */
 if(!section_range(&c->sections[0],offset,n)||!range(offset,n,c->io.size))return H3_INDEX_BOUNDS;
 *out=offset;return H3_INDEX_OK;
}
static enum h3_index_status load(struct h3_cache *c,uint8_t **dest,uint32_t off,uint32_t n){
 if(!range(off,n,c->io.size))return H3_INDEX_BOUNDS;
 if(!n){*dest=NULL;return H3_INDEX_OK;}
 *dest=malloc(n);if(!*dest)return H3_INDEX_MEMORY;
 if(c->io.read_at(c->io.context,off,*dest,n))return H3_INDEX_IO;
 return H3_INDEX_OK;
}
void h3_cache_close(struct h3_cache *c){
 if(!c)return;
 free(c->groups);free(c->instances);free(c->indices);free(c->names);free(c->globals);
 c->groups=c->instances=c->indices=c->names=c->globals=NULL;
}
enum h3_index_status h3_cache_tag(const struct h3_cache *c,uint32_t index,struct h3_tag *t){
 if(!c||!t||index>=c->tag_count||!c->instances||!c->indices)return H3_INDEX_NOT_FOUND;
 memset(t,0,sizeof(*t));t->index=index;
 const uint8_t *p=c->instances+index*8;uint32_t group=be16(p),salt=be16(p+2),ni=h3_be32(c->indices+index*4);
 t->id=(salt<<16)|index;t->address=h3_be32(p+4);
 if(ni!=UINT32_MAX){
  if(ni>=c->name_bytes||!c->names)return H3_INDEX_FORMAT;
  const uint8_t *end=memchr(c->names+ni,0,(c->name_bytes-ni)<512?(c->name_bytes-ni):512);
  if(!end)return H3_INDEX_FORMAT;
  for(const uint8_t *ch=c->names+ni;ch<end;ch++)if(*ch<32||*ch>126)return H3_INDEX_FORMAT;
  t->name=(const char *)c->names+ni;
 }
 if(group==0xffff)return H3_INDEX_OK;
 if(group>=c->group_count||!c->groups)return H3_INDEX_FORMAT;
 t->group=h3_be32(c->groups+group*16);
 enum h3_index_status rc=h3_tag_offset(c,t->address,1,&t->offset);
 if(rc!=H3_INDEX_OK)return rc;
 t->valid=1;return H3_INDEX_OK;
}
enum h3_index_status h3_cache_lookup(const struct h3_cache *c,uint32_t id,struct h3_tag *t){
 enum h3_index_status rc=h3_cache_tag(c,id&0xffffu,t);
 if(rc!=H3_INDEX_OK)return rc;
 return t->valid&&t->id==id?H3_INDEX_OK:H3_INDEX_NOT_FOUND;
}
enum h3_index_status h3_cache_open(struct h3_cache *c,const uint8_t *h,size_t len,struct h3_io io){
 if(!c)return H3_INDEX_HEADER;
 memset(c,0,sizeof(*c));c->io=io;
 if(!io.read_at||h3_map_parse(h,len,io.size,&c->map)!=H3_MAP_OK)return H3_INDEX_HEADER;
 enum h3_index_status rc=H3_INDEX_OK;uint32_t off;
 for(unsigned i=0;i<4;i++){
  c->sections[i].delta=h3_be32(h+0x46c+i*4);
  c->sections[i].start=h3_be32(h+0x47c+i*8)+c->sections[i].delta;
  c->sections[i].size=h3_be32(h+0x480+i*8);
  if(c->sections[i].size&&(!range(c->sections[i].start,c->sections[i].size,io.size)||c->sections[i].start<H3_MAP_HEADER_SIZE))return H3_INDEX_BOUNDS;
 }
 for(unsigned i=0;i<4;i++)for(unsigned j=i+1;j<4;j++){
  const struct h3_section *a=&c->sections[i],*b=&c->sections[j];
  if(a->size&&b->size&&(uint64_t)a->start<(uint64_t)b->start+b->size&&(uint64_t)b->start<(uint64_t)a->start+a->size)return H3_INDEX_BOUNDS;
 }
 c->virtual_base=h3_be32(h+0x2e8);
 if(!c->sections[2].size)return H3_INDEX_EMPTY;
 uint8_t th[40];
 rc=h3_tag_offset(c,h3_be32(h+0x10),sizeof(th),&c->header_offset);if(rc)return rc;
 if(io.read_at(io.context,c->header_offset,th,sizeof(th)))return H3_INDEX_IO;
 if(memcmp(th+36,"tags",4))return H3_INDEX_FORMAT;
 c->group_count=h3_be32(th);c->tag_count=h3_be32(th+8);c->global_count=h3_be32(th+16);c->name_bytes=h3_be32(h+0x2bc);
 if(c->group_count>4096||c->tag_count>65535||c->global_count>4096||c->name_bytes>8u*1024u*1024u)return H3_INDEX_LIMIT;
 if(!c->group_count||!c->tag_count||c->tag_count!=h3_be32(h+0x2b4))return H3_INDEX_FORMAT;
#define LOAD_TAG(member,address,bytes) do{rc=h3_tag_offset(c,address,bytes,&off);if(rc)goto fail;rc=load(c,&c->member,off,bytes);if(rc)goto fail;}while(0)
 LOAD_TAG(groups,h3_be32(th+4),c->group_count*16);
 LOAD_TAG(instances,h3_be32(th+12),c->tag_count*8);
 LOAD_TAG(globals,h3_be32(th+20),c->global_count*8);
#undef LOAD_TAG
 rc=string_offset(c,h3_be32(h+0x2c0),c->tag_count*4,&off);if(rc)goto fail;
 rc=load(c,&c->indices,off,c->tag_count*4);if(rc)goto fail;
 rc=string_offset(c,h3_be32(h+0x2b8),c->name_bytes,&off);if(rc)goto fail;
 rc=load(c,&c->names,off,c->name_bytes);if(rc)goto fail;
 for(uint32_t i=0;i<c->tag_count;i++){struct h3_tag t;rc=h3_cache_tag(c,i,&t);if(rc)goto fail;if(!t.valid)c->null_count++;}
 for(uint32_t i=0;i<c->global_count;i++){
  struct h3_tag t;rc=h3_cache_lookup(c,h3_be32(c->globals+i*8+4),&t);if(rc)goto fail;
  if(t.group!=h3_be32(c->globals+i*8)){rc=H3_INDEX_FORMAT;goto fail;}
 }
 c->index_crc32=h3_crc32_update(0,c->groups,c->group_count*16);
 c->index_crc32=h3_crc32_update(c->index_crc32,c->instances,c->tag_count*8);
 c->index_crc32=h3_crc32_update(c->index_crc32,c->indices,c->tag_count*4);
 c->index_crc32=h3_crc32_update(c->index_crc32,c->names,c->name_bytes);
 c->index_crc32=h3_crc32_update(c->index_crc32,c->globals,c->global_count*8);
 return H3_INDEX_OK;
fail:h3_cache_close(c);return rc;
}
const char *h3_index_status_text(enum h3_index_status s){
 static const char *text[]={"OK","EMPTY","header","bounds","limit","io","memory","format","not_found"};
 return (unsigned)s<sizeof(text)/sizeof(text[0])?text[s]:"unknown";
}

