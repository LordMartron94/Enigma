/* Created by LordMartron on 23/08/2025. */

#include "enigma_internal.h"
#include <enigma/enigma.h>

/* Resync using the file-handle API (peek + advance). */
static NEXUS_BOOL enigma__flac_try_resync(NEXUS_FILE_INFORMATION_HANDLE fh,
                                          size_t max_bytes_to_scan,
                                          char* errorBuffer, const size_t errorBufferSize)
{
    unsigned char two[2];
    for (size_t i = 0; i < max_bytes_to_scan; ++i) {
        nexus_u64 got = 0;
        if (!nexus_file_scan(fh, 2u, two, 2u, &got, errorBuffer, errorBufferSize) || got != 2u)
            return NEXUS_FALSE;

        unsigned short w = two[0] << 8 | two[1];
        if ((unsigned)(w >> 2) == 0x3FFEu) {
            return NEXUS_TRUE; /* at sync */
        }

        /* advance one byte */
        got = 0;
        if (!nexus_file_consume(fh, 1u, NULL, 0u, &got, errorBuffer, errorBufferSize) || got != 1u)
            return NEXUS_FALSE;
    }
    return NEXUS_FALSE;
}

void enigma__streaminfo_debug_print(ENIGMA_FLAC_INFORMATION_HANDLE handle, FILE *out) {
  if (!handle) return;

  fprintf(out, "---- FLAC STREAMINFO ----\n");

  fprintf(out, "Blocks:       %u .. %u samples per block\n",
          (unsigned)handle->minBlockSize, (unsigned)handle->maxBlockSize);

  if (handle->minFrameSize == 0 && handle->maxFrameSize == 0) {
    fprintf(out, "Frames:       (size unknown)\n");
  } else {
    fprintf(out, "Frames:       %u .. %u bytes per frame\n",
            handle->minFrameSize, handle->maxFrameSize);
  }

  {
    const double sr_khz = handle->sampleRate / 1000.0;
    fprintf(out, "Sample rate:  %u Hz (%.3f kHz)\n", handle->sampleRate, sr_khz);
  }
  fprintf(out, "Channels:     %u\n", (unsigned)handle->numChannels);
  fprintf(out, "Bit depth:    %u bits per sample\n", (unsigned)handle->bitsPerSample);

  fprintf(out, "Total samples:" NEXUS_U64_FMT "\n", NEXUS_U64_CAST(handle->totalSamples));

  if (handle->sampleRate != 0) {
    const nexus_u64 minMs =
        (nexus_u64)handle->minBlockSize * 1000ULL / handle->sampleRate;
    const nexus_u64 maxMs =
        (nexus_u64)handle->maxBlockSize * 1000ULL / handle->sampleRate;
    fprintf(out, "Block time:   " NEXUS_U64_FMT " .. " NEXUS_U64_FMT " ms\n",
            NEXUS_U64_CAST(minMs), NEXUS_U64_CAST(maxMs));
  }

  fputs("MD5 (raw):    ", out);
  nexus_bytes_byte_array_hex_print(handle->md5Hash, 16, out);
  fputc('\n', out);

  fputs("-------------------------\n", out);
}

void enigma_flac_frame_header_parse_test(ENIGMA_FLAC_INFORMATION_HANDLE handle) {
    if (!handle) return;

    /* 1) Open file handle at file start (logical cursor at 0). */
    NEXUS_FILE_INFORMATION_HANDLE fh = NULL;
    char errorBuffer[ENIGMA_MESSAGE_MAX];
    if (!nexus_file_information_open(handle->filePath, &fh, errorBuffer, sizeof errorBuffer)) {
        printf("Open error: '%s'\n", errorBuffer);
        return;
    }

    /* 2) Jump to absolute end-of-metadata offset using a single consume from start. */
    const nexus_u64 audio_start = handle->lastMetadataBlockEndOffset;
    {
        nexus_u64 got = 0;
        if (!nexus_file_consume(fh, audio_start, NULL, 0u, &got, errorBuffer, sizeof errorBuffer) || got != audio_start) {
            printf("Seek error: '%s'\n", errorBuffer);
            nexus_file_information_close(fh);
            return;
        }
    }

    /* 3) Resync to the 14-bit FLAC sync (scan up to a generous window). */
    if (!enigma__flac_try_resync(fh, 65536 /* 64 KiB window */, errorBuffer, sizeof errorBuffer)) {
        /* Dump a few bytes to help diagnosis */
        unsigned char dump[8] = {0};
        nexus_u64 got = 0;
        if (nexus_file_scan(fh, 8u, dump, 8u, &got, errorBuffer, sizeof errorBuffer) && got == 8u) {
            printf("Could not find frame sync within lookahead window. Next bytes: "
                   "0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X\n",
                   dump[0],dump[1],dump[2],dump[3],dump[4],dump[5],dump[6],dump[7]);
        } else {
            printf("Could not find frame sync and failed to peek: '%s'\n", errorBuffer);
        }
        nexus_file_information_close(fh);
        return;
    }

    /* 4) Read first 4 header bytes (peek then advance). */
    unsigned char hdr4[4];
    {
        nexus_u64 got = 0;
        if (!nexus_file_scan(fh, 4u, hdr4, 4u, &got, errorBuffer, sizeof errorBuffer) || got != 4u) {
            printf("Read error (peek 4 bytes): '%s'\n", errorBuffer);
            nexus_file_information_close(fh);
            return;
        }
        got = 0;
        if (!nexus_file_consume(fh, 4u, NULL, 0u, &got, errorBuffer, sizeof errorBuffer) || got != 4u) {
            printf("Read error (consume 4 bytes): '%s'\n", errorBuffer);
            nexus_file_information_close(fh);
            return;
        }
    }

    const unsigned char firstByte  = hdr4[0];
    const unsigned char secondByte = hdr4[1];
    const unsigned char thirdByte  = hdr4[2];
    const unsigned char fourthByte = hdr4[3];

    printf("Raw header bytes: 0x%02X 0x%02X\n", firstByte, secondByte);

    unsigned short syncWord = firstByte << 8 | secondByte;
    syncWord >>= 2;
    if (syncWord != 0x3FFE) {
        printf("Invalid sync code!\n");
        nexus_file_information_close(fh);
        return;
    }

    const nexus_u8 reserved_after_sync = secondByte >> 1 & 0x01u;
    if (reserved_after_sync != 0) {
        printf("Frame header reserved bit (after sync) not zero!\n");
        nexus_file_information_close(fh);
        return;
    }

    const nexus_u8 blockingStrategy = secondByte & 0x01u;
    printf("Block strategy: %u\n", blockingStrategy);

    const unsigned char blockSizeCode = (thirdByte & 0xF0u) >> 4;
    nexus_u32 blockSize = 0;
    switch (blockSizeCode) {
        case 0x0: printf("Invalid block size code (reserved)\n"); break;
        case 0x1: blockSize = 192;   break;
        case 0x2: blockSize = 576;   break;
        case 0x3: blockSize = 1152;  break;
        case 0x4: blockSize = 2304;  break;
        case 0x5: blockSize = 4608;  break;
        case 0x6: /* uncommon, 8-bit follows (+1) */ break;
        case 0x7: /* uncommon, 16-bit follows (+1) */ break;
        default:  blockSize = 1u << blockSizeCode; break;
    }

    const unsigned char sampleRateCode = thirdByte & 0x0Fu;
    nexus_u32 sampleRate = 0;
    switch (sampleRateCode) {
        case 0x0: sampleRate = handle->sampleRate; break;
        case 0x1: sampleRate = 88200;   break;
        case 0x2: sampleRate = 176400;  break;
        case 0x3: sampleRate = 192000;  break;
        case 0x4: sampleRate = 8000;    break;
        case 0x5: sampleRate = 16000;   break;
        case 0x6: sampleRate = 22050;   break;
        case 0x7: sampleRate = 24000;   break;
        case 0x8: sampleRate = 32000;   break;
        case 0x9: sampleRate = 44100;   break;
        case 0xA: sampleRate = 48000;   break;
        case 0xB: sampleRate = 96000;   break;
        case 0xC: break;
        case 0xD: break;
        case 0xE: break;
        default:
            printf("Invalid/forbidden sample rate code\n");
            break;
    }

    const unsigned char channelAssignment = (fourthByte & 0xF0u) >> 4;
    const unsigned char bitsPerSampleCode = fourthByte >> 1 & 0x07u;
    const unsigned char reserved0 = fourthByte & 0x01u;
    if (reserved0 != 0) {
        printf("Frame header reserved bit (LSB of 4th byte) not zero!\n");
        nexus_file_information_close(fh);
        return;
    }

    if (blockSize != 0) printf("Block size: %u\n", blockSize);
    if (sampleRate != 0) printf("Sample rate: %u Hz\n", sampleRate);
    switch (channelAssignment) {
        case 0x0: printf("Channels: mono\n"); break;
        case 0x1: printf("Channels: stereo (L,R)\n"); break;
        case 0x2: printf("Channels: 3ch (L,R,C)\n"); break;
        case 0x3: printf("Channels: 4ch (FL,FR,BL,BR)\n"); break;
        case 0x4: printf("Channels: 5ch (FL,FR,FC,SL,SR)\n"); break;
        case 0x5: printf("Channels: 6ch (FL,FR,FC,LFE,SL,SR)\n"); break;
        case 0x6: printf("Channels: 7ch (FL,FR,FC,LFE,BC,SL,SR)\n"); break;
        case 0x7: printf("Channels: 8ch (FL,FR,FC,LFE,BL,BR,SL,SR)\n"); break;
        case 0x8: printf("Channels: stereo stored as left+side\n"); break;
        case 0x9: printf("Channels: stereo stored as right+side\n"); break;
        case 0xA: printf("Channels: stereo stored as mid+side\n"); break;
        default:  printf("Channels: reserved/invalid channel assignment\n"); break;
    }
    switch (bitsPerSampleCode) {
        case 0x0: printf("Bit depth: from STREAMINFO (%u bits)\n", handle->bitsPerSample); break;
        case 0x1: printf("Bit depth: 8 bits\n");  break;
        case 0x2: printf("Bit depth: 12 bits\n"); break;
        case 0x3: printf("Bit depth: reserved\n"); break;
        case 0x4: printf("Bit depth: 16 bits\n"); break;
        case 0x5: printf("Bit depth: 20 bits\n"); break;
        case 0x6: printf("Bit depth: 24 bits\n"); break;
        case 0x7: printf("Bit depth: 32 bits\n"); break;
        default:  printf("Bit depth: invalid bit depth\n");
    }

    uint8_t crc8 = 0x00;
    crc8 = nexus_validation_crc8_update(crc8, firstByte);
    crc8 = nexus_validation_crc8_update(crc8, secondByte);
    crc8 = nexus_validation_crc8_update(crc8, thirdByte);
    crc8 = nexus_validation_crc8_update(crc8, fourthByte);

    unsigned char utf8First = 0;
    {
        nexus_u64 got = 0;
        if (!nexus_file_consume(fh, 1u, &utf8First, 1u, &got, errorBuffer, sizeof errorBuffer) || got != 1u) {
            printf("Read error (UTF-8 coded number first byte): '%s'\n", errorBuffer);
            nexus_file_information_close(fh);
            return;
        }
    }
    crc8 = nexus_validation_crc8_update(crc8, utf8First);

    int leadingOnes = 0;
    for (int b = 7; b >= 0; --b) {
        if (((utf8First >> b) & 1u) == 1u) leadingOnes++;
        else break;
    }
    int utf8Len = (leadingOnes == 0) ? 1 : leadingOnes;
    if (utf8Len > 6 || utf8Len < 1) {
        printf("Invalid UTF-8 coded integer length\n");
        nexus_file_information_close(fh);
        return;
    }

    nexus_u64 codedNumber = 0;
    if (utf8Len == 1) {
        codedNumber = (nexus_u64)(utf8First & 0x7Fu);
    } else {
        codedNumber = (nexus_u64)(utf8First & ((1u << (7 - utf8Len)) - 1u));
        for (int i = 1; i < utf8Len; ++i) {
            unsigned char cont = 0;
            nexus_u64 got = 0;
            if (!nexus_file_consume(fh, 1u, &cont, 1u, &got, errorBuffer, sizeof errorBuffer) || got != 1u) {
                printf("Read error (UTF-8 continuation): '%s'\n", errorBuffer);
                nexus_file_information_close(fh);
                return;
            }
            if ((cont & 0xC0u) != 0x80u) {
                printf("Invalid UTF-8 continuation byte in frame header\n");
                nexus_file_information_close(fh);
                return;
            }
            crc8 = nexus_validation_crc8_update(crc8, cont);
            codedNumber = (codedNumber << 6) | (nexus_u64)(cont & 0x3Fu);
        }
    }

    if (blockingStrategy == 0) {
        printf("Frame number (fixed-blocksize stream): %llu\n", NEXUS_U64_CAST(codedNumber));
    } else {
        printf("Starting sample number (variable-blocksize stream): %llu\n", NEXUS_U64_CAST(codedNumber));
    }

    if (blockSizeCode == 0x6) {
        unsigned char size8 = 0;
        nexus_u64 got = 0;
        if (!nexus_file_consume(fh, 1u, &size8, 1u, &got, errorBuffer, sizeof errorBuffer) || got != 1u) {
            printf("Read error (8-bit block size): '%s'\n", errorBuffer);
            nexus_file_information_close(fh);
            return;
        }
        crc8 = nexus_validation_crc8_update(crc8, size8);
        blockSize = (nexus_u32)size8 + 1u;
        printf("Block size (uncommon, 8-bit): %u\n", blockSize);
    } else if (blockSizeCode == 0x7) {
        unsigned char sizeHiLo[2];
        nexus_u64 got = 0;
        if (!nexus_file_consume(fh, 2u, sizeHiLo, 2u, &got, errorBuffer, sizeof errorBuffer) || got != 2u) {
            printf("Read error (16-bit block size): '%s'\n", errorBuffer);
            nexus_file_information_close(fh);
            return;
        }
        crc8 = nexus_validation_crc8_update(crc8, sizeHiLo[0]);
        crc8 = nexus_validation_crc8_update(crc8, sizeHiLo[1]);
        blockSize = (nexus_u32)((sizeHiLo[0] << 8) | sizeHiLo[1]) + 1u;
        printf("Block size (uncommon, 16-bit): %u\n", blockSize);
    }

    if (sampleRateCode == 0xC) {
        unsigned char sr_khz = 0;
        nexus_u64 got = 0;
        if (!nexus_file_consume(fh, 1u, &sr_khz, 1u, &got, errorBuffer, sizeof errorBuffer) || got != 1u) {
            printf("Read error (8-bit sample rate kHz): '%s'\n", errorBuffer);
            nexus_file_information_close(fh);
            return;
        }
        crc8 = nexus_validation_crc8_update(crc8, sr_khz);
        sampleRate = (nexus_u32)sr_khz * 1000u;
        printf("Sample rate (uncommon, 8-bit kHz): %u Hz\n", sampleRate);
    } else if (sampleRateCode == 0xD) {
        unsigned char srHiLo[2];
        nexus_u64 got = 0;
        if (!nexus_file_consume(fh, 2u, srHiLo, 2u, &got, errorBuffer, sizeof errorBuffer) || got != 2u) {
            printf("Read error (16-bit sample rate Hz): '%s'\n", errorBuffer);
            nexus_file_information_close(fh);
            return;
        }
        crc8 = nexus_validation_crc8_update(crc8, srHiLo[0]);
        crc8 = nexus_validation_crc8_update(crc8, srHiLo[1]);
        sampleRate = (nexus_u32)((srHiLo[0] << 8) | srHiLo[1]);
        printf("Sample rate (uncommon, 16-bit Hz): %u Hz\n", sampleRate);
    } else if (sampleRateCode == 0xE) {
        unsigned char sr10HiLo[2];
        nexus_u64 got = 0;
        if (!nexus_file_consume(fh, 2u, sr10HiLo, 2u, &got, errorBuffer, sizeof errorBuffer) || got != 2u) {
            printf("Read error (16-bit sample rate x10 Hz): '%s'\n", errorBuffer);
            nexus_file_information_close(fh);
            return;
        }
        crc8 = nexus_validation_crc8_update(crc8, sr10HiLo[0]);
        crc8 = nexus_validation_crc8_update(crc8, sr10HiLo[1]);
        sampleRate = ((nexus_u32)sr10HiLo[0] << 8 | sr10HiLo[1]) * 10u;
        printf("Sample rate (uncommon, 16-bit tens of Hz): %u Hz\n", sampleRate);
    }

    unsigned char crc8Stored = 0;
    {
        nexus_u64 got = 0;
        if (!nexus_file_consume(fh, 1u, &crc8Stored, 1u, &got, errorBuffer, sizeof errorBuffer) || got != 1u) {
            printf("Read error (CRC-8): '%s'\n", errorBuffer);
            nexus_file_information_close(fh);
            return;
        }
    }
    printf("CRC-8 stored: 0x%02X | computed: 0x%02X\n", crc8Stored, crc8);

    if (crc8Stored != crc8) {
        printf("Frame header CRC-8 mismatch!\n");
        nexus_file_information_close(fh);
        return;
    }

    printf("Frame header parsed successfully.\n");
    nexus_file_information_close(fh);
}
