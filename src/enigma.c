#include "enigma/enigma.h"

#include <stdio.h>
#include <string.h>
#include <nexus/nexus.h>

const char* enigma_version_string(void) {
    /* Keep this tiny and compile-time only */
    return "0.0.0";
}

#define FLAC_SIGNATURE_SIZE_BYTES 4

static const unsigned char flacSignature[4] = {0x66, 0x4C, 0x61, 0x43};

static ENIGMA_READ_FILE_TEST make_result(const NEXUS_BOOL success,
                                         const NEXUS_BOOL isFlac,
                                         const char* reason,
                                         const nexus_u32 code)
{
    ENIGMA_READ_FILE_TEST r;
    r.success     = success;
    r.isFlac      = isFlac;
    r.errorReason = reason;
    r.errorCode   = code;
    return r;
}

ENIGMA_READ_FILE_TEST enigma_file_signature_is_flac(const char* filePath)
{
    unsigned char readBuffer[FLAC_SIGNATURE_SIZE_BYTES];
    nexus_u32 readSize = sizeof readBuffer;

    FILE* stream = NULL;
    errno_t error = fopen_s(&stream, filePath, "rb");

    if (error != 0)
    {
        return make_result(NEXUS_FALSE, NEXUS_UNKNOWN,
                      "Something went wrong while opening the file stream.", error);
    }

    fread(readBuffer, 1, readSize, stream);
    fclose(stream);

    if (memcmp(readBuffer, flacSignature, FLAC_SIGNATURE_SIZE_BYTES) == 0)
    {
        return make_result(NEXUS_TRUE, NEXUS_TRUE, "", 0);
    }

    return make_result(NEXUS_TRUE, NEXUS_FALSE, "", 0);
}
