#include "enigma/enigma.h"

#include <stdio.h>
#include <string.h>
#include <nexus/nexus.h>

const char* enigma_version_string(void) {
    /* Keep this tiny and compile-time only */
    return "0.0.0";
}

/* ------------------------------------------------------------------------- */
/* Helpers                                                                   */
/* ------------------------------------------------------------------------- */

typedef struct {
    ENIGMA_SIGNATURE       code;
    const unsigned char*   bytes;
    nexus_u8               sizeBytes;
} ENIGMA_SIGNATURE_INFO;

/* Hex codes to be found here: https://en.wikipedia.org/wiki/List_of_file_signatures */
static const unsigned char SIGNATURE_FLAC[] = { 0x66, 0x4C, 0x61, 0x43 };

static const ENIGMA_SIGNATURE_INFO enigmaSignatureMap[] = {
    { ENIGMA_SIGNATURE_FLAC, SIGNATURE_FLAC, (nexus_u8)sizeof SIGNATURE_FLAC },
};

static ENIGMA_SIGNATURE_INFO enigma_get_signature_info(ENIGMA_SIGNATURE sig)
{
    {
        for (size_t i = 0; i < sizeof enigmaSignatureMap / sizeof enigmaSignatureMap[0]; ++i) {
            if (enigmaSignatureMap[i].code == sig) {
                return enigmaSignatureMap[i];
            }
        }
    }

    ENIGMA_SIGNATURE_INFO nullSignature;
    nullSignature.sizeBytes = 0;
    nullSignature.bytes = NULL;
    nullSignature.code = _INTERNAL_UNKNOWN;
    return nullSignature;
}

static ENIGMA_SIGNATURE_READING_STATUS make_status(
    ENIGMA_SIGNATURE_STATUS_CODE code,
    const char* message)
{
    ENIGMA_SIGNATURE_READING_STATUS status;
    status.code = code;

    if (message) {
        const errno_t err = strncpy_s(status.message,
                                sizeof status.message,
                                message,
                                _TRUNCATE);
        if (err != 0) {
            status.message[0] = '\0';
        }
    } else {
        status.message[0] = '\0';
    }

    return status;
}

static ENIGMA_READ_FILE_TEST make_result(NEXUS_BOOL signatureMatches,
                                         ENIGMA_SIGNATURE_STATUS_CODE code,
                                         const char* message)
{
    ENIGMA_READ_FILE_TEST r;
    r.status = make_status(code, message);
    r.signatureMatches = signatureMatches;
    return r;
}

static const char* enigma_status_code_to_string(const ENIGMA_SIGNATURE_STATUS_CODE code)
{
    switch (code) {
    case ENIGMA_STATUS_OK:                 return "OK";
    case ENIGMA_STATUS_INVALID_ARGUMENTS:  return "Invalid arguments";
    case ENIGMA_STATUS_OPEN_FAILED:        return "Open failed";
    case ENIGMA_STATUS_FILE_TOO_SHORT:     return "File too short";
    case ENIGMA_STATUS_SIGNATURE_MISMATCH: return "Signature mismatch";
    default:                               return "Unknown error";
    }
}

/* ------------------------------------------------------------------------- */
/* Formatter                                                                 */
/* ------------------------------------------------------------------------- */

void enigma_format_read_file_test(char* outBuffer, const size_t outSize,
                                  const ENIGMA_READ_FILE_TEST test)
{
    const char* base = enigma_status_code_to_string(test.status.code);
    const char* ctx  = test.status.message[0] != '\0' ? test.status.message : NULL;

    if (ctx) {
        snprintf(outBuffer, outSize,
                 "[status=%s (%u), context='%s', signatureMatches=%u]",
                 base, (unsigned)test.status.code, ctx, test.signatureMatches);
    } else {
        snprintf(outBuffer, outSize,
                 "[status=%s (%u), signatureMatches=%u]",
                 base, (unsigned)test.status.code, test.signatureMatches);
    }
}

/* ------------------------------------------------------------------------- */
/* Core function                                                             */
/* ------------------------------------------------------------------------- */

ENIGMA_READ_FILE_TEST enigma_file_signature_matches(const char* filePath, const ENIGMA_SIGNATURE signature)
{
    const ENIGMA_SIGNATURE_INFO signatureInfo = enigma_get_signature_info(signature);

    if (!filePath || !signatureInfo.bytes || signatureInfo.sizeBytes == 0 || signatureInfo.sizeBytes > 32) {
        return make_result(NEXUS_UNKNOWN,
                           ENIGMA_STATUS_INVALID_ARGUMENTS,
                           "Invalid arguments passed to enigma_file_signature_matches");
    }

    unsigned char readBuffer[signatureInfo.sizeBytes];

    FILE* stream = NULL;
    const errno_t error = fopen_s(&stream, filePath, "rb");
    if (error != 0) {
        char sysMsg[ENIGMA_MESSAGE_MAX];
        if (strerror_s(sysMsg, sizeof sysMsg, error) == 0) {
            return make_result(NEXUS_UNKNOWN, ENIGMA_STATUS_OPEN_FAILED, sysMsg);
        }

        return make_result(NEXUS_UNKNOWN, ENIGMA_STATUS_OPEN_FAILED,
                           "Unknown system error");
    }

    const size_t bytesRead = fread(readBuffer, 1, signatureInfo.sizeBytes, stream);
    fclose(stream);

    if (bytesRead < signatureInfo.sizeBytes) {
        return make_result(NEXUS_UNKNOWN,
                           ENIGMA_STATUS_FILE_TOO_SHORT,
                           "File shorter than expected signature size");
    }

    if (memcmp(readBuffer, signatureInfo.bytes, signatureInfo.sizeBytes) == 0) {
        return make_result(NEXUS_TRUE, ENIGMA_STATUS_OK, NULL);
    }

    return make_result(NEXUS_FALSE,
                       ENIGMA_STATUS_SIGNATURE_MISMATCH,
                       NULL);
}
