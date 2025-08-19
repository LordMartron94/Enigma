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

ENIGMA_READ_FILE_TEST enigma_file_signature_matches(const char* filePath,
                                                    const unsigned char* signature,
                                                    const nexus_u32 signatureSizeBytes)
{
    if (!filePath || !signature || signatureSizeBytes == 0 || signatureSizeBytes > 32) {
        return make_result(NEXUS_UNKNOWN,
                           ENIGMA_STATUS_INVALID_ARGUMENTS,
                           "Invalid arguments passed to enigma_file_signature_matches");
    }

    unsigned char readBuffer[32];

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

    const size_t bytesRead = fread(readBuffer, 1, signatureSizeBytes, stream);
    fclose(stream);

    if (bytesRead < signatureSizeBytes) {
        return make_result(NEXUS_UNKNOWN,
                           ENIGMA_STATUS_FILE_TOO_SHORT,
                           "File shorter than expected signature size");
    }

    if (memcmp(readBuffer, signature, signatureSizeBytes) == 0) {
        return make_result(NEXUS_TRUE, ENIGMA_STATUS_OK, NULL);
    }

    return make_result(NEXUS_FALSE,
                       ENIGMA_STATUS_SIGNATURE_MISMATCH,
                       NULL);
}
