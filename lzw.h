#ifndef LZW_INCLUDE
#define LZW_INCLUDE
#include <string.h>

inline int toDWORD(const char * ptr, int offset) {
    int res = (int)(ptr[offset++] & 0xFF) << 24;
    res |= (int)(ptr[offset++] & 0xFF) << 16;
    res |= (int)(ptr[offset++] & 0xFF) << 8;
    res |= (int)(ptr[offset] & 0xFF);
    return res;    
}

class CompositeDest {
    const char* rom; // already saved to ROM part
    size_t rOff = 0;
    char* dest; // not yet saved part (extertanl call RAM buffer)
    size_t destLen;
    size_t dOff = 0;
    char buff[128]; // one iteration overflow
    size_t bOff = 0;
public:
    CompositeDest(const char* rom_) { this->rom = rom_; }
    inline void nextEnter(char* dest_, size_t destLen_) {
        dest = dest_;
        destLen = destLen_;
        dOff = 0;
        if (bOff > 0) {
            memcpy(dest, buff, bOff);
            dOff += bOff;
        }
        bOff = 0;
    }
    inline void push(char d) {
        printf("ro(%d) ", rOff);
        if (dOff < destLen) {
            dest[dOff++] = d;
        } else {
            buff[bOff++] = d;
        }
        rOff++;
    }
    inline void copyFromBack(size_t len, size_t backOff) {
        int destIdx = dOff - backOff;
        int romIdx = rOff - backOff;
        for (size_t k = 0; k < len; ++destIdx, ++romIdx, ++k) {
            if (destIdx >= 0) {
                printf("(%d) ", romIdx);
                auto b = dest[destIdx];
                printf("0x%02X ", (unsigned int)(b & 0xFF));
                push(b);
            } else {
                printf("(%d) ", romIdx);
                auto b = rom[romIdx];
                printf("0x%02X ", (unsigned int)(b & 0xFF));
                push(b);
            }
        }
    }
    inline bool endIteration() { // true, while we have free space in the dest
        return bOff == 0;
    }
    inline int getDecompressedLen() {
        return dOff;
    }
};

struct LZWBlockInputStream {
    char * _src;
    size_t sOff = 0;
    CompositeDest * cd;
    LZWBlockInputStream(char * src, const char* rom_) {
        _src = src;
        cd = new CompositeDest(rom_);
    }
    ~LZWBlockInputStream() { delete cd; };
    inline int read(char * dest, size_t destBuffLen, int * decompressed) {
        auto s = sOff;
        cd->nextEnter(dest, destBuffLen);
        do {
            signed char token = _src[sOff++];
            printf("token: 0x%02X: ", (unsigned int)(token & 0xFF));
            if (token <= 0) {
                size_t len = 1 - token;
                printf("len: %d -> ", len);
                for (size_t j = 0; j < len; ++j) {
                    auto b = _src[sOff++];
                    printf("0x%02X ", (unsigned int)(b & 0xFF));
                    cd->push(b);
                }
            } else {
                printf("len: %d -> ", token);
                int backOff = _src[sOff++] & 0xFF;
                backOff |= (_src[sOff++] & 0xFF) << 8;
                printf("0x%04X (%d) -> ", backOff, backOff);
                cd->copyFromBack(token, backOff);
            }
            printf("\n");
        } while(cd->endIteration());
        *decompressed = cd->getDecompressedLen();
        return sOff - s;
    }
};

#endif
