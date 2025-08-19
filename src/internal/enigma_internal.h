#ifndef ENIGMA_INTERNAL_H
#define ENIGMA_INTERNAL_H

/* Hex codes: FLAC 'fLaC' */
static const unsigned char ENIGMA_FLAC_SIGNATURE[4] = { 0x66, 0x4C, 0x61, 0x43 };

struct ENIGMA_FLAC_INFORMATION {
  char* filePath;
};

#endif /* ENIGMA_INTERNAL_H */
