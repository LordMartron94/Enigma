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
// ReSharper disable once CppParameterMayBeConst
static NEXUS_ERROR_CODE enigma__validate_flac_signature( NEXUS_FILE_INFORMATION_HANDLE fileHandle,
                                                         char* errorBuffer,
                                                         const size_t errorBufferSize)
{
    unsigned char signature[ENIGMA_FLAC_SIGNATURE_BYTES_SIZE];
    if (!nexus_file_reader_consume(fileHandle, ENIGMA_FLAC_SIGNATURE_BYTES_SIZE, signature, ENIGMA_FLAC_SIGNATURE_BYTES_SIZE, NULL, errorBuffer, errorBufferSize)) {
        return NEXUS_FUNCTION_ERROR;
    }
    if (memcmp(signature, ENIGMA_FLAC_SIGNATURE, ENIGMA_FLAC_SIGNATURE_BYTES_SIZE) != 0) {
        nexus_string_message_copy(errorBuffer, errorBufferSize, "Not a FLAC file");
        return NEXUS_INVALID_ARGUMENT;
    }
    return NEXUS_OK;
}

/* Allocate and initialize the handle + duplicate path. */
static NEXUS_ERROR_CODE enigma__alloc_handle( const char *filePath,
                                              ENIGMA_FLAC_INFORMATION_HANDLE *outHandle,
                                              // ReSharper disable once CppParameterMayBeConst
                                              NEXUS_FILE_INFORMATION_HANDLE fileHandle,
                                              const char *errorBuffer,
                                              const size_t errorBufferSize)
{
    (void)errorBuffer; (void)errorBufferSize;

    // ReSharper disable once CppLocalVariableMayBeConst
    ENIGMA_FLAC_INFORMATION_HANDLE handle = NEXUS_ALLOC(sizeof *handle);
    if (!handle) return NEXUS_FUNCTION_ERROR;
    memset(handle, 0, sizeof *handle);

    handle->filePath = nexus_string_duplicate(filePath);
    if (!handle->filePath) {
        NEXUS_FREE(handle);
        return NEXUS_FUNCTION_ERROR;
    }

    handle->fileInformationHandle = fileHandle;

    *outHandle = handle;
    return NEXUS_OK;
}

/* Parse STREAMINFO payload (34 bytes) into handle fields. */
// ReSharper disable once CppParameterMayBeConst
static NEXUS_ERROR_CODE enigma__parse_streaminfo(ENIGMA_FLAC_INFORMATION_HANDLE handle,
                                                  const unsigned char streamInfoPayload[34])
{
    handle->minBlockSize = (nexus_u16)streamInfoPayload[0] << 8 | (nexus_u16)streamInfoPayload[1];
    handle->maxBlockSize = (nexus_u16)streamInfoPayload[2] << 8 | (nexus_u16)streamInfoPayload[3];

    handle->minFrameSize =
        (nexus_u32)streamInfoPayload[4] << 16 |
        (nexus_u32)streamInfoPayload[5] << 8  |
        (nexus_u32)streamInfoPayload[6];
    handle->maxFrameSize =
        (nexus_u32)streamInfoPayload[7] << 16 |
        (nexus_u32)streamInfoPayload[8] << 8  |
        (nexus_u32)streamInfoPayload[9];

    const nexus_u64 packed =
        (nexus_u64)streamInfoPayload[10] << 56 |
        (nexus_u64)streamInfoPayload[11] << 48 |
        (nexus_u64)streamInfoPayload[12] << 40 |
        (nexus_u64)streamInfoPayload[13] << 32 |
        (nexus_u64)streamInfoPayload[14] << 24 |
        (nexus_u64)streamInfoPayload[15] << 16 |
        (nexus_u64)streamInfoPayload[16] << 8  |
        (nexus_u64)streamInfoPayload[17];

    handle->sampleRate    = (nexus_u32)(packed >> 44 & 0xFFFFFULL);
    handle->numChannels   = (nexus_u8) ((packed >> 41 & 0x7) + 1);
    handle->bitsPerSample = (nexus_u8) ((packed >> 36 & 0x1F) + 1);
    handle->totalSamples  = packed & 0xFFFFFFFFFULL;

    memcpy(handle->md5Hash, &streamInfoPayload[18], 16);

    enigma__streaminfo_debug_print(handle, stdout);
    return NEXUS_OK;
}

static NEXUS_ERROR_CODE enigma__stream_info_handle(
                           const ENIGMA_FLAC_METADATA_HEADER header,
                           // ReSharper disable once CppParameterMayBeConst
                           ENIGMA_FLAC_INFORMATION_HANDLE informationHandle,
                           char *errorBuffer, const size_t errorBufferSize) {
  if (header.metadataBlockSize != 34u) {
    nexus_string_message_format_copy(errorBuffer, errorBufferSize,
                                     "Invalid STREAMINFO size: %u",
                                     (unsigned)header.metadataBlockSize);
    enigma_flac_close(informationHandle);
    return NEXUS_FUNCTION_ERROR;
  }

  unsigned char payload[34];
  if (!nexus_file_reader_consume(informationHandle->fileInformationHandle, 34,
                                 &payload, 34, NULL, errorBuffer,
                                 errorBufferSize)) {
    enigma_flac_close(informationHandle);
    return NEXUS_FUNCTION_ERROR;
  }

  enigma__parse_streaminfo(informationHandle, payload);
  return NEXUS_OK;
}

// ReSharper disable once CppParameterMayBeConst
static NEXUS_ERROR_CODE enigma__metadata_parse(ENIGMA_FLAC_INFORMATION_HANDLE handle,
                                               char* errorBuffer,
                                               const size_t errorBufferSize)
{
    NEXUS_BOOL streamInfoFound = NEXUS_FALSE;

    for (;;) {
        unsigned char headerBytes[ENIGMA_FLAC_BLOCK_HEADER_BYTES_SIZE];

        if (nexus_file_reader_consume(handle->fileInformationHandle,
                                      ENIGMA_FLAC_BLOCK_HEADER_BYTES_SIZE,
                                      headerBytes,
                                      ENIGMA_FLAC_BLOCK_HEADER_BYTES_SIZE,
                                      NULL,
                                      errorBuffer, errorBufferSize) == NEXUS_FALSE) {
            enigma_flac_close(handle);
            return NEXUS_FUNCTION_ERROR;
        }

        const ENIGMA_FLAC_METADATA_HEADER header = enigma_metadata_header_info_create(headerBytes);
        const nexus_u32 blockLength = header.metadataBlockSize;

        if (header.metadataBlockType == 0) {
            if (streamInfoFound == NEXUS_TRUE) {
                nexus_string_message_copy(errorBuffer, errorBufferSize, "WTF??? Double stream info!!!!");
                enigma_flac_close(handle);
                return NEXUS_FUNCTION_ERROR;
            }

            const NEXUS_ERROR_CODE r = enigma__stream_info_handle(header, handle, errorBuffer, errorBufferSize);
            if (r != NEXUS_OK) return r;

            streamInfoFound = NEXUS_TRUE;
        } else {
            if (nexus_file_reader_consume(handle->fileInformationHandle,
                                          blockLength,
                                          NULL, 0,
                                          NULL,
                                          errorBuffer, errorBufferSize) == NEXUS_FALSE) {
                enigma_flac_close(handle);
                return NEXUS_FUNCTION_ERROR;
            }
        }

        if (header.isLast) {
            handle->lastMetadataBlockEndOffset = nexus_file_reader_index_get(handle->fileInformationHandle);
            break;
        }
    }

    if (streamInfoFound == NEXUS_FALSE) {
        nexus_string_message_copy(errorBuffer, errorBufferSize,
                                  "Invalid FLAC file -- no STREAMINFO block");
        enigma_flac_close(handle);
        return NEXUS_FUNCTION_ERROR;
    }

    return NEXUS_OK;
}

/* ---- public API ---------------------------------------------------------- */

NEXUS_ERROR_CODE enigma_flac_open(const char *filePath,
                                  ENIGMA_FLAC_INFORMATION_HANDLE *outHandle,
                                  char *errorBuffer,
                                  const size_t errorBufferSize) {
  NEXUS_FILE_INFORMATION_HANDLE fileHandle = NULL;
  const NEXUS_BOOL openSuccess = nexus_file_information_open(
      filePath, &fileHandle, errorBuffer, errorBufferSize);

  if (openSuccess == NEXUS_FALSE) {
    return NEXUS_FUNCTION_ERROR;
  }

  nexus_errors_ok(errorBuffer, errorBufferSize);
  if (outHandle)
    *outHandle = NULL;
  if (!filePath || !outHandle)
    return nexus_errors_invalid_argument(errorBuffer, errorBufferSize);

  {
    const NEXUS_ERROR_CODE s = enigma__validate_flac_signature(
        fileHandle, errorBuffer, errorBufferSize);
    if (s != NEXUS_OK)
      return s;
  }

  ENIGMA_FLAC_INFORMATION_HANDLE handle = NULL;
  {
    const NEXUS_ERROR_CODE a = enigma__alloc_handle(
        filePath, &handle, fileHandle, errorBuffer, errorBufferSize);
    if (a != NEXUS_OK)
      return a;
  }

  {
    const NEXUS_ERROR_CODE r =
        enigma__metadata_parse(handle, errorBuffer, errorBufferSize);
    if (r != NEXUS_OK)
      return r;
  }

  *outHandle = handle;
  nexus_errors_ok(errorBuffer, errorBufferSize);
  return NEXUS_OK;
}

// ReSharper disable once CppParameterMayBeConst
void enigma_flac_close(ENIGMA_FLAC_INFORMATION_HANDLE handle) {
    if (!handle) return;
    if (handle->filePath) NEXUS_FREE(handle->filePath);
    if (handle->fileInformationHandle) nexus_file_information_close(handle->fileInformationHandle);
    NEXUS_FREE(handle);
}

nexus_u32 enigma_flac_samplerate_get(const ENIGMA_FLAC_INFORMATION_CHANDLE handle) {
    if (!handle) return 0;
    return handle->sampleRate;
}

nexus_u8 enigma_flac_channels_get(const ENIGMA_FLAC_INFORMATION_CHANDLE handle) {
    if (!handle) return 0;
    return handle->numChannels;
}

nexus_u8 enigma_flac_bit_depth_get(const ENIGMA_FLAC_INFORMATION_CHANDLE handle) {
    if (!handle) return 0;
    return handle->bitsPerSample;
}

nexus_u16 enigma_flac_last_metadata_block_end_offset_get(const ENIGMA_FLAC_INFORMATION_CHANDLE handle) {
    if (!handle) return 0;
    const nexus_u64 v = handle->lastMetadataBlockEndOffset;
    return v & 0xFFFFu;
}

float_real enigma_flac_duration_seconds_get(const ENIGMA_FLAC_INFORMATION_CHANDLE handle) {
    return (float_real)handle->totalSamples / (float_real)handle->sampleRate;
}

float_real enigma_flac_duration_milliseconds_get(const ENIGMA_FLAC_INFORMATION_CHANDLE handle) {
    return enigma_flac_duration_seconds_get(handle) * 1000;
}
