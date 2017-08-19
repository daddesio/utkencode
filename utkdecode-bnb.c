/*
** utkdecode-bnb
** Decode Beasts & Bumpkins M10 to wav.
** Authors: Andrew D'Addesio
** License: Public domain
** Compile: gcc -Wall -Wextra -Wno-unused-function -ansi -pedantic -O2 -ffast-math
**          -fwhole-program -g0 -s -o utkdecode-bnb utkdecode-bnb.c
*/
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "utk.h"
#include "io.h"
#include "eachunk.h"

#define MAKE_U32(a,b,c,d) ((a)|((b)<<8)|((c)<<16)|((d)<<24))
#define ROUND(x) ((x) >= 0.0f ? ((x)+0.5f) : ((x)-0.5f))
#define MIN(x,y) ((x)<(y)?(x):(y))
#define MAX(x,y) ((x)>(y)?(x):(y))
#define CLAMP(x,min,max) MIN(MAX(x,min),max)

typedef struct PTContext {
    FILE *infp, *outfp;
    uint32_t num_samples;
    uint32_t compression_type;
    UTKContext utk;
} PTContext;

static void pt_read_header(PTContext *pt)
{
    EAChunk *chunk = read_chunk(pt->infp);

    if ((chunk->type & 0xffff) != MAKE_U32('P','T','\x00','\x00')) {
        fprintf(stderr, "error: expected PT chunk\n");
        exit(EXIT_FAILURE);
    }

    while (1) {
        uint8_t cmd = chunk_read_u8(chunk);
        if (cmd == 0xFD) {
            while (1) {
                uint8_t key = chunk_read_u8(chunk);
                uint32_t value = chunk_read_var_int(chunk);

                if (key == 0xFF)
                    break;
                else if (key == 0x85)
                    pt->num_samples = value;
                else if (key == 0x83)
                    pt->compression_type = value;
            }
            break;
        } else {
            chunk_read_var_int(chunk);
        }
    }

    if (pt->compression_type != 9) {
        fprintf(stderr, "error: invalid compression type %u (expected 9 for MicroTalk 10:1)\n",
                (unsigned)pt->compression_type);
        exit(EXIT_FAILURE);
    }

    if (pt->num_samples >= 0x01000000) {
        fprintf(stderr, "error: invalid num_samples %u\n", pt->num_samples);
        exit(EXIT_FAILURE);
    }

    /* Initialize the decoder. */
    utk_init(&pt->utk);

    /* Write the WAV header. */
    write_u32(pt->outfp, MAKE_U32('R','I','F','F'));
    write_u32(pt->outfp, 36 + pt->num_samples*2);
    write_u32(pt->outfp, MAKE_U32('W','A','V','E'));
    write_u32(pt->outfp, MAKE_U32('f','m','t',' '));
    write_u32(pt->outfp, 16);
    write_u16(pt->outfp, 1);
    write_u16(pt->outfp, 1);
    write_u32(pt->outfp, 22050);
    write_u32(pt->outfp, 22050*2);
    write_u16(pt->outfp, 2);
    write_u16(pt->outfp, 16);
    write_u32(pt->outfp, MAKE_U32('d','a','t','a'));
    write_u32(pt->outfp, pt->num_samples*2);
}

static void pt_decode(PTContext *pt)
{
    UTKContext *utk = &pt->utk;
    uint32_t num_samples = pt->num_samples;

    utk_set_fp(utk, pt->infp);

    while (num_samples > 0) {
        int count = MIN(num_samples, 432);
        int i;

        utk_decode_frame(utk);

        for (i = 0; i < count; i++) {
            int x = ROUND(pt->utk.decompressed_frame[i]);
            write_u16(pt->outfp, (int16_t)CLAMP(x, -32768, 32767));
        }

        num_samples -= count;
    }
}

int main(int argc, char *argv[])
{
    PTContext pt;
    const char *infile, *outfile;
    FILE *infp, *outfp;
    int force = 0;

    /* Parse arguments. */
    if (argc == 4 && !strcmp(argv[1], "-f")) {
        force = 1;
        argv++, argc--;
    }

    if (argc != 3) {
        printf("Usage: utkdecode-bnb [-f] infile outfile\n");
        printf("Decode Beasts & Bumpkins M10 to wav.\n");
        return EXIT_FAILURE;
    }

    infile = argv[1];
    outfile = argv[2];

    /* Open the input/output files. */
    infp = fopen(infile, "rb");
    if (!infp) {
        fprintf(stderr, "error: failed to open '%s' for reading: %s\n", infile, strerror(errno));
        return EXIT_FAILURE;
    }

    if (!force && fopen(outfile, "rb")) {
        fprintf(stderr, "error: '%s' already exists\n", outfile);
        return EXIT_FAILURE;
    }

    outfp = fopen(outfile, "wb");
    if (!outfp) {
        fprintf(stderr, "error: failed to create '%s': %s\n", outfile, strerror(errno));
        return EXIT_FAILURE;
    }

    memset(&pt, 0, sizeof(pt));
    pt.infp = infp;
    pt.outfp = outfp;

    pt_read_header(&pt);
    pt_decode(&pt);

    if (fclose(outfp) != 0) {
        fprintf(stderr, "error: failed to close '%s': %s\n", outfile, strerror(errno));
        return EXIT_FAILURE;
    }

    fclose(infp);

    return EXIT_SUCCESS;
}