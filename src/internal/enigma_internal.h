#ifndef ENIGMA_INTERNAL_H
#define ENIGMA_INTERNAL_H

#include <nexus/nexus.h>

/* Hex codes: FLAC 'fLaC' */
static const unsigned char ENIGMA_FLAC_SIGNATURE[4] = { 0x66, 0x4C, 0x61, 0x43 };

typedef struct {
  NEXUS_BOOL isLast;
  nexus_u8 metadataBlockType;
  nexus_u32 metadataBlockSize;
} ENIGMA_FLAC_METADATA_HEADER;

inline ENIGMA_FLAC_METADATA_HEADER enigma_metadata_header_info_create(
    const unsigned char *bytes, const size_t byteAmount)
{
  ENIGMA_FLAC_METADATA_HEADER header = (ENIGMA_FLAC_METADATA_HEADER){0};
  if (!bytes || byteAmount != 4) return header;

  header.isLast            = bytes[0] & 0x80u ? NEXUS_TRUE : NEXUS_FALSE; /* MSB */
  header.metadataBlockType = (nexus_u8)(bytes[0] & 0x7Fu); /* low 7 bits */
  header.metadataBlockSize =
      (nexus_u32)bytes[1] << 16 | /* High */
      (nexus_u32)bytes[2] << 8 | /* Mid */
      (nexus_u32)bytes[3]; /*Low */

  return header;
}

typedef struct ENIGMA_FLAC_INFORMATION {
  char*       filePath;       /* owning pointer to path string */

  nexus_u64   totalSamples;   /* 36-bit in spec -> 64 here for headroom */
  nexus_u64   lastMetadataBlockEndOffset;

  nexus_u32   sampleRate;     /* 20-bit in spec (1..1,048,575) */
  nexus_u32   minFrameSize;   /* 24-bit; 0 = unknown */
  nexus_u32   maxFrameSize;   /* 24-bit; 0 = unknown */

  nexus_u16   minBlockSize;   /* 16-bit */
  nexus_u16   maxBlockSize;   /* 16-bit */

  nexus_u8    numChannels;    /* 1..8 */
  nexus_u8    bitsPerSample;  /* 4..32 */

  unsigned char md5Hash[16];
} ENIGMA_FLAC_INFORMATION;

typedef enum {
  ENIGMA_CHANNEL_FRONT_LEFT = 0,
  ENIGMA_CHANNEL_FRONT_RIGHT = 1,
  ENIGMA_CHANNEL_FRONT_CENTER = 2,
  ENIGMA_CHANNEL_LOW_FREQUENCY_EFFECTS = 3,
  ENIGMA_CHANNEL_BACK_LEFT = 4,
  ENIGMA_CHANNEL_BACK_RIGHT = 5,
  ENIGMA_CHANNEL_FRONT_LEFT_OF_CENTER = 6,
  ENIGMA_CHANNEL_FRONT_RIGHT_OF_CENTER = 7,
  ENIGMA_CHANNEL_BACK_CENTER = 8,
  ENIGMA_CHANNEL_SIDE_LEFT = 9,
  ENIGMA_CHANNEL_SIDE_RIGHT = 10,
  ENIGMA_CHANNEL_TOP_CENTER = 11,
  ENIGMA_CHANNEL_TOP_FRONT_LEFT = 12,
  ENIGMA_CHANNEL_TOP_FRONT_CENTER = 13,
  ENIGMA_CHANNEL_TOP_FRONT_RIGHT = 14,
  ENIGMA_CHANNEL_TOP_REAR_LEFT = 15,
  ENIGMA_CHANNEL_TOP_REAR_CENTER = 16,
  ENIGMA_CHANNEL_TOP_REAR_RIGHT = 17
} ENIGMA_FLAC_CHANNEL_MAP;

#endif /* ENIGMA_INTERNAL_H */

typedef ENIGMA_FLAC_INFORMATION *ENIGMA_FLAC_INFORMATION_HANDLE;

void enigma__streaminfo_debug_print(ENIGMA_FLAC_INFORMATION_HANDLE handle, FILE *out);
