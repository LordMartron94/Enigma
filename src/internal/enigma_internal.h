#ifndef ENIGMA_INTERNAL_H
#define ENIGMA_INTERNAL_H

#include <nexus/nexus.h>

/* Hex codes: FLAC 'fLaC' */
static const unsigned char ENIGMA_FLAC_SIGNATURE[4] = { 0x66, 0x4C, 0x61, 0x43 };

typedef struct ENIGMA_FLAC_INFORMATION {
  char*       filePath;       /* owning pointer to path string */

  nexus_u64   totalSamples;   /* 36-bit in spec -> 64 here for headroom */

  nexus_u32   sampleRate;     /* 20-bit in spec (1..1,048,575) */
  nexus_u32   minFrameSize;   /* 24-bit; 0 = unknown */
  nexus_u32   maxFrameSize;   /* 24-bit; 0 = unknown */

  nexus_u16   minBlockSize;   /* 16-bit */
  nexus_u16   maxBlockSize;   /* 16-bit */

  nexus_u8    numChannels;    /* 1..8 */
  nexus_u8    bitsPerSample;  /* 4..32 */

  unsigned char md5Hash[16];  /* store the 16 bytes inline, not as a pointer */
} ENIGMA_FLAC_INFORMATION;

static float_real enigma_flac_duration_seconds_get(const ENIGMA_FLAC_INFORMATION *info) {
  return (float_real)info->totalSamples / (float_real)info->sampleRate;
}

static float_real enigma_flac_duration_milliseconds_get(const ENIGMA_FLAC_INFORMATION *info) {
  return enigma_flac_duration_seconds_get(info) * 1000;
}

#endif /* ENIGMA_INTERNAL_H */
