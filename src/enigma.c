#include "enigma/enigma.h"
#include "internal/enigma_internal.h"

#include <nexus/nexus.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>

/* ---- public API ------------------------------------------------------------ */

const char* enigma_version_string(void) {
    return "0.0.0";
}

ENIGMA_ERROR_CODE enigma_flac_open(const char* filePath,
                                   ENIGMA_FLAC_INFORMATION_HANDLE* outHandle,
                                   char* errorBuffer,
                                   const size_t errorBufferSize)
{
    unsigned char readBuffer[sizeof ENIGMA_FLAC_SIGNATURE];

    if (outHandle) *outHandle = NULL;
    if (!filePath || !outHandle) {
      nexus_string_message_copy(errorBuffer, errorBufferSize, "Invalid argument");
      return ENIGMA_INVALID_ARGUMENT;
    }

    size_t *bytesRead = 0;
    nexus_file_read_at(filePath, 0, 0, readBuffer, sizeof ENIGMA_FLAC_SIGNATURE, bytesRead, errorBuffer, errorBufferSize);

    if (errorBuffer[0] != NEXUS_STRING_TERMINATOR) {
        return ENIGMA_FUNCTION_ERROR;
    }

    if (memcmp(readBuffer, ENIGMA_FLAC_SIGNATURE, sizeof ENIGMA_FLAC_SIGNATURE) != 0) {
        nexus_string_message_copy(errorBuffer, errorBufferSize, "Not a FLAC file");
        return ENIGMA_INVALID_ARGUMENT;
    }

    {
      ENIGMA_FLAC_INFORMATION_HANDLE handle = NEXUS_ALLOC(sizeof *handle);
        if (!handle) {
            nexus_string_message_copy(errorBuffer, errorBufferSize, "Out of memory");
            return ENIGMA_FUNCTION_ERROR;
        }

        handle->filePath = nexus_string_duplicate(filePath);
        if (!handle->filePath) {
            NEXUS_FREE(handle);
            nexus_string_message_copy(errorBuffer, errorBufferSize, "Out of memory");
            return ENIGMA_FUNCTION_ERROR;
        }

        *outHandle = handle;
    }

    if (errorBuffer && errorBufferSize) errorBuffer[0] = NEXUS_STRING_TERMINATOR;
    return ENIGMA_OK;
}

void enigma_flac_close(ENIGMA_FLAC_INFORMATION_HANDLE handle) {
    if (!handle) return;
    if (handle->filePath) NEXUS_FREE(handle->filePath);
    NEXUS_FREE(handle);
}

typedef struct {
    NEXUS_BOOL isLast;
    nexus_u8 metadataBlockType;
    nexus_u32 metadataBlockSize;
} ENIGMA_FLAC_METADATA_HEADER;

ENIGMA_FLAC_METADATA_HEADER enigma_metadata_header_info_create(const unsigned char *bytes, const size_t byteAmount) {
    ENIGMA_FLAC_METADATA_HEADER header = {0};

    if (!bytes || byteAmount != 4) {
        return header;
    }

    header.isLast = bytes[0] & 0x80u /*Highest 1 bit of 8 bit byte*/ ? NEXUS_TRUE : NEXUS_FALSE;
    header.metadataBlockType = bytes[0] & 0x7Fu /* Lowest 7 bits of 8 bit byte. */;

    /* High - Mid - Low combine because 3 bytes big-endian storage. */
    header.metadataBlockSize =
        (nexus_u32)bytes[1] << 16 |
        (nexus_u32)bytes[2] << 8 |
        (nexus_u32)bytes[3];

    printf("IsLast: %u, Type: %u, Length: %u\n",
           header.isLast, header.metadataBlockType, header.metadataBlockSize);

    return header;
}

void enigma_read_metadata_block_test(
    ENIGMA_FLAC_INFORMATION_HANDLE handle,
    char* errorBuffer,
    const size_t errorBufferSize)
{
    if (!handle) return;

    unsigned char readBuffer[4];
    size_t bytesRead = 0;

    nexus_file_read_at(handle->filePath, 4, 0,
                       readBuffer, sizeof readBuffer,
                       &bytesRead, errorBuffer, errorBufferSize);

    if (bytesRead != sizeof readBuffer) {
        printf("Short read: got %zu bytes\n", bytesRead);
        return;
    }

    ENIGMA_FLAC_METADATA_HEADER header =
        enigma_metadata_header_info_create(readBuffer, sizeof readBuffer);
}
