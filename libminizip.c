#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#include "crc32.h"
#include "libminizip.h"

/* see http://proger.i-forge.net/MS-DOS_date_and_time_format/OFz */
#define MSDOS_DATE(tm) (((tm)->tm_year-80) << 9 | ((tm)->tm_mon+1) << 5 | (tm)->tm_mday)
#define MSDOS_TIME(tm) ((tm)->tm_hour << 11 | (tm)->tm_min << 5 | ((tm)->tm_sec/2))

static size_t zip_fwrite(mzip_t *zip, const void *ptr, size_t size)
{
    return fwrite(ptr, size, 1, zip->stream);
}

int mzip_file_add(mzip_t *zip, const char *filename)
{
    void *ptr;
    mzip_file_t *file;
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    uint32_t signature = MZIP_LOCAL_FILE_HEADER_SIGNATURE;

    if((ptr = realloc(zip->files, (zip->number_files+1)*sizeof(mzip_file_t *))) == NULL)
        return -1;

    zip->files = ptr;
    zip->number_files++;

    if((zip->files[zip->number_files-1] = malloc(sizeof(mzip_file_t))) == NULL)
        return -2;

    file = zip->files[zip->number_files-1];

    file->offset             = ftell(zip->stream);
    file->version            = MZIP_VERSION;
    file->version_min        = MZIP_VERSION;
    file->gpbit_flag         = 0x08;
    file->compression_method = MZIP_COMPRESSION_STORE;
    file->mtime              = MSDOS_TIME(tm);
    file->mdate              = MSDOS_DATE(tm);
    file->crc                = 0;
    file->size_compressed    = 0;
    file->size_uncompressed  = 0;
    file->len_filename       = strlen(filename);
    file->len_extra_field    = 0;
    file->filename           = malloc(file->len_filename*sizeof(uint8_t));
    memcpy(file->filename, filename, file->len_filename);
    file->extra_field        = NULL;

    zip_fwrite(zip, &signature,                4);
    zip_fwrite(zip, &file->version_min,        2);
    zip_fwrite(zip, &file->gpbit_flag,         2);
    zip_fwrite(zip, &file->compression_method, 2);
    zip_fwrite(zip, &file->mtime,              2);
    zip_fwrite(zip, &file->mdate,              2);
    zip_fwrite(zip, &file->crc,                4);
    zip_fwrite(zip, &file->size_compressed,    4);
    zip_fwrite(zip, &file->size_uncompressed,  4);
    zip_fwrite(zip, &file->len_filename,       2);
    zip_fwrite(zip, &file->len_extra_field,    2);
    zip_fwrite(zip, file->filename, file->len_filename);
    if(file->extra_field != NULL)
        zip_fwrite(zip, &file->extra_field, file->len_extra_field);

    return 0;
}

void mzip_file_write(mzip_t *zip, void *buf, size_t size)
{
    mzip_file_t *file = zip->files[zip->number_files-1];

    file->crc = crc32(file->crc, buf, size);
    file->size_compressed   += size;
    file->size_uncompressed += size;

    zip_fwrite(zip, buf, size);
}

void mzip_file_close(mzip_t *zip)
{
    mzip_file_t *file = zip->files[zip->number_files-1];
    uint32_t signature = MZIP_DATA_DESCRIPTOR_SIGNATURE;

    zip_fwrite(zip, &signature,               4);
    zip_fwrite(zip, &file->crc,               4);
    zip_fwrite(zip, &file->size_compressed,   4);
    zip_fwrite(zip, &file->size_uncompressed, 4);
}


int mzip_create(mzip_t *zip, const char *filename)
{
    zip->files = NULL;
    zip->number_files = 0;
    zip->stream = fopen(filename, "w");

    if(zip->stream == NULL)
        return -1;
    return 0;
}

void mzip_cdfs(mzip_t *zip)
{
    int i;
    uint32_t signature = MZIP_CDF_SIGNATURE;
    uint16_t len_file_comment = 0;
    uint16_t number_disk = 0;
    uint16_t attrs_internal = 0;
    uint32_t attrs_external = 0;

    zip->cdfs_offset = ftell(zip->stream);
    for(i = 0; i < zip->number_files; i++)
    {
        mzip_file_t *file = zip->files[i];

        zip_fwrite(zip, &signature,                4);
        zip_fwrite(zip, &file->version,            2);
        zip_fwrite(zip, &file->version_min,        2);
        zip_fwrite(zip, &file->gpbit_flag,         2);
        zip_fwrite(zip, &file->compression_method, 2);
        zip_fwrite(zip, &file->mtime,              2);
        zip_fwrite(zip, &file->mdate,              2);
        zip_fwrite(zip, &file->crc,                4);
        zip_fwrite(zip, &file->size_compressed,    4);
        zip_fwrite(zip, &file->size_uncompressed,  4);
        zip_fwrite(zip, &file->len_filename,       2);
        zip_fwrite(zip, &file->len_extra_field,    2);
        zip_fwrite(zip, &len_file_comment,         2);
        zip_fwrite(zip, &number_disk,              2);
        zip_fwrite(zip, &attrs_internal,           2);
        zip_fwrite(zip, &attrs_external,           4);
        zip_fwrite(zip, &file->offset,             4);
        zip_fwrite(zip, file->filename, file->len_filename);
        if(file->extra_field != NULL)
            zip_fwrite(zip, file->extra_field, file->len_extra_field);
    }
}

int mzip_close(mzip_t *zip)
{
    int ret;

    mzip_cdfs(zip);

    /* write End of central directory record (EOCD) */
    uint32_t eocd_signature      = MZIP_EOCD_SIGNATURE;
    uint16_t number_of_this_disk = 0;
    uint16_t disk_of_cdfs        = 0;
    uint32_t cdfs_size           = ftell(zip->stream) - zip->cdfs_offset;
    uint16_t comment_len         = strlen(MZIP_COMMENT);

    zip_fwrite(zip, &eocd_signature,      4);
    zip_fwrite(zip, &number_of_this_disk, 2);
    zip_fwrite(zip, &disk_of_cdfs,        2);
    zip_fwrite(zip, &zip->number_files,   2);
    zip_fwrite(zip, &zip->number_files,   2);
    zip_fwrite(zip, &cdfs_size,           4);
    zip_fwrite(zip, &zip->cdfs_offset,    4);
    zip_fwrite(zip, &comment_len,         2);
    zip_fwrite(zip, MZIP_COMMENT, comment_len);

    ret = fclose(zip->stream);

    zip_free(zip);

    return ret;
}

void zip_free(mzip_t *zip)
{
    int i;

    if(zip->files != NULL)
    {
        for(i = 0; i < zip->number_files; i++)
        {
            mzip_file_t *file = zip->files[i];
            if(file != NULL)
            {
                if(file->filename != NULL)
                {
                    free(file->filename);
                    file->filename = NULL;
                }

                free(file);
                file = NULL;
            }
        }

        free(zip->files);
        zip->files = NULL;
    }

}

int main(int argc, char *argv[])
{
    mzip_t zip;
    char content[] = "Computerworld";

    mzip_create(&zip, "mzip.zip");


    mzip_file_add(&zip, "datei1.txt");
    mzip_file_write(&zip, content, strlen(content));
    mzip_file_close(&zip);

    mzip_file_add(&zip, "libminizip.c");
    FILE *fh = fopen("libminizip.c", "r");
    while(!feof(fh))
    {
        char buffer[128];
        size_t size = fread(buffer, sizeof(char), 128, fh);
        mzip_file_write(&zip, buffer, size);
    }
    fclose(fh);

    mzip_file_close(&zip);

    mzip_close(&zip);

    return 0;
}
