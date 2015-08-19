#ifndef LIBMINIZIP_H
#define LIBMINIZIP_H

#include <stdint.h>

    #define MZIP_LOCAL_FILE_HEADER_SIGNATURE 0x04034b50
    #define MZIP_VERSION                     0x0a
    #define MZIP_COMPRESSION_STORE           0x00
    #define MZIP_DATA_DESCRIPTOR_SIGNATURE   0x08074b50
    #define MZIP_CDF_SIGNATURE               0x02014b50
    #define MZIP_EOCD_SIGNATURE              0x06054b50
    #define MZIP_COMMENT "created by libminizip"

    typedef struct  {
        uint32_t offset;

        uint16_t version;
        uint16_t version_min;
        uint16_t gpbit_flag;
        uint16_t compression_method;
        uint16_t mtime;
        uint16_t mdate;
        uint32_t crc;
        uint32_t size_compressed;
        uint32_t size_uncompressed;
        uint16_t len_filename;
        uint16_t len_extra_field;
        uint8_t *filename;
        uint8_t *extra_field;
    } mzip_file_t;

    typedef struct {
        FILE *stream;
        uint32_t offset;
        mzip_file_t **files;
        uint16_t number_files;
        uint32_t cdfs_offset;
    } mzip_t;

    int mzip_create(mzip_t *zip, const char *filename);
    int mzip_file_add(mzip_t *zip, const char *filename);
    int mzip_close(mzip_t *zip);

    void zip_free(mzip_t *zip);

#endif
