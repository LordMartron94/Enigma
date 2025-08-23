#ifndef ENIGMA_H
#define ENIGMA_H

#ifdef __cplusplus
extern "C" {
#endif

#include <nexus/nexus.h>

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

    const char* enigma_version_string(void);

    typedef struct ENIGMA_FLAC_INFORMATION *ENIGMA_FLAC_INFORMATION_HANDLE;

    NEXUS_ERROR_CODE  enigma_flac_open(const char* filePath, ENIGMA_FLAC_INFORMATION_HANDLE* outHandle, char* errorBuffer, size_t errorBufferSize);
    void              enigma_flac_close(ENIGMA_FLAC_INFORMATION_HANDLE handle);
    nexus_u32         enigma_flac_samplerate_get(ENIGMA_FLAC_INFORMATION_HANDLE handle);
    nexus_u8          enigma_flac_channels_get(ENIGMA_FLAC_INFORMATION_HANDLE handle);
    nexus_u8          enigma_flac_bit_depth_get(ENIGMA_FLAC_INFORMATION_HANDLE handle);
    float_real        enigma_flac_duration_seconds_get(ENIGMA_FLAC_INFORMATION_HANDLE handle);
    float_real        enigma_flac_duration_milliseconds_get(ENIGMA_FLAC_INFORMATION_HANDLE handle);
    nexus_u16         enigma_flac_last_metadata_block_end_offset_get(ENIGMA_FLAC_INFORMATION_HANDLE handle);
    void              enigma_flac_frame_header_parse_test(ENIGMA_FLAC_INFORMATION_HANDLE handle);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* ENIGMA_H */
