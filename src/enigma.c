#include "enigma/enigma.h"
#include "internal/enigma_internal.h"

#include <inttypes.h>
#include <limits.h>
#include <nexus/nexus.h>
#include <stdio.h>
#include <string.h>

/* ---- public API ------------------------------------------------------------ */

const char* enigma_version_string(void) {
    return "0.0.0";
}

typedef struct {
    NEXUS_BOOL isLast;
    nexus_u8   metadataBlockType;
    nexus_u32  metadataBlockSize;
} ENIGMA_FLAC_METADATA_HEADER;

ENIGMA_FLAC_METADATA_HEADER enigma_metadata_header_info_create(
    const unsigned char *bytes, const size_t byteAmount)
{
    ENIGMA_FLAC_METADATA_HEADER header = (ENIGMA_FLAC_METADATA_HEADER){0};
    if (!bytes || byteAmount != 4) return header;

    header.isLast            = bytes[0] & 0x80u ? NEXUS_TRUE : NEXUS_FALSE; /* MSB */
    header.metadataBlockType = (nexus_u8)(bytes[0] & 0x7Fu); /* low 7 bits */
    header.metadataBlockSize =
        (nexus_u32)bytes[1] << 16 | /* High */
        (nexus_u32)bytes[2] << 8 | /* Mid */
        (nexus_u32)bytes[3]; /*Low */

    return header;
}

/* --- Private helpers ------------------------------------------------------ */

static void enigma__ok(char* errorBuffer, const size_t errorBufferSize) {
    if (errorBuffer && errorBufferSize) errorBuffer[0] = NEXUS_STRING_TERMINATOR;
}

static ENIGMA_ERROR_CODE enigma__invalid_arg(char* errorBuffer, const size_t errorBufferSize) {
    nexus_string_message_copy(errorBuffer, errorBufferSize, "Invalid argument");
    return ENIGMA_INVALID_ARGUMENT;
}

static ENIGMA_ERROR_CODE enigma__oom(char* errorBuffer, const size_t errorBufferSize) {
    nexus_string_message_copy(errorBuffer, errorBufferSize, "Out of memory");
    return ENIGMA_FUNCTION_ERROR;
}

/* Read exactly n bytes from filePath@offset or set error; returns NEXUS_TRUE on success. */
static NEXUS_BOOL enigma__read_exact_at(const char* filePath,
                                        const nexus_i64 offset,
                                        void* dst,
                                        size_t n,
                                        char* errorBuffer,
                                        const size_t errorBufferSize)
{
    size_t got = 0;
    nexus_file_read_at(filePath, offset, /*origin=*/0,
                       dst, n, &got, errorBuffer, errorBufferSize);

    if (errorBuffer && errorBuffer[0] != NEXUS_STRING_TERMINATOR) {
        return NEXUS_FALSE;
    }
    if (got != n) {
        nexus_string_message_format_copy(errorBuffer, errorBufferSize,
                                         "Short read at offset %u: got %zu bytes",
                                         (unsigned)offset, got);
        return NEXUS_FALSE;
    }
    return NEXUS_TRUE;
}

/* Validate the "fLaC" signature at start of file. */
static ENIGMA_ERROR_CODE enigma__validate_flac_signature(const char* filePath,
                                                         char* errorBuffer,
                                                         const size_t errorBufferSize)
{
    unsigned char sig[sizeof ENIGMA_FLAC_SIGNATURE];
    if (!enigma__read_exact_at(filePath, 0, sig, sizeof sig, errorBuffer, errorBufferSize)) {
        return ENIGMA_FUNCTION_ERROR;
    }
    if (memcmp(sig, ENIGMA_FLAC_SIGNATURE, sizeof ENIGMA_FLAC_SIGNATURE) != 0) {
        nexus_string_message_copy(errorBuffer, errorBufferSize, "Not a FLAC file");
        return ENIGMA_INVALID_ARGUMENT;
    }
    return ENIGMA_OK;
}

/* Allocate and initialize the handle + duplicate path. */
static ENIGMA_ERROR_CODE enigma__alloc_handle(const char* filePath,
                                              ENIGMA_FLAC_INFORMATION_HANDLE* outHandle,
                                              char* errorBuffer,
                                              const size_t errorBufferSize)
{
    ENIGMA_FLAC_INFORMATION_HANDLE handle = NEXUS_ALLOC(sizeof *handle);
    if (!handle) return enigma__oom(errorBuffer, errorBufferSize);
    memset(handle, 0, sizeof *handle);

    handle->filePath = nexus_string_duplicate(filePath);
    if (!handle->filePath) {
        NEXUS_FREE(handle);
        return enigma__oom(errorBuffer, errorBufferSize);
    }
    *outHandle = handle;
    return ENIGMA_OK;
}

/* Parse STREAMINFO payload (34 bytes) into handle fields. */
static ENIGMA_ERROR_CODE enigma__parse_streaminfo(ENIGMA_FLAC_INFORMATION_HANDLE handle,
                                                  const unsigned char payload[34])
{
    /* 16-bit big-endian block sizes */
    handle->minBlockSize = (nexus_u16)payload[0] << 8 | (nexus_u16)payload[1];
    handle->maxBlockSize = (nexus_u16)payload[2] << 8 | (nexus_u16)payload[3];

    /* 24-bit big-endian frame sizes (0 means unknown) */
    handle->minFrameSize =
        (nexus_u32)payload[4] << 16 |
        (nexus_u32)payload[5] << 8  |
        (nexus_u32)payload[6];
    handle->maxFrameSize =
        (nexus_u32)payload[7] << 16 |
        (nexus_u32)payload[8] << 8  |
        (nexus_u32)payload[9];

    /* Bytes 10..17 packed fields */
    const nexus_u64 packed =
        (nexus_u64)payload[10] << 56 |
        (nexus_u64)payload[11] << 48 |
        (nexus_u64)payload[12] << 40 |
        (nexus_u64)payload[13] << 32 |
        (nexus_u64)payload[14] << 24 |
        (nexus_u64)payload[15] << 16 |
        (nexus_u64)payload[16] << 8  |
        (nexus_u64)payload[17];

    handle->sampleRate    = (nexus_u32)(packed >> 44 & 0xFFFFFULL); /* top 20 bits */
    handle->numChannels   = (nexus_u8) ((packed >> 41 & 0x7) + 1);  /* 3b + 1 */
    handle->bitsPerSample = (nexus_u8) ((packed >> 36 & 0x1F) + 1); /* 5b + 1 */
    handle->totalSamples  = packed & 0xFFFFFFFFFULL;    /* low 36 bits */

    /* MD5 (bytes 18..33) */
    memcpy(handle->md5Hash, &payload[18], 16);

    return ENIGMA_OK;
}

/* Scan metadata blocks from offset=4, read STREAMINFO (type 0) into handle. */
static ENIGMA_ERROR_CODE enigma__find_and_read_streaminfo(ENIGMA_FLAC_INFORMATION_HANDLE handle,
                                                          char* errorBuffer,
                                                          const size_t errorBufferSize)
{
  nexus_i64 offset = 4;

    for (;;) {
        const nexus_u8 blockHeaderSize = 4;
        unsigned char headerBytes[blockHeaderSize];

        if (!enigma__read_exact_at(handle->filePath, offset, headerBytes, blockHeaderSize,
                                   errorBuffer, errorBufferSize)) {
            NEXUS_FREE(handle);
            return ENIGMA_FUNCTION_ERROR;
        }

        const ENIGMA_FLAC_METADATA_HEADER header =
            enigma_metadata_header_info_create(headerBytes, blockHeaderSize);

        if (header.metadataBlockType == 0) { /* STREAMINFO */
            if (header.metadataBlockSize != 34u) {
                nexus_string_message_format_copy(errorBuffer, errorBufferSize,
                                                 "Invalid STREAMINFO size: %u",
                                                 (unsigned)header.metadataBlockSize);
                NEXUS_FREE(handle);
                return ENIGMA_FUNCTION_ERROR;
            }

            unsigned char payload[34];
            if (!enigma__read_exact_at(handle->filePath,
                                       offset + blockHeaderSize,
                                       payload, sizeof payload,
                                       errorBuffer, errorBufferSize)) {
                NEXUS_FREE(handle);
                return ENIGMA_FUNCTION_ERROR;
            }

            enigma__parse_streaminfo(handle, payload);
            return ENIGMA_OK; /* done */
        }

        /* Not STREAMINFO: advance. */
        offset += (nexus_u64)blockHeaderSize + (nexus_u64)header.metadataBlockSize;

        if (header.isLast) {
            nexus_string_message_copy(errorBuffer, errorBufferSize,
                                      "Invalid FLAC file -- no STREAMINFO block");
            NEXUS_FREE(handle);
            return ENIGMA_FUNCTION_ERROR;
        }
    }
}

/* --- Public API ------------------------------------------------------------ */

ENIGMA_ERROR_CODE enigma_flac_open(const char* filePath,
                                   ENIGMA_FLAC_INFORMATION_HANDLE* outHandle,
                                   char* errorBuffer,
                                   const size_t errorBufferSize)
{
    enigma__ok(errorBuffer, errorBufferSize);
    if (outHandle) *outHandle = NULL;
    if (!filePath || !outHandle) return enigma__invalid_arg(errorBuffer, errorBufferSize);

    /* 1) Validate signature */
    {
        const ENIGMA_ERROR_CODE s = enigma__validate_flac_signature(filePath, errorBuffer, errorBufferSize);
        if (s != ENIGMA_OK) return s;
    }

    /* 2) Allocate handle */
    ENIGMA_FLAC_INFORMATION_HANDLE handle = NULL;
    {
        const ENIGMA_ERROR_CODE a = enigma__alloc_handle(filePath, &handle, errorBuffer, errorBufferSize);
        if (a != ENIGMA_OK) return a;
    }

    /* 3) Find + read STREAMINFO */
    {
        const ENIGMA_ERROR_CODE r = enigma__find_and_read_streaminfo(handle, errorBuffer, errorBufferSize);
        if (r != ENIGMA_OK) return r;
    }

    *outHandle = handle;
    enigma__ok(errorBuffer, errorBufferSize);
    return ENIGMA_OK;
}

void enigma_flac_close(ENIGMA_FLAC_INFORMATION_HANDLE handle) {
    if (!handle) return;
    if (handle->filePath) NEXUS_FREE(handle->filePath);
    NEXUS_FREE(handle);
}


