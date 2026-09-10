/*
 * Copyright (c) 2022-2024 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */

#ifdef USE_DUKTAPE_JS_ENGINE

#include <unistd.h>

#include "duktape.h"
#include "device_js.h"
#include "deconz/u_assert.h"
#include "deconz/u_arena.h"
#include "deconz/u_memory.h"
#include "deconz/aps.h"
#include "deconz/dbg_trace.h"
#include "device.h"
#include "resource.h"
#include "utils/utils.h"

#define U_UNUSED(x) (void)x
#define DJS_GLOBAL_ITEM_MAGIC -777

#define DJS_SENTINAL_ALLOCATED 0xAAAAAAAA
#define DJS_SENTINAL_FREED     0x55555555

static DeviceJs *_djs = nullptr; // singleton
static DeviceJsPrivate *_djsPriv = nullptr; // singleton

static unsigned statFreed;

class DeviceJsPrivate
{
public:
    U_Arena arena;
    // snapshot of the fully initialized arena
    std::vector<uint8_t> initial_context;
    int errFatal = 0;
    bool isReset = false;
    QString errString;
    QVariant result;
    duk_context *dukContext = nullptr;
    const deCONZ::ApsDataIndication *apsInd = nullptr;
    const deCONZ::ZclFrame *zclFrame = nullptr;
    const deCONZ::ZclAttribute *attr = nullptr;
    int attrIndex = 0;
    std::vector<ResourceItem*> itemsSet;
    Resource *resource = nullptr;
    ResourceItem *ritem = nullptr;
};

static const deCONZ::Node *getResourceCoreNode(const Resource *r)
{
    if (r)
    {
        const ResourceItem *uuid = r->item(RAttrUniqueId);

        if (uuid && !uuid->toString().isEmpty())
        {
            const uint64_t extAddr = extAddressFromUniqueId(uuid->toString());

            return DEV_GetCoreNode(extAddr);
        }
    }
    return nullptr;
}

void *U_duk_alloc(void *udata, duk_size_t size)
{
    U_UNUSED(udata);

    if (size == 0)
    {
        return NULL;
    }

    void *ptr;
    U_ASSERT(size > 0 && "expected size > 0");

    ptr = U_AllocArena(&_djsPriv->arena, size + 8, U_ARENA_ALIGN_8);
    U_ASSERT(ptr && "U_duk_alloc out of memory");
    if (!ptr)
    {
        return NULL;
    }
    U_ASSERT(((uintptr_t)ptr & 0x7) == 0); // must be 8 byte aligned boundary
    // put size before data for realloc
    uint32_t *hdr = (uint32_t*)ptr;
    hdr[0] = size;
    hdr[1] = DJS_SENTINAL_ALLOCATED; // mark allocated

    return (void*)((uint8_t*)ptr + 8);
}

void U_duk_free(void *udata, void *ptr)
{
    U_UNUSED(udata);
    if (ptr)
    {
        U_ASSERT(((uintptr_t)ptr & 0x7) == 0); // must be 8 byte aligned boundary
        uint32_t *hdr = (uint32_t*)ptr;
        hdr -= 2;
        U_ASSERT(hdr[1] == DJS_SENTINAL_ALLOCATED);
        statFreed += hdr[0];
        hdr[1] = DJS_SENTINAL_FREED; // mark as free
        // arena allocator doesn't really free ...
        // TODO(mpi): We could keep a free list to reduce memory consumption further in
        //            in low spec setups in future.
    }
}

void *U_duk_realloc(void *udata, void *ptr, duk_size_t new_size)
{
	uint8_t *p;
    uint8_t *beg;
    uint8_t *end;
    uint32_t *hdr;
    void *p_new;
    uint32_t bytes_to_copy;

    if (ptr == NULL)
    {
        return U_duk_alloc(udata, new_size);
    }

    if (new_size == 0)
    {
        /* man realloc:
           If size is equal to zero, and ptr is not NULL, then the call is equivalent to free(ptr)
        */

        U_duk_free(udata, ptr);
        return NULL;
    }

    // paranoid check that this is our memory
    {

        beg = (uint8_t*)_djsPriv->arena.buf;
        end = beg + _djsPriv->arena.size;
        p = (uint8_t*)ptr;

        U_ASSERT(beg < p);
        U_ASSERT(end > p);
    }

    {
        U_ASSERT(((uintptr_t)p & 0x7) == 0); // must be aligned to 8 byte boundary
        hdr = (uint32_t*)ptr;
        hdr -= 2;
        U_ASSERT(hdr[1] == DJS_SENTINAL_ALLOCATED);

        if (hdr[1] == DJS_SENTINAL_ALLOCATED && new_size <= hdr[0])
        {
            // buffer already large enough
            return ptr;
        }

        p_new = U_duk_alloc(udata, new_size);

        bytes_to_copy = new_size <= *hdr ? new_size : *hdr;
        U_ASSERT(bytes_to_copy <= new_size);
        U_memcpy(p_new, p, bytes_to_copy);

        U_duk_free(udata, ptr);
        ptr = p_new;
    }

    return ptr;
}

void U_duk_fatal(void *udata, const char *msg)
{
    U_UNUSED(udata);
    _djsPriv->errFatal = 1;
    _djsPriv->errString = QLatin1String(msg);
    DBG_Printf(DBG_JS, "%s: %s\n", __FUNCTION__, msg);
}

/* R.item(suffix)

   \param    suffix: String
   \returns  RItem object
 */
static duk_ret_t DJS_GetResourceItem(duk_context *ctx)
{
    int i;
    const char *suffix;
    int16_t item_index;
    Resource *r;
    unsigned len0;
    unsigned len1;
    ResourceItem *item;

    if (duk_is_string(ctx, 0) == 0)
    {
        return duk_type_error(ctx, "R.item(suffix) suffix MUST be a string");
    }

    r = _djsPriv->resource;
    suffix = duk_safe_to_string(ctx, 0);
    DBG_Printf(DBG_JS, "%s: -> R.item('%s')\n", __FUNCTION__, suffix);

    // search for ResourceItem ...
    item_index = -1;

    if (r)
    {
        len0 = strlen(suffix);
        for (i = 0; i < r->itemCount(); i++)
        {
            item = r->itemForIndex((size_t)i);
            len1 = strlen(item->descriptor().suffix);

            if (len1 == len0 && memcmp(suffix, item->descriptor().suffix, len0) == 0)
            {
                suffix = NULL;
                item_index = i;
                break;
            }
        }
    }

    duk_pop(ctx); /* safe string of 'suffix' */

    /* create new instance of RItem */
    duk_get_global_string(ctx, "RItem");
    /* TODO push args...*/

    duk_new(ctx, 0 /*nargs*/);

    /* Keep the index of ResourceItem in the Resource container. */
    duk_bool_t rc;
    duk_push_int(ctx, item_index);
    //duk_push_string(ctx, "asdasd");
    rc = duk_put_prop_string(ctx, -2, "ridx");
    U_ASSERT(rc == 1);

    U_ASSERT(duk_is_object(ctx, -1) != 0);
    return 1;  /* one return value */
}

static duk_ret_t DJS_GetResourceEndpoints(duk_context *ctx)
{
    printf("%s\n", __FUNCTION__);

    int i;
    duk_idx_t arr_idx;

    i = 0;
    arr_idx = duk_push_array(ctx);

    if (_djsPriv->resource)
    {
        const deCONZ::Node *node = getResourceCoreNode(_djsPriv->resource);
        if (node)
        {
            for (auto ep : node->endpoints())
            {
                duk_push_int(ctx, (int)ep);
                duk_put_prop_index(ctx, arr_idx, i);
                i++;
            }
        }
    }

    return 1;  /* one return value */
}

static duk_ret_t DJS_GetResourceHasCluster(duk_context *ctx)
{
    int ep;
    int cluster;
    int side = 0;
    int nargs = duk_get_top(ctx);

    if (nargs < 2)
    {
        return duk_type_error(ctx, "R.hasCluster(ep,cluster[,side]) invalid arguments");
    }

    if (duk_is_number(ctx, 0) == 0)
    {
        return duk_type_error(ctx, "R.hasCluster(ep,cluster[,side]) ep MUST be a number");
    }
    ep = duk_to_int(ctx, 0);

    if (duk_is_number(ctx, 1) == 0)
    {
        return duk_type_error(ctx, "R.hasCluster(ep,cluster,side) cluster MUST be a number");
    }
    cluster = duk_to_int(ctx, 1);

    if (nargs == 3)
    {
        if (duk_is_number(ctx, 2) == 0)
        {
            return duk_type_error(ctx, "R.hasCluster(ep,cluster,side) side MUST be a number");
        }
        side = duk_to_int(ctx, 2);
    }

    if (_djsPriv->resource)
    {
        const deCONZ::Node *node = getResourceCoreNode(_djsPriv->resource);
        if (node)
        {
            const deCONZ::SimpleDescriptor *sd = nullptr;

            for (size_t i = 0; i < node->simpleDescriptors().size(); i++)
            {
                sd = &node->simpleDescriptors()[i];
                if (sd->endpoint() != ep)
                {
                    continue;
                }

                const auto &clusters = (side == 0) ? sd->inClusters() : sd->outClusters();

                for (size_t j = 0; j < clusters.size(); j++)
                {
                    if (clusters[j].id() == cluster)
                    {
                        duk_push_boolean(ctx, 1);
                        return 1;
                    }
                }
            }
        }
    }

    duk_push_boolean(ctx, 0);
    return 1;  /* one return value */
}

/* Creates 'R' global scope object. */
static void DJS_InitGlobalResource(duk_context *ctx)
{
    const duk_function_list_entry my_module_funcs[] = {
        { "item", DJS_GetResourceItem, 1 /* 1 arg */ },
        { "hasCluster", DJS_GetResourceHasCluster, DUK_VARARGS /* 2..3 args */ },
        { NULL, NULL, 0 }
    };

    duk_push_global_object(ctx);
    duk_push_object(ctx);  /* -> [ ... global obj ] */
    duk_put_function_list(ctx, -1, my_module_funcs);

    /* R.endpoints */
    duk_push_string(ctx, "endpoints");
    duk_push_c_function(ctx, DJS_GetResourceEndpoints, 0 /*nargs*/);
    duk_def_prop(ctx,
                 -3,
                 DUK_DEFPROP_HAVE_GETTER);

    duk_put_prop_string(ctx, -2, "R");  /* -> [ ... global ] */
    duk_pop(ctx);
}

static duk_ret_t DJS_GetAttributeValue(duk_context *ctx)
{
    DBG_Printf(DBG_JS, "%s\n", __FUNCTION__);

    const deCONZ::ZclAttribute *attr;

    attr = _djsPriv->attr;

    if (!attr)
    {
        return duk_reference_error(ctx, "attribute not defined");
    }

    const int type = attr->dataType();

    if (type == deCONZ::ZclBoolean)
    {
        duk_push_boolean(ctx, attr->numericValue().u8 > 0 ? 1 : 0);
        return 1;  /* one return value */
    }

    // JS supports integers 2^52, therefore types above 48-bit are converted to strings
    switch (type)
    {
    case deCONZ::Zcl8BitBitMap:
    case deCONZ::Zcl8BitData:
    case deCONZ::Zcl8BitUint:
    case deCONZ::Zcl8BitEnum:
    case deCONZ::Zcl16BitBitMap:
    case deCONZ::Zcl16BitData:
    case deCONZ::Zcl16BitUint:
    case deCONZ::Zcl16BitEnum:
    case deCONZ::Zcl24BitBitMap:
    case deCONZ::Zcl24BitData:
    case deCONZ::Zcl24BitUint:
    case deCONZ::Zcl32BitBitMap:
    case deCONZ::Zcl32BitData:
    case deCONZ::Zcl32BitUint:
    case deCONZ::Zcl40BitBitMap:
    case deCONZ::Zcl40BitData:
    case deCONZ::Zcl40BitUint:
    case deCONZ::Zcl48BitBitMap:
    case deCONZ::Zcl48BitData:
    case deCONZ::Zcl48BitUint:
    {
        //return QVariant::fromValue(quint64(attr->numericValue().u64));
        duk_push_number(ctx, (double)attr->numericValue().u64);
        break;
    }

    case deCONZ::Zcl56BitBitMap:
    case deCONZ::Zcl56BitData:
    case deCONZ::Zcl56BitUint:
    // TODO(mpi): 64-bit types should be exposed as strings?!
    case deCONZ::Zcl64BitBitMap:
    case deCONZ::Zcl64BitUint:
    case deCONZ::Zcl64BitData:
    case deCONZ::ZclIeeeAddress:
    {
        //return QString::number(quint64(attr->numericValue().u64));
        duk_push_number(ctx, (double)attr->numericValue().u64);
        break;
    }

    case deCONZ::Zcl8BitInt:
    case deCONZ::Zcl16BitInt:
    case deCONZ::Zcl24BitInt:
    case deCONZ::Zcl32BitInt:
    case deCONZ::Zcl48BitInt:
    {
        duk_push_number(ctx, attr->toVariant().toDouble());
        break;
    }

    case deCONZ::Zcl56BitInt:
    case deCONZ::Zcl64BitInt:
    {
        // TODO(mpi): slow and dynamic allocations, replace by something faster
        duk_push_string(ctx, qPrintable(QString::number(qint64(attr->numericValue().s64))));
        break;
    }

    case deCONZ::ZclSingleFloat:
    {
        duk_push_number(ctx, attr->numericValue().real);
        break;
    }

    case deCONZ::ZclOctedString:
    {
        QString str = attr->toString();
        duk_push_string(ctx, qPrintable(str));
        break;
    }

    case deCONZ::ZclCharacterString:
    {
        QString str = attr->toString();
        duk_push_string(ctx, qPrintable(str));
        break;
    }

    default:
    {
        const QVariant var = attr->toVariant();
        if (var.isValid())
        {
            const QString str = var.toString();
            duk_push_string(ctx, qPrintable(str));
            // U_ASSERT(0 && "TODO(mpi): handle custom types");
        }
        else
        {
            duk_push_undefined(ctx);
        }
        break;
    }
    }

    return 1;  /* one return value */
}

static duk_ret_t DJS_GetAttributeId(duk_context *ctx)
{
    DBG_Printf(DBG_JS, "%s\n", __FUNCTION__);

    if (!_djsPriv->attr)
    {
        return duk_reference_error(ctx, "attribute not defined");
    }

    duk_push_int(ctx, _djsPriv->attr->id());
    return 1;  /* one return value */
}

static duk_ret_t DJS_GetAttributeIndex(duk_context *ctx)
{
    DBG_Printf(DBG_JS, "%s\n", __FUNCTION__);

    if (!_djsPriv->attr)
    {
        return duk_reference_error(ctx, "attribute not defined");
    }

    duk_push_int(ctx, _djsPriv->attrIndex);
    return 1;  /* one return value */
}

static duk_ret_t DJS_GetAttributeDataType(duk_context *ctx)
{
    DBG_Printf(DBG_JS, "%s\n", __FUNCTION__);

    if (!_djsPriv->attr)
    {
        return duk_reference_error(ctx, "attribute not defined");
    }

    duk_push_int(ctx, _djsPriv->attr->dataType());
    return 1;  /* one return value */
}

/* Creates 'Attr' global scope object. */
static void DJS_InitGlobalAttribute(duk_context *ctx)
{
    const duk_function_list_entry attr_module_funcs[] = {
        { NULL, NULL, 0 }
    };

    duk_push_global_object(ctx);
    duk_push_object(ctx);  /* -> [ ... global obj ] */
    duk_put_function_list(ctx, -1, attr_module_funcs);

    /* Attr.val */
    duk_push_string(ctx, "val");
    duk_push_c_function(ctx, DJS_GetAttributeValue, 0 /*nargs*/);
    duk_def_prop(ctx,
             -3,
             DUK_DEFPROP_HAVE_GETTER);

    /* Attr.id */
    duk_push_string(ctx, "id");
    duk_push_c_function(ctx, DJS_GetAttributeId, 0 /*nargs*/);
    duk_def_prop(ctx,
             -3,
             DUK_DEFPROP_HAVE_GETTER);

    /* Attr.index */
    duk_push_string(ctx, "index");
    duk_push_c_function(ctx, DJS_GetAttributeIndex, 0 /*nargs*/);
    duk_def_prop(ctx,
             -3,
             DUK_DEFPROP_HAVE_GETTER);

    /* Attr.dataType */
    duk_push_string(ctx, "dataType");
    duk_push_c_function(ctx, DJS_GetAttributeDataType, 0 /*nargs*/);
    duk_def_prop(ctx,
             -3,
             DUK_DEFPROP_HAVE_GETTER);

    duk_put_prop_string(ctx, -2, "Attr");  /* -> [ ... global ] */
    duk_pop(ctx);
}

static duk_ret_t DJS_GetZclFrameCmd(duk_context *ctx)
{
    DBG_Printf(DBG_JS, "%s\n", __FUNCTION__);

    if (!_djsPriv->zclFrame)
    {
        return duk_reference_error(ctx, "ZclFrame not defined");
    }

    duk_push_int(ctx, _djsPriv->zclFrame->commandId());
    return 1;  /* one return value */
}

static duk_ret_t DJS_GetZclFramePayloadSize(duk_context *ctx)
{
    DBG_Printf(DBG_JS, "%s\n", __FUNCTION__);

    if (!_djsPriv->zclFrame)
    {
        return duk_reference_error(ctx, "ZclFrame not defined");
    }

    duk_push_int(ctx, _djsPriv->zclFrame->payload().size());
    return 1;  /* one return value */
}

static duk_ret_t DJS_GetZclFrameIsClusterCommand(duk_context *ctx)
{
    DBG_Printf(DBG_JS, "%s\n", __FUNCTION__);

    if (!_djsPriv->zclFrame)
    {
        return duk_reference_error(ctx, "ZclFrame not defined");
    }

    duk_push_boolean(ctx, _djsPriv->zclFrame->isClusterCommand() ? 1 : 0);
    return 1;  /* one return value */
}

static duk_ret_t DJS_GetZclFramePayloadAt(duk_context *ctx)
{
    int i;
    const deCONZ::ZclFrame *zf;

    i = duk_get_int(ctx, 0);
//    DBG_Printf(DBG_JS, "%s index: %d\n", __FUNCTION__, i);

    zf = _djsPriv->zclFrame;

    if (zf)
    {
        if (i >= 0 && i < zf->payload().size())
        {
            duk_push_int(ctx, (uint8_t) zf->payload().at(i));
            return 1;  /* one return value */

        }

        return duk_range_error(ctx, "index out of range");
    }

    return duk_reference_error(ctx, "ZclFrame not defined");
}

/* Creates 'ZclFrame' global scope object. */
static void DJS_InitGlobalZclFrame(duk_context *ctx)
{
    const duk_function_list_entry attr_module_funcs[] = {
        { "at", DJS_GetZclFramePayloadAt, 1 /* 1 arg */ },
        { NULL, NULL, 0 }
    };

    duk_push_global_object(ctx);
    duk_push_object(ctx);  /* -> [ ... global obj ] */
    duk_put_function_list(ctx, -1, attr_module_funcs);

    /* ZclFrame.cmd */
    duk_push_string(ctx, "cmd");
    duk_push_c_function(ctx, DJS_GetZclFrameCmd, 0 /*nargs*/);
    duk_def_prop(ctx,
             -3,
             DUK_DEFPROP_HAVE_GETTER);

    /* ZclFrame.payloadSize */
    duk_push_string(ctx, "payloadSize");
    duk_push_c_function(ctx, DJS_GetZclFramePayloadSize, 0 /*nargs*/);
    duk_def_prop(ctx,
             -3,
             DUK_DEFPROP_HAVE_GETTER);

    /* ZclFrame.isClCmd */
    duk_push_string(ctx, "isClCmd");
    duk_push_c_function(ctx, DJS_GetZclFrameIsClusterCommand, 0 /*nargs*/);
    duk_def_prop(ctx,
             -3,
             DUK_DEFPROP_HAVE_GETTER);

    duk_put_prop_string(ctx, -2, "ZclFrame");  /* -> [ ... global ] */
    duk_pop(ctx);
}

/* Creates 'Item' global scope object. */
static void DJS_InitGlobalItem(duk_context *ctx)
{
    int i;
    int item_index;
    Resource *r;
    ResourceItem *item;

    duk_push_global_object(ctx);

    /* create new instance of RItem */
    duk_get_global_string(ctx, "RItem");
    duk_new(ctx, 0 /*nargs*/);
    U_ASSERT(duk_is_object(ctx, -1) != 0);

    /* Keep the index of ResourceItem into the Resource container. */
    item_index = -1;
    r = _djsPriv->resource;
    item = _djsPriv->ritem;

    if (r) // cumbersome way to get the item index..
    {
        for (i = 0; i < r->itemCount(); i++)
        {
            if (item == r->itemForIndex((size_t)i))
            {
                item_index = i;
                break;
            }
        }
    }

    duk_bool_t rc;
    duk_push_int(ctx, item_index);
    rc = duk_put_prop_string(ctx, -2, "ridx");
    U_ASSERT(rc == 1);

    duk_put_prop_string(ctx, -2, "Item");  /* -> [ ... global ] */
    duk_pop(ctx);
}

/* Returns ResourceItem based on this.ridx property of the RItem.
   That is the index of the ResourceItem in the Resource.items container.
   If it is -1 rItem is returned.
 */
static ResourceItem *DJS_GetItemIndexHelper(duk_context * ctx)
{
    duk_bool_t rc;
    int16_t item_index;
    ResourceItem *item = nullptr;

    item_index = -1;
    duk_push_this(ctx);
    duk_push_string(ctx, "ridx");
    rc = duk_get_prop(ctx, -2);
    U_ASSERT(rc == 1);
    if (rc == 1)
    {
        item_index = duk_get_int(ctx, -1);
    }
    duk_pop(ctx); /* prop.ridx */
    duk_pop(ctx); /* this */

    if (item_index >= 0 && _djsPriv->resource)
    {
        item = _djsPriv->resource->itemForIndex((size_t)item_index);
    }
    else if (_djsPriv->ritem)
    {
        item = _djsPriv->ritem;
    }

    return item;
}

static duk_ret_t DJS_GetItemVal(duk_context *ctx)
{
    ResourceItem *item;

    item = DJS_GetItemIndexHelper(ctx);

    if (item)
    {
        DBG_Printf(DBG_JS, "%s: %s\n", __FUNCTION__, item->descriptor().suffix);
        const ApiDataType type = item->descriptor().type;
        if (type == DataTypeBool)
        {
            duk_push_boolean(ctx, item->toBool() ? 1 : 0);
        }
        else if (type == DataTypeString || type == DataTypeTime ||  type == DataTypeTimePattern)
        {
            duk_push_string(ctx, qPrintable(item->toString()));
        }
        else if (type == DataTypeUInt8 || type == DataTypeUInt16 || type == DataTypeUInt32 ||
                 type == DataTypeInt8 || type == DataTypeInt16 || type == DataTypeInt32)
        {
            duk_push_number(ctx, (double)item->toNumber());
        }
        else if (type == DataTypeInt64 || type == DataTypeUInt64)
        {

            duk_push_string(ctx, qPrintable(QString::number(item->toNumber())));
        }
        else
        {
            return duk_type_error(ctx, "unsupported ApiDataType");
        }

        return 1;  /* one return value */
    }

    return duk_reference_error(ctx, "item not defined");
}

static duk_ret_t DJS_SetItemVal(duk_context *ctx)
{
    ResourceItem *item;

    item = DJS_GetItemIndexHelper(ctx);

    if (item)
    {
        bool ok = false;
        if (duk_is_boolean(ctx, 0))
        {
            bool val = duk_to_boolean(ctx, 0);
            DBG_Printf(DBG_JS, "%s: %s --> %u\n", __FUNCTION__, item->descriptor().suffix, val ? 1 : 0);
            ok = item->setValue(val, ResourceItem::SourceDevice);
            duk_pop(ctx); /* conversion result*/
        }
        else if (duk_is_number(ctx, 0))
        {
            double num = duk_to_number(ctx, 0);
            DBG_Printf(DBG_JS, "%s: %s --> %f\n", __FUNCTION__, item->descriptor().suffix, num);
            ok = item->setValue(QVariant(num), ResourceItem::SourceDevice);
            duk_pop(ctx); /* conversion result*/
        }
        else if (duk_is_string(ctx, 0))
        {
            duk_size_t out_len = 0;
            const char *str = duk_to_lstring(ctx, 0, &out_len);
            U_ASSERT(str);
            if (out_len)
            {
                DBG_Printf(DBG_JS, "%s: %s --> %s\n", __FUNCTION__, item->descriptor().suffix, str);
                ok = item->setValue(QString(QLatin1String(str, out_len)), ResourceItem::SourceDevice);
            }
            else
            {
                ok = item->setValue(QString(""), ResourceItem::SourceDevice);
            }
            duk_pop(ctx); /* conversion result*/
        }
        else
        {
            const char *str = duk_safe_to_string(ctx, 0);
            DBG_Printf(DBG_JS, "%s: failed to set %s --> '%s' (unsupported)\n", __FUNCTION__, item->descriptor().suffix, str);
            duk_pop(ctx); /* conversion result*/
        }

        if (!ok)
        {
            DBG_Printf(DBG_DDF, "JS failed to set Item.val for %s\n", item->descriptor().suffix);
            return duk_type_error(ctx, "failed to set Item.val");
        }
        else
        {
            DeviceJS_ResourceItemValueChanged(item);
            return 0;
        }
    }

    return duk_reference_error(ctx, "item not defined");
}

static duk_ret_t DJS_GetItemName(duk_context *ctx)
{
    ResourceItem *item;

    item = DJS_GetItemIndexHelper(ctx);

    if (item)
    {
        duk_push_string(ctx, item->descriptor().suffix);
        return 1;  /* one return value */
    }

    return duk_reference_error(ctx, "item not defined");
}

/* RItem constructor */
static duk_ret_t DJS_ItemConstructor(duk_context *ctx)
{
    if (!duk_is_constructor_call(ctx)) {
        return DUK_RET_TYPE_ERROR;
    }

//    DBG_Printf(DBG_JS, "%s\n", __FUNCTION__);

    return 0;  /* use default instance */
}


/* Creates 'RItem' function prototype at global scope. */
static void DJS_InitResourceItemPrototype(duk_context * ctx)
{
    duk_push_c_function(ctx, DJS_ItemConstructor, 0 /*nargs*/);
    duk_push_object(ctx);

    /* prototype.val */
    duk_push_string(ctx, "val");
    duk_push_c_function(ctx, DJS_GetItemVal, 0 /*nargs*/);
    duk_push_c_function(ctx, DJS_SetItemVal, 1 /*nargs*/);
    duk_def_prop(ctx,
                 -4,
                 DUK_DEFPROP_HAVE_GETTER |
                 DUK_DEFPROP_HAVE_SETTER);

    duk_push_string(ctx, "name");
    duk_push_c_function(ctx, DJS_GetItemName, 0 /*nargs*/);
    duk_def_prop(ctx,
                 -3,
                 DUK_DEFPROP_HAVE_GETTER);

    duk_put_prop_string(ctx, -2, "prototype");
    duk_put_global_string(ctx, "RItem");

}

/* Utils.padStart(str, maxLength, fillString)

   Polyfill for ECMAScript String.prototype.padStart(targetLength, padString)
   https://tc39.es/ecma262/multipage/text-processing.html#sec-string.prototype.padstart
 */
static duk_ret_t DJS_UtilsPadStart(duk_context *ctx)
{
    int nargs;
    int maxLength;
    const char *arg_string;
    duk_size_t stringLength;
    const char *fillString;
    duk_size_t fillStringLength;
    int fillLen;

    nargs = duk_get_top(ctx);
    arg_string = NULL;
    stringLength = 0;
    fillString = NULL;
    fillStringLength = 0;

    std::string result;

    if (nargs < 2)
    {
        return duk_type_error(ctx, "Utils.padStart(str, maxLength [,fillString]) missing args");
    }

    /* arg: str */
    int type = duk_get_type(ctx, 0);
    if (duk_is_string(ctx, 0) == 0 && type == DUK_TYPE_STRING)
    {
        return duk_type_error(ctx, "Utils.padStart(str, _ [, _]) str MUST be a string");
    }

    arg_string = duk_get_lstring(ctx, 0, &stringLength);
    if (!arg_string || stringLength == 0)
    {
        return duk_type_error(ctx, "Utils.padStart(str, _ [, _]) str.length MUST be > 0");
    }

    // 2. Let intMaxLength be R(? ToLength(maxLength)).
    if (duk_is_number(ctx, 1) == 0)
    {
        return duk_type_error(ctx, "Utils.padStart(str, maxLength [, _]) maxLength MUST be a number");
    }

    maxLength = duk_get_int(ctx, 1);
    if (maxLength > 32) // cap
    {
        return duk_type_error(ctx, "Utils.padStart(str, maxLength [, _]) maxLength MUST be < 32");
    }

    // 4. If intMaxLength <= stringLength, return S.
    if (maxLength <= (int)stringLength)
    {
        duk_push_string(ctx, arg_string);
        return 1;  /* one return value */
    }

    if (nargs == 3)
    {
        if (duk_is_string(ctx, 2) == 0)
        {
            return duk_type_error(ctx, "Utils.padStart(str, maxLength, fillString) fillString MUST be a string");
        }

        fillString = duk_get_lstring(ctx, 2, &fillStringLength);

        // 7. If filler is the empty String, return S.
        if (fillStringLength == 0)
        {
            duk_push_string(ctx, arg_string);
            return 1;  /* one return value */
        }
    }
    else
    {
        // 5. If fillString is undefined, let filler be the String value consisting solely of the code unit 0x0020 (SPACE).
        fillString = " ";
        fillStringLength = 1;
    }

    result.reserve(maxLength);
    fillLen = maxLength - stringLength;

    // 9. Let truncatedStringFiller be the String value consisting of repeated concatenations of filler truncated to length fillLen.
    for (int i = 0; i < fillLen; i++)
    {
        result.append(&fillString[i % fillStringLength], 1);
    }

    result = result.append(arg_string);
    duk_push_string(ctx, result.c_str());

    return 1;  /* one return value */
}

/* Creates 'Utils' global scope object. */
static void DJS_InitGlobalUtils(duk_context *ctx)
{
    const duk_function_list_entry attr_module_funcs[] = {
        { "padStart", DJS_UtilsPadStart, DUK_VARARGS /* 2-3 args */ },
        { NULL, NULL, 0 }
    };

    duk_push_global_object(ctx);
    duk_push_object(ctx);  /* -> [ ... global obj ] */
    duk_put_function_list(ctx, -1, attr_module_funcs);

    duk_put_prop_string(ctx, -2, "Utils");  /* -> [ ... global ] */
    duk_pop(ctx);
}

void DJS_InitDuktape(DeviceJsPrivate *d)
{
    void *user_ptr = NULL;
    duk_context *ctx;

    ctx = duk_create_heap(U_duk_alloc,
                          U_duk_realloc,
                          U_duk_free,
                          user_ptr,
                          U_duk_fatal);

    d->dukContext = ctx;

    DJS_InitResourceItemPrototype(ctx);
    DJS_InitGlobalResource(ctx);
    DJS_InitGlobalAttribute(ctx);
    DJS_InitGlobalZclFrame(ctx);
    DJS_InitGlobalUtils(ctx);

    // Polyfills
    const char *PF_String_prototype_padStart = "String.prototype.padStart = String.prototype.padStart || "
                                               "function (targetLength, padString) { return Utils.padStart(this.toString(), targetLength, padString); } ";

    if (duk_peval_string(ctx, PF_String_prototype_padStart) != 0)
    {
        const char *str = duk_safe_to_string(ctx, -1);
        DBG_Printf(DBG_JS, "failed to apply String.prototype.padStart polyfill: %s\n", str);
    }
    duk_pop(ctx);

    // Qt JS engine used Utils.log10 as it was missing in Math.log10
    // keep existing JS working until updated
    if (duk_peval_string(ctx, "Utils.log10 = Math.log10") != 0)
    {
        const char *str = duk_safe_to_string(ctx, -1);
        DBG_Printf(DBG_JS, "failed to apply Utils.log10 = Math.log10: %s\n", str);
    }
    duk_pop(ctx);

    U_ASSERT(d->arena.size > 0);

    // snaphot of the memory state to jump back on reset()
    d->initial_context.reserve(d->arena.size);
    d->initial_context.resize(d->arena.size);
    U_memcpy(d->initial_context.data(), d->arena.buf, d->arena.size);
}

DeviceJs::DeviceJs() :
    d(new DeviceJsPrivate)
{
    Q_ASSERT(_djs == nullptr); // enforce singleton

    _djsPriv = d.get();
    _djs = this;

    U_InitArena(&d->arena, U_KILO_BYTES(2048));
    DJS_InitDuktape(d.get());
}

DeviceJs::~DeviceJs()
{
    U_FreeArena(&d->arena);
    d->dukContext = nullptr;
    _djs = nullptr;
    _djsPriv = nullptr;
}

DeviceJs *DeviceJs::instance()
{
    Q_ASSERT(_djs);
    return _djs;
}

const std::vector<ResourceItem *> &DeviceJs::itemsSet() const
{
    return d->itemsSet;
}

/* Keep track of all resource items which are set via:
   Item.val = 1 and R.item('..').val = 2)
   during JS evaluation.
 */
void DeviceJS_ResourceItemValueChanged(ResourceItem *item)
{
    U_ASSERT(_djsPriv);
    U_ASSERT(item);

    const auto i = std::find(_djsPriv->itemsSet.cbegin(), _djsPriv->itemsSet.cend(), item);

    if (i == _djsPriv->itemsSet.cend())
    {
        //DBG_Printf(DBG_JS, "%s: %s\n", __FUNCTION__, item->descriptor().suffix);
        _djsPriv->itemsSet.push_back(item);
    }
}

/*r

   ES5 limitations:

   - no 'let' use 'var' instead
   - no arrow functions, (a) => {}, use function(a) {} instead

*/

JsEvalResult DeviceJs::evaluate(const QString &expr)
{
    duk_context *ctx;

    ctx = d->dukContext;

    U_ASSERT(ctx);
    U_ASSERT(d->isReset);

    DBG_Printf(DBG_JS, "DJS evaluate()\n");

    // require that the DeviceJs::reset() has been called
    if (!ctx || !d->isReset)
    {
        DBG_Printf(DBG_ERROR, "calles DeviceJs::evaluate() without prior reset, skip\n");
        d->result = {};
        return JsEvalResult::Error;
    }

    d->errFatal = 0;
    d->isReset = false;

    if (d->ritem)
    {
        DJS_InitGlobalItem(ctx);
    }

    {
        int srcEp = 255;
        int clusterId = 0xFFFF;
        if (d->apsInd)
        {
            srcEp = d->apsInd->srcEndpoint();
            clusterId = d->apsInd->clusterId();
        }

        int ret;

        duk_push_int(ctx, srcEp);
        ret = duk_put_global_string(ctx, "SrcEp");
        U_ASSERT(ret == 1);

        duk_push_int(ctx, clusterId);
        ret = duk_put_global_string(ctx, "ClusterId");
        U_ASSERT(ret == 1);
    }

    if (duk_peval_string(ctx, expr.toUtf8().constData()) != 0)
    {
        d->errString = duk_safe_to_string(ctx, -1);
        return JsEvalResult::Error;
    }

    if (d->errFatal)
    {
        // TODO(mpi): can likely be removed, shouldn't happen anymore due duk_peval_string()
        return JsEvalResult::Error;
    }
    else if (duk_is_error(ctx, -3))
    {
        /* Inherits from Error, attempt to print stack trace */
        duk_get_prop_string(ctx, -3, "stack");
        d->errString = QLatin1String(duk_safe_to_string(ctx, -1));
        return JsEvalResult::Error;
    }

    if (duk_is_number(ctx, -1))
    {
        d->result = duk_to_number(ctx, -1); // double
    }
    else if (duk_is_boolean(ctx, -1))
    {
        d->result = duk_to_boolean(ctx, -1) ? true : false;
    }
    else
    {
        d->result = duk_safe_to_string(ctx, -1);
    }

    if (DBG_IsEnabled(DBG_JS))
    {
        DBG_Printf(DBG_JS, "DJS result  %s, memory peak: %lu bytes, freed: %u\n", duk_safe_to_string(ctx, -1), d->arena.size - (unsigned)d->initial_context.size(), statFreed);
    }

    duk_pop(ctx);

    return JsEvalResult::Ok;
}

JsEvalResult DeviceJs::testCompile(const QString &expr)
{
    duk_context *ctx;
    JsEvalResult result = JsEvalResult::Error;


    if (expr.isEmpty())
    {
        return result;
    }

    reset();

    ctx = d->dukContext;
    d->errFatal = 0;
    d->isReset = false;

    ResourceItemDescriptor rInvalidItemDescriptor;

    if (!getResourceItemDescriptor(RInvalidSuffix, rInvalidItemDescriptor))
    {
        return result;
    }
    ResourceItem dummyItem(rInvalidItemDescriptor);
    d->ritem = &dummyItem;

    if (d->ritem)
    {
        DJS_InitGlobalItem(ctx);
    }

    duk_uint_t flags = 0;
    if (duk_pcompile_string(ctx, flags, expr.toUtf8().constData()) != 0)
    {
        d->errString = duk_safe_to_string(ctx, -1);
    }
    else
    {
        result = JsEvalResult::Ok;
    }

    return result;
}

void DeviceJs::setResource(Resource *r)
{
    d->resource = r;
}

void DeviceJs::setResource(const Resource *r)
{
    d->resource = const_cast<Resource*>(r);
}

void DeviceJs::setApsIndication(const deCONZ::ApsDataIndication &ind)
{
    d->apsInd = &ind;
}

void DeviceJs::setZclFrame(const deCONZ::ZclFrame &zclFrame)
{
    d->zclFrame = &zclFrame;
}

void DeviceJs::setZclAttribute(int attrIndex, const deCONZ::ZclAttribute &attr)
{
    d->attrIndex = attrIndex;
    d->attr = &attr;
}

void DeviceJs::setItem(ResourceItem *item)
{
    d->ritem = item;
}

void DeviceJs::setItem(const ResourceItem *item)
{
    d->ritem = const_cast<ResourceItem*>(item);
}

QVariant DeviceJs::result()
{
    return _djsPriv->result;
}

void DeviceJs::reset()
{
    d->apsInd = nullptr;
    d->ritem = nullptr;
    d->resource = nullptr;
    d->attr = nullptr;
    d->attrIndex = 0;
    d->zclFrame = nullptr;
    d->isReset = true;
    d->result = {};
    d->errString.clear();

    statFreed = 0;
    U_ASSERT(d->dukContext);
    U_ASSERT(d->arena.size > 0);
    U_ASSERT(d->initial_context.size() > 0);

    /* Restore snapshot ca. 200 KB!
       Measurements yield 0 ms, memcpy is highly optimized and the arena memory
       is in L2/L3 cache restoring the snapshot is very fast.
     */

    // DBG_MEASURE_START(DJS_Reset);

    U_memcpy(d->arena.buf, d->initial_context.data(), d->initial_context.size());
    d->arena.size = d->initial_context.size();

    // DBG_MEASURE_END(DJS_Reset);
}

void DeviceJs::clearItemsSet()
{
    d->itemsSet.clear();
}

QString DeviceJs::errorString() const
{
    return d->errString;
}

#endif // USE_DUKTAPE_JS_ENGINE
