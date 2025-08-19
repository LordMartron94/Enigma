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

typedef struct ENIGMA_READ_FILE_TEST
{
    NEXUS_BOOL success;
    NEXUS_BOOL isFlac;
    const char* errorReason;
    nexus_u32 errorCode;
} ENIGMA_READ_FILE_TEST;

/* Public API */
const char* enigma_version_string(void);

ENIGMA_READ_FILE_TEST enigma_file_signature_is_flac(const char* filePath);

#ifdef __cplusplus
}
#endif

#endif /* ENIGMA_H */
