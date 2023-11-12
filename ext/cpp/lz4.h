#ifndef LZ4_INCLUDE
#define LZ4_INCLUDE

inline int toDWORD(const char * ptr, int offset) {
    int res = (int)(ptr[offset++] & 0xFF) << 24;
    res |= (int)(ptr[offset++] & 0xFF) << 16;
    res |= (int)(ptr[offset++] & 0xFF) << 8;
    res |= (int)(ptr[offset] & 0xFF);
    return res;    
}

inline void safeArraycopy(const char * src, int sOff, char * dest, int dOff, int literalLen) {
    if (literalLen == 0) return;
    memcpy(dest + dOff, src + sOff, literalLen);
}

inline void copy8Bytes(const char * src, int sOff, char * dest, int dOff) {
    for (int i = 0; i < 8; ++i) {
        dest[dOff + i] = src[sOff + i];
    }
}

inline void wildArraycopy(const char * src, int sOff, char * dest, int dOff, int len) {
    for (int i = 0; i < len; i += 8) {
        copy8Bytes(src, sOff + i, dest, dOff + i);
    }
}

inline void safeIncremenatalCopy(char * dest, int matchOff, int dOff, int matchLen) {
    printf("safeIncremenatalCopy matchOff: %d, dOff: %d, matchLen: %d\n", matchOff, dOff, matchLen);
    for (int i = 0; i < matchLen; ++i) {
        dest[dOff + i] = dest[matchOff + i];
    }
}

inline void wildIncremenatalCopy(char * dest, int matchOff, int dOff, int matchCopyEnd) {
    printf("safeIncremenatalCopy matchOff: %d, dOff: %d, matchCopyEnd: %d\n", matchOff, dOff, matchCopyEnd);
    do {
        for (size_t i = 0; i < 8; ++i) {
            dest[matchOff + i] = dest[dOff + i];
        }
        matchOff += 8;
        dOff += 8;
    } while (dOff < matchCopyEnd);
}

const int ML_BITS = 4;
const int RUN_BITS = 8 - ML_BITS;
const int RUN_MASK = (1 << RUN_BITS) - 1;
const int COPY_LENGTH = 8;
const int ML_MASK = (1 << ML_BITS) - 1;
const int MIN_MATCH = 4;
int decompress(char * src, int srcOff, int srcLen, char * dest, int destOff, int destLen) {
    if (destLen == 0) {
        return 1;
    }
    const int destEnd = destOff + destLen;
    int sOff = srcOff;
    int dOff = destOff;
    while (true) {
        int token = src[sOff++] & 0xFF;
        int literalLen = token >> ML_BITS;
        if (literalLen == RUN_MASK) {
            signed char len;
            while ((len = src[sOff++]) == (char)0xFF) {
                literalLen += 0xFF;
            }
            literalLen += len & 0xFF;
        }
        const int literalCopyEnd = dOff + literalLen;
        if (literalCopyEnd > destEnd - COPY_LENGTH) {
            safeArraycopy(src, sOff, dest, dOff, literalLen);
            sOff += literalLen;
            break; // EOF
        }
        wildArraycopy(src, sOff, dest, dOff, literalLen);
        sOff += literalLen;
        dOff = literalCopyEnd;
        int left = src[sOff++];
        int right = src[sOff++];
        right = right << 8;
        int matchDec = left | right;
        int matchOff = dOff - matchDec;
        int matchLen = token & ML_MASK;
        if (matchLen == ML_MASK) {
            char len;
            while ((len = src[sOff++]) == (char)0xFF) {
                matchLen += 0xFF;
            }
            matchLen += len & 0xFF;
        }
        matchLen += MIN_MATCH;
        const int matchCopyEnd = dOff + matchLen;
        if (matchCopyEnd > destEnd - COPY_LENGTH) {
            safeIncremenatalCopy(dest, matchOff, dOff, matchLen);
        } else {
            wildIncremenatalCopy(dest, matchOff, dOff, matchCopyEnd);
        }
        dOff = matchCopyEnd;
    }
    return sOff - srcOff;
}

const int LZ4_BLOCK_SIZE = 16 * 1024;

struct LZ4BlockInputStream {
    char * _src;
    LZ4BlockInputStream(char * src) {
        _src = src;
    }
    inline int read(char * dest, int * decompressed) {
        int compressedLen = toDWORD(_src, 0); _src += 4;
        *decompressed = toDWORD(_src, 0); _src += 4;
        printf("decompress(0x%x, %d, 0x%x, %d)\n", _src, compressedLen, dest, *decompressed);
        int read = decompress(_src, 0, compressedLen, dest, 0, *decompressed);
        printf("decompressed: %d\n", *decompressed);
        _src += compressedLen; // == read
        return compressedLen;
    }
};

#endif
