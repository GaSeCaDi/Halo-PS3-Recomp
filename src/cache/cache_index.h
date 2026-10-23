#ifndef H3_CACHE_INDEX_H
#define H3_CACHE_INDEX_H
#include <stddef.h>
#include <stdint.h>
#include "../map_reader/map_header.h"
typedef int (*h3_read_at_fn)(void *,uint64_t,void *,size_t);
struct h3_io {h3_read_at_fn read_at;void *context;uint64_t size;};
struct h3_section {uint32_t start,size,delta;};
struct h3_tag {uint32_t index,id,address,offset,group;const char *name;int valid;};
enum h3_index_status {H3_INDEX_OK=0,H3_INDEX_EMPTY,H3_INDEX_HEADER,H3_INDEX_BOUNDS,
 H3_INDEX_LIMIT,H3_INDEX_IO,H3_INDEX_MEMORY,H3_INDEX_FORMAT,H3_INDEX_NOT_FOUND};
struct h3_cache {
 struct h3_io io;struct h3_map_header map;struct h3_section sections[4];
 uint32_t virtual_base,header_offset,group_count,tag_count,global_count,name_bytes,null_count,index_crc32;
 uint8_t *groups,*instances,*indices,*names,*globals;
};
uint32_t h3_be32(const uint8_t *p);
uint32_t h3_crc32_update(uint32_t previous,const void *data,size_t size);
enum h3_index_status h3_cache_open(struct h3_cache *,const uint8_t *,size_t,struct h3_io);
void h3_cache_close(struct h3_cache *);
enum h3_index_status h3_tag_offset(const struct h3_cache *,uint32_t,uint32_t,uint32_t *);
enum h3_index_status h3_cache_tag(const struct h3_cache *,uint32_t,struct h3_tag *);
enum h3_index_status h3_cache_lookup(const struct h3_cache *,uint32_t,struct h3_tag *);
const char *h3_index_status_text(enum h3_index_status);
#endif
