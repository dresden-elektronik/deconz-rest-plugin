/*
 * Copyright (c) 2025 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */

#include <stdint.h>
#include "actor/plugin.h"
#include "actor/cxx_helper.h"
#include "deconz/atom_table.h"
#include "deconz/am_vfs.h"
#include "deconz/dbg_trace.h"
#include "deconz/u_timer.h"
#include "deconz/u_sstream_ex.h"
#include "deconz/u_memory.h"
#include "device.h"
#include "utils/scratchmem.h"

#define VFS_MAX_URL_LENGTH 256
#define AM_ACTOR_ID_REST_PLUGIN 4001


DeviceContainer* DEV_GetDevices(); // in de_web_plugin.cpp

static struct am_actor am_actor_rest_plugin;
static struct am_api_functions *am = 0;


struct LS_Element
{
    struct LS_Element *next;
    int flags;
    char name[32];
};

void PL_NotifyDeviceEvent(const Device *device, const Resource *rsub, const char *what)
{
    if (!device || !what || !am)
        return;

    ScratchMemWaypoint swp;

    char *url = SCRATCH_ALLOC(char*, VFS_MAX_URL_LENGTH);
    U_ASSERT(url);
    if (!url)
        return;

    U_SStream ss;
    U_sstream_init(&ss, url, VFS_MAX_URL_LENGTH);

    U_sstream_put_str(&ss, "devices/");
    U_sstream_put_mac_address(&ss, device->key());
    U_sstream_put_str(&ss, "/");

    if (rsub)
    {
        const ResourceItem *item = rsub->item(RAttrUniqueId);
        if (!item)
            return;

        const char *uniqueId = item->toCString();
        if (!uniqueId[0])
            return;

        U_sstream_put_str(&ss, "subdevices/");
        U_sstream_put_str(&ss, uniqueId);
        U_sstream_put_str(&ss, "/");
    }

    U_sstream_put_str(&ss, what);

    struct am_message *m = am->msg_alloc();
    U_ASSERT(m);
    if (!m)
        return;

    am_u32 flags = 0;
    m->src = AM_ACTOR_ID_REST_PLUGIN;
    m->dst = AM_ACTOR_ID_SUBSCRIBERS;
    m->id = VFS_M_ID_CHANGED_NTFY;
    am->msg_put_cstring(m, url);
    am->msg_put_u32(m, flags);

    am->send_message(m);
}

uint64_t plMacFromUrlString(am_string str_mac)
{
    uint64_t mac;
    U_SStream ss;

    U_sstream_init(&ss, str_mac.data, str_mac.size);
    mac = U_sstream_get_mac_address(&ss);
    if (ss.status == U_SSTREAM_OK && mac != 0)
    {
        return mac;
    }

    return 0;
}

static Device *plGetDeviceForMac(uint64_t mac)
{
    DeviceContainer *devs = DEV_GetDevices();
    if (!devs)
        return nullptr;

    Device *device = DEV_GetDevice(*devs, mac);

    return device;
}

void plListElements(const char *prefix, const ResourceItem *item, LS_Element *ls)
{
    unsigned i;
    char buf[32] = {0};
    int is_dir = 0;
    unsigned prefixLen;
    unsigned suffixLen;
    const ResourceItemDescriptor &rid = item->descriptor();

    // first filter for matching prefix
    prefixLen = U_strlen(prefix);
    suffixLen = U_strlen(rid.suffix);
    if (suffixLen <= prefixLen)
        return;

    for (unsigned i = 0; i < prefixLen; i++)
    {
        if (prefix[i] != rid.suffix[i])
            return;
    }

    // go up to next seperator or if not present end of string
    const char *str = rid.suffix + prefixLen;

    for (i = 0; i < (sizeof(buf) - 1) && str[i]; i++)
    {
        if (str[i] == '/')
        {
            is_dir = 1;
            break;
        }
        buf[i] = str[i];
    }
    if (i == 0) // unlikely
        return;

    buf[i] = '\0';

    // check if the element is alreay in the list
    LS_Element *l = ls;
    for (;;l = l->next)
    {
        unsigned j;
        for (j = 0; l->name[j] == buf[j] && j < sizeof(buf); j++)
        {
            if (buf[j] == '\0')
                break;
        }

        if (j == i)
            return; // already in list


        if (!l->next)
            break;
    }

    // not found, create new list element
    // on first call the name is empty and that element is filled
    if (l->name[0])
    {
        l->next = SCRATCH_ALLOC(LS_Element*, sizeof(*l));
        l = l->next;
    }

    U_memcpy(l->name, buf, sizeof(buf));
    l->flags = 0;
    if (is_dir)
        l->flags = VFS_LS_DIR_ENTRY_FLAGS_IS_DIR;
    l->next = nullptr;
}

/*! Returns a list of unique elements for a specific path depth on ResourceItem suffixes.
 */
static LS_Element *plCreateItemElementListForPathIndex(am_url_parse *url_parse, Resource *r, unsigned pathAt)
{
    LS_Element *ls = SCRATCH_ALLOC(LS_Element*, sizeof(LS_Element));
    ls->next = nullptr;
    ls->name[0] = '\0';

    // 1) Build a prefix string which is used to match item suffixes.
    // pathAt points at the <suffix>:
    //   devices/<mac>/<suffix>   -> pathAt = 2
    //   devices/<mac>/subdevices/<uniqueid>/<suffix> --> pathAt = 4
    char *prefix = SCRATCH_ALLOC(char*, VFS_MAX_URL_LENGTH);
    prefix[0] = '\0';

    if (url_parse->element_count > pathAt)
    {
        char *p = prefix;
        for (unsigned i = pathAt; i < url_parse->element_count; i++)
        {
            // TODO U_SStream put string with size
            am_string elem = AM_UrlElementAt(url_parse, i);
            for (unsigned j = 0; j < elem.size; j++)
                *p++ = (char)elem.data[j];

            *p++ = '/';
            *p = '\0';
        }
    }

    // Match prefix against each item and put elements in the list for the next level.
    // For example if prefix is 'cap/color', following items:
    //   cap/color/ct/min
    //   cap/color/ct/max
    //   cap/color/xy/blue_x
    //   cap/color/xy/blue_y
    // would yield the list ["ct", "xy"] since these are unique in 'cap/color'.
    for (int i = 0; i < r->itemCount(); i++)
    {
        const ResourceItem *item = r->itemForIndex((size_t)i);
        plListElements(prefix, item, ls);
    }

    return ls;
}

static Resource *plGetSubDevice(Device *device, am_string subUniqueId)
{
    AT_AtomIndex atiUniueId;

    if (0 == AT_GetAtomIndex(subUniqueId.data, subUniqueId.size, &atiUniueId))
        return nullptr;

    const auto &subs = device->subDevices();

    Resource *sub = nullptr;
    for (Resource *r : subs)
    {
        const ResourceItem *item = r->item(RAttrUniqueId);
        if (item && item->atomIndex() == atiUniueId.index)
        {
            return r;
        }
    }

    return nullptr;
}

static void PL_ListDirectoryDevicesSubdevicesReq(struct am_message *m, am_ls_dir_req *req, Device *device)
{
    if (req->url_parse.element_count == 3) // devices/<mac>/subdevices
    {
        const auto &subs = device->subDevices();

        am->msg_put_u8(m, AM_RESPONSE_STATUS_OK);
        am->msg_put_u32(m, req->req_index);

        unsigned hdrPos = m->pos;
        am->msg_put_u32(m, 0); /* dummy next index */
        am->msg_put_u32(m, 2); /* dummy count */

        /*******************************************/

        unsigned count = 0;

        for (size_t i = 0; i < subs.size(); i++)
        {
            if (req->req_index <= i)
            {
                if (count == req->max_count)
                    continue;


                const ResourceItem *uniqueId = subs[i]->item(RAttrUniqueId);
                if (!uniqueId)
                    continue; // unlikely

                const char *uniqueIdStr = uniqueId->toCString();
                if (*uniqueIdStr == '\0')
                    continue;

                count++;

                am->msg_put_cstring(m, uniqueIdStr);
                am->msg_put_u16(m, VFS_LS_DIR_ENTRY_FLAGS_IS_DIR); /* flags */
                am->msg_put_u16(m, 0); /* icon */
            }
        }

        // fill in real header
        unsigned endPos = m->pos;
        m->pos = hdrPos;

        count += req->req_index;
        if (count == subs.size())
            am->msg_put_u32(m, 0); /* next index (done) */
        else
            am->msg_put_u32(m, count); /* next index */
        am->msg_put_u32(m, count);

        m->pos = endPos;
        return;
    }

    am->msg_put_u8(m, AM_RESPONSE_STATUS_NOT_FOUND);
    return;
}

static void PL_ListDirectoryDevices2Req(struct am_message *m, am_ls_dir_req *req)
{
    U_ASSERT(req->url_parse.element_count >= 2);

    if (VFS_MAX_URL_LENGTH < req->url_parse.url.size)
    {
        am->msg_put_u8(m, AM_RESPONSE_STATUS_FAIL);
        return;
    }

    const uint64_t mac = plMacFromUrlString(AM_UrlElementAt(&req->url_parse, 1));

    if (mac == 0)
    {
        am->msg_put_u8(m, AM_RESPONSE_STATUS_NOT_FOUND);
        return;
    }

    Device *device = plGetDeviceForMac(mac);
    Resource *subDevice = nullptr;

    if (!device)
    {
        am->msg_put_u8(m, AM_RESPONSE_STATUS_NOT_FOUND);
        return;
    }

    // check if this request is about subdevices
    if (req->url_parse.element_count >= 3 && AM_UrlElementAt(&req->url_parse, 2) == "subdevices")
    {
        // devices/<mac>/subdevices
        // devices/<mac>/subdevices/<item-suffix>


        if (req->url_parse.element_count == 3) // devices/<mac>/subdevices
        {
            PL_ListDirectoryDevicesSubdevicesReq(m, req, device);
            return;
        }
        else // devices/<mac>/subdevices/<item-suffix>
        {
            subDevice = plGetSubDevice(device, AM_UrlElementAt(&req->url_parse, 3));
            if (!subDevice)
            {
                am->msg_put_u8(m, AM_RESPONSE_STATUS_NOT_FOUND);
                return;
            }

            // handle listing items below
        }

    }

    // devices/<mac>
    // devices/<mac>/<item-suffix>
    // devices/<mac>/subdevices/<subdevice-uniqueid>
    // devices/<mac>/subdevices/<subdevice-uniqueid>/<item-suffix>
    if (req->url_parse.element_count >= 2)
    {
        am->msg_put_u8(m, AM_RESPONSE_STATUS_OK);
        am->msg_put_u32(m, req->req_index);

        LS_Element *ls;

        if (subDevice)
        {
            U_ASSERT(req->url_parse.element_count >= 4);
            ls = plCreateItemElementListForPathIndex(&req->url_parse, subDevice, 4);
        }
        else
        {
            ls = plCreateItemElementListForPathIndex(&req->url_parse, device, 2);
        }

        unsigned hdrPos = m->pos;
        am->msg_put_u32(m, 0); /* dummy next index */
        am->msg_put_u32(m, 2); /* dummy count */

        /*******************************************/

        unsigned i = 0;
        unsigned count = 0;

        for (LS_Element *l = ls; l && l->name[0]; l = l->next, i++)
        {
            if (req->req_index <= i)
            {
                if (count == req->max_count)
                    continue;

                count++;

                am->msg_put_cstring(m, l->name);
                am->msg_put_u16(m, l->flags); /* flags */
                if (l->flags & VFS_LS_DIR_ENTRY_FLAGS_IS_DIR)
                    am->msg_put_u16(m, 0); /* icon */
                else
                    am->msg_put_u16(m, 1); /* icon */
            }
        }

        // append /devices/<mac>/subdevices after items
        if (req->url_parse.element_count == 2) // devices/<mac>
        {
            if (req->req_index <= i && count < req->max_count)
            {
                if (!device->subDevices().empty())
                {
                    am->msg_put_cstring(m, "subdevices");
                    am->msg_put_u16(m, VFS_LS_DIR_ENTRY_FLAGS_IS_DIR); /* flags */
                    am->msg_put_u16(m, 0); /* icon */
                    count++;
                }
            }
        }

        // fill in real header
        unsigned endPos = m->pos;
        m->pos = hdrPos;

        am->msg_put_u32(m, 0); /* dummy next index */
        am->msg_put_u32(m, count);

        m->pos = endPos;
    }
    else
    {
        am->msg_put_u8(m, AM_RESPONSE_STATUS_NOT_FOUND);
    }
}

static void PL_ListDirectoryDevicesReq(struct am_message *m, am_ls_dir_req *req)
{
    if (req->url_parse.element_count == 1) // devices
    {
        am->msg_put_u8(m, AM_RESPONSE_STATUS_OK);
        am->msg_put_u32(m, req->req_index);

        am_u32 next_index = req->req_index;
        am_u32 count = 0;
        unsigned hdr_pos = m->pos;

        am->msg_put_u32(m, 0); /* dummy next index */
        am->msg_put_u32(m, 0); /* dummy count */

        /*******************************************/

        unsigned i = req->req_index;
        const DeviceContainer *devs = DEV_GetDevices();


        for (; devs && i < (unsigned)devs->size(); i++)
        {
            const Device *device = (*devs)[i].get();

            next_index++;

            if (count < req->max_count)
            {
                count++;
                char buf[28];
                U_SStream ss;
                U_sstream_init(&ss, buf, sizeof(buf));
                U_sstream_put_mac_address(&ss, device->key());

                am->msg_put_cstring(m, ss.str);
                am->msg_put_u16(m, VFS_LS_DIR_ENTRY_FLAGS_IS_DIR); /* flags */
                am->msg_put_u16(m, 0); /* icon */

                if (count == 16)
                    break;
            }
        }

        if (!devs || i == (unsigned)devs->size() || count == 0)
            next_index = 0;

        // fill in real header data
        unsigned pos1 = m->pos;
        m->pos = hdr_pos;

        am->msg_put_u32(m, next_index); /* no next index */
        am->msg_put_u32(m, count); /* count */

        m->pos = pos1;
    }
    else if (req->url_parse.element_count >= 2)
    {
        PL_ListDirectoryDevices2Req(m, req);
    }
    else
    {
        am->msg_put_u8(m, AM_RESPONSE_STATUS_NOT_FOUND);
    }
}

static int PL_ListDirectoryReq(struct am_message *msg)
{
    struct am_message *m;
    am_ls_dir_req req;

    if (AM_ParseListDirectoryRequest(am, msg, &req) != AM_MSG_STATUS_OK)
        return AM_CB_STATUS_INVALID;

    /* end of parsing */

    if (msg->status != AM_MSG_STATUS_OK)
        return AM_CB_STATUS_INVALID;

    m = am->msg_alloc();
    if (!m)
        return AM_CB_STATUS_MESSAGE_ALLOC_FAILED;

    m->src = msg->dst;
    m->dst = msg->src;
    m->id = VFS_M_ID_LIST_DIR_RSP;
    am->msg_put_u16(m, req.tag);

    if (req.url_parse.url.size == 0 && req.req_index == 0)
    {
        /*
         * root directory
         */
        am->msg_put_u8(m, AM_RESPONSE_STATUS_OK);
        am->msg_put_u32(m, req.req_index);
        am->msg_put_u32(m, 0); /* no next index */

        am->msg_put_u32(m, 2); /* count */
        /*************************************/
        am->msg_put_cstring(m, ".actor");
        am->msg_put_u16(m, VFS_LS_DIR_ENTRY_FLAGS_IS_DIR); /* flags */
        am->msg_put_u16(m, 0); /* icon */

        am->msg_put_cstring(m, "devices");
        am->msg_put_u16(m, VFS_LS_DIR_ENTRY_FLAGS_IS_DIR); /* flags */
        am->msg_put_u16(m, 0); /* icon */
    }
    else if (req.url_parse.element_count >= 1)
    {
        am_string elem1 = AM_UrlElementAt(&req.url_parse, 0);
        if (elem1 == "devices")
        {
            PL_ListDirectoryDevicesReq(m, &req);
        }
        else if (req.url_parse.url == ".actor" && req.req_index == 0 && req.url_parse.element_count == 1)
        {
            /*
             * hidden .actor directory
             */
            am->msg_put_u8(m, AM_RESPONSE_STATUS_OK);
            am->msg_put_u32(m, req.req_index);
            am->msg_put_u32(m, 0); /* no next index */

            am->msg_put_u32(m, 1); /* count */
            /*************************************/

            am->msg_put_cstring(m, "name");
            am->msg_put_u16(m, 0); /* flags */
            am->msg_put_u16(m, 1); /* icon */
        }
        else
        {
            am->msg_put_u8(m, AM_RESPONSE_STATUS_NOT_FOUND);
        }
    }
    else
    {
        am->msg_put_u8(m, AM_RESPONSE_STATUS_NOT_FOUND);
    }

    am->send_message(m);

    return AM_CB_STATUS_OK;
}

static void PL_ReadEntryDevicesReq(struct am_message *m, am_read_entry_req *req)
{
    //
    // return in this function means NOT_FOUND
    //

    if (req->url_parse.element_count > 3)
    {
        const uint64_t mac = plMacFromUrlString(AM_UrlElementAt(&req->url_parse, 1));

        if (mac == 0)
            return;

        unsigned suffixAt = 2;
        Device *device = plGetDeviceForMac(mac);
        Resource *r = device;

        if (!r)
            return;

        // devices/<mac>/subdevices/
        if (AM_UrlElementAt(&req->url_parse, 2) == "subdevices")
        {
            // devices/<mac>/subdevices/<subdevice-uniqueid>/<item-suffix>
            if (req->url_parse.element_count >= 4)
            {
                suffixAt = 4;
                r = plGetSubDevice(device, AM_UrlElementAt(&req->url_parse, 3));
            }

            if (!r)
                return;
        }

        ResourceItemDescriptor rid;

        {
            //                                 <------------->
            // devices/f0:d1:b8:be:24:0a:d5:6a/state/reachable
            // create suffix string
            am_string url = req->url_parse.url;
            am_string suffixStart = AM_UrlElementAt(&req->url_parse, suffixAt);
            U_ASSERT(suffixStart.size);
            intptr_t suffixLen = (url.data + url.size) - suffixStart.data;
            U_ASSERT(suffixLen > 1);

            QString suffix = QString::fromLatin1((char*)suffixStart.data, suffixLen);
            if (!getResourceItemDescriptor(suffix, rid))
                return;
        }

        const ResourceItem *item = r->item(rid.suffix);
        if (!item)
            return;

        am_u32 mode = 0;
        am_u64 mtime = 0;

        /*    DataTypeUnknown,
    DataTypeBool,
    DataTypeUInt8,
    DataTypeUInt16,
    DataTypeUInt32,
    DataTypeUInt64,
    DataTypeInt8,
    DataTypeInt16,
    DataTypeInt32,
    DataTypeInt64,
    DataTypeReal,
    DataTypeString,
    DataTypeTime,
    DataTypeTimePattern
         */
        if (item->lastSet().isValid())
            mtime = item->lastSet().toMSecsSinceEpoch();

        if (rid.type == DataTypeBool)
        {
            am->msg_put_cstring(m, "bool");
            am->msg_put_u32(m, mode);
            am->msg_put_u64(m, mtime);
            am->msg_put_u8(m, item->toBool() ? 1 : 0);
        }
        else if (rid.type == DataTypeUInt8)
        {
            am->msg_put_cstring(m, "u8");
            am->msg_put_u32(m, mode);
            am->msg_put_u64(m, mtime);
            am->msg_put_u8(m, (uint8_t)item->toNumber());
        }
        else if (rid.type == DataTypeUInt16)
        {
            am->msg_put_cstring(m, "u16");
            am->msg_put_u32(m, mode);
            am->msg_put_u64(m, mtime);
            am->msg_put_u16(m, (uint16_t)item->toNumber());
        }
        else if (rid.type == DataTypeUInt32)
        {
            am->msg_put_cstring(m, "u32");
            am->msg_put_u32(m, mode);
            am->msg_put_u64(m, mtime);
            am->msg_put_u32(m, (uint32_t)item->toNumber());
        }
        else if (rid.type == DataTypeUInt64)
        {
            am->msg_put_cstring(m, "u64");
            am->msg_put_u32(m, mode);
            am->msg_put_u64(m, mtime);
            am->msg_put_u64(m, (uint64_t)item->toNumber());
        }
        else if (rid.type == DataTypeInt8)
        {
            am->msg_put_cstring(m, "i8");
            am->msg_put_u32(m, mode);
            am->msg_put_u64(m, mtime);
            am->msg_put_s8(m, (int8_t)item->toNumber());
        }
        else if (rid.type == DataTypeInt16)
        {
            am->msg_put_cstring(m, "i16");
            am->msg_put_u32(m, mode);
            am->msg_put_u64(m, mtime);
            am->msg_put_s16(m, (int16_t)item->toNumber());
        }
        else if (rid.type == DataTypeInt32)
        {
            am->msg_put_cstring(m, "i32");
            am->msg_put_u32(m, mode);
            am->msg_put_u64(m, mtime);
            am->msg_put_s32(m, (int32_t)item->toNumber());
        }
        else if (rid.type == DataTypeUInt64)
        {
            am->msg_put_cstring(m, "i64");
            am->msg_put_u32(m, mode);
            am->msg_put_u64(m, mtime);
            am->msg_put_s64(m, (int64_t)item->toNumber());
        }
        else if (rid.type == DataTypeTime)
        {
            am->msg_put_cstring(m, "time");
            am->msg_put_u32(m, mode);
            am->msg_put_u64(m, mtime);
            am->msg_put_s64(m, (int64_t)item->toNumber());
        }
        else if (rid.type == DataTypeString)
        {
            am->msg_put_cstring(m, "str");
            am->msg_put_u32(m, mode);
            am->msg_put_u64(m, mtime);
            am->msg_put_cstring(m, item->toCString());
        }
    }

    return;
}

static int PL_ReadEntryReq(struct am_message *msg)
{
    struct am_message *m;
    am_read_entry_req req;

    if (AM_ParseReadEntryRequest(am, msg, &req) != AM_MSG_STATUS_OK)
        return AM_CB_STATUS_INVALID;

    uint32_t mode = VFS_ENTRY_MODE_READONLY;
    uint64_t mtime = 0;

    if (msg->status != AM_MSG_STATUS_OK)
        return AM_CB_STATUS_INVALID;

    m = am->msg_alloc();
    if (!m)
        return AM_CB_STATUS_MESSAGE_ALLOC_FAILED;

    am->msg_put_u16(m, req.tag);
    am->msg_put_u8(m, AM_RESPONSE_STATUS_OK);

    unsigned empty_pos = m->pos; // to check if entry was put into message

    am_string elem0 = AM_UrlElementAt(&req.url_parse, 0);

    if (req.url_parse.element_count == 1)
    {

    }
    else if (req.url_parse.element_count >= 2)
    {
        if (elem0 == "devices")
        {
            PL_ReadEntryDevicesReq(m, &req);
        }
        else if (elem0 == ".actor")
        {
            if (AM_UrlElementAt(&req.url_parse, 1) == "name")
            {
                am->msg_put_cstring(m, "str");
                am->msg_put_u32(m, mode);
                am->msg_put_u64(m, mtime);
                am->msg_put_cstring(m, "rest_plugin");
            }
        }
    }

    if (m->pos == empty_pos)
    {
        m->pos = 0;
        am->msg_put_u16(m, req.tag);
        am->msg_put_u8(m, AM_RESPONSE_STATUS_NOT_FOUND);

        am_string url = req.url_parse.url;
        DBG_Printf(DBG_VFS, "read entry NOT_FOUND: %.*s\n", url.size, url.data);
    }

    m->src = msg->dst;
    m->dst = msg->src;
    m->id = VFS_M_ID_READ_ENTRY_RSP;
    am->send_message(m);

    return AM_CB_STATUS_OK;
}

static int PL_MessageCallback(struct am_message *msg)
{
    ScratchMemWaypoint swp;

    if (msg->src == AM_ACTOR_ID_TIMERS)
    {
        DBG_Printf(DBG_INFO, "timer fired\n");
    }

    if (msg->id == VFS_M_ID_READ_ENTRY_REQ)
        return PL_ReadEntryReq(msg);

    if (msg->id == VFS_M_ID_LIST_DIR_REQ)
        return PL_ListDirectoryReq(msg);

    DBG_Printf(DBG_INFO, "rest_plugin: msg from: %u\n", msg->src);
    return AM_CB_STATUS_UNSUPPORTED;
}

extern "C" DECONZ_EXPORT
int am_plugin_init(struct am_api_functions *api)
{
    struct am_message *m;

    am = api;

    AM_INIT_ACTOR(&am_actor_rest_plugin, AM_ACTOR_ID_REST_PLUGIN, PL_MessageCallback);
    am->register_actor(&am_actor_rest_plugin);

    if (U_TimerStart(AM_ACTOR_ID_REST_PLUGIN, 1, 10000, 0))
    {

    }

    return 1;
}
