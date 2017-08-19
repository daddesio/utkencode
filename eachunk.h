typedef struct EAChunk {
    uint32_t type;
    uint8_t *start;
    uint8_t *ptr;
    uint8_t *end;
} EAChunk;

static void chunk_read_bytes(EAChunk *chunk, uint8_t *dest, size_t size)
{
    size_t bytes_remaining = chunk->end - chunk->ptr;

    if (bytes_remaining < size) {
        fprintf(stderr, "error: unexpected end of chunk\n");
        exit(EXIT_FAILURE);
    }

    memcpy(dest, chunk->ptr, size);
    chunk->ptr += size;
}

static uint32_t chunk_read_u32(EAChunk *chunk)
{
    uint8_t dest[4];
    chunk_read_bytes(chunk, dest, sizeof(dest));
    return dest[0] | (dest[1] << 8) | (dest[2] << 16) | (dest[3] << 24);
}

static uint32_t chunk_read_u8(EAChunk *chunk)
{
    uint8_t dest;
    chunk_read_bytes(chunk, &dest, sizeof(dest));
    return dest;
}

static uint32_t chunk_read_var_int(EAChunk *chunk)
{
    uint8_t dest[4];
    uint8_t size = chunk_read_u8(chunk);

    if (size > 4) {
        fprintf(stderr, "error: invalid varint size %u\n", (unsigned)size);
        exit(EXIT_FAILURE);
    }

    chunk_read_bytes(chunk, dest, size);

    /* read a big-endian integer of variable length */
    switch (size) {
    case 1: return dest[0];
    case 2: return (dest[0]<<8) | dest[1];
    case 3: return (dest[0]<<16) | (dest[1] << 8) | dest[2];
    case 4: return (dest[0]<<24) | (dest[1] << 16) | (dest[2] << 8) | dest[3];
    default: return 0;
    }
}

static EAChunk *read_chunk(FILE *fp)
{
    uint32_t size;
    static EAChunk chunk;
    static uint8_t buffer[4096];

    chunk.type = read_u32(fp);

    size = read_u32(fp);
    if (size < 8 || size-8 > sizeof(buffer)) {
        fprintf(stderr, "error: invalid chunk size %u\n", (unsigned)size);
        exit(EXIT_FAILURE);
    }

    size -= 8;
    read_bytes(fp, buffer, size);
    chunk.start = chunk.ptr = buffer;
    chunk.end = buffer+size;

    return &chunk;
}