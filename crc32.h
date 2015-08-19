#ifndef CRC32_H
#define CRC32_H

#include <stddef.h>

uint32_t crc32(uint32_t crc, const void *buf, size_t size);

#endif
