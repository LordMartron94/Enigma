#ifndef ENIGMA_H
#define ENIGMA_H

#ifdef __cplusplus
extern "C" {
#endif

#include <nexus/nexus.h>

/* Version macros (optional) */
#ifndef ENIGMA_VERSION_MAJOR
#define ENIGMA_VERSION_MAJOR 0
#endif
#ifndef ENIGMA_VERSION_MINOR
#define ENIGMA_VERSION_MINOR 0
#endif
#ifndef ENIGMA_VERSION_PATCH
#define ENIGMA_VERSION_PATCH 0
#endif

#define ENIGMA_FORMAT_MAX 256
#define ENIGMA_MESSAGE_MAX 128

typedef enum ENIGMA_SIGNATURE_STATUS_CODE {
    ENIGMA_STATUS_OK = 0,
    ENIGMA_STATUS_INVALID_ARGUMENTS = 1,
    ENIGMA_STATUS_OPEN_FAILED = 2,
    ENIGMA_STATUS_FILE_TOO_SHORT = 3,
    ENIGMA_STATUS_SIGNATURE_MISMATCH = 4,
    ENIGMA_STATUS_UNKNOWN_ERROR = 255
} ENIGMA_SIGNATURE_STATUS_CODE;

typedef struct ENIGMA_SIGNATURE_READING_STATUS {
    ENIGMA_SIGNATURE_STATUS_CODE code;
    char message[ENIGMA_MESSAGE_MAX];
} ENIGMA_SIGNATURE_READING_STATUS;

typedef struct ENIGMA_READ_FILE_TEST
{
    ENIGMA_SIGNATURE_READING_STATUS status;
    NEXUS_BOOL signatureMatches;
} ENIGMA_READ_FILE_TEST;

/* Public API */
const char* enigma_version_string(void);

    void enigma_format_read_file_test(char* outBuffer, size_t outSize,
                                      ENIGMA_READ_FILE_TEST test);

ENIGMA_READ_FILE_TEST enigma_file_signature_matches(const char* filePath, const unsigned char* signature, nexus_u32 signatureSizeBytes);

#ifdef __cplusplus
}
#endif

#endif /* ENIGMA_H */
