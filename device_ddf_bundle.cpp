#include "deconz/u_assert.h"
#include "device_ddf_bundle.h"

int DDFB_FindChunk(U_BStream *bs, const char *tag, unsigned *size)
{
    char fourcc[4];
    unsigned sz;
    unsigned long origPos = bs->pos;

    for (;bs->pos < bs->size && bs->status == U_BSTREAM_OK;)
    {
        fourcc[0] = U_bstream_get_u8(bs);
        fourcc[1] = U_bstream_get_u8(bs);
        fourcc[2] = U_bstream_get_u8(bs);
        fourcc[3] = U_bstream_get_u8(bs);
        sz = U_bstream_get_u32_le(bs);

        if (bs->pos + sz > bs->size)
        {
            break; // invalid size
        }

        if (fourcc[0] == tag[0] && fourcc[1] == tag[1] && fourcc[2] == tag[2] && fourcc[3] == tag[3])
        {
            *size = sz;
            return 1;
        }

        bs->pos += sz;
    }

    bs->pos = origPos;
    *size = 0;
    return 0;
}

int DDFB_IsChunk(U_BStream *bs, const char *tag)
{
    unsigned char *fourcc;
    if (bs->pos + 4 < bs->size)
    {
        fourcc = &bs->data[bs->pos];
        return fourcc[0] == tag[0] && fourcc[1] == tag[1] && fourcc[2] == tag[2] && fourcc[3] == tag[3];
    }

    return 0;
}

int DDFB_SkipChunk(U_BStream *bs)
{
    char fourcc[4];
    unsigned sz;

    fourcc[0] = U_bstream_get_u8(bs);
    fourcc[1] = U_bstream_get_u8(bs);
    fourcc[2] = U_bstream_get_u8(bs);
    fourcc[3] = U_bstream_get_u8(bs);
    sz = U_bstream_get_u32_le(bs);

    if (bs->status == U_BSTREAM_OK)
    {
        if (bs->pos + sz <= bs->size)
        {
            bs->pos += sz;
            return 1;
        }
    }

    return 0;
}

/*
    https://github.com/deconz-community/ddf-tools/blob/main/packages/bundler/README.md
*/
int IsValidDDFBundle(U_BStream *bs, unsigned char sha256[U_SHA256_HASH_SIZE])
{
    unsigned chunkSize;

    if (DDFB_FindChunk(bs, "RIFF", &chunkSize) == 0)
    {
        return 0;
    }

    if (DDFB_FindChunk(bs, "DDFB", &chunkSize) == 0)
    {
        return 0;
    }

    {
        // Bundle hash over DDFB chunk (header + data)
        if (U_Sha256(&bs->data[bs->pos - 8], chunkSize + 8, sha256) == 0)
        {
            return 0; // this
        }
    }

    U_BStream bsDDFB;
    U_bstream_init(&bsDDFB, &bs->data[bs->pos], chunkSize);

    // check DESC JSON has required fields
    if (DDFB_FindChunk(&bsDDFB, "DESC", &chunkSize) == 0)
    {
        return 0;
    }

    // DBG_Printf(DBG_INFO, "DESC: %.*s\n", chunkSize, &bsDDFB.data[bsDDFB.pos]);

    return 1;
}

bool DDFB_SanitizeBundleHashString(char *str, unsigned len)
{
    if (len != 64)
        return false;

    for (unsigned i = 0; i < len; i++)
    {
        char ch = str[i];

        if      (ch >= '0' && ch <= '9') { } // ok
        else if (ch >= 'a' && ch <= 'f') { } // ok
        else if (ch >= 'A' && ch <= 'F') { str[i] = ch + ('a' - 'A'); } // convert to lower case
        else
        {
            return false; // invalid hex char
        }
    }

    return true;
}

int DDFB_ReadExtfChunk(U_BStream *bs, DDFB_ExtfChunk *extf)
{
    U_BStream bs1;
    char fourcc[4];
    unsigned chunkSize;
    unsigned long origPos = bs->pos;

    extf->emptyString = '\0';

    fourcc[0] = U_bstream_get_u8(bs);
    fourcc[1] = U_bstream_get_u8(bs);
    fourcc[2] = U_bstream_get_u8(bs);
    fourcc[3] = U_bstream_get_u8(bs);
    chunkSize = U_bstream_get_u32_le(bs);

    if (bs->status != U_BSTREAM_OK)
    {
        return 0;
    }

    if (bs->size < bs->pos + chunkSize)
    {
        return 0;
    }

    U_ASSERT(fourcc[0] == 'E');
    U_ASSERT(fourcc[1] == 'X');
    U_ASSERT(fourcc[2] == 'T');
    U_ASSERT(fourcc[3] == 'F');

    U_bstream_init(&bs1, &bs->data[bs->pos], chunkSize);
    bs->pos += chunkSize; // move bs behind chunk

    extf->fileType[0] = (char)U_bstream_get_u8(&bs1);
    extf->fileType[1] = (char)U_bstream_get_u8(&bs1);
    extf->fileType[2] = (char)U_bstream_get_u8(&bs1);
    extf->fileType[3] = (char)U_bstream_get_u8(&bs1);
    extf->fileType[4] = '\0';

    // file path string
    extf->pathLength = U_bstream_get_u16_le(&bs1);
    if (bs1.size < bs1.pos + extf->pathLength)
    {
        return 0;
    }
    extf->path = (const char*)&bs1.data[bs1.pos];
    bs1.pos += extf->pathLength;

    // modification time string
    extf->modificationTimeLength = U_bstream_get_u16_le(&bs1);
    if (bs1.size < bs1.pos + extf->modificationTimeLength)
    {
        return 0;
    }
    if (extf->modificationTimeLength == 0) // optional
    {
        extf->modificationTime = &extf->emptyString;
    }
    else
    {
        extf->modificationTime = (const char*)&bs1.data[bs1.pos];
    }

    bs1.pos += extf->modificationTimeLength;

    // file content
    extf->fileSize = U_bstream_get_u32_le(&bs1);
    if (bs1.size < bs1.pos + extf->fileSize)
    {
        return 0;
    }
    extf->fileData = &bs1.data[bs1.pos];

    return 1;
}
