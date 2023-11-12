#include <iostream>
#include <string.h>
#include "lzwSource.h"
#include "lzw.h"

struct DIR {
    size_t clust;
    size_t blk_ofs;
};

struct FILINFO {
    char fname[255];
    size_t fsize;
};

bool in_readdir (
	DIR*     dp,
	FILINFO* fno
) {
    dp->clust++;
    size_t numberOfFiles = toDWORD((char*)lz4source, 0) & 0xFF;
    if (dp->clust > numberOfFiles) {
        return false;
    }
    size_t fnz = 0;
    while (lz4source[dp->blk_ofs] != 0) {
        fno->fname[fnz++] = lz4source[dp->blk_ofs++];
    }
    fno->fname[fnz] = 0; dp->blk_ofs++; // traling zero
    dp->blk_ofs += 4; // ignore compressedSize there
    fno->fsize = toDWORD((char*)lz4source, dp->blk_ofs); dp->blk_ofs += 4;
//    fno->fattrib = AM_RDO;
    return true;
}
typedef unsigned int DWORD;

struct FILE_LZW {
    DWORD compressed;
    int remainsDecompressed;
};

static LZWBlockInputStream * pIs = 0;
static char ROM[256*1024]= {};

bool in_open (
    FILE_LZW* fp,
    char* fn
) {
    size_t fptr = 0;
    DWORD token = toDWORD((char*)lz4source, fptr);
    DWORD numberOfFiles = token & 0xFF;
    DWORD bbOffset = token >> 8;
    fptr += 4;
    DWORD fileNum = 0;
    DWORD compressed = 0;
    DWORD compressedOff = 0;
    fp->compressed = 0;
    while (fileNum++ < numberOfFiles) {
        int i = 5; // ignore trailing "\NES\"
        size_t fnStart = (size_t)fptr;
        while (fn[i++] == lz4source[fptr] && lz4source[fptr] != 0) {
            fptr++;
        }
        if (lz4source[fptr++] == 0) { // file was found
            compressed = toDWORD((char*)lz4source, fptr); fptr += 4;
            fp->remainsDecompressed = toDWORD((char*)lz4source, fptr); fptr += 4;
            fp->compressed = compressed;
            fptr = (size_t)lz4source + bbOffset + compressedOff;
            if (pIs) {
                delete pIs;
            }
            pIs = new LZWBlockInputStream((char*)fptr, (const char*)ROM);
            return true;
        } else {
            while(lz4source[fptr++] != 0) {}
            compressed = toDWORD((char*)lz4source, fptr); fptr += 8;
            compressedOff += compressed;
        }
    }
    return false;
}

void in_read (
	FILE_LZW* fp, 	/* Open file to be read */
	char* buff,	/* Data buffer to store the read data */
	size_t  btr,	/* Number of bytes to read */
	size_t* br	/* Number of bytes read */
) {
    if (fp->remainsDecompressed <= 0) {
       printf((char*)"Final\n");
        *br = 0;
    } else {
        int decompressed = btr;
        size_t remains = fp->remainsDecompressed;
        size_t read = pIs->read(buff, btr, &decompressed);
        fp->remainsDecompressed -= decompressed;
        *br = read;
    }
}

void printROM(size_t romOff, size_t len) {
    for (int i = romOff; i < romOff + len; ++i) {
        if (i % 16 == 0) {
            printf("\n%d: ", i);
        }
        if (i == 0) {
            printf("  ");
        } else {
            printf(", ");
        }
        printf("0x%02X", (unsigned int)(ROM[i] & 0xFF));
    }
    printf("\n");
}

int main()
{
    DIR dir;
    dir.blk_ofs = 4;
    dir.clust = 0;
    FILINFO fi;
    while(in_readdir(& dir, &fi) ) {
      std::cout << fi.fname << "; sz: " << fi.fsize << std::endl;
      FILE_LZW lzw;
      char tmp[255];
      strcpy(tmp, "\\NES\\");
      strcpy(tmp + 5, fi.fname);
      in_open(&lzw, tmp);
      std::cout << " compressed:" << lzw.compressed << "; remainsDecompressed: " << lzw.remainsDecompressed << std::endl;
      char buff[16*1024];
      const size_t len = sizeof(buff);
      size_t romOff = 0;
      while(1) {
           size_t bytesRead;
           in_read(&lzw, buff, len, &bytesRead);
           if (!bytesRead) break;
           memcpy((char*)(ROM + romOff), buff, len);
           printROM(romOff, len);
           romOff += len;
      }
      if (pIs) { delete pIs; pIs = 0; }
      break; ///
    }
}
