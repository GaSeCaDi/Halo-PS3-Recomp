/* Original bounded reader for the observed Halo 3 retail cache header.
 * No packed casts: byte order is decoded explicitly on any host architecture.
 * Layout cross-checked against the local map and public TagTool Gen3 metadata.
 */
#include "map_header.h"
#include <string.h>
static uint32_t be32(const uint8_t *p) {
    return ((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|((uint32_t)p[2]<<8)|p[3];
}
static uint32_t crc32(const uint8_t *p,size_t n) {
    uint32_t crc=0xffffffffu;
    for(size_t i=0;i<n;i++) {
        crc^=p[i];
        for(unsigned j=0;j<8;j++)crc=(crc>>1)^(0xedb88320u & (0u-(crc&1u)));
    }
    return ~crc;
}
static int fixed_string(char *dst,const uint8_t *src,size_t cap) {
    size_t i;
    for(i=0;i<cap;i++) {
        if(src[i]==0) { if(i==0)return 0; memcpy(dst,src,i);dst[i]=0;return 1; }
        if(src[i]<32 || src[i]>126)return 0;
    }
    return 0;
}
enum h3_map_status h3_map_parse(const uint8_t *data,size_t length,
                               uint64_t file_size,struct h3_map_header *out) {
    if(!data||!out||length<H3_MAP_HEADER_SIZE)return H3_MAP_SHORT_HEADER;
    memset(out,0,sizeof(*out));
    if(memcmp(data,"head",4)!=0)return H3_MAP_BAD_HEAD;
    out->version=be32(data+4);
    if(out->version!=11)return H3_MAP_BAD_VERSION;
    out->declared_size=be32(data+8);out->compressed_size=be32(data+12);
    if(out->declared_size<H3_MAP_HEADER_SIZE || file_size!=out->declared_size)return H3_MAP_BAD_SIZE;
    if(out->compressed_size!=0)return H3_MAP_COMPRESSED;
    if(memcmp(data+0x2ffc,"foot",4)!=0)return H3_MAP_BAD_FOOT;
    if(!fixed_string(out->build,data+0x11c,sizeof(out->build)) ||
       !fixed_string(out->name,data+0x18c,sizeof(out->name)) ||
       !fixed_string(out->scenario,data+0x1b0,sizeof(out->scenario)))return H3_MAP_BAD_STRING;
    if(!strstr(out->build,".halo3_ship"))return H3_MAP_UNSUPPORTED_BUILD;
    out->header_crc32=crc32(data,H3_MAP_HEADER_SIZE);
    return H3_MAP_OK;
}
const char *h3_map_status_text(enum h3_map_status s) {
    static const char *text[]={"OK","short header","bad head magic","unsupported version",
        "file size mismatch","bad foot magic","invalid or unterminated string",
        "unsupported build","compressed cache unsupported"};
    return (unsigned)s<sizeof(text)/sizeof(text[0])?text[s]:"unknown error";
}
