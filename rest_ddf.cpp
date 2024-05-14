/*
 * Copyright (c) 2024 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */

#include <QTcpSocket>
#include <deconz/dbg_trace.h>
#include "device_ddf_bundle.h"
#include "rest_api.h"
#include "rest_ddf.h"
#include "deconz/file.h"
#include "deconz/u_sstream.h"
#include "deconz/u_memory.h"
#include "deconz/util.h"
#include "utils/scratchmem.h"

#define MAX_PATH_LENGTH 2048

/*!
    Converts data into hex-ascii string.

    The memory area \p ascii must have at least the
    size \p 2 * length + 1 bytes.

    A terminating zero will be appended at the end.

    \returns a pointer to the last byte (zero).
 */
static unsigned char *BinToHexAscii(const void *hex, unsigned length, void *ascii)
{
    unsigned i;
    unsigned char *h;
    unsigned char *a;

    static char lookup[] = { '0', '1', '2','3','4','5','6','7','8','9','a','b','c','d','e','f' };

    if (!hex || length == 0 || !ascii)
        return 0;

    h = (unsigned char*)hex;
    a = (unsigned char*)ascii;

    for (i = 0; i < length; i++)
    {
        *a++ = lookup[(h[i] & 0xf0) >> 4];
        *a++ = lookup[h[i] & 0xf];
    }

    *a = '\0';

    return a;
}

/*

Test upload of .ddf file

curl -F 'data=@/home/mpi/some.ddf' 127.0.0.1:8090/api/12345/ddf

*/

static int WriteBundleDescriptorToResponse(U_BStream *bs, U_SStream *ss, unsigned nRecords)
{
    unsigned chunkSize;
    char sha256Str[(U_SHA256_HASH_SIZE * 2) + 1];

    if (DDFB_FindChunk(bs, "RIFF", &chunkSize) == 0)
    {
        return 0;
    }

    if (DDFB_FindChunk(bs, "DDFB", &chunkSize) == 0)
    {
        return 0;
    }

    {
        unsigned char sha256[U_SHA256_HASH_SIZE];
        // Bundle hash over DDFB chunk (header + data)
        if (U_Sha256(&bs->data[bs->pos - 8], chunkSize + 8, sha256) == 0)
        {
            return 0; // should not happen
        }

        BinToHexAscii(sha256, U_SHA256_HASH_SIZE, sha256Str);
    }

    U_BStream bsDDFB;
    U_bstream_init(&bsDDFB, &bs->data[bs->pos], chunkSize);

    // check DESC JSON has required fields
    if (DDFB_FindChunk(&bsDDFB, "DESC", &chunkSize) == 0)
    {
        return 0;
    }

    // enough space for descriptor plus hash key
    if ((ss->pos + chunkSize + 128) < ss->len)
    {
        if (nRecords > 0)
            U_sstream_put_str(ss, ",");

        U_sstream_put_str(ss, "\"");
        U_sstream_put_str(ss, sha256Str);
        U_sstream_put_str(ss, "\":");

        U_memcpy(&ss->str[ss->pos], &bsDDFB.data[bsDDFB.pos], chunkSize);
        ss->pos += chunkSize;

        return 1;
    }

    DBG_Printf(DBG_INFO, "DESC: %.*s\n", chunkSize, &bsDDFB.data[bsDDFB.pos]);


    return 0;
}

int REST_DDF_GetDescriptors(const ApiRequest &req, ApiResponse &rsp)
{
    // TEST call
    // curl -vv 127.0.0.1:8090/api/12345/ddf/descriptors
    // curl -vv 127.0.0.1:8090/api/12345/ddf/descriptors?next=<token>
    unsigned reqCursor = 1;
    unsigned curCursor = 1;
    unsigned nextCursor = 0;
    unsigned nRecords = 0;
    unsigned maxRecords = 64;

    auto url = req.hdr.url();

    { // get page from query string if exist
        U_SStream ss;
        U_sstream_init(&ss, (void*)url.data(), (unsigned)url.size());

        if (U_sstream_find(&ss, "?next="))
        {
            U_sstream_find(&ss, "=");
            ss.pos++;
            long n = U_sstream_get_long(&ss);
            if (ss.status == U_SSTREAM_OK && n > 0)
            {
                reqCursor = (unsigned)n;
            }
            else
            {
                rsp.httpStatus = HttpStatusBadRequest;
                return REQ_READY_SEND;
            }
        }
    }

    unsigned maxResponseSize = 1 << 20;  // 1 MB
    char *bundleData = SCRATCH_ALLOC(char*, MAX_BUNDLE_SIZE);
    char *path = SCRATCH_ALLOC(char*, MAX_PATH_LENGTH);
    char *rspData = SCRATCH_ALLOC(char*, maxResponseSize);

    if (!bundleData || !path || !rspData)
    {
        rsp.httpStatus = HttpStatusServiceUnavailable;
        return REQ_READY_SEND;
    }

    FS_Dir dir;
    FS_File fp;
    U_SStream ss;
    U_SStream ssRsp;
    unsigned basePathLength;

    deCONZ::StorageLocation locations[2] = { deCONZ::DdfBundleUserLocation, deCONZ::DdfBundleLocation };

    U_sstream_init(&ssRsp, rspData, maxResponseSize);
    U_sstream_put_str(&ssRsp, "{");

    for (int locIt = 0; locIt < 2; locIt++)
    {
        {
            QString loc = deCONZ::getStorageLocation(locations[locIt]);
            U_sstream_init(&ss, path, MAX_PATH_LENGTH);
            U_sstream_put_str(&ss, qPrintable(loc));
            basePathLength = ss.pos;
        }

        if (FS_OpenDir(&dir, path))
        {
            for (;FS_ReadDir(&dir);)
            {
                if (dir.entry.type != FS_TYPE_FILE)
                    continue;

                U_sstream_init(&ss, dir.entry.name, strlen(dir.entry.name));

                if (U_sstream_find(&ss, ".ddf") == 0)
                    continue;

                if (curCursor < reqCursor)
                {
                    curCursor++;
                    continue;
                }

                if (nRecords < maxRecords)
                {
                    U_sstream_init(&ss, path, MAX_PATH_LENGTH);
                    ss.pos = basePathLength; // reuse path and append the filename to existing base path
                    U_sstream_put_str(&ss, "/");
                    U_sstream_put_str(&ss, dir.entry.name);

                    if (FS_OpenFile(&fp, FS_MODE_R, path))
                    {
                        long n = FS_ReadFile(&fp, bundleData, MAX_BUNDLE_SIZE);
                        if (n > 32)
                        {
                            U_BStream bs;
                            U_bstream_init(&bs, bundleData, (unsigned)n);
                            if (WriteBundleDescriptorToResponse(&bs, &ssRsp, nRecords))
                            {
                                curCursor++;
                                nRecords++;
                            }
                        }

                        FS_CloseFile(&fp);
                    }
                }
                else
                {
                    nextCursor = curCursor;
                    break;
                }

                DBG_Printf(DBG_INFO, "BUNDLE: %s\n", ss.str);
            }

            if (nextCursor != 0)
            {
                U_sstream_put_str(&ssRsp, ",\"next\":");
                U_sstream_put_long(&ssRsp, (long)nextCursor);
            }

            FS_CloseDir(&dir);
        }
    }

    U_sstream_put_str(&ssRsp, "}");

    rsp.httpStatus = HttpStatusOk;
    rsp.str = ssRsp.str;

    return REQ_READY_SEND;
}

int REST_DDF_GetDescriptor(const ApiRequest &req, ApiResponse &rsp)
{
    // TEST call
    // curl -vv 127.0.0.1:8090/api/12345/ddf/descriptors/0a34938f63f0ccb40e1672799c898889989574f617c103fb64496e9ad78c29a2

    auto bundleHash = req.hdr.pathAt(4);

    if (bundleHash.size() != 64)
    {
        rsp.httpStatus = HttpStatusBadRequest;
        return REQ_READY_SEND;
    }

    rsp.list.append(errorToMap(ERR_RESOURCE_NOT_AVAILABLE,
                               QString("/ddf/descriptors/%1").arg(bundleHash),
                               QString("resource, /ddf/descriptors/%1, not available").arg(bundleHash)));


    rsp.httpStatus = HttpStatusNotFound;

    return REQ_READY_SEND;
}

int REST_DDF_GetBundle(const ApiRequest &req, ApiResponse &rsp)
{
    // TEST call
    // curl -vv -O --remote-header-name 127.0.0.1:8090/api/12345/ddf/bundles/0a34938f63f0ccb40e1672799c898889989574f617c103fb64496e9ad78c29a2
    // wget --content-disposition 127.0.0.1:8090/api/12345/ddf/bundles/0a34938f63f0ccb40e1672799c898889989574f617c103fb64496e9ad78c29a2

    FS_File fp;
    FS_Dir dir;
    unsigned basePathLength;
    char bundleHashStr[U_SHA256_HASH_SIZE * 2 + 1];

    {
        auto urlBundleHash = req.hdr.pathAt(4);

        if (urlBundleHash.size() != (U_SHA256_HASH_SIZE * 2))
        {
            rsp.httpStatus = HttpStatusBadRequest;
            return REQ_READY_SEND;
        }

        U_memcpy(bundleHashStr, urlBundleHash.data(), U_SHA256_HASH_SIZE * 2);
        bundleHashStr[U_SHA256_HASH_SIZE * 2] = '\0';

        if (!DDFB_SanitizeBundleHashString(bundleHashStr, U_SHA256_HASH_SIZE * 2))
        {
            rsp.httpStatus = HttpStatusBadRequest;
            return REQ_READY_SEND;
        }
    }

    unsigned maxFileNameLength = 72;
    char *path = SCRATCH_ALLOC(char*, MAX_PATH_LENGTH);
    char *fileName = SCRATCH_ALLOC(char*, maxFileNameLength);
    rsp.bin = SCRATCH_ALLOC(char*, MAX_BUNDLE_SIZE);

    if (!path || !fileName || !rsp.bin)
    {
        rsp.httpStatus = HttpStatusServiceUnavailable;
        return REQ_READY_SEND;
    }

    {
        U_SStream ssFileName;
        U_sstream_init(&ssFileName, fileName, maxFileNameLength);
        U_sstream_put_str(&ssFileName, bundleHashStr);
        U_sstream_put_str(&ssFileName, ".ddf");
    }

    deCONZ::StorageLocation locations[2] = { deCONZ::DdfBundleUserLocation, deCONZ::DdfBundleLocation };

    for (int locIt = 0; locIt < 2; locIt++)
    {
        U_SStream ss;
        {
            QString loc = deCONZ::getStorageLocation(locations[locIt]);
            U_sstream_init(&ss, path, MAX_PATH_LENGTH);
            U_sstream_put_str(&ss, qPrintable(loc));
            basePathLength = ss.pos;
        }

        if (FS_OpenDir(&dir, path))
        {
            for (;FS_ReadDir(&dir);)
            {
                if (dir.entry.type != FS_TYPE_FILE)
                    continue;

                U_sstream_init(&ss, dir.entry.name, strlen(dir.entry.name));

                if (U_sstream_find(&ss, ".ddf") == 0)
                    continue;

                U_sstream_init(&ss, path, MAX_PATH_LENGTH);
                ss.pos = basePathLength; // reuse path and append the filename to existing base path
                U_sstream_put_str(&ss, "/");
                U_sstream_put_str(&ss, dir.entry.name);

                if (FS_OpenFile(&fp, FS_MODE_R, path))
                {
                    long fileSize = FS_GetFileSize(&fp);
                    if (fileSize > 0 && fileSize <= MAX_BUNDLE_SIZE)
                    {
                        if (FS_ReadFile(&fp, rsp.bin, fileSize) == fileSize)
                        {
                            U_BStream bs;
                            uint8_t sha256[U_SHA256_HASH_SIZE];

                            U_bstream_init(&bs, rsp.bin, fileSize);

                            if (IsValidDDFBundle(&bs, sha256))
                            {
                                char sha256Str[(U_SHA256_HASH_SIZE * 2) + 1];
                                BinToHexAscii(sha256, U_SHA256_HASH_SIZE, sha256Str);

                                if (U_memcmp(bundleHashStr, sha256Str, U_SHA256_HASH_SIZE * 2) == 0)
                                {
                                    FS_CloseFile(&fp);
                                    FS_CloseDir(&dir);

                                    rsp.contentLength = (unsigned)fileSize;
                                    rsp.fileName = fileName;
                                    rsp.httpStatus = HttpStatusOk;
                                    rsp.contentType = HttpContentOctetStream;
                                    return REQ_READY_SEND;
                                }
                            }
                        }
                    }

                    FS_CloseFile(&fp);
                }
            }

            FS_CloseDir(&dir);
        }
    }

    {
        rsp.httpStatus = HttpStatusNotFound;
        rsp.list.append(errorToMap(ERR_RESOURCE_NOT_AVAILABLE,
                               QString("/ddf/bundles/%1").arg(bundleHashStr),
                               QString("resource, /ddf/bundles/%1, not available").arg(bundleHashStr)));
    }

    return REQ_READY_SEND;
}

int REST_DDF_PostBundles(const ApiRequest &req, ApiResponse &rsp)
{
    ScratchMemWaypoint swp;

    // TEST call
    // curl -F 'data=@./starkvind_air_purifier_toolbox.ddf' 127.0.0.1:8090/api/12345/ddf/bundles

    if (req.hdr.contentLength() < 32 || req.hdr.contentLength() > 512000)
    {
        return REQ_NOT_HANDLED;
    }

    // TODO(mpi) upload via multipart/form-data should be moved to generic http handling

    // Content-Type HTTP header contains the boundary:
    //    "Content-Type: multipart/form-data; boundary=------------------------Y8hknTumhcaM4YjkoVup1T"

    QLatin1String contentType = req.hdr.value(QLatin1String("Content-Type"));

    U_SStream ss;
    U_sstream_init(&ss, (void*)contentType.data(), contentType.size());

    if (U_sstream_starts_with(&ss, "multipart/form-data") == 0)
    {
        rsp.httpStatus = HttpStatusBadRequest;
        return REQ_READY_SEND;
    }

    if (U_sstream_find(&ss, "boundary=") == 0) // no boundary marker?
    {
        rsp.httpStatus = HttpStatusBadRequest;
        return REQ_READY_SEND;
    }

    if (U_sstream_find(&ss, "=") == 0)
    {
        rsp.httpStatus = HttpStatusBadRequest;
        return REQ_READY_SEND;
    }
    ss.pos++;
    const unsigned boundaryLen = ss.len - ss.pos;
    //boundary = static_cast<char*>(ScratchMemAlloc((boundaryLen) + 8));
    char *boundary = SCRATCH_ALLOC(char*, boundaryLen + 8);

    if (!boundary)
    {
        rsp.httpStatus = HttpStatusServiceUnavailable;
        return REQ_READY_SEND;
    }
    U_memcpy(boundary, &ss.str[ss.pos], boundaryLen);
    boundary[boundaryLen] = '\0';

    unsigned dataSize = req.hdr.contentLength() + 1;
    if (MAX_BUNDLE_SIZE < dataSize)
    {
        rsp.httpStatus = HttpStatusBadRequest;
        return REQ_READY_SEND;
    }

    char *data = SCRATCH_ALLOC(char*, dataSize);

    if (!data)
    {
        rsp.httpStatus = HttpStatusServiceUnavailable;
        return REQ_READY_SEND;
    }

    unsigned binStart = 0;
    unsigned binEnd = 0;
    int n = req.sock->read(data, dataSize - 1);

    if (n > 0)
    {
        data[dataSize - 1] = '\0';

        U_sstream_init(&ss, data, n);

        if (U_sstream_find(&ss, boundary) == 0) // there might be a preample before the first boundary
        {
            rsp.httpStatus = HttpStatusBadRequest;
            return REQ_READY_SEND;
        }

        // actual data starts behind first two CRLF
        //
        // --------------------------Y8hknTumhcaM4YjkoVup1T
        // Content-Disposition: form-data; name="data"; filename="steam.md"
        // Content-Type: application/octet-stream
        // \r\n\r\n
        if (U_sstream_find(&ss, "\r\n\r\n") == 0)
        {
            return REQ_NOT_HANDLED;
        }

        ss.pos += 4;
        binStart = ss.pos;

        if (U_sstream_find(&ss, boundary) == 0)
        {
            rsp.httpStatus = HttpStatusBadRequest;
            return REQ_READY_SEND;
        }

        binEnd = ss.pos;
    }

    if (binStart < binEnd && (binEnd - binStart) > 16) // TODO use some reasonable min. size instead 16
    {
        if (data[binEnd - 1] == '-' && data[binEnd - 2] == '-')
        {
            binEnd -= 2; // end boundary preceeded by two dashes
        }

        if (data[binEnd - 1] == '\n' && data[binEnd - 2] == '\r')
        {
            binEnd -= 2; // end boundary also preceeded with CRLF
        }

        data[binEnd] = '\0';

        U_BStream bs;

        U_bstream_init(&bs, &data[binStart], binEnd - binStart);

        // unsigned char bundleHash[U_SHA256_HASH_SIZE] = {0};
        // char bundleHashStr[(U_SHA256_HASH_SIZE * 2) + 1];

        unsigned char *bundleHash = SCRATCH_ALLOC(unsigned char*, U_SHA256_HASH_SIZE);
        char *bundleHashStr = SCRATCH_ALLOC(char*, (U_SHA256_HASH_SIZE * 2) + 1);

        if (!bundleHash || !bundleHashStr)
        {
            rsp.httpStatus = HttpStatusServiceUnavailable;
            return REQ_READY_SEND;
        }


        if (IsValidDDFBundle(&bs, bundleHash) == 0)
        {
            rsp.httpStatus = HttpStatusBadRequest;
            return REQ_READY_SEND;
        }

        BinToHexAscii(bundleHash, U_SHA256_HASH_SIZE, bundleHashStr);
        DBG_Printf(DBG_INFO, "received %d bytes (binary: %u), bundle-hash: %s\n", n, binEnd - binStart, bundleHashStr);

        QString loc = deCONZ::getStorageLocation(deCONZ::DdfBundleUserLocation);

        FS_File fp;
        char *bundlePath = SCRATCH_ALLOC(char*, MAX_PATH_LENGTH);

        if (!bundlePath)
        {
            rsp.httpStatus = HttpStatusServiceUnavailable;
            return REQ_READY_SEND;
        }

        {
            U_SStream ssPath;
            U_sstream_init(&ssPath, bundlePath, MAX_PATH_LENGTH);
            U_sstream_put_str(&ssPath, qPrintable(loc));
            U_sstream_put_str(&ssPath, "/");
            U_sstream_put_str(&ssPath, bundleHashStr);
            U_sstream_put_str(&ssPath, ".ddf");
        }

        if (FS_OpenFile(&fp, FS_MODE_R, bundlePath))
        {
            FS_CloseFile(&fp);
            // TODO alreadly exists, delete and create fresh one (might have different signatures)
            FS_DeleteFile(bundlePath);
        }

        if (FS_OpenFile(&fp, FS_MODE_RW, bundlePath))
        {
            long n = FS_WriteFile(&fp, bs.data, bs.size);
            FS_CloseFile(&fp);
            if (n != bs.size)
            {
                rsp.httpStatus = HttpStatusBadRequest; // TODO different error
                return REQ_READY_SEND;
            }

            // notify device descriptions to trigger reload
            DEV_DDF_BundleUpdated((uint8_t*)&data[binStart], binEnd - binStart);

            DBG_Printf(DBG_INFO, "DDF bundle written: %s\n", bundlePath);

            rsp.httpStatus = HttpStatusOk;
            QVariantMap result;
            QVariantMap item;

            item["id"] = bundleHashStr;
            result["success"] = item;
            rsp.list.append(result);
            return REQ_READY_SEND;
        }
    }

    return REQ_NOT_HANDLED;
}

int REST_DDF_HandleApi(const ApiRequest &req, ApiResponse &rsp)
{
    // GET /api/<apikey>/ddf/descriptors
    if (req.hdr.pathComponentsCount() == 4 && req.hdr.httpMethod() == HttpGet && req.hdr.pathAt(3) == "descriptors")
    {
        return REST_DDF_GetDescriptors(req, rsp);
    }

    // GET /api/<apikey>/ddf/bundles/<sha256-hash>
    if (req.hdr.pathComponentsCount() == 5 && req.hdr.httpMethod() == HttpGet && req.hdr.pathAt(3) == "bundles")
    {
        return REST_DDF_GetBundle(req, rsp);
    }

    // GET /api/<apikey>/ddf/descriptors/<sha256-hash>
    if (req.hdr.pathComponentsCount() == 5 && req.hdr.httpMethod() == HttpGet && req.hdr.pathAt(3) == "descriptors")
    {
        return REST_DDF_GetDescriptor(req, rsp);
    }

    // POST /api/<apikey>/ddf/bundles
    if (req.hdr.pathComponentsCount() == 4 && req.hdr.httpMethod() == HttpPost && req.hdr.pathAt(3) == "bundles")
    {
        return REST_DDF_PostBundles(req, rsp);
    }

    return REQ_NOT_HANDLED;
}
