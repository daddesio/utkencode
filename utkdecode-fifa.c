/*
** utkdecode-fifa
** Decode FIFA 2001/2002 MicroTalk to wav.
** Authors: Andrew D'Addesio
** License: Public domain
** Compile: gcc -Wall -Wextra -Wno-unused-function -ansi -pedantic -O2 -ffast-math
**          -fwhole-program -g0 -s -o utkdecode-fifa utkdecode-fifa.c
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

typedef struct EAContext {
    FILE *infp, *outfp;
    uint32_t audio_pos;
    uint32_t num_samples;
    uint32_t num_data_chunks;
    uint32_t compression_type;
    uint32_t codec_revision;
    UTKContext utk;
} EAContext;

static void ea_read_schl(EAContext *ea)
{
    uint32_t id;
    EAChunk *chunk = read_chunk(ea->infp);

    if (chunk->type != MAKE_U32('S','C','H','l')) {
        fprintf(stderr, "error: expected SCHl chunk\n");
        exit(EXIT_FAILURE);
    }

    id = chunk_read_u32(chunk);
    if ((id & 0xffff) != MAKE_U32('P','T','\x00','\x00')) {
        fprintf(stderr, "error: expected PT chunk in SCHl header\n");
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
                else if (key == 0x80)
                    ea->codec_revision = value;
                else if (key == 0x85)
                    ea->num_samples = value;
                else if (key == 0xA0)
                    ea->compression_type = value;
            }
            break;
        } else {
            chunk_read_var_int(chunk);
        }
    }

    if (ea->compression_type != 4 && ea->compression_type != 22) {
        fprintf(stderr, "error: invalid compression type %u (expected 4 for MicroTalk 10:1 or 22 for MicroTalk 5:1)\n",
                (unsigned)ea->compression_type);
        exit(EXIT_FAILURE);
    }

    if (ea->num_samples >= 0x01000000) {
        fprintf(stderr, "error: invalid num_samples %u\n", ea->num_samples);
        exit(EXIT_FAILURE);
    }

    /* Initialize the decoder. */
    utk_init(&ea->utk);

    /* Write the WAV header. */
    write_u32(ea->outfp, MAKE_U32('R','I','F','F'));
    write_u32(ea->outfp, 36 + ea->num_samples*2);
    write_u32(ea->outfp, MAKE_U32('W','A','V','E'));
    write_u32(ea->outfp, MAKE_U32('f','m','t',' '));
    write_u32(ea->outfp, 16);
    write_u16(ea->outfp, 1);
    write_u16(ea->outfp, 1);
    write_u32(ea->outfp, 22050);
    write_u32(ea->outfp, 22050*2);
    write_u16(ea->outfp, 2);
    write_u16(ea->outfp, 16);
    write_u32(ea->outfp, MAKE_U32('d','a','t','a'));
    write_u32(ea->outfp, ea->num_samples*2);
}

static void ea_read_sccl(EAContext *ea)
{
    EAChunk *chunk = read_chunk(ea->infp);

    if (chunk->type != MAKE_U32('S','C','C','l')) {
        fprintf(stderr, "error: expected SCCl chunk\n");
        exit(EXIT_FAILURE);
    }

    ea->num_data_chunks = chunk_read_u32(chunk);
    if (ea->num_data_chunks >= 0x01000000) {
        fprintf(stderr, "error: invalid num_data_chunks %u\n", (unsigned)ea->num_data_chunks);
        exit(EXIT_FAILURE);
    }
}

static void ea_read_scdl(EAContext *ea)
{
    EAChunk *chunk = read_chunk(ea->infp);
    UTKContext *utk = &ea->utk;
    uint32_t num_samples;

    if (chunk->type != MAKE_U32('S','C','D','l')) {
        fprintf(stderr, "error: expected SCDl chunk\n");
        exit(EXIT_FAILURE);
    }

    num_samples = chunk_read_u32(chunk);
    chunk_read_u32(chunk); /* unknown */
    chunk_read_u8(chunk);  /* unknown */

    if (num_samples > ea->num_samples - ea->audio_pos)
        num_samples = ea->num_samples - ea->audio_pos;

    utk_set_ptr(utk, chunk->ptr, chunk->end);

    while (num_samples > 0) {
        int count = MIN(num_samples, 432);
        int i;

        if (ea->codec_revision >= 3)
            utk_rev3_decode_frame(utk);
        else
            utk_decode_frame(utk);

        for (i = 0; i < count; i++) {
            int x = ROUND(ea->utk.decompressed_frame[i]);
            write_u16(ea->outfp, (int16_t)CLAMP(x, -32768, 32767));
        }

        ea->audio_pos += count;
        num_samples -= count;
    }
}

static void ea_read_scel(const EAContext *ea)
{
    EAChunk *chunk = read_chunk(ea->infp);

    if (chunk->type != MAKE_U32('S','C','E','l')) {
        fprintf(stderr, "error: expected SCEl chunk\n");
        exit(EXIT_FAILURE);
    }

    if (ea->audio_pos != ea->num_samples) {
        fprintf(stderr, "error: failed to decode the correct number of samples\n");
        exit(EXIT_FAILURE);
    }
}

int main(int argc, char *argv[])
{
    EAContext ea;
    const char *infile, *outfile;
    FILE *infp, *outfp;
    int force = 0;
    unsigned int i;

    if (argc == 4 && !strcmp(argv[1], "-f")) {
        force = 1;
        argv++, argc--;
    }

    if (argc != 3) {
        printf("Usage: utkdecode-fifa [-f] infile outfile\n");
        printf("Decode FIFA 2001/2002 MicroTalk to wav.\n");
        return EXIT_FAILURE;
    }

    infile = argv[1];
    outfile = argv[2];

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

    memset(&ea, 0, sizeof(ea));
    ea.infp = infp;
    ea.outfp = outfp;

    ea_read_schl(&ea);
    ea_read_sccl(&ea);

    for (i = 0; i < ea.num_data_chunks; i++)
        ea_read_scdl(&ea);

    ea_read_scel(&ea);

    if (!outfp) {
        fprintf(stderr, "error: failed to close '%s': %s\n", outfile, strerror(errno));
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}