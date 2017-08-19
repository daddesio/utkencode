/*
** utkdecode
** Decode Maxis UTK to wav.
** Authors: Andrew D'Addesio
** License: Public domain
** Compile: gcc -Wall -Wextra -Wno-unused-function -ansi -pedantic -O2 -ffast-math
**          -fwhole-program -g0 -s -o utkdecode utkdecode.c
*/
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "utk.h"
#include "io.h"

#define MAKE_U32(a,b,c,d) ((a)|((b)<<8)|((c)<<16)|((d)<<24))
#define ROUND(x) ((x) >= 0.0f ? ((x)+0.5f) : ((x)-0.5f))
#define MIN(x,y) ((x)<(y)?(x):(y))
#define MAX(x,y) ((x)>(y)?(x):(y))
#define CLAMP(x,min,max) MIN(MAX(x,min),max)

int main(int argc, char *argv[])
{
    const char *infile, *outfile;
    UTKContext ctx;
    uint32_t sID;
    uint32_t dwOutSize;
    uint32_t dwWfxSize;
    uint16_t wFormatTag;
    uint16_t nChannels;
    uint32_t nSamplesPerSec;
    uint32_t nAvgBytesPerSec;
    uint16_t nBlockAlign;
    uint16_t wBitsPerSample;
    uint16_t cbSize;
    uint32_t num_samples;
    FILE *infp, *outfp;
    int force = 0;
    int error = 0;
    int i;

    /* Parse arguments. */
    if (argc == 4 && !strcmp(argv[1], "-f")) {
        force = 1;
        argv++, argc--;
    }

    if (argc != 3) {
        printf("Usage: utkdecode [-f] infile outfile\n");
        printf("Decode Maxis UTK to wav.\n");
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

    /* Parse the UTK header. */
    sID = read_u32(infp);
    dwOutSize = read_u32(infp);
    dwWfxSize = read_u32(infp);
    wFormatTag = read_u16(infp);
    nChannels = read_u16(infp);
    nSamplesPerSec = read_u32(infp);
    nAvgBytesPerSec = read_u32(infp);
    nBlockAlign = read_u16(infp);
    wBitsPerSample = read_u16(infp);
    cbSize = read_u16(infp);
    read_u16(infp); /* padding */

    if (sID != MAKE_U32('U','T','M','0')) {
        fprintf(stderr, "error: not a valid UTK file (expected UTM0 signature)\n");
        return EXIT_FAILURE;
    } else if ((dwOutSize & 0x01) != 0 || dwOutSize >= 0x01000000) {
        fprintf(stderr, "error: invalid dwOutSize %u\n", (unsigned)dwOutSize);
        return EXIT_FAILURE;
    } else if (dwWfxSize != 20) {
        fprintf(stderr, "error: invalid dwWfxSize %u (expected 20)\n", (unsigned)dwWfxSize);
        return EXIT_FAILURE;
    } else if (wFormatTag != 1) {
        fprintf(stderr, "error: invalid wFormatTag %u (expected 1)\n", (unsigned)wFormatTag);
        return EXIT_FAILURE;
    }

    if (nChannels != 1) {
        fprintf(stderr, "error: invalid nChannels %u (only mono is supported)\n", (unsigned)nChannels);
        error = 1;
    }
    if (nSamplesPerSec < 8000 || nSamplesPerSec > 192000) {
        fprintf(stderr, "error: invalid nSamplesPerSec %u\n", (unsigned)nSamplesPerSec);
        error = 1;
    }
    if (nAvgBytesPerSec != nSamplesPerSec * nBlockAlign) {
        fprintf(stderr, "error: invalid nAvgBytesPerSec %u (expected nSamplesPerSec * nBlockAlign)\n", (unsigned)nAvgBytesPerSec);
        error = 1;
    }
    if (nBlockAlign != 2) {
        fprintf(stderr, "error: invalid nBlockAlign %u (expected 2)\n", (unsigned)nBlockAlign);
        error = 1;
    }
    if (wBitsPerSample != 16) {
        fprintf(stderr, "error: invalid wBitsPerSample %u (expected 16)\n", (unsigned)wBitsPerSample);
        error = 1;
    }
    if (cbSize != 0) {
        fprintf(stderr, "error: invalid cbSize %u (expected 0)\n", (unsigned)cbSize);
        error = 1;
    }
    if (error)
        return EXIT_FAILURE;

    num_samples = dwOutSize/2;

    /* Write the WAV header. */
    write_u32(outfp, MAKE_U32('R','I','F','F'));
    write_u32(outfp, 36 + num_samples*2);
    write_u32(outfp, MAKE_U32('W','A','V','E'));
    write_u32(outfp, MAKE_U32('f','m','t',' '));
    write_u32(outfp, 16);
    write_u16(outfp, wFormatTag);
    write_u16(outfp, nChannels);
    write_u32(outfp, nSamplesPerSec);
    write_u32(outfp, nAvgBytesPerSec);
    write_u16(outfp, nBlockAlign);
    write_u16(outfp, wBitsPerSample);
    write_u32(outfp, MAKE_U32('d','a','t','a'));
    write_u32(outfp, num_samples*2);

    /* Decode. */
    utk_init(&ctx);
    utk_set_fp(&ctx, infp);

    while (num_samples > 0) {
        int count = MIN(num_samples, 432);

        utk_decode_frame(&ctx);

        for (i = 0; i < count; i++) {
            int x = ROUND(ctx.decompressed_frame[i]);
            write_u16(outfp, (int16_t)CLAMP(x, -32768, 32767));
        }

        num_samples -= count;
    }

    if (fclose(outfp) != 0) {
        fprintf(stderr, "error: failed to close '%s': %s\n", outfile, strerror(errno));
        return EXIT_FAILURE;
    }

    fclose(infp);

    return EXIT_SUCCESS;
}