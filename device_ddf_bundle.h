#ifndef DEVICE_DDF_BUNDLE_H
#define DEVICE_DDF_BUNDLE_H

#include "deconz/u_bstream.h"
#include "deconz/u_sha256.h"

#define MAX_BUNDLE_SIZE (1 << 20) // 1 MB

struct DDFB_ExtfChunk
{
    struct DDFB_ExtfChunk *next;
    char fileType[5]; // fourcc + '\0'
    char emptyString;
    unsigned pathLength;
    const char *path;
    unsigned modificationTimeLength;
    const char *modificationTime;
    unsigned fileSize;
    unsigned char *fileData;
};

int DDFB_FindChunk(U_BStream *bs, const char *tag, unsigned *size);
int DDFB_IsChunk(U_BStream *bs, const char *tag);
int DDFB_SkipChunk(U_BStream *bs);
int DDFB_ReadExtfChunk(U_BStream *bs, DDFB_ExtfChunk *extf);
int IsValidDDFBundle(U_BStream *bs, unsigned char sha256[U_SHA256_HASH_SIZE]);
bool DDFB_SanitizeBundleHashString(char *str, unsigned len);


#endif // DEVICE_DDF_BUNDLE_H
