#include "enigma/enigma.h"
#include "internal/enigma_internal.h"

#include <nexus/nexus.h>
#include <stdio.h>
#include <string.h>

/* ---- small internal helpers ------------------------------------------------
 */

static void enigma_message_copy(char* errorBuffer, const size_t errorBufferSize, const char* message) {
    size_t i = 0;
    if (!errorBuffer || errorBufferSize == 0) return;
    while (message[i] != '\0' && i + 1 < errorBufferSize) {
        errorBuffer[i] = message[i];
        ++i;
    }
    errorBuffer[i] = '\0';
}

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
    FILE* fileStream;

    if (outHandle) *outHandle = NULL;
    if (!filePath || !outHandle) {
        enigma_message_copy(errorBuffer, errorBufferSize, "Invalid argument");
        return ENIGMA_INVALID_ARGUMENT;
    }

    fileStream = NULL;
    const errno_t openError = fopen_s(&fileStream, filePath, "rb");
    if (openError != 0 || !fileStream) {
      char systemMessage[ENIGMA_MESSAGE_MAX];
      if (strerror_s(systemMessage, sizeof systemMessage, openError) == 0) {
        enigma_message_copy(errorBuffer, errorBufferSize, systemMessage);
      } else {
        enigma_message_copy(errorBuffer, errorBufferSize, "Cannot open file");
      }
      return ENIGMA_FUNCTION_ERROR;
    }

    const size_t bytesRead = fread(readBuffer, 1u, sizeof readBuffer, fileStream);
    fclose(fileStream);

    if (bytesRead < sizeof readBuffer) {
        enigma_message_copy(errorBuffer, errorBufferSize, "File shorter than signature");
        return ENIGMA_FUNCTION_ERROR;
    }

    if (memcmp(readBuffer, ENIGMA_FLAC_SIGNATURE, sizeof ENIGMA_FLAC_SIGNATURE) != 0) {
        enigma_message_copy(errorBuffer, errorBufferSize, "Not a FLAC file");
        return ENIGMA_INVALID_ARGUMENT;
    }

    /* Allocate and populate handle */
    {
      ENIGMA_FLAC_INFORMATION_HANDLE handle =
            NEXUS_ALLOC(sizeof *handle);
        if (!handle) {
            enigma_message_copy(errorBuffer, errorBufferSize, "Out of memory");
            return ENIGMA_FUNCTION_ERROR;
        }

        handle->filePath = nexus_string_duplicate(filePath);
        if (!handle->filePath) {
            NEXUS_FREE(handle);
            enigma_message_copy(errorBuffer, errorBufferSize, "Out of memory");
            return ENIGMA_FUNCTION_ERROR;
        }

        *outHandle = handle;
    }

    if (errorBuffer && errorBufferSize) errorBuffer[0] = '\0';
    return ENIGMA_OK;
}

void enigma_flac_close(ENIGMA_FLAC_INFORMATION_HANDLE handle) {
    if (!handle) return;
    if (handle->filePath) NEXUS_FREE(handle->filePath);
    NEXUS_FREE(handle);
}
