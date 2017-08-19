#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

static void read_bytes(FILE *fp, uint8_t *dest, size_t size)
{
    size_t bytes_copied;

    if (!size)
        return;

    bytes_copied = fread(dest, 1, size, fp);
    if (bytes_copied < size) {
        if (ferror(fp))
            fprintf(stderr, "error: fread failed: %s\n", strerror(errno));
        else
            fprintf(stderr, "error: unexpected end of file\n");

        exit(EXIT_FAILURE);
    }
}

static uint32_t read_u32(FILE *fp)
{
    uint8_t dest[4];
    read_bytes(fp, dest, sizeof(dest));
    return dest[0] | (dest[1] << 8) | (dest[2] << 16) | (dest[3] << 24);
}

static uint16_t read_u16(FILE *fp)
{
    uint8_t dest[2];
    read_bytes(fp, dest, sizeof(dest));
    return dest[0] | (dest[1] << 8);
}

static uint16_t read_u8(FILE *fp)
{
    uint8_t dest;
    read_bytes(fp, &dest, sizeof(dest));
    return dest;
}

static void write_bytes(FILE *fp, const uint8_t *dest, size_t size)
{
    if (!size)
        return;

    if (fwrite(dest, 1, size, fp) != size) {
        fprintf(stderr, "error: fwrite failed: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
}

static void write_u32(FILE *fp, uint32_t x)
{
    uint8_t dest[4];
    dest[0] = (uint8_t)x;
    dest[1] = (uint8_t)(x>>8);
    dest[2] = (uint8_t)(x>>16);
    dest[3] = (uint8_t)(x>>24);
    write_bytes(fp, dest, sizeof(dest));
}

static void write_u16(FILE *fp, uint16_t x)
{
    uint8_t dest[2];
    dest[0] = (uint8_t)x;
    dest[1] = (uint8_t)(x>>8);
    write_bytes(fp, dest, sizeof(dest));
}

static void write_u8(FILE *fp, uint8_t x)
{
    write_bytes(fp, &x, sizeof(x));
}