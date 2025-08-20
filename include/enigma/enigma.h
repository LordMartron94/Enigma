#ifndef ENIGMA_H
#define ENIGMA_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

    /* ---- Version (optional) -------------------------------------------------- */
#ifndef ENIGMA_VERSION_MAJOR
#define ENIGMA_VERSION_MAJOR 0
#endif
#ifndef ENIGMA_VERSION_MINOR
#define ENIGMA_VERSION_MINOR 0
#endif
#ifndef ENIGMA_VERSION_PATCH
#define ENIGMA_VERSION_PATCH 0
#endif

    enum { ENIGMA_MESSAGE_MAX = 128 };

    /* ---- Error codes --------------------------------------------------------- */
    typedef enum {
        ENIGMA_OK = 0,
        ENIGMA_INVALID_ARGUMENT = 1,
        ENIGMA_FUNCTION_ERROR = 2,
        ENIGMA_UNKNOWN_ERROR = 255
    } ENIGMA_ERROR_CODE;

    const char* enigma_version_string(void);

    typedef struct ENIGMA_FLAC_INFORMATION *ENIGMA_FLAC_INFORMATION_HANDLE;

    ENIGMA_ERROR_CODE enigma_flac_open(const char* filePath,
                                       ENIGMA_FLAC_INFORMATION_HANDLE* outHandle,
                                       char* errorBuffer, size_t errorBufferSize);

    void enigma_flac_close(ENIGMA_FLAC_INFORMATION_HANDLE handle);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* ENIGMA_H */
