#ifndef H3_TEXTURE_H
#define H3_TEXTURE_H
#include <stddef.h>
#include <stdint.h>
int h3_bc1_xenos(const uint8_t *,size_t,unsigned,unsigned,uint32_t *,size_t);
unsigned h3_texture_tests(void);
int h3_preview(const uint32_t *,unsigned,unsigned);
#endif
