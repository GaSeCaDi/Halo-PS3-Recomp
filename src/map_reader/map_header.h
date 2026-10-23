#ifndef H3_MAP_HEADER_H
#define H3_MAP_HEADER_H
#include <stddef.h>
#include <stdint.h>
#define H3_MAP_HEADER_SIZE 0x3000u
struct h3_map_header {
    uint32_t version;
    uint32_t declared_size;
    uint32_t compressed_size;
    uint32_t header_crc32;
    char build[32];
    char name[32];
    char scenario[256];
};
enum h3_map_status {
    H3_MAP_OK=0, H3_MAP_SHORT_HEADER, H3_MAP_BAD_HEAD,
    H3_MAP_BAD_VERSION, H3_MAP_BAD_SIZE, H3_MAP_BAD_FOOT,
    H3_MAP_BAD_STRING, H3_MAP_UNSUPPORTED_BUILD, H3_MAP_COMPRESSED
};
enum h3_map_status h3_map_parse(const uint8_t *data, size_t length,
                               uint64_t file_size, struct h3_map_header *out);
const char *h3_map_status_text(enum h3_map_status status);
#endif
