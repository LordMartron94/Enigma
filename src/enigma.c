#include "enigma/enigma.h"
#include "internal/enigma_internal.h"

#include <inttypes.h>
#include <nexus/nexus.h>
#include <stdio.h>
#include <string.h>


/* ---- small helpers ------------------------------------------------------- */

const char* enigma_version_string(void) {
    return "0.0.0";
}

/* ---- I/O & parsing ------------------------------------------------------- */

/* Validate the "fLaC" signature at start of file. */
static NEXUS_ERROR_CODE enigma__validate_flac_signature(const char* filePath,
                                                         char* errorBuffer,
                                                         const size_t errorBufferSize)
{
    unsigned char sig[sizeof ENIGMA_FLAC_SIGNATURE];
    if (!nexus_file_scan_at(filePath, 0, sig, sizeof sig, errorBuffer, errorBufferSize)) {
        return NEXUS_FUNCTION_ERROR;
    }
    if (memcmp(sig, ENIGMA_FLAC_SIGNATURE, sizeof ENIGMA_FLAC_SIGNATURE) != 0) {
        nexus_string_message_copy(errorBuffer, errorBufferSize, "Not a FLAC file");
        return NEXUS_INVALID_ARGUMENT;
    }
    return NEXUS_OK;
}

/* Allocate and initialize the handle + duplicate path. */
static NEXUS_ERROR_CODE enigma__alloc_handle(const char* filePath,
                                              ENIGMA_FLAC_INFORMATION_HANDLE* outHandle,
                                              char* errorBuffer,
                                              const size_t errorBufferSize)
{
    (void)errorBuffer; (void)errorBufferSize;

    ENIGMA_FLAC_INFORMATION_HANDLE handle = NEXUS_ALLOC(sizeof *handle);
    if (!handle) return NEXUS_FUNCTION_ERROR;
    memset(handle, 0, sizeof *handle);

    handle->filePath = nexus_string_duplicate(filePath);
    if (!handle->filePath) {
        NEXUS_FREE(handle);
        return NEXUS_FUNCTION_ERROR;
    }
    *outHandle = handle;
    return NEXUS_OK;
}

/* Parse STREAMINFO payload (34 bytes) into handle fields. */
static NEXUS_ERROR_CODE enigma__parse_streaminfo(ENIGMA_FLAC_INFORMATION_HANDLE handle,
                                                  const unsigned char payload[34])
{
    handle->minBlockSize = (nexus_u16)payload[0] << 8 | (nexus_u16)payload[1];
    handle->maxBlockSize = (nexus_u16)payload[2] << 8 | (nexus_u16)payload[3];

    handle->minFrameSize =
        (nexus_u32)payload[4] << 16 |
        (nexus_u32)payload[5] << 8  |
        (nexus_u32)payload[6];
    handle->maxFrameSize =
        (nexus_u32)payload[7] << 16 |
        (nexus_u32)payload[8] << 8  |
        (nexus_u32)payload[9];

    const nexus_u64 packed =
        (nexus_u64)payload[10] << 56 |
        (nexus_u64)payload[11] << 48 |
        (nexus_u64)payload[12] << 40 |
        (nexus_u64)payload[13] << 32 |
        (nexus_u64)payload[14] << 24 |
        (nexus_u64)payload[15] << 16 |
        (nexus_u64)payload[16] << 8  |
        (nexus_u64)payload[17];

    handle->sampleRate    = (nexus_u32)(packed >> 44 & 0xFFFFFULL);
    handle->numChannels   = (nexus_u8) ((packed >> 41 & 0x7) + 1);
    handle->bitsPerSample = (nexus_u8) ((packed >> 36 & 0x1F) + 1);
    handle->totalSamples  = packed & 0xFFFFFFFFFULL;

    memcpy(handle->md5Hash, &payload[18], 16);

    enigma__streaminfo_debug_print(handle, stdout);
    return NEXUS_OK;
}

static NEXUS_ERROR_CODE enigma__stream_info_handle(
    const ENIGMA_FLAC_METADATA_HEADER header,
    ENIGMA_FLAC_INFORMATION_HANDLE handle,
    char *errorBuffer, const size_t errorBufferSize,
    const nexus_i64 offset, const nexus_u8 blockHeaderSize
) {
    if (header.metadataBlockSize != 34u) {
        nexus_string_message_format_copy(errorBuffer, errorBufferSize,
                                         "Invalid STREAMINFO size: %u",
                                         (unsigned)header.metadataBlockSize);
        NEXUS_FREE(handle);
        return NEXUS_FUNCTION_ERROR;
    }

    unsigned char payload[34];
    if (!nexus_file_scan_at(handle->filePath,
                 offset + blockHeaderSize,
                 payload, sizeof payload,
                 errorBuffer, errorBufferSize)) {
        NEXUS_FREE(handle);
        return NEXUS_FUNCTION_ERROR;
    }

    enigma__parse_streaminfo(handle, payload);
    return NEXUS_OK;
}

/* Parse all metadata using absolute offsets only. */
static NEXUS_ERROR_CODE enigma__metadata_parse(ENIGMA_FLAC_INFORMATION_HANDLE handle,
                                                char* errorBuffer,
                                                const size_t errorBufferSize)
{
    nexus_i64 logical_offset = 4; /* after "fLaC" */
    NEXUS_BOOL streamInfoFound = NEXUS_FALSE;

    for (;;) {
        const nexus_u8 blockHeaderSize = 4;
        unsigned char headerBytes[4];

        if (!nexus_file_scan_at(handle->filePath, logical_offset, headerBytes, blockHeaderSize,
                     errorBuffer, errorBufferSize)) {
            NEXUS_FREE(handle);
            return NEXUS_FUNCTION_ERROR;
        }

        const ENIGMA_FLAC_METADATA_HEADER header =
            enigma_metadata_header_info_create(headerBytes, blockHeaderSize);

        if (header.metadataBlockType == 0) {
            const NEXUS_ERROR_CODE r =
                enigma__stream_info_handle(header, handle, errorBuffer, errorBufferSize,
                                           logical_offset, blockHeaderSize);
            if (r != NEXUS_OK) return r;
            streamInfoFound = NEXUS_TRUE;
        }

        logical_offset += (nexus_i64)blockHeaderSize + (nexus_i64)header.metadataBlockSize;

        if (header.isLast) {
            handle->lastMetadataBlockEndOffset = (nexus_u64)logical_offset;
            break;
        }
    }

    if (streamInfoFound == NEXUS_FALSE) {
        nexus_string_message_copy(errorBuffer, errorBufferSize,
                                  "Invalid FLAC file -- no STREAMINFO block");
        NEXUS_FREE(handle);
        return NEXUS_FUNCTION_ERROR;
    }

    return NEXUS_OK;
}

/* ---- public API ---------------------------------------------------------- */

NEXUS_ERROR_CODE enigma_flac_open(const char *filePath,
                                   ENIGMA_FLAC_INFORMATION_HANDLE* outHandle,
                                   char *errorBuffer,
                                   const size_t errorBufferSize)
{
    nexus_errors_ok(errorBuffer, errorBufferSize);
    if (outHandle) *outHandle = NULL;
    if (!filePath || !outHandle) return nexus_errors_invalid_argument(errorBuffer, errorBufferSize);

    {
        const NEXUS_ERROR_CODE s = enigma__validate_flac_signature(filePath, errorBuffer, errorBufferSize);
        if (s != NEXUS_OK) return s;
    }

    ENIGMA_FLAC_INFORMATION_HANDLE handle = NULL;
    {
        const NEXUS_ERROR_CODE a = enigma__alloc_handle(filePath, &handle, errorBuffer, errorBufferSize);
        if (a != NEXUS_OK) return a;
    }

    {
        const NEXUS_ERROR_CODE r = enigma__metadata_parse(handle, errorBuffer, errorBufferSize);
        if (r != NEXUS_OK) return r;
    }

    *outHandle = handle;
    nexus_errors_ok(errorBuffer, errorBufferSize);
    return NEXUS_OK;
}

void enigma_flac_close(ENIGMA_FLAC_INFORMATION_HANDLE handle) {
    if (!handle) return;
    if (handle->filePath) NEXUS_FREE(handle->filePath);
    NEXUS_FREE(handle);
}

nexus_u32 enigma_flac_samplerate_get(ENIGMA_FLAC_INFORMATION_HANDLE handle) {
    if (!handle) return 0;
    return handle->sampleRate;
}

nexus_u8 enigma_flac_channels_get(ENIGMA_FLAC_INFORMATION_HANDLE handle) {
    if (!handle) return 0;
    return handle->numChannels;
}

nexus_u8 enigma_flac_bit_depth_get(ENIGMA_FLAC_INFORMATION_HANDLE handle) {
    if (!handle) return 0;
    return handle->bitsPerSample;
}

nexus_u16 enigma_flac_last_metadata_block_end_offset_get(ENIGMA_FLAC_INFORMATION_HANDLE handle) {
    if (!handle) return 0;
    const nexus_u64 v = handle->lastMetadataBlockEndOffset;
    return v & 0xFFFFu;
}

float_real enigma_flac_duration_seconds_get(ENIGMA_FLAC_INFORMATION_HANDLE handle) {
    return (float_real)handle->totalSamples / (float_real)handle->sampleRate;
}

float_real enigma_flac_duration_milliseconds_get(ENIGMA_FLAC_INFORMATION_HANDLE handle) {
    return enigma_flac_duration_seconds_get(handle) * 1000;
}
