/*
 * Copyright (c) 2022-2024 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */

#include <array>
#include <QDirIterator>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QSettings>
#include <QTimer>
#include <deconz/atom_table.h>
#include <deconz/dbg_trace.h>
#include <deconz/file.h>
#include <deconz/u_assert.h>
#include <deconz/u_sstream_ex.h>
#include <deconz/buffer_pool.h>
#include <deconz/u_memory.h>
#include <deconz/u_time.h>
#include <deconz/u_ecc.h>
#include "database.h"
#include "device_ddf_bundle.h"
#include "device_ddf_init.h"
#include "device_descriptions.h"
#include "device_js/device_js.h"
#include "utils/scratchmem.h"
#include "json.h"
#include "event.h"
#include "resource.h"

#define DDF_MAX_PATH_LENGTH 1024
#define DDF_MAX_PUBLIC_KEYS 64

#define HND_MIN_LOAD_COUNTER 1
#define HND_MAX_LOAD_COUNTER 15
#define HND_MAX_DESCRIPTIONS 16383
#define HND_MAX_ITEMS        1023
#define HND_MAX_SUB_DEVS     15

/*! \union ItemHandlePack

    Packs location to an DDF item into a opaque 32-bit unsigned int handle.
    The DDF item lookup complexity is O(1) via DDF_GetItem() function.
 */
union ItemHandlePack
{
    // 32-bit memory layout
    // llll dddd dddd dddd ddss ssii iiii iiii
    struct {
        //! Max: 15, check for valid handle, for each DDF reload the counter is incremented (wraps to 0).
        unsigned int loadCounter : 4;
        unsigned int description : 14; //! Max: 16383, index into descriptions[].
        unsigned int subDevice: 4;     //! Max: 15, index into description -> subdevices[]
        unsigned int item : 10;        //! Max: 1023, index into subdevice -> items[]
    };
    uint32_t handle;
};

static unsigned atDDFPolicyLatestPreferStable;
static unsigned atDDFPolicyLatest;
static unsigned atDDFPolicyPin;
static unsigned atDDFPolicyRawJson;

static DeviceDescriptions *_instance = nullptr;
static DeviceDescriptionsPrivate *_priv = nullptr;

class DDF_ParseContext
{
public:
    deCONZ::StorageLocation fileLocation;
    char filePath[DDF_MAX_PATH_LENGTH];
    unsigned filePathLength = 0;
    uint8_t *fileData = nullptr;
    unsigned fileDataSize = 0;
    std::array<cj_token, 8192> tokens;
    DDFB_ExtfChunk *extChunks;
    int64_t bundleLastModified;
    uint64_t signatures; // bitmap as index in publicKeys[] array

    uint32_t scratchPos = 0;
    std::array<unsigned char, 1 << 20> scratchMem; // 1.05 MB

    // stats
    int n_rawDDF = 0;
    int n_devIdentifiers = 0;
};

struct ConstantEntry
{
    AT_AtomIndex key;
    AT_AtomIndex value;
};


/*
 * Lookup which DDFs already have been queried.
 *
 */
enum DDF_LoadState
{
    DDF_LoadStateScheduled,
    DDF_LoadStateLoaded
};

enum DDF_ReloadWhat
{
    DDF_ReloadIdle,
    DDF_ReloadBundles,
    DDF_ReloadAll
};

struct DDF_LoadRecord
{
    AT_AtomIndex modelid;
    AT_AtomIndex mfname;
    uint32_t mfnameLowerCaseHash;
    DDF_LoadState loadState;
};

class DeviceDescriptionsPrivate
{
public:
    uint loadCounter = HND_MIN_LOAD_COUNTER;

    std::vector<ConstantEntry> constants2;
    std::vector<DeviceDescription::Item> genericItems;
    std::vector<DeviceDescription> descriptions;

    DeviceDescription invalidDescription;
    DeviceDescription::Item invalidItem;
    DeviceDescription::SubDevice invalidSubDevice;

    QStringList enabledStatusFilter;

    std::vector<DDF_SubDeviceDescriptor> subDevices;

    std::vector<DDF_FunctionDescriptor> readFunctions;
    std::vector<DDF_FunctionDescriptor> writeFunctions;
    std::vector<DDF_FunctionDescriptor> parseFunctions;

    std::vector<DDF_LoadRecord> ddfLoadRecords;
    std::vector<U_ECC_PublicKeySecp256k1> publicKeys;

    DDF_ReloadWhat ddfReloadWhat = DDF_ReloadIdle;
    QTimer *ddfReloadTimer = nullptr;
};

static int DDF_ReadFileInMemory(DDF_ParseContext *pctx);
static int DDF_ReadConstantsJson(DDF_ParseContext *pctx, std::vector<ConstantEntry> & constants);
static DeviceDescription::Item DDF_ReadItemFile(DDF_ParseContext *pctx);
static DeviceDescription DDF_ReadDeviceFile(DDF_ParseContext *pctx);
static DDF_SubDeviceDescriptor DDF_ReadSubDeviceFile(DDF_ParseContext *pctx);
static DeviceDescription DDF_MergeGenericItems(const std::vector<DeviceDescription::Item> &genericItems, const DeviceDescription &ddf);
static int DDF_MergeGenericBundleItems(DeviceDescription &ddf, DDF_ParseContext *pctx);
static int DDF_ProcessSignatures(DDF_ParseContext *pctx, std::vector<U_ECC_PublicKeySecp256k1> &publicKeys, U_BStream *bs, uint32_t *bundleHash);
static DeviceDescription::Item *DDF_GetItemMutable(const ResourceItem *item);
static void DDF_UpdateItemHandlesForIndex(std::vector<DeviceDescription> &descriptions, uint loadCounter, size_t index);
static void DDF_TryCompileAndFixJavascript(QString *expr, const QString &path);
DeviceDescription DDF_LoadScripts(const DeviceDescription &ddf);

/*
 * https://maskray.me/blog/2023-04-12-elf-hash-function
 *
 * PJW hash adapted from musl libc.
 *
 * TODO(mpi): make this a own module U_StringHash()
 */
static uint32_t DDF_StringHash(const void *s0, unsigned size)
{
    uint32_t h;
    const unsigned char *s;

    h = 0;
    s = (const unsigned char*)s0;

    while (size--)
    {
        h = 16 * h + *s++;
        h ^= h >> 24 & 0xF0;
    }
    return h & 0xfffffff;
}

/*! Helper to create a 32-bit string hash from an atom string.

    This is mainly used to get a unique number to compare case insensitive manufacturer names.
    For example for "HEIMAN", "heiman" and "Heiman" atoms this function returns the same hash.
 */
static uint32_t DDF_AtomLowerCaseStringHash(AT_AtomIndex ati)
{
    unsigned len;
    AT_Atom atom;
    char str[192];

    str[0] = '\0';
    atom = AT_GetAtomByIndex(ati);

    if (atom.len == 0)
        return 0;

    if (sizeof(str) <= atom.len) // should not happen
        return DDF_StringHash(atom.data, atom.len);

    for (len = 0; len < atom.len; len++)
    {
        uint8_t ch = atom.data[len];

        if (ch & 0x80) // non ASCII UTF-8 string, don't bother
            return DDF_StringHash(atom.data, atom.len);

        if (ch >= 'A' && ch <= 'Z')
            ch += (unsigned char)('a' - 'A');

        str[len] = (char)ch;
    }

    str[len] = '\0';
    return DDF_StringHash(str, len);
}

/*! Constructor. */
DeviceDescriptions::DeviceDescriptions(QObject *parent) :
    QObject(parent),
    d_ptr2(new DeviceDescriptionsPrivate)
{
    _instance = this;
    _priv = d_ptr2;

    d_ptr2->ddfReloadTimer = new QTimer(this);
    d_ptr2->ddfReloadTimer->setSingleShot(true);
    connect(d_ptr2->ddfReloadTimer, &QTimer::timeout, this, &DeviceDescriptions::ddfReloadTimerFired);

    {
        // register DDF policy atoms used later on for fast comparisons
        AT_AtomIndex ati;
        const char *str;

        str = "latest_prefer_stable";
        AT_AddAtom(str, U_strlen(str), &ati);
        atDDFPolicyLatestPreferStable = ati.index;

        str = "latest";
        AT_AddAtom(str, U_strlen(str), &ati);
        atDDFPolicyLatest = ati.index;

        str = "pin";
        AT_AddAtom(str, U_strlen(str), &ati);
        atDDFPolicyPin = ati.index;

        str = "raw_json";
        AT_AddAtom(str, U_strlen(str), &ati);
        atDDFPolicyRawJson = ati.index;
    }

    {
        /*
         * Register offical public keys for beta and stable signed bundles.
         * These are used to to select bundles according to the attr/ddf_policy
         */
        U_ECC_PublicKeySecp256k1 pk;
        uint8_t stable_key[33] = {
            0x03, 0x93, 0x2D, 0x60, 0xA3, 0x35, 0x44, 0xFD, 0xB9, 0x20, 0x2B, 0x41, 0xA7, 0x68, 0xCD, 0xD8,
            0x70, 0x90, 0x82, 0xBD, 0xE8, 0xCD, 0x85, 0x47, 0x21, 0x68, 0xC5, 0x2A, 0xD8, 0xC3, 0xE5, 0x76, 0xF6 };

        uint8_t beta_key[33] = {
            0x02, 0xAB, 0x93, 0x42, 0x38, 0x60, 0xD3, 0x9D, 0x2C, 0xDC, 0xBC, 0xA0, 0xF9, 0x04, 0x2B, 0xD1,
            0xA2, 0x45, 0xED, 0xB6, 0xDC, 0xC1, 0x0C, 0x4C, 0xFF, 0x1B, 0x78, 0xE9, 0xF2, 0x43, 0xF5, 0x3F, 0x1E };

        U_memcpy(pk.key, stable_key, sizeof(pk.key));
        d_ptr2->publicKeys.push_back(pk);

        U_memcpy(pk.key, beta_key, sizeof(pk.key));
        d_ptr2->publicKeys.push_back(pk);
    }

    {  // Parse function as shown in the DDF editor.
        DDF_FunctionDescriptor fn;
        fn.name = "zcl:attr";
        fn.description = "Generic function to parse ZCL attributes.";

        DDF_FunctionDescriptor::Parameter param;

        param.name = "Endpoint";
        param.key = "ep";
        param.description = "255 means any endpoint, 0 means auto selected from subdevice.";
        param.dataType = DataTypeUInt8;
        param.defaultValue = 0;
        param.isOptional = 1;
        param.isHexString = 0;
        param.supportsArray = 0;
        fn.parameters.push_back(param);

        param.name = "Cluster ID";
        param.key = "cl";
        param.description = "As string hex value";
        param.dataType = DataTypeUInt16;
        param.defaultValue = 0;
        param.isOptional = 0;
        param.isHexString = 1;
        param.supportsArray = 0;
        fn.parameters.push_back(param);

        param.name = "Attribute ID";
        param.key = "at";
        param.description = "As string hex value";
        param.dataType = DataTypeUInt16;
        param.defaultValue = 0;
        param.isOptional = 0;
        param.isHexString = 1;
        param.supportsArray = 0;
        fn.parameters.push_back(param);

        param.name = "Manufacturer code";
        param.key = "mf";
        param.description = "As string hex value.";
        param.dataType = DataTypeUInt16;
        param.defaultValue = 0;
        param.isOptional = 1;
        param.isHexString = 1;
        param.supportsArray = 0;
        fn.parameters.push_back(param);

        param.name = "Javascript file";
        param.key = "script";
        param.description = "Relative path of a Javascript .js file.";
        param.dataType = DataTypeString;
        param.defaultValue = {};
        param.isOptional = 1;
        param.isHexString = 0;
        param.supportsArray = 0;
        fn.parameters.push_back(param);

        param.name = "Expression";
        param.key = "eval";
        param.description = "Javascript expression to transform the raw value.";
        param.dataType = DataTypeString;
        param.defaultValue = QLatin1String("Item.val = Attr.val");
        param.isOptional = 1;
        param.isHexString = 0;
        param.supportsArray = 0;
        fn.parameters.push_back(param);

        d_ptr2->parseFunctions.push_back(fn);
    }

    {  // Read function as shown in the DDF editor.
        DDF_FunctionDescriptor fn;
        fn.name = "zcl:attr";
        fn.description = "Generic function to read ZCL attributes.";

        DDF_FunctionDescriptor::Parameter param;

        param.name = "Endpoint";
        param.key = "ep";
        param.description = "255 means any endpoint, 0 means auto selected from subdevice.";
        param.dataType = DataTypeUInt8;
        param.defaultValue = 255;
        param.isOptional = 0;
        param.isHexString = 0;
        param.supportsArray = 0;
        fn.parameters.push_back(param);

        param.name = "Cluster ID";
        param.key = "cl";
        param.description = "As string hex value";
        param.dataType = DataTypeUInt16;
        param.defaultValue = 0;
        param.isOptional = 0;
        param.isHexString = 1;
        param.supportsArray = 0;
        fn.parameters.push_back(param);

        param.name = "Attribute ID";
        param.key = "at";
        param.description = "As string hex value";
        param.dataType = DataTypeUInt16;
        param.defaultValue = 0;
        param.isOptional = 0;
        param.isHexString = 1;
        param.supportsArray = 1;
        fn.parameters.push_back(param);

        param.name = "Manufacturer code";
        param.key = "mf";
        param.description = "As string hex value.";
        param.dataType = DataTypeUInt16;
        param.defaultValue = 0;
        param.isOptional = 1;
        param.isHexString = 1;
        param.supportsArray = 0;
        fn.parameters.push_back(param);

        d_ptr2->readFunctions.push_back(fn);
    }
    
    {  // Write function as shown in the DDF editor.
        DDF_FunctionDescriptor fn;
        fn.name = "zcl:attr";
        fn.description = "Generic function to write ZCL attributes.";

        DDF_FunctionDescriptor::Parameter param;

        param.name = "Endpoint";
        param.key = "ep";
        param.description = "255 means any endpoint, 0 means auto selected from subdevice.";
        param.dataType = DataTypeUInt8;
        param.defaultValue = 0;
        param.isOptional = 1;
        param.isHexString = 0;
        param.supportsArray = 0;
        fn.parameters.push_back(param);

        param.name = "Cluster ID";
        param.key = "cl";
        param.description = "As string hex value";
        param.dataType = DataTypeUInt16;
        param.defaultValue = 0;
        param.isOptional = 0;
        param.isHexString = 1;
        param.supportsArray = 0;
        fn.parameters.push_back(param);

        param.name = "Attribute ID";
        param.key = "at";
        param.description = "As string hex value";
        param.dataType = DataTypeUInt16;
        param.defaultValue = 0;
        param.isOptional = 0;
        param.isHexString = 1;
        param.supportsArray = 0;
        fn.parameters.push_back(param);
        
        param.name = "Datatype";
        param.key = "dt";
        param.description = "Datatype of the data to be written.";
        param.dataType = DataTypeUInt8;
        param.defaultValue = 0;
        param.isOptional = 0;
        param.isHexString = 1;
        param.supportsArray = 0;
        fn.parameters.push_back(param);

        param.name = "Manufacturer code";
        param.key = "mf";
        param.description = "As string hex value.";
        param.dataType = DataTypeUInt16;
        param.defaultValue = 0;
        param.isOptional = 1;
        param.isHexString = 1;
        param.supportsArray = 0;
        fn.parameters.push_back(param);

        param.name = "Javascript file";
        param.key = "script";
        param.description = "Relative path of a Javascript .js file.";
        param.dataType = DataTypeString;
        param.defaultValue = {};
        param.isOptional = 1;
        param.isHexString = 0;
        param.supportsArray = 0;
        fn.parameters.push_back(param);

        param.name = "Expression";
        param.key = "eval";
        param.description = "Javascript expression to transform the raw value.";
        param.dataType = DataTypeString;
        param.defaultValue = QLatin1String("Item.val;");
        param.isOptional = 1;
        param.isHexString = 0;
        param.supportsArray = 0;
        fn.parameters.push_back(param);

        d_ptr2->writeFunctions.push_back(fn);
    }
    
    {
        DDF_FunctionDescriptor fn;
        fn.name = "zcl:cmd";
        fn.description = "Generic function to parse ZCL commands.";

        DDF_FunctionDescriptor::Parameter param;

        param.name = "Endpoint";
        param.key = "ep";
        param.description = "255 means any endpoint, 0 means auto selected from subdevice.";
        param.dataType = DataTypeUInt8;
        param.defaultValue = 0;
        param.isOptional = 1;
        param.isHexString = 0;
        param.supportsArray = 0;
        fn.parameters.push_back(param);

        param.name = "Cluster ID";
        param.key = "cl";
        param.description = "As string hex value.";
        param.dataType = DataTypeUInt16;
        param.defaultValue = 0;
        param.isOptional = 0;
        param.isHexString = 1;
        param.supportsArray = 0;
        fn.parameters.push_back(param);

        param.name = "Command ID";
        param.key = "cmd";
        param.description = "As string hex value.";
        param.dataType = DataTypeUInt8;
        param.defaultValue = 0;
        param.isOptional = 0;
        param.isHexString = 1;
        param.supportsArray = 0;
        fn.parameters.push_back(param);

        param.name = "Manufacturer code";
        param.key = "mf";
        param.description = "As string hex value.";
        param.dataType = DataTypeUInt16;
        param.defaultValue = 0;
        param.isOptional = 1;
        param.isHexString = 1;
        param.supportsArray = 0;
        fn.parameters.push_back(param);

        param.name = "Javascript file";
        param.key = "script";
        param.description = "Relative path of a Javascript .js file.";
        param.dataType = DataTypeString;
        param.defaultValue = {};
        param.isOptional = 1;
        param.isHexString = 0;
        param.supportsArray = 0;
        fn.parameters.push_back(param);

        param.name = "Expression";
        param.key = "eval";
        param.description = "Javascript expression to transform the raw value.";
        param.dataType = DataTypeString;
        param.defaultValue = 0;
        param.isOptional = 1;
        param.isHexString = 0;
        param.supportsArray = 0;
        fn.parameters.push_back(param);

        d_ptr2->parseFunctions.push_back(fn);
    }
    
    {
        DDF_FunctionDescriptor fn;
        fn.name = "zcl:cmd";
        fn.description = "Generic function to read ZCL commands.";

        DDF_FunctionDescriptor::Parameter param;

        param.name = "Endpoint";
        param.key = "ep";
        param.description = "255 means any endpoint, 0 means auto selected from subdevice.";
        param.dataType = DataTypeUInt8;
        param.defaultValue = 255;
        param.isOptional = 0;
        param.isHexString = 0;
        param.supportsArray = 0;
        fn.parameters.push_back(param);

        param.name = "Cluster ID";
        param.key = "cl";
        param.description = "As string hex value.";
        param.dataType = DataTypeUInt16;
        param.defaultValue = 0;
        param.isOptional = 0;
        param.isHexString = 1;
        param.supportsArray = 0;
        fn.parameters.push_back(param);

        param.name = "Command ID";
        param.key = "cmd";
        param.description = "As string hex value.";
        param.dataType = DataTypeUInt8;
        param.defaultValue = 0;
        param.isOptional = 0;
        param.isHexString = 1;
        param.supportsArray = 0;
        fn.parameters.push_back(param);

        param.name = "Manufacturer code";
        param.key = "mf";
        param.description = "As string hex value.";
        param.dataType = DataTypeUInt16;
        param.defaultValue = 0;
        param.isOptional = 1;
        param.isHexString = 1;
        param.supportsArray = 0;
        fn.parameters.push_back(param);
        
        param.name = "Javascript file";
        param.key = "script";
        param.description = "Relative path of a Javascript .js file.";
        param.dataType = DataTypeString;
        param.defaultValue = {};
        param.isOptional = 1;
        param.isHexString = 0;
        param.supportsArray = 0;
        fn.parameters.push_back(param);

        param.name = "Expression";
        param.key = "eval";
        param.description = "Javascript expression to transform the raw value.";
        param.dataType = DataTypeString;
        param.defaultValue = 0;
        param.isOptional = 1;
        param.isHexString = 0;
        param.supportsArray = 0;
        fn.parameters.push_back(param);

        d_ptr2->readFunctions.push_back(fn);
    }
    
    {
        DDF_FunctionDescriptor fn;
        fn.name = "zcl:cmd";
        fn.description = "Generic function to send ZCL commands.";

        DDF_FunctionDescriptor::Parameter param;

        param.name = "Endpoint";
        param.key = "ep";
        param.description = "255 means any endpoint, 0 means auto selected from subdevice.";
        param.dataType = DataTypeUInt8;
        param.defaultValue = 0;
        param.isOptional = 1;
        param.isHexString = 0;
        param.supportsArray = 0;
        fn.parameters.push_back(param);

        param.name = "Cluster ID";
        param.key = "cl";
        param.description = "As string hex value.";
        param.dataType = DataTypeUInt16;
        param.defaultValue = 0;
        param.isOptional = 0;
        param.isHexString = 1;
        param.supportsArray = 0;
        fn.parameters.push_back(param);

        param.name = "Command ID";
        param.key = "cmd";
        param.description = "As string hex value.";
        param.dataType = DataTypeUInt8;
        param.defaultValue = 0;
        param.isOptional = 0;
        param.isHexString = 1;
        param.supportsArray = 0;
        fn.parameters.push_back(param);

        param.name = "Manufacturer code";
        param.key = "mf";
        param.description = "As string hex value.";
        param.dataType = DataTypeUInt16;
        param.defaultValue = 0;
        param.isOptional = 1;
        param.isHexString = 1;
        param.supportsArray = 0;
        fn.parameters.push_back(param);

        param.name = "Javascript file";
        param.key = "script";
        param.description = "Relative path of a Javascript .js file.";
        param.dataType = DataTypeString;
        param.defaultValue = {};
        param.isOptional = 1;
        param.isHexString = 0;
        param.supportsArray = 0;
        fn.parameters.push_back(param);

        param.name = "Expression";
        param.key = "eval";
        param.description = "Javascript expression to transform the raw value.";
        param.dataType = DataTypeString;
        param.defaultValue = 0;
        param.isOptional = 1;
        param.isHexString = 0;
        param.supportsArray = 0;
        fn.parameters.push_back(param);

        d_ptr2->writeFunctions.push_back(fn);
    }
    
    {
        DDF_FunctionDescriptor fn;
        fn.name = "tuya";
        fn.description = "Generic function to parse Tuya data.";

        DDF_FunctionDescriptor::Parameter param;

        param.name = "Datapoint";
        param.key = "dpid";
        param.description = "1-255 the datapoint ID.";
        param.dataType = DataTypeUInt8;
        param.defaultValue = 0;
        param.isOptional = 0;
        param.isHexString = 0;
        param.supportsArray = 0;
        fn.parameters.push_back(param);

        param.name = "Javascript file";
        param.key = "script";
        param.description = "Relative path of a Javascript .js file.";
        param.dataType = DataTypeString;
        param.defaultValue = {};
        param.isOptional = 1;
        param.isHexString = 0;
        param.supportsArray = 0;
        fn.parameters.push_back(param);

        param.name = "Expression";
        param.key = "eval";
        param.description = "Javascript expression to transform the raw value.";
        param.dataType = DataTypeString;
        param.defaultValue = QLatin1String("Item.val = Attr.val");
        param.isOptional = 1;
        param.isHexString = 0;
        param.supportsArray = 0;
        fn.parameters.push_back(param);

        d_ptr2->parseFunctions.push_back(fn);
    }

    {
        DDF_FunctionDescriptor fn;
        fn.name = "tuya";
        fn.description = "Generic function to read all Tuya datapoints. It has no parameters.";
        d_ptr2->readFunctions.push_back(fn);
    }

    {
        DDF_FunctionDescriptor fn;
        fn.name = "tuya";
        fn.description = "Generic function to write Tuya data.";

        DDF_FunctionDescriptor::Parameter param;

        param.name = "Datapoint";
        param.key = "dpid";
        param.description = "1-255 the datapoint ID.";
        param.dataType = DataTypeUInt8;
        param.defaultValue = 0;
        param.isOptional = 0;
        param.isHexString = 0;
        param.supportsArray = 0;
        fn.parameters.push_back(param);
        
        param.name = "Datatype";
        param.key = "dt";
        param.description = "Datatype of the data to be written.";
        param.dataType = DataTypeUInt8;
        param.defaultValue = 0;
        param.isOptional = 0;
        param.isHexString = 1;
        param.supportsArray = 0;
        fn.parameters.push_back(param);

        param.name = "Expression";
        param.key = "eval";
        param.description = "Javascript expression to transform the raw value.";
        param.dataType = DataTypeString;
        param.defaultValue = QLatin1String("Item.val;");
        param.isOptional = 1;
        param.isHexString = 0;
        param.supportsArray = 0;
        fn.parameters.push_back(param);

        d_ptr2->writeFunctions.push_back(fn);
    }

    {
        DDF_FunctionDescriptor fn;
        fn.name = "ias:zonestatus";
        fn.description = "Generic function to parse IAS ZONE status change notifications or zone status from read/report command.";

        DDF_FunctionDescriptor::Parameter param;

        param.name = "IAS Zone status mask";
        param.key = "mask";
        param.description = "Sets the bitmask for Alert1 and Alert2 item of the IAS Zone status.";
        param.dataType = DataTypeString;
        param.defaultValue = QLatin1String("alarm1,alarm2");
        param.isOptional = 1;
        param.isHexString = 0;
        param.supportsArray = 0;
        fn.parameters.push_back(param);

        d_ptr2->parseFunctions.push_back(fn);
    }

    {
        DDF_FunctionDescriptor fn;
        fn.name = "numtostr";
        fn.description = "Generic function to to convert number to string.";

        DDF_FunctionDescriptor::Parameter param;

        param.name = "Source item";
        param.key = "srcitem";
        param.description = "The source item holding the number.";
        param.dataType = DataTypeString;
        param.defaultValue = 0;
        param.isOptional = 0;
        param.isHexString = 0;
        param.supportsArray = 0;
        fn.parameters.push_back(param);

        param.name = "Operator";
        param.key = "op";
        param.description = "Comparison operator (lt | le | eq | gt | ge)";
        param.dataType = DataTypeString;
        param.defaultValue = 0;
        param.isOptional = 0;
        param.isHexString = 0;
        param.supportsArray = 0;
        fn.parameters.push_back(param);

        param.name = "Mapping";
        param.key = "to";
        param.description = "Array of (num, string) mappings";
        param.dataType = DataTypeString;
        param.defaultValue = 0;
        param.isOptional = 0;
        param.isHexString = 0;
        param.supportsArray = 1;
        fn.parameters.push_back(param);

        d_ptr2->parseFunctions.push_back(fn);
    }

    {
        DDF_FunctionDescriptor fn;
        fn.name = "time";
        fn.description = "Specialized function to parse time, local and last set time from read/report commands of the time cluster and auto-sync time if needed.";

        d_ptr2->parseFunctions.push_back(fn);
    }

    {
        DDF_FunctionDescriptor fn;
        fn.name = "xiaomi:special";
        fn.description = "Generic function to parse custom Xiaomi attributes and commands.";

        DDF_FunctionDescriptor::Parameter param;

        param.name = "Endpoint";
        param.key = "ep";
        param.description = "Source endpoint of the incoming command, default value 255 means any endpoint.";
        param.dataType = DataTypeUInt8;
        param.defaultValue = 255;
        param.isOptional = 1;
        param.isHexString = 0;
        param.supportsArray = 0;
        fn.parameters.push_back(param);

        param.name = "Attribute ID";
        param.key = "at";
        param.description = "The attribute to parse, shall be 0xff01, 0xff02 or 0x00f7";
        param.dataType = DataTypeUInt16;
        param.defaultValue = 0;
        param.isOptional = 0;
        param.isHexString = 1;
        param.supportsArray = 0;
        fn.parameters.push_back(param);

        param.name = "Index";
        param.key = "idx";
        param.description = "A 8-bit string hex value.";
        param.dataType = DataTypeUInt8;
        param.defaultValue = 0;
        param.isOptional = 0;
        param.isHexString = 1;
        param.supportsArray = 0;
        fn.parameters.push_back(param);

        param.name = "Expression";
        param.key = "eval";
        param.description = "Javascript expression to transform the raw value.";
        param.dataType = DataTypeString;
        param.defaultValue = {};
        param.isOptional = 0;
        param.isHexString = 0;
        param.supportsArray = 0;
        fn.parameters.push_back(param);

        d_ptr2->parseFunctions.push_back(fn);
    }
}

/*! Query the database which mfname/modelid pairs are present.
    Use these to load only DDFs and bundles which are in use.
 */
void DeviceDescriptions::prepare()
{
    auto &records = _priv->ddfLoadRecords;
    const auto res = DB_LoadIdentifierPairs();

    for (size_t i = 0; i < res.size(); i++)
    {
        size_t j = 0;
        for (j = 0; j < records.size(); j++)
        {
            if (records[j].mfname.index == res[i].mfnameAtomIndex &&
                records[j].modelid.index == res[i].modelIdAtomIndex)
            {
                break;
            }
        }

        if (j == records.size())
        {
            DDF_LoadRecord rec;
            rec.mfname.index = res[i].mfnameAtomIndex;
            rec.mfnameLowerCaseHash = DDF_AtomLowerCaseStringHash(rec.mfname);
            rec.modelid.index = res[i].modelIdAtomIndex;
            rec.loadState = DDF_LoadStateScheduled;
            records.push_back(rec);
        }
    }
}

/*! Destructor. */
DeviceDescriptions::~DeviceDescriptions()
{
    Q_ASSERT(_instance == this);
    _instance = nullptr;
    _priv = nullptr;
    Q_ASSERT(d_ptr2);
    delete d_ptr2;
    d_ptr2 = nullptr;
}

void DeviceDescriptions::setEnabledStatusFilter(const QStringList &filter)
{
    if (d_ptr2->enabledStatusFilter != filter)
    {
        d_ptr2->enabledStatusFilter = filter;
        DBG_Printf(DBG_INFO, "DDF enabled for %s status\n", qPrintable(filter.join(QLatin1String(", "))));
    }
}

const QStringList &DeviceDescriptions::enabledStatusFilter() const
{
    return d_ptr2->enabledStatusFilter;
}

/*! Returns the DeviceDescriptions singleton instance.
 */
DeviceDescriptions *DeviceDescriptions::instance()
{
    Q_ASSERT(_instance);
    return _instance;
}

bool DDF_IsStatusEnabled(const QString &status)
{
    if (_priv)
    {
        return _priv->enabledStatusFilter.contains(status, Qt::CaseInsensitive);
    }
    return false;
}

/*! Helper to transform hard C++ coded parse functions to DDF.
 */
void DDF_AnnoteZclParse1(int line, const char *file, const Resource *resource, ResourceItem *item, quint8 ep, quint16 clusterId, quint16 attributeId, const char *eval)
{
    DBG_Assert(resource);
    DBG_Assert(item);
    DBG_Assert(eval);

    if (!_instance || !resource || !item || !eval)
    {
        return;
    }

    if (item->ddfItemHandle() == DeviceDescription::Item::InvalidItemHandle)
    {
        const Device *device = nullptr;
        if (resource->parentResource())
        {
            device = static_cast<const Device*>(resource->parentResource());
        }

        if (!device)
        {
            return;
        }

        const auto *uniqueId = resource->item(RAttrUniqueId);
        if (!uniqueId)
        {
            return;
        }

        auto &ddf = _instance->get(device);
        if (!ddf.isValid())
        {
            return;
        }

        // this is pretty heavy but will be removed later
        const QStringList u = uniqueId->toString().split(QLatin1Char('-'), SKIP_EMPTY_PARTS);

        for (const auto &sub : ddf.subDevices)
        {
            if (u.size() != sub.uniqueId.size())
            {
                continue;
            }

            bool ok = true;
            for (int i = 1; i < qMin(u.size(), sub.uniqueId.size()); i++)
            {
                if (u[i].toUInt(0, 16) != sub.uniqueId[i].toUInt(0, 16))
                {
                    ok = false;
                }
            }

            if (!ok)
            {
                continue;
            }

            for (const auto &ddfItem : sub.items)
            {
                if (ddfItem.name == item->descriptor().suffix)
                {
                    item->setDdfItemHandle(ddfItem.handle);
                    break;
                }
            }

            break;
        }
    }

    if (item->ddfItemHandle() != DeviceDescription::Item::InvalidItemHandle)
    {
        DeviceDescription::Item *ddfItem = DDF_GetItemMutable(item);

        if (ddfItem && ddfItem->isValid())
        {
            if (ddfItem->parseParameters.isNull())
            {
                char buf[255];

                QVariantMap param;
                param[QLatin1String("ep")] = int(ep);
                snprintf(buf, sizeof(buf), "0x%04X", clusterId);
                param[QLatin1String("cl")] = QLatin1String(buf);
                snprintf(buf, sizeof(buf), "0x%04X", attributeId);
                param[QLatin1String("at")] = QLatin1String(buf);
                param[QLatin1String("eval")] = QLatin1String(eval);

                size_t fileLen = strlen(file);
                const char *fileName = file + fileLen;

                for (size_t i = fileLen; i > 0; i--, fileName--)
                {
                    if (*fileName == '/')
                    {
                        fileName++;
                        break;
                    }
                }

                snprintf(buf, sizeof(buf), "%s:%d", fileName, line);
                param[QLatin1String("cppsrc")] = QLatin1String(buf);

                ddfItem->parseParameters = param;

                DBG_Printf(DBG_DDF, "DDF %s:%d: %s updated ZCL function cl: 0x%04X, at: 0x%04X, eval: %s\n", fileName, line, qPrintable(resource->item(RAttrUniqueId)->toString()), clusterId, attributeId, eval);
            }
        }
    }
}

void DeviceDescriptions::handleEvent(const Event &event)
{
    if (event.what() == REventDDFInitRequest)
    {
        handleDDFInitRequest(event);
    }
    else if (event.what() == REventDDFReload)
    {
        if (event.num() == 0)
        {
        }
    }
}

/*! Get the DDF object for a \p resource.
    \returns The DDF object, DeviceDescription::isValid() to check for success.
 */
const DeviceDescription &DeviceDescriptions::get(const Resource *resource, DDF_MatchControl match)
{
    U_ASSERT(resource);

    Q_D(const DeviceDescriptions);

    const ResourceItem *modelidItem = resource->item(RAttrModelId);
    const ResourceItem *mfnameItem = resource->item(RAttrManufacturerName);
    const ResourceItem *typeItem = resource->item(RAttrType);

    /*
     * Collect all matching DDFs.
     * The result is than sorted and according to the attr/ddf_policy the best candidate
     * will be selected.
     */
    unsigned matchedCount = 0;
    static std::array<int, 16> matchedIndices;

    U_ASSERT(modelidItem);
    U_ASSERT(mfnameItem);

    if (typeItem)
    {
        const char *type = typeItem->toCString();
        if (type[0] == 'Z' && type[1] == 'G')
        {
            return d->invalidDescription; // TODO(mpi): For now ZGP devices aren't supported in DDF
        }

        if (type[0] == 'C' && type[1] == 'o' && type[2] == 'n')
        {
            return d->invalidDescription; // filter "Configuration tool" aka the coordinator
        }
    }

    unsigned modelidAtomIndex = modelidItem->atomIndex();
    unsigned mfnameAtomIndex = mfnameItem->atomIndex();

    if (modelidAtomIndex == 0 || mfnameAtomIndex == 0)
    {
        return d->invalidDescription; // happens when called from legacy init code addLightNode() etc.
    }

    U_ASSERT(modelidAtomIndex != 0);
    U_ASSERT(mfnameAtomIndex != 0);

    uint32_t mfnameLowerCaseHash = DDF_AtomLowerCaseStringHash(AT_AtomIndex{mfnameAtomIndex});

    /*
     * Filter matching DDFs, there can be multiple entries for the same modelid and manufacturer name.
     * Further sorting for the 'best' match according to attr/ddf_policy is done afterwards.
     */
    {
        auto i = d->descriptions.begin();

        for (;matchedCount < matchedIndices.size();)
        {
            i = std::find_if(i, d->descriptions.end(), [modelidAtomIndex, mfnameAtomIndex, mfnameLowerCaseHash](const DeviceDescription &ddf)
            {
                if (ddf.mfnameAtomIndices.size() != ddf.modelidAtomIndices.size())
                {
                    return false; // should not happen
                }

                for (size_t j = 0; j < ddf.modelidAtomIndices.size(); j++)
                {
                    if (ddf.modelidAtomIndices[j] == modelidAtomIndex)
                    {
                        if (ddf.mfnameAtomIndices[j] == mfnameAtomIndex) // exact manufacturer name match
                            return true;

                        // tolower() manufacturer name match
                        uint32_t mfnameLowerCaseHash2 = DDF_AtomLowerCaseStringHash(AT_AtomIndex{ddf.mfnameAtomIndices[j]});
                        if (mfnameLowerCaseHash == mfnameLowerCaseHash2)
                            return true;
                    }
                }

                return false;
            });

            if (i == d->descriptions.end())
            {
                // nothing found, try to load further DDFs
                if (loadDDFAndBundlesFromDisc(resource))
                {
                    i = d->descriptions.begin();
                    continue; // found DDFs or bundles, try again
                }
                break;
            }

            if (!i->matchExpr.isEmpty() && match == DDF_EvalMatchExpr)
            {
                DeviceJs *djs = DeviceJs::instance();
                djs->reset();
                djs->setResource(resource->parentResource() ? resource->parentResource() : resource);
                if (djs->evaluate(i->matchExpr) == JsEvalResult::Ok)
                {
                    const auto res = djs->result();
                    DBG_Printf(DBG_DDF, "matchexpr: %s --> %s\n", qPrintable(i->matchExpr), qPrintable(res.toString()));
                    if (res.toBool()) // needs to evaluate to true
                    {
                        matchedIndices[matchedCount] = i->handle;
                        matchedCount++;
                    }
                }
                else
                {
                    DBG_Printf(DBG_DDF, "failed to evaluate matchexpr for %s: %s, err: %s\n", qPrintable(resource->item(RAttrUniqueId)->toString()), qPrintable(i->matchExpr), qPrintable(djs->errorString()));
                }
            }
            else
            {
                matchedIndices[matchedCount] = i->handle;
                matchedCount++;
            }

            i++; // proceed search
        }
    }

    if (matchedCount != 0)
    {
        /*
         * Now split the matches up in categories sorted by latest timestamp.
         */
        unsigned invalidIndex = 0xFFFFFFFF;
        unsigned rawJsonIndex = invalidIndex;
        unsigned latestStableBundleIndex = invalidIndex;
        unsigned latestBetaBundleIndex = invalidIndex;
        unsigned latestUserBundleIndex = invalidIndex;

        for (size_t i = 0; i < matchedCount; i++)
        {
            const DeviceDescription &ddf1 = d->descriptions[matchedIndices[i]];

            if (ddf1.storageLocation == deCONZ::DdfLocation || ddf1.storageLocation == deCONZ::DdfUserLocation)
            {
                if (rawJsonIndex == invalidIndex)
                {
                    rawJsonIndex = matchedIndices[i];
                }
                else if (d->descriptions[rawJsonIndex].status == QLatin1String("Draft"))
                {
                    rawJsonIndex = matchedIndices[i];
                }
                else if (ddf1.storageLocation == deCONZ::DdfUserLocation && d->descriptions[rawJsonIndex].storageLocation == deCONZ::DdfLocation)
                {
                    // already had a match but user location has more precedence than system location
                    rawJsonIndex = matchedIndices[i];
                }
                continue;
            }

            if (ddf1.storageLocation == deCONZ::DdfBundleUserLocation || ddf1.storageLocation == deCONZ::DdfBundleLocation)
            {
                if (ddf1.signedBy & 1) // has stable signature
                {
                    if (latestStableBundleIndex == invalidIndex)
                    {
                        latestStableBundleIndex = matchedIndices[i];
                    }
                    else
                    {
                        const DeviceDescription &ddf0 = d->descriptions[latestStableBundleIndex];
                        if (ddf0.lastModified < ddf1.lastModified)
                        {
                            latestStableBundleIndex = matchedIndices[i]; // newer
                        }
                    }
                }

                if (ddf1.signedBy & 2) // has beta signature
                {
                    if (latestBetaBundleIndex == invalidIndex)
                    {
                        latestBetaBundleIndex = matchedIndices[i];
                    }
                    else
                    {
                        const DeviceDescription &ddf0 = d->descriptions[latestBetaBundleIndex];
                        if (ddf0.lastModified < ddf1.lastModified)
                        {
                            latestBetaBundleIndex = matchedIndices[i]; // newer
                        }
                    }
                }

                if ((ddf1.signedBy & 3) == 0) // has neither beta or stable signature
                {
                    if (latestUserBundleIndex == invalidIndex)
                    {
                        latestUserBundleIndex = matchedIndices[i];
                    }
                    else
                    {
                        const DeviceDescription &ddf0 = d->descriptions[latestUserBundleIndex];
                        if (ddf0.lastModified < ddf1.lastModified)
                        {
                            latestUserBundleIndex = matchedIndices[i]; // newer
                        }
                    }
                }
            }
        }

        unsigned policy = atDDFPolicyLatestPreferStable; // default

        {
            const Resource *rParent = resource->parentResource() ? resource->parentResource() : resource;
            const ResourceItem *ddfPolicyItem = rParent->item(RAttrDdfPolicy);

            if (ddfPolicyItem)
            {
                policy = ddfPolicyItem->atomIndex();
            }
        }

        if (policy == atDDFPolicyRawJson && rawJsonIndex != invalidIndex)
        {
            return d->descriptions[rawJsonIndex];
        }

        if (policy == atDDFPolicyLatestPreferStable)
        {
            if (latestStableBundleIndex != invalidIndex)
                return d->descriptions[latestStableBundleIndex];
        }

        if (policy == atDDFPolicyLatest || policy == atDDFPolicyLatestPreferStable)
        {
            unsigned bundleCount = 0;
            std::array<unsigned, 3> bundleIndices;

            if (latestStableBundleIndex != invalidIndex)
                bundleIndices[bundleCount++] = latestStableBundleIndex;

            if (latestBetaBundleIndex != invalidIndex)
                bundleIndices[bundleCount++] = latestBetaBundleIndex;

            if (latestUserBundleIndex != invalidIndex)
                bundleIndices[bundleCount++] = latestUserBundleIndex;

            if (bundleCount != 0)
            {
                unsigned bestMatch = bundleIndices[0];
                for (unsigned i = 1; i < bundleCount; i++)
                {
                    const DeviceDescription &ddf0 = d->descriptions[bestMatch];
                    const DeviceDescription &ddf1 = d->descriptions[i];

                    if (ddf0.lastModified < ddf1.lastModified)
                        bestMatch = i;
                }

                return d->descriptions[bestMatch];
            }
        }

        if (policy == atDDFPolicyPin)
        {
            /*
             * Lookup a matching bundle by its hash. This is finicky but ensures no bugus is matched.
             */
            const Resource *rParent = resource->parentResource() ? resource->parentResource() : resource;
            const ResourceItem *ddfHashItem = rParent->item(RAttrDdfHash);

            if (ddfHashItem)
            {
                unsigned len = U_strlen(ddfHashItem->toCString());
                if (len == 64)
                {
                    uint32_t hash[8] = {0};

                    { // convert bundle hash string to binary
                        U_SStream ss;
                        uint8_t *byte = reinterpret_cast<uint8_t*>(&hash[0]);

                        U_sstream_init(&ss, (void*)ddfHashItem->toCString(), len);

                        for (int i = 0; i < 32; i++)
                            byte[i] = U_sstream_get_hex_byte(&ss);
                    }

                    // The hash must belong to one of the earlier matched bundles.
                    for (unsigned i = 0; i < matchedCount; i++)
                    {
                        const DeviceDescription &ddf = d->descriptions[matchedIndices[i]];

                        int j = 0;
                        for (; j < 8; j++)
                        {
                            if (ddf.sha256Hash[j] != hash[j])
                                break;
                        }

                        if (j == 8) // match
                            return ddf;
                    }
                }
            }
        }

        /*
         * Fallback: If none of above matches pick your poison.
         */

        if (latestStableBundleIndex != invalidIndex)
            return d->descriptions[latestStableBundleIndex];

        if (latestBetaBundleIndex != invalidIndex)
            return d->descriptions[latestBetaBundleIndex];

        if (latestUserBundleIndex != invalidIndex)
            return d->descriptions[latestUserBundleIndex];

        if (rawJsonIndex != invalidIndex)
            return d->descriptions[rawJsonIndex];
    }

    return d->invalidDescription;
}

bool DeviceDescriptions::loadDDFAndBundlesFromDisc(const Resource *resource)
{
    Q_D(DeviceDescriptions);

    const ResourceItem *modelidItem = resource->item(RAttrModelId);
    const ResourceItem *mfnameItem = resource->item(RAttrManufacturerName);

    U_ASSERT(modelidItem);
    U_ASSERT(mfnameItem);

    unsigned modelidAtomIndex = modelidItem->atomIndex();
    unsigned mfnameAtomIndex = mfnameItem->atomIndex();

    U_ASSERT(modelidAtomIndex != 0);
    U_ASSERT(mfnameAtomIndex != 0);

    if (modelidAtomIndex == 0 || mfnameAtomIndex == 0)
    {
        return false;
    }

    uint32_t mfnameLowerCaseHash = DDF_AtomLowerCaseStringHash(AT_AtomIndex{mfnameAtomIndex});

    for (const DDF_LoadRecord &loadRecord : d->ddfLoadRecords)
    {
        if (loadRecord.mfnameLowerCaseHash == mfnameLowerCaseHash && loadRecord.modelid.index == modelidAtomIndex)
        {
            return false; // been here before
        }

        if (loadRecord.mfname.index == mfnameAtomIndex && loadRecord.modelid.index == modelidAtomIndex)
        {
            return false; // been here before, note this check can likely be removed due the lower case check already been hit
        }
    }

    DBG_Printf(DBG_DDF, "try load DDF from disc for %s -- %s\n", mfnameItem->toCString(), modelidItem->toCString());

    // mark for loading
    DDF_LoadRecord loadRecord;
    loadRecord.modelid.index = modelidAtomIndex;
    loadRecord.mfname.index = mfnameAtomIndex;
    loadRecord.mfnameLowerCaseHash = mfnameLowerCaseHash;
    loadRecord.loadState = DDF_LoadStateScheduled;
    d->ddfLoadRecords.push_back(loadRecord);

    unsigned countBefore = d->descriptions.size();
    readAll();

    return countBefore < d->descriptions.size();
}

const DeviceDescription &DeviceDescriptions::getFromHandle(DeviceDescription::Item::Handle hnd) const
{
    Q_D(const DeviceDescriptions);
    ItemHandlePack h;
    h.handle = hnd;
    if (h.handle != DeviceDescription::Item::InvalidItemHandle)
    {
        if (h.description < d->descriptions.size())
        {
            return d->descriptions[h.description];
        }
    }

    return d->invalidDescription;
}

void DeviceDescriptions::put(const DeviceDescription &ddf)
{
    if (!ddf.isValid())
    {
        return;
    }

    Q_D(DeviceDescriptions);

    if (ddf.handle >= 0 && ddf.handle <= int(d->descriptions.size()))
    {
        DeviceDescription &ddf0 = d->descriptions[ddf.handle];

        DBG_Assert(ddf0.handle == ddf.handle);
        if (ddf.handle == ddf0.handle)
        {
            DBG_Printf(DBG_DDF, "update ddf %s index %d\n", qPrintable(ddf0.modelIds.front()), ddf.handle);
            ddf0 = ddf;
            DDF_UpdateItemHandlesForIndex(d->descriptions, d->loadCounter, static_cast<size_t>(ddf.handle));
            return;
        }
    }
}

const DeviceDescription &DeviceDescriptions::load(const QString &path)
{
    Q_UNUSED(path)

    // TODO(mpi) implement

    Q_D(DeviceDescriptions);

#if 0

    auto i = std::find_if(d->descriptions.begin(), d->descriptions.end(), [&path](const auto &ddf){ return ddf.path == path; });
    if (i != d->descriptions.end())
    {
        return *i;
    }

    auto result = DDF_ReadDeviceFile(path);

    if (!result.empty())
    {
        for (auto &ddf : result)
        {
            ddf = DDF_MergeGenericItems(d->genericItems, ddf);
            ddf = DDF_LoadScripts(ddf);

            i = std::find_if(d->descriptions.begin(), d->descriptions.end(), [&ddf](const DeviceDescription &b)
            {
                return ddf.modelIds == b.modelIds && ddf.manufacturerNames == b.manufacturerNames;
            });

            if (i != d->descriptions.end())
            {
                *i = ddf; // update
            }
            else
            {
                d->descriptions.push_back(ddf);
            }
        }

        DDF_UpdateItemHandles(d->descriptions, d->loadCounter);

        i = std::find_if(d->descriptions.begin(), d->descriptions.end(), [&path](const auto &ddf){ return ddf.path == path; });
        if (i != d->descriptions.end())
        {
            return *i;
        }
    }

#endif

    return d->invalidDescription;
}

/*! Returns the DDF sub device belonging to a resource. */
const DeviceDescription::SubDevice &DeviceDescriptions::getSubDevice(const Resource *resource) const
{
    Q_D(const DeviceDescriptions);

    if (resource)
    {
        ItemHandlePack h;
        for (int i = 0; i < resource->itemCount(); i++)
        {
            const ResourceItem *item = resource->itemForIndex(size_t(i));
            U_ASSERT(item);

            h.handle = item->ddfItemHandle();
            if (h.handle == DeviceDescription::Item::InvalidItemHandle)
            {
                continue;
            }

            if (h.loadCounter != d->loadCounter)
            {
                return d->invalidSubDevice;
            }

            DBG_Assert(h.description < d->descriptions.size());
            if (h.description >= d->descriptions.size())
            {
                return d->invalidSubDevice;
            }

            auto &ddf = d->descriptions[h.description];

            DBG_Assert(h.subDevice < ddf.subDevices.size());
            if (h.subDevice >= ddf.subDevices.size())
            {
                return d->invalidSubDevice;
            }

            return ddf.subDevices[h.subDevice];
        }
    }

    return d->invalidSubDevice;
}

/*! Turns a string constant into it's value.
    \returns The constant value on success, or the constant itself on error.
 */
QString DeviceDescriptions::constantToString(const QString &constant) const
{
    Q_D(const DeviceDescriptions);

    if (constant.startsWith('$'))
    {
        char buf[128];
        AT_AtomIndex key;

        int len;
        for (len = 0; len < constant.size() && len < 127; len++)
        {
            buf[len] = constant.at(len).toLatin1();
        }
        buf[len] = '\0';

        if (AT_GetAtomIndex(buf, (unsigned)len, &key))
        {
            for (size_t i = 0; i < d->constants2.size(); i++)
            {
                if (d->constants2[i].key.index == key.index)
                {
                    AT_Atom a = AT_GetAtomByIndex(d->constants2[i].value);
                    if (a.len)
                    {
                        return QString::fromUtf8((const char*)a.data, a.len);
                    }
                }
            }
        }
    }

    return constant;
}

QString DeviceDescriptions::stringToConstant(const QString &str) const
{
    Q_D(const DeviceDescriptions);

    if (str.startsWith('$'))
    {
        return str;
    }

    char buf[128];
    AT_AtomIndex val;

    int len;
    for (len = 0; len < str.size() && len < 127; len++)
    {
        buf[len] = str.at(len).toLatin1();
    }
    buf[len] = '\0';

    if (len)
    {
        if (AT_GetAtomIndex(buf, (unsigned)len, &val))
        {
            for (size_t i = 0; i < d->constants2.size(); i++)
            {
                if (d->constants2[i].value.index == val.index)
                {
                    AT_Atom a = AT_GetAtomByIndex(d->constants2[i].key);
                    if (a.len)
                    {
                        return QString::fromUtf8((const char*)a.data, a.len);
                    }
                    break;
                }
            }
        }
    }

    return str;
}

static DeviceDescription::Item *DDF_GetItemMutable(const ResourceItem *item)
{
    if (!_priv || !item)
    {
        return nullptr;
    }

    DeviceDescriptionsPrivate *d = _priv;

    ItemHandlePack h;
    h.handle = item->ddfItemHandle(); // unpack

    if (h.handle == DeviceDescription::Item::InvalidItemHandle)
    {
        return nullptr;
    }

    if (h.loadCounter != d->loadCounter)
    {
        return nullptr;
    }

    DBG_Assert(h.description < d->descriptions.size());
    if (h.description >= d->descriptions.size())
    {
        return nullptr;
    }

    auto &ddf = d->descriptions[h.description];

    DBG_Assert(h.subDevice < ddf.subDevices.size());
    if (h.subDevice >= ddf.subDevices.size())
    {
        return nullptr;
    }

    auto &sub = ddf.subDevices[h.subDevice];

    DBG_Assert(h.item < sub.items.size());

    if (h.item < sub.items.size())
    {
        return &sub.items[h.item];
    }

    return nullptr;
}

/*! Retrieves the DDF item for the given \p item.

    If \p item has a valid DDF item handle the respective entry is returned.
    Otherwise the generic item list is searched based on the item.suffix.

    The returned entry can be check with DeviceDescription::Item::isValid().
 */
const DeviceDescription::Item &DDF_GetItem(const ResourceItem *item)
{
    Q_ASSERT(_instance);
    return _instance->getItem(item);
}

/*! \see DDF_GetItem() description.
 */
const DeviceDescription::Item &DeviceDescriptions::getItem(const ResourceItem *item) const
{
    Q_D(const DeviceDescriptions);

    ItemHandlePack h;
    h.handle = item->ddfItemHandle(); // unpack

    if (h.handle == DeviceDescription::Item::InvalidItemHandle)
    {
        return getGenericItem(item->descriptor().suffix);
    }

    if (h.loadCounter != d->loadCounter)
    {
        return d->invalidItem;
    }

    // Note: There are no further if conditions since at this point it's certain that a handle must be valid.

    Q_ASSERT(h.description < d->descriptions.size());

    const auto &ddf = d->descriptions[h.description];

    Q_ASSERT(h.subDevice < ddf.subDevices.size());

    const auto &sub = ddf.subDevices[h.subDevice];

    Q_ASSERT(h.item < sub.items.size());

    return sub.items[h.item];
}

const DDF_Items &DeviceDescriptions::genericItems() const
{
    return d_ptr2->genericItems;
}

const DeviceDescription::Item &DeviceDescriptions::getGenericItem(const char *suffix) const
{
    Q_D(const DeviceDescriptions);

    for (const auto &item : d->genericItems)
    {
        if (item.name == QLatin1String(suffix))
        {
            return item;
        }
    }

    return d->invalidItem;
}

const std::vector<DDF_FunctionDescriptor> &DeviceDescriptions::getParseFunctions() const
{
    return d_ptr2->parseFunctions;
}

const std::vector<DDF_FunctionDescriptor> &DeviceDescriptions::getReadFunctions() const
{
    return d_ptr2->readFunctions;
}

const std::vector<DDF_FunctionDescriptor> &DeviceDescriptions::getWriteFunctions() const
{
    return d_ptr2->writeFunctions;
}

const std::vector<DDF_SubDeviceDescriptor> &DeviceDescriptions::getSubDevices() const
{
    return d_ptr2->subDevices;
}

static void DDF_UpdateItemHandlesForIndex(std::vector<DeviceDescription> &descriptions, uint loadCounter, size_t index)
{
    U_ASSERT(index < descriptions.size());
    if (descriptions.size() <= index)
    {
        return; // should not happen
    }

    U_ASSERT(index < HND_MAX_DESCRIPTIONS);
    U_ASSERT(loadCounter >= HND_MIN_LOAD_COUNTER);
    U_ASSERT(loadCounter <= HND_MAX_LOAD_COUNTER);

    ItemHandlePack handle;
    DeviceDescription &ddf = descriptions[index];

    ddf.handle = static_cast<int>(index);

    handle.description = static_cast<unsigned>(index);
    handle.loadCounter = loadCounter;
    handle.subDevice = 0;

    for (DeviceDescription::SubDevice &sub : ddf.subDevices)
    {
        handle.item = 0;

        for (DeviceDescription::Item &item : sub.items)
        {
            item.handle = handle.handle;
            U_ASSERT(handle.item < HND_MAX_ITEMS);
            handle.item++;
        }

        U_ASSERT(handle.subDevice < HND_MAX_SUB_DEVS);
        handle.subDevice++;
    }
}

/*! Temporary workaround since DuktapeJS doesn't support 'let', try replace it with 'var'.

    The fix only applies if the JS doesn't compile and after the modified version successfully
    compiles. Can be removed onces all DDFs have been updated.

    Following cases are fixed:

    '^let '   // let at begin of the expression
    ' let '
    '\nlet '
    '\tlet '
    '(let '   // let within a scope like: for (let i=0; i < 3; i++) {...}
*/
static void DDF_TryCompileAndFixJavascript(QString *expr, const QString &path)
{
#ifdef USE_DUKTAPE_JS_ENGINE
    if (DeviceJs::instance()->testCompile(*expr) == JsEvalResult::Ok)
    {
        return;
    }

    int idx = 0;
    int nfixes = 0;
    QString fix = *expr;
    const QString letSearch("let");

    for ( ; idx != -1; )
    {
        idx = fix.indexOf(letSearch, idx);
        if (idx < 0)
        {
            break;
        }

        if (idx == 0 || fix.at(idx - 1).isSpace() || fix.at(idx - 1) == '(')
        {
            fix[idx + 0] = 'v';
            fix[idx + 1] = 'a';
            fix[idx + 2] = 'r';
            idx += 4;
            nfixes++;
        }
    }

    if (nfixes > 0 && DeviceJs::instance()->testCompile(fix) == JsEvalResult::Ok)
    {
        *expr = fix;
        return;
    }

    // if we get here, the expressions has other problems, print compile error and path
    DBG_Printf(DBG_DDF, "DDF failed to compile JS: %s\n%s\n", qPrintable(path), qPrintable(DeviceJs::instance()->errorString()));

#else
    Q_UNUSED(expr)
    Q_UNUSED(path)
#endif
}

enum JSON_Schema
{
    JSON_SCHEMA_UNKNOWN,
    JSON_SCHEMA_CONSTANTS_1,
    JSON_SCHEMA_CONSTANTS_2,
    JSON_SCHEMA_RESOURCE_ITEM_1,
    JSON_SCHEMA_SUB_DEVICE_1,
    JSON_SCHEMA_DEV_CAP_1
};

/*! Returns the JSON schema of a file.

    The function doesn't actually parse the full JSON document
    but instead just extracts the "schema": "<SCHEMA>" content.
*/
JSON_Schema DDF_GetJsonSchema(uint8_t *data, unsigned dataSize)
{
    U_SStream ss[1];
    unsigned beg = 0;
    unsigned end = 0;
    unsigned len;

    U_ASSERT(data);
    U_ASSERT(dataSize > 0);

    if (*data != '{' && *data != '[') // not JSON object or array
    {
        return JSON_SCHEMA_UNKNOWN;
    }

    U_sstream_init(ss, data, dataSize);

    if (U_sstream_find(ss, "\"schema\""))
    {
        U_sstream_seek(ss, ss[0].pos + 8);

        if (U_sstream_find(ss, "\""))
        {
            U_sstream_seek(ss, ss[0].pos + 1);
            beg = ss[0].pos;

            if (U_sstream_find(ss, "\""))
            {
                end = ss[0].pos;
            }
        }
    }

    if (beg < end)
    {
        len = end - beg;
        U_sstream_init(ss, &data[beg], len);

        if (len == 19 && U_sstream_starts_with(ss, "devcap1.schema.json"))
        {
            return JSON_SCHEMA_DEV_CAP_1;
        }
        else if (len == 25 && U_sstream_starts_with(ss, "resourceitem1.schema.json"))
        {
            return JSON_SCHEMA_RESOURCE_ITEM_1;
        }
        else if (len == 22 && U_sstream_starts_with(ss, "constants1.schema.json"))
        {
            return JSON_SCHEMA_CONSTANTS_1;
        }
        else if (len == 22 && U_sstream_starts_with(ss, "constants2.schema.json"))
        {
            return JSON_SCHEMA_CONSTANTS_2;
        }
        else if (len == 22 && U_sstream_starts_with(ss, "subdevice1.schema.json"))
        {
            return JSON_SCHEMA_SUB_DEVICE_1;
        }
    }

    return JSON_SCHEMA_UNKNOWN;
}

/*! Reads all DDF related files.
 */
void DeviceDescriptions::readAll()
{
    readAllRawJson();
    readAllBundles();
}

/*! Reads all scheduled raw JSON DDF files.
 */
void DeviceDescriptions::readAllRawJson()
{
    Q_D(DeviceDescriptions);

    d->loadCounter = (d->loadCounter + 1) % HND_MAX_LOAD_COUNTER;
    if (d->loadCounter <= HND_MIN_LOAD_COUNTER)
    {
        d->loadCounter = HND_MIN_LOAD_COUNTER;
    }

    ScratchMemWaypoint swp;
    uint8_t *ctx_mem = SCRATCH_ALLOC(uint8_t*, sizeof(DDF_ParseContext) + 64);
    U_ASSERT(ctx_mem);
    if (!ctx_mem)
    {
        DBG_Printf(DBG_ERROR, "DDF not enough memory to create DDF_ParseContext\n");
        return;
    }

    DBG_Printf(DBG_DDF, "DDF try to find raw JSON DDFs for %u identifier pairs\n", (unsigned)d->ddfLoadRecords.size());

    DDF_ParseContext *pctx = new(ctx_mem)DDF_ParseContext; // placement new into scratch memory, no further cleanup needed
    U_ASSERT(pctx);
    pctx->extChunks = nullptr;

    DBG_MEASURE_START(DDF_ReadRawJson);

    std::vector<DDF_SubDeviceDescriptor> subDevices;

    std::array<deCONZ::StorageLocation, 2> locations = { deCONZ::DdfLocation, deCONZ::DdfUserLocation};

    bool hasConstants = false;

    // need to resolve constants first
    for (size_t dit = 0; dit < locations.size(); dit++)
    {
        const QString filePath = deCONZ::getStorageLocation(locations[dit]) + "/generic/constants.json";

        pctx->filePath[0] = '\0';
        pctx->filePathLength = 0;
        pctx->scratchPos = 0;

        {
            U_SStream ss;
            U_sstream_init(&ss, pctx->filePath, sizeof(pctx->filePath));
            U_sstream_put_str(&ss, filePath.toUtf8().data());
            pctx->filePathLength = ss.pos;
        }

        if (DDF_ReadFileInMemory(pctx))
        {
            if (DDF_ReadConstantsJson(pctx, d->constants2))
            {
                DBG_Printf(DBG_DDF, "DDF loaded %d string constants from %s\n", (int)d->constants2.size(), pctx->filePath);
                hasConstants = true;
            }
        }
    }

    if (d->constants2.empty() || !hasConstants) // should not happen
    {
        DBG_Printf(DBG_DDF, "DDF failed to load string constants\n");
    }

    U_ASSERT(hasConstants);

    for (size_t dit = 0; dit < locations.size(); dit++)
    {
        const QString dirpath = deCONZ::getStorageLocation(locations[dit]);
        QDirIterator it(dirpath, QDirIterator::Subdirectories | QDirIterator::FollowSymlinks);

        while (it.hasNext())
        {
            it.next();

            pctx->filePath[0] = '\0';
            pctx->filePathLength = 0;
            pctx->scratchPos = 0;
            QString filePath = it.filePath();
            {
                U_SStream ss;
                U_sstream_init(&ss, pctx->filePath, sizeof(pctx->filePath));
                U_sstream_put_str(&ss, filePath.toUtf8().data());
                pctx->filePathLength = ss.pos;
            }

            if (it.filePath().endsWith(QLatin1String("generic/constants.json")))
            {
            }
            else if (it.fileName() == QLatin1String("button_maps.json"))
            {  }
            else if (it.fileName().endsWith(QLatin1String(".json")))
            {
                if (it.filePath().contains(QLatin1String("generic/items/")))
                {
                    if (DDF_ReadFileInMemory(pctx))
                    {
                        DeviceDescription::Item result = DDF_ReadItemFile(pctx);
                        if (result.isValid())
                        {
                            result.isGenericRead = !result.readParameters.isNull() ? 1 : 0;
                            result.isGenericWrite = !result.writeParameters.isNull() ? 1 : 0;
                            result.isGenericParse = !result.parseParameters.isNull() ? 1 : 0;

                            size_t j = 0;
                            for (j = 0; j < d->genericItems.size(); j++)
                            {
                                DeviceDescription::Item  &genItem = d->genericItems[j];
                                if (genItem.name == result.name)
                                {
                                    // replace
                                    genItem = result;
                                    break;
                                }
                            }

                            if (j == d->genericItems.size())
                            {
                                d->genericItems.push_back(result);
                            }
                        }
                    }
                }
                else if (it.filePath().contains(QLatin1String("generic/subdevices/")))
                {
                    if (DDF_ReadFileInMemory(pctx))
                    {
                        DDF_SubDeviceDescriptor result = DDF_ReadSubDeviceFile(pctx);
                        if (isValid(result))
                        {
                            subDevices.push_back(result);
                        }
                    }
                }
                else
                {
                    if (DDF_ReadFileInMemory(pctx))
                    {
                        DeviceDescription result = DDF_ReadDeviceFile(pctx);
                        if (result.isValid())
                        {
                            result.storageLocation = locations[dit];
                            if (U_Sha256(pctx->fileData, pctx->fileDataSize, (unsigned char*)&result.sha256Hash[0]) == 0)
                            {
                                DBG_Printf(DBG_DDF, "DDF failed to create SHA-256 hash of DDF\n");
                            }

                            unsigned j = 0;
                            unsigned k = 0;
                            bool found = false;

                            /*
                             * Check if this DDF is already loaded.
                             */
                            for (j = 0; j < d->descriptions.size(); j++)
                            {
                                const DeviceDescription &ddf = d->descriptions[j];

                                for (k = 0; k < 8; k++)
                                {
                                    if (ddf.sha256Hash[k] != result.sha256Hash[k])
                                    {
                                        break;
                                    }
                                }

                                if (k == 8)
                                {
                                    found = true;
                                    break;
                                }
                            }

                            if (!found)
                            {
                                /*
                                 * Further check if the DDF is scheduled for loading.
                                 * That is when an actual possibly matching device exists in the setup.
                                 */
                                bool scheduled = false;
                                if (result.manufacturerNames.size() == result.modelIds.size())
                                {
                                    for (j = 0; j < result.manufacturerNames.size(); j++)
                                    {
                                        AT_AtomIndex mfnameIndex;
                                        AT_AtomIndex modelidIndex;
                                        uint32_t mfnameLowerCaseHash = 0;

                                        mfnameIndex.index = 0;
                                        modelidIndex.index = 0;

                                        /*
                                         * Try to get atoms for the mfname/modelid pair.
                                         * Note: If they don't exist, this isn't the pair we are looking for!
                                         * We don't add atoms for all strings found in DDFs to safe memory.
                                         */

                                        {
                                            const QByteArray m = constantToString(result.manufacturerNames[j]).toUtf8();
                                            if (AT_GetAtomIndex(m.constData(), (unsigned)m.size(), &mfnameIndex) != 1)
                                            {
                                                if (m.startsWith('$'))
                                                {
                                                    DBG_Printf(DBG_DDF, "DDF failed to resolve constant %s\n", m.data());
                                                    // continue here anyway as long as modelid matches
                                                }
                                                else
                                                {
                                                    continue;
                                                }
                                            }
                                            else
                                            {
                                                mfnameLowerCaseHash = DDF_AtomLowerCaseStringHash(mfnameIndex);
                                            }
                                        }

                                        {
                                            const QByteArray m = constantToString(result.modelIds[j]).toUtf8();
                                            if (AT_GetAtomIndex(m.constData(), (unsigned)m.size(), &modelidIndex) != 1)
                                            {
                                                continue;
                                            }
                                        }

                                        for (k = 0; k < d->ddfLoadRecords.size(); k++)
                                        {
                                            if (modelidIndex.index == d->ddfLoadRecords[k].modelid.index)
                                            {
                                                if (mfnameLowerCaseHash == 0)
                                                {
                                                    // ignore for now, in worst case we load a DDF to memory which isn't used
                                                    U_ASSERT(0);
                                                }
                                                else if (mfnameLowerCaseHash != d->ddfLoadRecords[k].mfnameLowerCaseHash)
                                                {
                                                    continue;
                                                }
                                                scheduled = true;
                                                break;
                                            }
                                        }

                                        if (scheduled)
                                        {
                                            break;
                                        }
                                    }
                                }
                                else
                                {
                                    DBG_Printf(DBG_DDF, "DDF ignore %s due unequal manufacturername/modelid array sizes\n", pctx->filePath);
                                }

                                if (scheduled)
                                {
                                    /*
                                     * The DDF is of interest, now register all atoms for faster lookups.
                                     */
                                    for (const auto &mfname : result.manufacturerNames)
                                    {
                                        const QString m = DeviceDescriptions::instance()->constantToString(mfname);

                                        AT_AtomIndex ati;
                                        if (AT_AddAtom(m.toUtf8().data(), m.size(), &ati) && ati.index != 0)
                                        {
                                            result.mfnameAtomIndices.push_back(ati.index);
                                        }
                                    }

                                    for (const auto &modelId : result.modelIds)
                                    {
                                        const QString m = DeviceDescriptions::instance()->constantToString(modelId);

                                        AT_AtomIndex ati;
                                        if (AT_AddAtom(m.toUtf8().data(), m.size(), &ati) && ati.index != 0)
                                        {
                                            result.modelidAtomIndices.push_back(ati.index);
                                        }
                                    }

                                    DBG_Printf(DBG_DDF, "DDF cache raw JSON DDF %s\n", pctx->filePath);
                                    d->descriptions.push_back(std::move(result));
                                    DDF_UpdateItemHandlesForIndex(d->descriptions, d->loadCounter, d->descriptions.size() - 1);
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    if (!subDevices.empty())
    {
        std::sort(subDevices.begin(), subDevices.end(), [](const auto &a, const auto &b){
            return a.name < b.name;
        });

        d->subDevices = std::move(subDevices);
    }

    if (!d->descriptions.empty())
    {
        for (auto &ddf : d->descriptions)
        {
            ddf = DDF_MergeGenericItems(d->genericItems, ddf);
            ddf = DDF_LoadScripts(ddf);
        }

        DBG_Printf(DBG_DDF, "DDF loaded %d raw JSON DDFs\n", (int)d->descriptions.size());
    }

    DBG_MEASURE_END(DDF_ReadRawJson);
}

#if 0
    {
        U_ECC_PrivateKeySecp256k1 privkey = {0};
        U_ECC_PublicKeySecp256k1 pubkey = {0};
        U_ECC_SignatureSecp256k1 sig = {0};
        unsigned char hash[U_SHA256_HASH_SIZE] = {0};

        if (U_ECC_CreateKeyPairSecp256k1(&privkey, &pubkey) == 1)
        {
            DBG_Printf(DBG_INFO, "created keypair\n");

            U_Sha256(&privkey, sizeof(privkey), hash); // just to have some hash

            if (U_ECC_SignSecp256K1(&privkey, hash, sizeof(hash), &sig))
            {
                DBG_Printf(DBG_INFO, "created signature\n");

                if (U_ECC_VerifySignatureSecp256k1(&pubkey, &sig, hash, sizeof(hash)))
                {
                    DBG_Printf(DBG_INFO, "verified signature\n");
                }

                sig.sig[6] = sig.sig[6] + 9; // invalidate
                if (U_ECC_VerifySignatureSecp256k1(&pubkey, &sig, hash, sizeof(hash)) == 0)
                {
                    DBG_Printf(DBG_INFO, "invalid signature [OK]\n");
                }
            }
        }
    }

    {
        U_HmacSha256Test();
    }
#endif

/*! Trigger REventDDFReload for all present devices which match with device identifier pair of DDF bundle.
 */
static int DDF_ReloadBundleDevices(const char *desc, unsigned descSize, std::vector<DDF_LoadRecord> &ddfLoadRecords)
{
    cj_ctx cj[1];
    char buf[96];
    cj_token_ref ref;
    cj_token_ref parent_ref;
    cj_token_ref deviceids_ref;
    AT_AtomIndex modelid_ati;
    AT_AtomIndex mfname_ati;
    cj_token *tokens;
    unsigned n_tokens = 1024;
    int n_marked = 0;

    ScratchMemWaypoint swp;
    tokens = SCRATCH_ALLOC(cj_token*, n_tokens * sizeof(*tokens));
    U_ASSERT(tokens);
    if (!tokens)
        return 0;

    cj_parse_init(cj, desc, descSize, tokens, n_tokens);
    cj_parse(cj);

    if (cj->status != CJ_OK)
        return 0;

    parent_ref = 0;
    deviceids_ref = cj_value_ref(cj, parent_ref, "device_identifiers");

    // array of 2 string element arrays
    // "device_identifiers":[["LUMI","lumi.sensor_magnet"]]

    if (deviceids_ref == CJ_INVALID_TOKEN_INDEX)
        return 0;

    if (tokens[deviceids_ref].type != CJ_TOKEN_ARRAY_BEG)
        return 0;

    // verify flat string array, and equal size for manufacturer names and modelids
    for (ref = deviceids_ref + 1; tokens[ref].type != CJ_TOKEN_ARRAY_END && ref < cj[0].tokens_pos; )
    {
        if (tokens[ref].type == CJ_TOKEN_ITEM_SEP)
        {
            ref++;
            continue;
        }

        // inner array for each entry
        if (tokens[ref].type != CJ_TOKEN_ARRAY_BEG)
            break;

        if (tokens[ref + 1].type != CJ_TOKEN_STRING) // mfname
            break;

        if (tokens[ref + 2].type != CJ_TOKEN_ITEM_SEP)
            break;

        if (tokens[ref + 3].type != CJ_TOKEN_STRING) // modelid
            break;

        if (tokens[ref + 4].type != CJ_TOKEN_ARRAY_END)
            break;

        /*
         * Lookup if the manufacturername and modelid pair has registered atoms.
         * If not this can't be a bundle of interest.
         */
        bool foundAtoms = true;

        if (cj_copy_ref_utf8(cj, buf, sizeof(buf), ref + 1) == 0)
            break; // this has to be a valid string

        if (AT_GetAtomIndex(buf, U_strlen(buf), &mfname_ati) == 0)
            foundAtoms = false; // unknown, can be ok

        if (cj_copy_ref_utf8(cj, buf, sizeof(buf), ref + 3) == 0)
            break; // this has to be a valid string

        if (AT_GetAtomIndex(buf, U_strlen(buf), &modelid_ati) == 0)
            foundAtoms = false; // unknown, can be ok

        ref += 5;
        if (!foundAtoms)
            continue;

        for (size_t j = 0; j < ddfLoadRecords.size(); j++)
        {
            DDF_LoadRecord &rec = ddfLoadRecords[j];

            if (rec.mfname.index != mfname_ati.index)
                continue;

            if (rec.modelid.index != modelid_ati.index)
                continue;

            rec.loadState = DDF_LoadStateScheduled;
            n_marked++;
        }
    }

    return n_marked > 0;
}

static int DDF_IsBundleScheduled(DDF_ParseContext *pctx, const char *desc, unsigned descSize, const std::vector<DDF_LoadRecord> &ddfLoadRecords)
{
    cj_ctx cj[1];
    char buf[96];
    cj_token_ref ref;
    cj_token_ref parent_ref;
    cj_token_ref deviceids_ref;
    cj_token_ref last_modified_ref;
    AT_AtomIndex modelid_ati;
    AT_AtomIndex mfname_ati;
    cj_token *tokens = pctx->tokens.data();

    cj_parse_init(cj, desc, descSize, pctx->tokens.data(), (cj_size)pctx->tokens.size());
    cj_parse(cj);

    if (cj->status != CJ_OK)
        return 0;

    parent_ref = 0;
    deviceids_ref = cj_value_ref(cj, parent_ref, "device_identifiers");
    last_modified_ref = cj_value_ref(cj, parent_ref, "last_modified");

    // array of 2 string element arrays
    // "device_identifiers":[["LUMI","lumi.sensor_magnet"]]

    if (last_modified_ref == CJ_INVALID_TOKEN_INDEX)
        return 0;

    if (tokens[last_modified_ref].type != CJ_TOKEN_STRING)
        return 0;

    {
        cj_token *tok = &tokens[last_modified_ref];
        pctx->bundleLastModified = U_TimeFromISO8601(&desc[tok->pos], tok->len);
    }

    if (deviceids_ref == CJ_INVALID_TOKEN_INDEX)
        return 0;

    if (tokens[deviceids_ref].type != CJ_TOKEN_ARRAY_BEG)
        return 0;

    // verify flat string array, and equal size for manufacturer names and modelids
    for (ref = deviceids_ref + 1; tokens[ref].type != CJ_TOKEN_ARRAY_END && ref < cj[0].tokens_pos; )
    {
        if (tokens[ref].type == CJ_TOKEN_ITEM_SEP)
        {
            ref++;
            continue;
        }

        // inner array for each entry
        if (tokens[ref].type != CJ_TOKEN_ARRAY_BEG)
            break;

        if (tokens[ref + 1].type != CJ_TOKEN_STRING) // mfname
            break;

        if (tokens[ref + 2].type != CJ_TOKEN_ITEM_SEP)
            break;

        if (tokens[ref + 3].type != CJ_TOKEN_STRING) // modelid
            break;

        if (tokens[ref + 4].type != CJ_TOKEN_ARRAY_END)
            break;

        /*
         * Lookup if the manufacturername and modelid pair has registered atoms.
         * If not this can't be a bundle of interest.
         */
        bool foundAtoms = true;

        if (cj_copy_ref_utf8(cj, buf, sizeof(buf), ref + 1) == 0)
            break; // this has to be a valid string

        if (AT_GetAtomIndex(buf, U_strlen(buf), &mfname_ati) == 0)
            foundAtoms = false; // unknown, can be ok

        if (cj_copy_ref_utf8(cj, buf, sizeof(buf), ref + 3) == 0)
            break; // this has to be a valid string

        if (AT_GetAtomIndex(buf, U_strlen(buf), &modelid_ati) == 0)
            foundAtoms = false; // unknown, can be ok

        ref += 5;
        if (!foundAtoms)
            continue;

        uint32_t mfnameLowerCaseHash = DDF_AtomLowerCaseStringHash(mfname_ati);

        for (size_t j = 0; j < ddfLoadRecords.size(); j++)
        {
            const DDF_LoadRecord &rec = ddfLoadRecords[j];

            if (rec.modelid.index != modelid_ati.index)
                continue;

            if (rec.mfnameLowerCaseHash != mfnameLowerCaseHash)
                continue;

            return 1;
        }
    }

    return 0;
}

void DEV_DDF_BundleUpdated(unsigned char *data, unsigned dataSize)
{
    U_BStream bs;
    unsigned chunkSize;

    U_bstream_init(&bs, data, dataSize);

    if (DDFB_FindChunk(&bs, "RIFF", &chunkSize) == 0)
        return;

    if (DDFB_FindChunk(&bs, "DDFB", &chunkSize) == 0)
        return;

    if (DDFB_FindChunk(&bs, "DESC", &chunkSize) == 0)
        return;

    if (DDF_ReloadBundleDevices((char*)&bs.data[bs.pos], chunkSize, _priv->ddfLoadRecords) != 0)
    {
        _priv->ddfReloadWhat = DDF_ReloadBundles;
        _priv->ddfReloadTimer->stop();
        _priv->ddfReloadTimer->start(2000);
    }
}

void DeviceDescriptions::reloadAllRawJsonAndBundles(const Resource *resource)
{

    const ResourceItem *mfnameItem = resource->item(RAttrManufacturerName);
    const ResourceItem *modelidItem = resource->item(RAttrModelId);
    unsigned mfnameAtomIndex = mfnameItem->atomIndex();
    unsigned modelidAtomIndex = modelidItem->atomIndex();
    uint32_t mfnameLowerCaseHash = DDF_AtomLowerCaseStringHash(AT_AtomIndex{mfnameAtomIndex});

    for (size_t j = 0; j < d_ptr2->ddfLoadRecords.size(); j++)
    {
        DDF_LoadRecord &rec = d_ptr2->ddfLoadRecords[j];

        if (rec.mfnameLowerCaseHash != mfnameLowerCaseHash)
            continue;

        if (rec.modelid.index != modelidAtomIndex)
            continue;

        if (rec.loadState != DDF_LoadStateScheduled)
        {
            rec.loadState = DDF_LoadStateScheduled;
        }
    }

    d_ptr2->ddfReloadWhat = DDF_ReloadAll;
    d_ptr2->ddfReloadTimer->stop();
    d_ptr2->ddfReloadTimer->start(1000);
}

void DeviceDescriptions::ddfReloadTimerFired()
{
    if (d_ptr2->ddfReloadWhat == DDF_ReloadAll)
    {
        readAll();
    }
    else if (d_ptr2->ddfReloadWhat == DDF_ReloadBundles)
    {
        readAllBundles();
    }

    d_ptr2->ddfReloadWhat = DDF_ReloadIdle;

     for (DDF_LoadRecord &rec : d_ptr2->ddfLoadRecords)
     {
        // trigger DDF reload event for matching devices
        if (rec.loadState == DDF_LoadStateScheduled)
        {
            rec.loadState = DDF_LoadStateLoaded;
            DEV_ReloadDeviceIdendifier(rec.mfname.index, rec.modelid.index);
        }
     }
}

/*! Reads all scheduled DDF bundles.
 */
void DeviceDescriptions::readAllBundles()
{
    Q_D(DeviceDescriptions);

    ScratchMemWaypoint swp;
    uint8_t *ctx_mem = SCRATCH_ALLOC(uint8_t*, sizeof(DDF_ParseContext) + 64);
    U_ASSERT(ctx_mem);
    if (!ctx_mem)
    {
        DBG_Printf(DBG_ERROR, "DDF not enough memory to create DDF_ParseContext\n");
        return;
    }

    DDF_ParseContext *pctx = new(ctx_mem)DDF_ParseContext; // placement new into scratch memory, no further cleanup needed
    U_ASSERT(pctx);

    DBG_MEASURE_START(DDF_ReadBundles);

    FS_Dir dir;
    FS_File fp;
    U_SStream ss;
    U_BStream bs;
    unsigned chunkSize;
    unsigned basePathLength;
    unsigned scratchPosPerBundle;

    deCONZ::StorageLocation locations[2] = { deCONZ::DdfBundleUserLocation, deCONZ::DdfBundleLocation };

    for (int dit = 0; dit < 2; dit++)
    {
        {
            QByteArray loc = deCONZ::getStorageLocation(locations[dit]).toUtf8();
            U_sstream_init(&ss, pctx->filePath, sizeof(pctx->filePath));
            U_sstream_put_str(&ss, loc.data());
            basePathLength = ss.pos;
        }

        // during processing of a bundle additional memory might be allocated
        // restore this point for each bundle to be processeed
        scratchPosPerBundle = ScratchMemPos();

        if (FS_OpenDir(&dir, pctx->filePath))
        {
            for (;FS_ReadDir(&dir);)
            {
                if (dir.entry.type != FS_TYPE_FILE)
                    continue;

                U_sstream_init(&ss, dir.entry.name, strlen(dir.entry.name));

                if (U_sstream_find(&ss, ".ddf") == 0 && U_sstream_find(&ss, ".ddb") == 0)
                    continue;

                ScratchMemRewind(scratchPosPerBundle);

                U_sstream_init(&ss, pctx->filePath, sizeof(pctx->filePath));
                ss.pos = basePathLength; // reuse path and append the filename to existing base path
                U_sstream_put_str(&ss, "/");
                U_sstream_put_str(&ss, dir.entry.name);
                pctx->filePathLength = ss.pos;
                pctx->bundleLastModified = 0;
                pctx->extChunks = nullptr;
                pctx->signatures = 0;

                if (DDF_ReadFileInMemory(pctx) == 0)
                    continue;

                // keep copy here since pcxt vars are adjusted to sub sections during read
                unsigned ddfbChunkOffset;
                unsigned ddfbChunkSize;
                uint32_t ddfbHash[8];
                unsigned char *fileData;
                unsigned fileDataSize;

                fileData = pctx->fileData;
                fileDataSize = pctx->fileDataSize;

                U_bstream_init(&bs, pctx->fileData, pctx->fileDataSize);

                if (DDFB_FindChunk(&bs, "RIFF", &chunkSize) == 0)
                    continue;

                if (DDFB_FindChunk(&bs, "DDFB", &chunkSize) == 0)
                    continue;

                ddfbChunkOffset = bs.pos;
                ddfbChunkSize = chunkSize;

                {   // check if bundle is already loaded
                    // bundle hash over DDFB chunk (header + data)
                    U_Sha256(&pctx->fileData[ddfbChunkOffset - 8], ddfbChunkSize + 8, (uint8_t*)&ddfbHash[0]);

                    unsigned i;
                    unsigned k;

                    for (i = 0; i < d->descriptions.size(); i++)
                    {
                        uint32_t *hash0 = d->descriptions[i].sha256Hash;

                        for (k = 0; k < 8; k++)
                        {
                            if (hash0[k] != ddfbHash[k])
                                break;
                        }

                        if (k == 8)
                            break; // match
                    }

                    if (i < d->descriptions.size()) // skip, already known
                        continue;
                }

                if (DDFB_FindChunk(&bs, "DESC", &chunkSize) == 0)
                    continue;

                /*
                 * Only load bundles into memory for devices which are present.
                 */
                if (DDF_IsBundleScheduled(pctx, (char*)&bs.data[bs.pos], chunkSize, d->ddfLoadRecords) == 0)
                    continue;

                // limit to DDFB content
                U_bstream_init(&bs, &fileData[ddfbChunkOffset], ddfbChunkSize);

                // read external files first
                for (;bs.status == U_BSTREAM_OK;)
                {
                    if (DDFB_IsChunk(&bs, "EXTF"))
                    {
                        DDFB_ExtfChunk *extf = SCRATCH_ALLOC(DDFB_ExtfChunk*, sizeof (*extf));
                        if (extf && DDFB_ReadExtfChunk(&bs, extf))
                        {
                            // collect external chunk descriptors in a temporary list
                            extf->next = pctx->extChunks;
                            pctx->extChunks = extf;
                            continue;
                        }
                    }

                    DDFB_SkipChunk(&bs);
                }

                if (!pctx->extChunks)
                    continue; // must not be empty

                if (DDF_ReadConstantsJson(pctx, d->constants2))
                {

                }

                /*
                 * Now process the actual DDF content which is in ETXF chunk with type DDFC.
                 */

                DDFB_ExtfChunk *extfDDFC = nullptr;

                for (DDFB_ExtfChunk *extf = pctx->extChunks; extf; extf = extf->next)
                {
                    if (extf->fileType[0] != 'D' || extf->fileType[1] != 'D' || extf->fileType[2] != 'F' || extf->fileType[3] != 'C')
                        continue;

                    JSON_Schema schema = DDF_GetJsonSchema(extf->fileData, extf->fileSize);

                    if (schema == JSON_SCHEMA_DEV_CAP_1)
                    {
                        extfDDFC = extf;
                        break;
                    }
                }

                if (!extfDDFC)
                    continue; // main DDF JSON must be present

                // tmp swap where data points
                pctx->fileData = extfDDFC->fileData;
                pctx->fileDataSize = extfDDFC->fileSize;

                DeviceDescription ddf = DDF_ReadDeviceFile(pctx);
                if (!ddf.isValid())
                {
                    continue;
                }

                // process signatures
                U_bstream_init(&bs, &fileData[8], fileDataSize - 8); // after RIFF header
                if (DDFB_FindChunk(&bs, "SIGN", &chunkSize) == 1)
                {
                    U_bstream_init(&bs, &bs.data[bs.pos], chunkSize);
                    DDF_ProcessSignatures(pctx, d->publicKeys, &bs, ddfbHash);
                }

                ddf.storageLocation = locations[dit];
                ddf.lastModified = pctx->bundleLastModified;
                ddf.signedBy = pctx->signatures;

                if (DDF_MergeGenericBundleItems(ddf, pctx) == 0)
                {
                    continue;
                }

                // copy bundle hash generated earlier
                for (unsigned i = 0; i < 8; i++)
                    ddf.sha256Hash[i] = ddfbHash[i];

                {
                    /*
                     * The DDF is of interest, now register all atoms for faster lookups.
                     */
                    for (const auto &mfname : ddf.manufacturerNames)
                    {
                        const QString m = constantToString(mfname);

                        AT_AtomIndex ati;
                        if (AT_AddAtom(m.toUtf8().data(), m.size(), &ati) && ati.index != 0)
                        {
                            ddf.mfnameAtomIndices.push_back(ati.index);
                        }
                    }

                    for (const auto &modelId : ddf.modelIds)
                    {
                        const QString m = constantToString(modelId);

                        AT_AtomIndex ati;
                        if (AT_AddAtom(m.toUtf8().data(), m.size(), &ati) && ati.index != 0)
                        {
                            ddf.modelidAtomIndices.push_back(ati.index);
                        }
                    }

                    d->descriptions.push_back(std::move(ddf));
                    DDF_UpdateItemHandlesForIndex(d->descriptions, d->loadCounter, d->descriptions.size() - 1);
                }

                DBG_Printf(DBG_DDF, "DDF bundle: %s, size: %u bytes\n", ss.str, pctx->fileDataSize);
            }

            FS_CloseDir(&dir);
        }
    }

    DBG_MEASURE_END(DDF_ReadBundles);
}

/*! Tries to init a Device from an DDF file.

    Currently this is done syncronously later on it will be async to not block
    the main thread while loading DDF files.
 */
void DeviceDescriptions::handleDDFInitRequest(const Event &event)
{
    Q_D(DeviceDescriptions);

    auto *resource = DEV_GetResource(RDevices, QString::number(event.deviceKey()));

    int result = -1; // error

    if (resource)
    {
        const DeviceDescription &ddf = get(resource, DDF_EvalMatchExpr);

        if (ddf.isValid())
        {
            result = 0;

            if (!DEV_TestManaged() && !DDF_IsStatusEnabled(ddf.status))
            {
                result = 2;
            }
            else if (DEV_InitDeviceFromDescription(static_cast<Device*>(resource), ddf))
            {
                result = 1; // ok

                if (ddf.status == QLatin1String("Draft"))
                {
                    result = 2;
                }
                else if (ddf.storageLocation == deCONZ::DdfBundleLocation || ddf.storageLocation == deCONZ::DdfBundleUserLocation)
                {
                    result = 3;
                }
            }
        }

        if (result >= 0)
        {
            DBG_Printf(DBG_INFO, "DEV found DDF for " FMT_MAC ", path: %s, result: %d\n", FMT_MAC_CAST(event.deviceKey()), qPrintable(ddf.path), result);
        }

        if (result == 0)
        {
            DBG_Printf(DBG_INFO, "DEV init Device from DDF for " FMT_MAC " failed\n", FMT_MAC_CAST(event.deviceKey()));
        }
        else if (result == -1)
        {
            DBG_Printf(DBG_INFO, "DEV no DDF for " FMT_MAC ", modelId: %s\n", FMT_MAC_CAST(event.deviceKey()), resource->item(RAttrModelId)->toCString());
            DBG_Printf(DBG_INFO, "DEV create on-the-fly DDF for " FMT_MAC "\n", FMT_MAC_CAST(event.deviceKey()));

            DeviceDescription ddf1;
            Device *device = static_cast<Device*>(resource);

            if (DEV_InitBaseDescriptionForDevice(device, ddf1))
            {
                /*
                 * Register all atoms for faster lookups.
                 */
                for (const auto &mfname : ddf1.manufacturerNames)
                {
                    const QString m = constantToString(mfname);

                    AT_AtomIndex ati;
                    if (AT_AddAtom(m.toUtf8().data(), m.size(), &ati) && ati.index != 0)
                    {
                        ddf1.mfnameAtomIndices.push_back(ati.index);
                    }
                }

                for (const auto &modelId : ddf1.modelIds)
                {
                    const QString m = constantToString(modelId);

                    AT_AtomIndex ati;
                    if (AT_AddAtom(m.toUtf8().data(), m.size(), &ati) && ati.index != 0)
                    {
                        ddf1.modelidAtomIndices.push_back(ati.index);
                    }
                }

                ddf1.storageLocation = deCONZ::DdfUserLocation;
                d->descriptions.push_back(std::move(ddf1));
                DDF_UpdateItemHandlesForIndex(d->descriptions, d->loadCounter, d->descriptions.size() - 1);
            }
        }
    }

    emit eventNotify(Event(RDevices, REventDDFInitResponse, result, event.deviceKey()));
}

/*! Reads constants.json file and places them into \p constants map.
 */
static int DDF_ReadConstantsJson(DDF_ParseContext *pctx, std::vector<ConstantEntry> &constants)
{
    cj_ctx ctx;
    cj_ctx *cj;
    cj_token *tok;
    cj_token_ref ref;
    ConstantEntry constEntry;

    const char *fileData = (const char*)pctx->fileData;
    unsigned fileDataSize = pctx->fileDataSize;

    // if this is a bundle point to data within the bundle
    if (pctx->extChunks)
    {
        fileData = nullptr;
        fileDataSize = 0;

        for (DDFB_ExtfChunk *extf = pctx->extChunks; extf; extf = extf->next)
        {
            if (extf->fileType[0] != 'J' || extf->fileType[1] != 'S' || extf->fileType[2] != 'O' || extf->fileType[3] != 'N')
                continue;

            JSON_Schema schema = DDF_GetJsonSchema(extf->fileData, extf->fileSize);

            if (schema == JSON_SCHEMA_CONSTANTS_2)
            {
                fileData = (const char*)extf->fileData;
                fileDataSize = extf->fileSize;
                break;
            }
        }
    }

    if (!fileData || fileDataSize == 0)
    {
        return 0;
    }

    auto &tokens = pctx->tokens;

    cj = &ctx;

    cj_parse_init(cj, fileData, fileDataSize, tokens.data(), tokens.size());
    cj_parse(cj);

    if (cj->status == CJ_OK)
    {
        for (ref = 0; ref < cj->tokens_pos; ref++)
        {
            tok = &cj->tokens[ref];

            if (tok->type == CJ_TOKEN_STRING && (ref + 2) < cj->tokens_pos)
            {
                if (cj->buf[tok->pos] == '$' && tok[1].type == CJ_TOKEN_NAME_SEP && tok[2].type == CJ_TOKEN_STRING)
                {
                    if (tok[0].len < 2 || tok[2].len < 2)
                    {
                        // min size of strings, should perhaps be longer ...
                    }
                    else if (tok[0].len > AT_MAX_ATOM_SIZE || tok[2].len > AT_MAX_ATOM_SIZE)
                    {

                    }
                    else if (AT_AddAtom(&cj->buf[tok[0].pos], tok[0].len, &constEntry.key) == 1 &&
                             AT_AddAtom(&cj->buf[tok[2].pos], tok[2].len, &constEntry.value) == 1)
                    {
                        // check already known
                        for (size_t i = 0; i < constants.size(); i++)
                        {
                            if (constants[i].key.index == constEntry.key.index && constants[i].value.index == constEntry.value.index)
                            {
                                constEntry.key.index = 0;
                                constEntry.value.index = 0;
                                break;
                            }
                        }

                        // The code allows to add same keys with different values,
                        // this might be a problem, but so is replacing a existing key.
                        // When doing a lookup we can iterate in reverse to yield the newest key-value pair.
                        if (constEntry.key.index != 0 && constEntry.value.index != 0)
                        {
                            constants.push_back(constEntry);
                        }
                    }
                }
            }
        }

        return 1;
    }

    return 0;
}

ApiDataType API_DataTypeFromString(const QString &str)
{
    if (str == QLatin1String("bool")) return DataTypeBool;
    if (str == QLatin1String("uint8")) return DataTypeUInt8;
    if (str == QLatin1String("uint16")) return DataTypeUInt16;
    if (str == QLatin1String("uint32")) return DataTypeUInt32;
    if (str == QLatin1String("uint64")) return DataTypeUInt64;
    if (str == QLatin1String("int8")) return DataTypeInt8;
    if (str == QLatin1String("int16")) return DataTypeInt16;
    if (str == QLatin1String("int32")) return DataTypeInt32;
    if (str == QLatin1String("int64")) return DataTypeInt64;
    if (str == QLatin1String("string")) return DataTypeString;
    if (str == QLatin1String("double")) return DataTypeReal;
    if (str == QLatin1String("time")) return DataTypeTime;
    if (str == QLatin1String("timepattern")) return DataTypeTimePattern;

    return DataTypeUnknown;
}

static int DDF_ReadFileInMemory(DDF_ParseContext *pctx)
{
    FS_File f;
    long remaining = (pctx->scratchPos < pctx->scratchMem.size()) ? pctx->scratchMem.size() - pctx->scratchPos : 0;

    pctx->scratchPos = 0;
    pctx->fileData = nullptr;
    pctx->fileDataSize = 0;
    if (FS_OpenFile(&f, FS_MODE_R, pctx->filePath))
    {
        long fsize = FS_GetFileSize(&f);

        if (fsize + 1 > remaining)
        {

        }
        else if (fsize > 0)
        {
            unsigned char *data = pctx->scratchMem.data() + pctx->scratchPos;
            long n = FS_ReadFile(&f, data, remaining);
            if (n == fsize)
            {
                FS_CloseFile(&f);
                data[n] = '\0';
                pctx->scratchPos += (n + 1);
                pctx->fileData = data;
                pctx->fileDataSize = n;
                return 1;
            }
        }

        FS_CloseFile(&f);
    }


    return 0;
}

/*! Parses an item object.
    \returns A parsed item, use DeviceDescription::Item::isValid() to check for success.
 */
static DeviceDescription::Item DDF_ParseItem(DDF_ParseContext *pctx, const QJsonObject &obj)
{
    DeviceDescription::Item result{};
    bool hasSchema = obj.contains(QLatin1String("schema"));

    if (obj.contains(QLatin1String("name")))
    {
        result.name = obj.value(QLatin1String("name")).toString().toUtf8().constData();
    }
    else if (obj.contains(QLatin1String("id"))) // generic/item TODO align name/id?
    {
        result.name = obj.value(QLatin1String("id")).toString().toUtf8().constData();
    }

    // Handle deprecated names/ids within DDFs, but not in generic/items
    if (!hasSchema)
    {
        if (result.name == RConfigColorCapabilities) { result.name = RCapColorCapabilities; }
        if (result.name == RConfigCtMax) { result.name = RCapColorCtMax; }
        if (result.name == RConfigCtMin) { result.name = RCapColorCtMin; }
    }

    if (obj.contains(QLatin1String("description")))
    {
        result.description = obj.value(QLatin1String("description")).toString();
    }

    if (result.name.empty())
    {
        return {};
    }

    // try to create a dynamic ResourceItemDescriptor
    if (!getResourceItemDescriptor(result.name, result.descriptor))
    {
        QString schema;
        if (hasSchema)
        {
            schema = obj.value(QLatin1String("schema")).toString();
        }

        if (schema == QLatin1String("resourceitem1.schema.json"))
        {
            QString dataType;
            ResourceItemDescriptor rid{};

            if (obj.contains(QLatin1String("access")))
            {
                const auto access = obj.value(QLatin1String("access")).toString();
                if (access == QLatin1String("R"))
                {
                    rid.access = ResourceItemDescriptor::Access::ReadOnly;
                }
                else if (access == QLatin1String("RW"))
                {
                    rid.access = ResourceItemDescriptor::Access::ReadWrite;
                }
            }

            if (obj.contains(QLatin1String("datatype")))
            {
                QString dataType = obj.value(QLatin1String("datatype")).toString().toLower();
                rid.type = API_DataTypeFromString(dataType);
                if (dataType.startsWith("uint") || dataType.startsWith("int"))
                {
                    rid.qVariantType = QVariant::Double;
                }
                else if (rid.type == DataTypeReal)
                {
                    rid.qVariantType = QVariant::Double;
                }
                else if (rid.type == DataTypeBool)
                {
                    rid.qVariantType = QVariant::Bool;
                }
                else
                {
                    DBG_Assert(rid.type == DataTypeString || rid.type == DataTypeTime || rid.type == DataTypeTimePattern);
                    rid.qVariantType = QVariant::String;
                }
            }

            if (obj.contains(QLatin1String("range")))
            {
                const auto range = obj.value(QLatin1String("range")).toArray();
                if (range.count() == 2)
                {
                    bool ok1 = false;
                    bool ok2 = false;
                    double rangeMin = range.at(0).toString().toDouble(&ok1);
                    double rangeMax = range.at(1).toString().toDouble(&ok2);

                    if (ok1 && ok2)
                    {
                        rid.validMin = rangeMin;
                        rid.validMax = rangeMax;
                    }
                    // TODO validate range according to datatype
                }
            }

            if (rid.isValid())
            {
                rid.flags = ResourceItem::FlagDynamicDescriptor;

                // TODO this is fugly, should later on be changed to use the atom table
                size_t len = result.name.size();
                char *dynSuffix  = new char[len + 1];
                memcpy(dynSuffix, result.name.c_str(), len);
                dynSuffix[len] = '\0';
                rid.suffix = dynSuffix;

                // TODO ResourceItemDescriptor::flags (push, etc.)
                if (R_AddResourceItemDescriptor(rid))
                {
                    DBG_Printf(DBG_DDF, "DDF added dynamic ResourceItemDescriptor %s\n", result.name.c_str());
                }
            }
        }
        else
        {
            DBG_Printf(DBG_DDF, "DDF unsupported ResourceItem schema: %s\n", qPrintable(schema));
        }
    }

    if (getResourceItemDescriptor(result.name, result.descriptor))
    {
        if (obj.contains(QLatin1String("access")))
        {
            const auto access = obj.value(QLatin1String("access")).toString();
            if (access == "R")
            {
                result.descriptor.access = ResourceItemDescriptor::Access::ReadOnly;
            }
            else if (access == "RW")
            {
                result.descriptor.access = ResourceItemDescriptor::Access::ReadWrite;
            }
        }

        if (obj.contains(QLatin1String("public")))
        {
            result.isPublic = obj.value(QLatin1String("public")).toBool() ? 1 : 0;
            result.hasIsPublic = 1;
        }

        if (obj.contains(QLatin1String("implicit")))
        {
            result.isImplicit = obj.value(QLatin1String("implicit")).toBool() ? 1 : 0;
        }

        if (obj.contains(QLatin1String("awake")))
        {
            result.awake = obj.value(QLatin1String("awake")).toBool() ? 1 : 0;
        }

        if (obj.contains(QLatin1String("managed")))
        {
            result.isManaged = obj.value(QLatin1String("managed")).toBool() ? 1 : 0;
        }

        if (obj.contains(QLatin1String("static")))
        {
            result.isStatic = 1;
            result.defaultValue = obj.value(QLatin1String("static")).toVariant();
        }
        else
        {
            if (obj.contains(QLatin1String("default")))
            {
                result.defaultValue = obj.value(QLatin1String("default")).toVariant();
            }

            const auto parse = obj.value(QLatin1String("parse"));
            if (parse.isObject())
            {
                result.parseParameters = parse.toVariant();
            }

            const auto read = obj.value(QLatin1String("read"));
            if (read.isObject())
            {
                result.readParameters = read.toVariant();
            }

            if (obj.contains(QLatin1String("refresh.interval")))
            {
                result.refreshInterval = obj.value(QLatin1String("refresh.interval")).toInt(0);
            }

            const auto write = obj.value(QLatin1String("write"));
            if (write.isObject())
            {
                result.writeParameters = write.toVariant();
            }
        }

        if (DBG_IsEnabled(DBG_INFO_L2))
        {
            DBG_Printf(DBG_DDF, "DDF loaded resource item descriptor: %s, public: %u\n", result.descriptor.suffix, (result.isPublic ? 1 : 0));
        }
    }
    else
    {
        DBG_Printf(DBG_DDF, "DDF failed to load resource item descriptor: %s\n", result.name.c_str());
    }

    return result;
}

/*! Parses a sub device in a DDF object "subdevices" array.
    \returns The sub device object, use DeviceDescription::SubDevice::isValid() to check for success.
 */
static DeviceDescription::SubDevice DDF_ParseSubDevice(DDF_ParseContext *pctx, const QJsonObject &obj)
{
    DeviceDescription::SubDevice result;

    result.type = obj.value(QLatin1String("type")).toString();
    if (result.type.isEmpty())
    {
        return result;
    }

    result.restApi = obj.value(QLatin1String("restapi")).toString();
    if (result.restApi.isEmpty())
    {
        return result;
    }

    if (obj.contains(QLatin1String("meta")))
    {
        auto meta = obj.value(QLatin1String("meta"));
        if (meta.isObject())
        {
            result.meta = meta.toVariant().toMap();
        }
    }

    const auto uniqueId = obj.value(QLatin1String("uuid"));
    if (uniqueId.isArray())
    {
        const auto arr = uniqueId.toArray();
        for (const auto &i : arr)
        {
            result.uniqueId.push_back(i.toString());
        }
    }

    const auto fingerPrint = obj.value(QLatin1String("fingerprint"));
    if (fingerPrint.isObject())
    {
        bool ok;
        const auto fp = fingerPrint.toObject();
        result.fingerPrint.endpoint = fp.value(QLatin1String("endpoint")).toString().toUInt(&ok, 0);
        result.fingerPrint.profileId = ok ? fp.value(QLatin1String("profile")).toString().toUInt(&ok, 0) : 0;
        result.fingerPrint.deviceId = ok ? fp.value(QLatin1String("device")).toString().toUInt(&ok, 0) : 0;

        if (fp.value(QLatin1String("in")).isArray())
        {
            const auto arr = fp.value(QLatin1String("in")).toArray();
            for (const auto &cl : arr)
            {
                const auto clusterId = ok ? cl.toString().toUInt(&ok, 0) : 0;
                if (ok)
                {
                    result.fingerPrint.inClusters.push_back(clusterId);
                }
            }
        }

        if (fp.value(QLatin1String("out")).isArray())
        {
            const auto arr = fp.value(QLatin1String("out")).toArray();
            for (const auto &cl : arr)
            {
                const auto clusterId = ok ? cl.toString().toUInt(&ok, 0) : 0;
                if (ok)
                {
                    result.fingerPrint.outClusters.push_back(clusterId);
                }
            }
        }

        if (!ok)
        {
            result.fingerPrint = { };
        }
    }

    const auto items = obj.value(QLatin1String("items"));
    if (!items.isArray())
    {
        return result;
    }

    {
        const auto arr = items.toArray();
        for (const auto &i : arr)
        {
            if (i.isObject())
            {
                const auto item = DDF_ParseItem(pctx, i.toObject());

                if (item.isValid())
                {
                    result.items.push_back(item);
                }
                else
                {

                }
            }
        }
    }

    return result;
}

// {"at": "0x0021", "dt": "u8", "min": 5, "max": 3600, "change": 1 },

/*! Parses a ZCL report in a DDF_Binding object "report" array.
    \returns The ZCL report, use DDF_ZclReport::isValid() to check for success.
 */
static DDF_ZclReport DDF_ParseZclReport(const QJsonObject &obj)
{
    DDF_ZclReport result{};

    // check required fields
    if (!obj.contains(QLatin1String("at")) ||
        !obj.contains(QLatin1String("dt")) ||
        !obj.contains(QLatin1String("min")) ||
        !obj.contains(QLatin1String("max")))
    {
        return {};
    }

    bool ok = false;
    result.attributeId = obj.value(QLatin1String("at")).toString().toUShort(&ok, 0);

    if (!ok)
    {
        return {};
    }

    {
        auto dataType = obj.value(QLatin1String("dt")).toString().toUShort(&ok, 0);
        if (!ok || dataType > 0xFF)
        {
            return {};
        }
        result.dataType = dataType;
    }

    {
        const auto minInterval = obj.value(QLatin1String("min")).toInt(-1);

        if (minInterval < 0 || minInterval > UINT16_MAX)
        {
            return {};
        }

        result.minInterval = minInterval;
    }

    {
        const auto maxInterval = obj.value(QLatin1String("max")).toInt(-1);

        if (maxInterval < 0 || maxInterval > UINT16_MAX)
        {
            return {};
        }

        result.maxInterval = maxInterval;
    }

    if (obj.contains(QLatin1String("change")))
    {
        result.reportableChange = obj.value(QLatin1String("change")).toString().toUInt(&ok, 0);

        if (!ok)
        {
            return {};
        }
    }

    if (obj.contains(QLatin1String("mf")))
    {
        result.manufacturerCode = obj.value(QLatin1String("mf")).toString().toUShort(&ok, 0);

        if (!ok)
        {
            return {};
        }
    }

    result.valid = true;

    return result;
}

/*! Parses a binding in a DDF object "bindings" array.
    \returns The binding, use DDF_Binding::isValid() to check for success.
 */
static DDF_Binding DDF_ParseBinding(const QJsonObject &obj)
{
    DDF_Binding result{};

    // check required fields
    if (!obj.contains(QLatin1String("bind")) ||
        !obj.contains(QLatin1String("src.ep")) ||
        !obj.contains(QLatin1String("cl")))
    {
        return {};
    }

    const auto type = obj.value(QLatin1String("bind")).toString();

    if (type == QLatin1String("unicast"))
    {
        result.isUnicastBinding = 1;
    }
    else if (type == QLatin1String("groupcast"))
    {
        result.isGroupBinding = 1;
    }
    else
    {
        return {};
    }

    bool ok = false;
    {
        const auto srcEndpoint = obj.value(QLatin1String("src.ep")).toInt(-1);

        if (srcEndpoint < 0 || srcEndpoint > UINT8_MAX)
        {
            return {};
        }
        result.srcEndpoint = srcEndpoint;
    }

    {
        result.clusterId = obj.value(QLatin1String("cl")).toString().toUShort(&ok, 0);

        if (!ok)
        {
            return {};
        }
    }

    if (obj.contains(QLatin1String("dst.ep")))
    {
        const auto dstEndpoint = obj.value(QLatin1String("dst.ep")).toInt(-1);
        if (dstEndpoint < 0 || dstEndpoint >= 255)
        {
            return {};
        }
        result.dstEndpoint = dstEndpoint;
    }
    else
    {
        result.dstEndpoint = 0;
    }

    if (result.isGroupBinding && obj.contains(QLatin1String("config.group")))
    {
        const auto configGroup = obj.value(QLatin1String("config.group")).toInt(-1);
        if (configGroup < 0 || configGroup >= 255)
        {
            return {};
        }
        result.configGroup = configGroup;
    }
    else
    {
        result.configGroup = 0;
    }

    const auto report = obj.value(QLatin1String("report"));
    if (report.isArray())
    {
        const auto reportArr = report.toArray();
        for (const auto &i : reportArr)
        {
            if (i.isObject())
            {
                const auto rep = DDF_ParseZclReport(i.toObject());
                if (isValid(rep))
                {
                    result.reporting.push_back(rep);
                }
            }
        }
    }

    return result;
}

/*! Parses a single string or array of strings in DDF JSON object.
    The obj[key] value can be a string or array of strings.
    \returns List of parsed strings.
 */
static QStringList DDF_ParseStringOrList(const QJsonObject &obj, QLatin1String key)
{
    QStringList result;
    const auto val = obj.value(key);

    if (val.isString()) // "key": "alpha.sensor"
    {
        result.push_back(val.toString());
    }
    else if (val.isArray()) // "key": [ "alpha.sensor", "beta.sensor" ]
    {
        const auto arr = val.toArray();
        for (const auto &i : arr)
        {
            if (i.isString())
            {
                result.push_back(i.toString());
            }
        }
    }

    return result;
}

/*! Parses an DDF JSON object.
    \returns DDF object, use DeviceDescription::isValid() to check for success.
 */
static DeviceDescription DDF_ParseDeviceObject(DDF_ParseContext *pctx, const QJsonObject &obj)
{
    DeviceDescription result;

    const auto schema = obj.value(QLatin1String("schema")).toString();

    if (schema != QLatin1String("devcap1.schema.json"))
    {
        return result;
    }

    const auto subDevices = obj.value(QLatin1String("subdevices"));
    if (!subDevices.isArray())
    {
        return result;
    }

    U_ASSERT(pctx->filePathLength != 0);
    U_ASSERT(pctx->filePath[pctx->filePathLength] == '\0');
    result.path = &pctx->filePath[0];

    // TODO(mpi): get rid of QStringLists and only use atoms
    result.manufacturerNames = DDF_ParseStringOrList(obj, QLatin1String("manufacturername"));
    result.modelIds = DDF_ParseStringOrList(obj, QLatin1String("modelid"));
    result.product = obj.value(QLatin1String("product")).toString();

    if (obj.contains(QLatin1String("status")))
    {
        result.status = obj.value(QLatin1String("status")).toString();
    }

    if (obj.contains(QLatin1String("vendor")))
    {
        result.vendor = obj.value(QLatin1String("vendor")).toString();
    }

    if (obj.contains(QLatin1String("sleeper")))
    {
        result.sleeper = obj.value(QLatin1String("sleeper")).toBool() ? 1 : 0;
    }

    if (obj.contains(QLatin1String("supportsMgmtBind")))
    {
        result.supportsMgmtBind = obj.value(QLatin1String("supportsMgmtBind")).toBool() ? 1 : 0;
    }

    if (obj.contains(QLatin1String("matchexpr")))
    {
        result.matchExpr = obj.value(QLatin1String("matchexpr")).toString();
    }

    const auto keys = obj.keys();
    for (const auto &key : keys)
    {
        DBG_Printf(DBG_DDF, "DDF %s: %s\n", qPrintable(key), qPrintable(obj.value(key).toString()));
    }

    const auto subDevicesArr = subDevices.toArray();
    for (const auto &i : subDevicesArr)
    {
        if (i.isObject())
        {
            const auto sub = DDF_ParseSubDevice(pctx, i.toObject());
            if (sub.isValid())
            {
                result.subDevices.push_back(sub);
            }
        }
    }

    const auto bindings = obj.value(QLatin1String("bindings"));
    if (bindings.isArray())
    {
        const auto bindingsArr = bindings.toArray();
        for (const auto &i : bindingsArr)
        {
            if (i.isObject())
            {
                const auto bnd = DDF_ParseBinding(i.toObject());
                if (isValid(bnd))
                {
                    result.bindings.push_back(bnd);
                }
            }
        }
    }

    return result;
}

/*! Reads an item file under (generic/items/).
    \returns A parsed item, use DeviceDescription::Item::isValid() to check for success.
 */
static DeviceDescription::Item DDF_ReadItemFile(DDF_ParseContext *pctx)
{
    if (!pctx->fileData || pctx->fileDataSize < 16)
    {
        return { };
    }

    const QByteArray data = QByteArray::fromRawData((const char*)pctx->fileData, pctx->fileDataSize);

    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(data, &error);

    if (error.error != QJsonParseError::NoError)
    {
        DBG_Printf(DBG_DDF, "DDF failed to read %s, err: %s, offset: %d\n", pctx->filePath, qPrintable(error.errorString()), error.offset);
        return { };
    }

    if (doc.isObject())
    {
        return DDF_ParseItem(pctx, doc.object());
    }

    return { };
}

/*! Reads an subdevice file under (generic/subdevices/).
    \returns A parsed subdevice, use isValid(DDF_SubDeviceDescriptor) to check for success.
 */
static DDF_SubDeviceDescriptor DDF_ReadSubDeviceFile(DDF_ParseContext *pctx)
{
    DDF_SubDeviceDescriptor result = { };

    if (!pctx->fileData || pctx->fileDataSize < 16)
    {
        return result;
    }

    const QByteArray data = QByteArray::fromRawData((const char*)pctx->fileData, pctx->fileDataSize);

    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(data, &error);

    if (error.error != QJsonParseError::NoError)
    {
        DBG_Printf(DBG_DDF, "DDF failed to read %s, err: %s, offset: %d\n", pctx->filePath, qPrintable(error.errorString()), error.offset);
        return result;
    }

    if (doc.isObject())
    {
        const auto obj = doc.object();
        QString schema;
        if (obj.contains(QLatin1String("schema")))
        {
            schema = obj.value(QLatin1String("schema")).toString();
        }

        if (schema != QLatin1String("subdevice1.schema.json"))
        {
            return result;
        }

        if (obj.contains(QLatin1String("name")))
        {
            result.name = obj.value(QLatin1String("name")).toString();
        }
        if (obj.contains(QLatin1String("type")))
        {
            result.type = obj.value(QLatin1String("type")).toString();
        }
        if (obj.contains(QLatin1String("restapi")))
        {
            result.restApi = obj.value(QLatin1String("restapi")).toString();
        }

        result.order = obj.value(QLatin1String("order")).toInt(SUBDEVICE_DEFAULT_ORDER);

        if (obj.contains(QLatin1String("uuid")))
        {
            const auto uniqueId = obj.value(QLatin1String("uuid"));
            if (uniqueId.isArray())
            {
                const auto arr = uniqueId.toArray();
                for (const auto &i : arr)
                {
                    DBG_Assert(i.isString());
                    result.uniqueId.push_back(i.toString());
                }
            }
        }
        if (obj.contains(QLatin1String("items")))
        {
            const auto items = obj.value(QLatin1String("items"));
            if (items.isArray())
            {
                const auto arr = items.toArray();
                for (const auto &i : arr)
                {
                    DBG_Assert(i.isString());
                    ResourceItemDescriptor rid;
                    if (getResourceItemDescriptor(i.toString(), rid))
                    {
                        result.items.push_back(rid.suffix);
                    }
                }
            }
        }
    }

    return result;
}

static QVariant DDF_ResolveParamScript(const QVariant &param, const QString &path)
{
    auto result = param;

    if (param.type() != QVariant::Map)
    {
        return result;
    }

    auto map = param.toMap();

    if (map.contains(QLatin1String("script")))
    {
        const auto script = map["script"].toString();

        const QFileInfo fi(path);
        QFile f(fi.canonicalPath() + "/" + script);

        if (f.exists() && f.open(QFile::ReadOnly))
        {
            QString content = f.readAll();
            if (!content.isEmpty())
            {
                DDF_TryCompileAndFixJavascript(&content, path);
                map["eval"] = content;
                result = std::move(map);
            }
        }
    }
    else if (map.contains(QLatin1String("eval")))
    {
        QString content = map[QLatin1String("eval")].toString();
        if (!content.isEmpty())
        {
            DDF_TryCompileAndFixJavascript(&content, path);
            map[QLatin1String("eval")] = content;
            result = std::move(map);
        }
    }

    return result;
}

static QVariant DDF_ResolveBundleParamScript(const QVariant &param, DDF_ParseContext *pctx)
{
    auto result = param;

    if (param.type() != QVariant::Map)
    {
        return result;
    }

    auto map = param.toMap();
    unsigned fnameStart;

    if (map.contains(QLatin1String("script")))
    {
        const std::string script = map["script"].toString().toStdString();

        for (DDFB_ExtfChunk *extf = pctx->extChunks; extf; extf = extf->next)
        {
            if (extf->fileType[0] != 'S' || extf->fileType[1] != 'C' || extf->fileType[2] != 'J' || extf->fileType[3] != 'S')
                continue;

            if (extf->pathLength < 4) // should not happen: a.js
                continue;

            /*
             * Lookup just the filename of the Javascript file in the bundle.
             * While this could be wrong in theory it's unlikely.
             * If needed we can resolve relative paths later on to be more strict.
             */
            fnameStart = extf->pathLength;
            for (;fnameStart; fnameStart--)
            {
                if (extf->path[fnameStart] == '/')
                    break;
            }

            unsigned fnameLength = extf->pathLength - fnameStart;
            if (script.size() < fnameLength)
                continue;

            if (U_memcmp(script.c_str() + (script.size() - fnameLength), &extf->path[fnameStart], fnameLength) != 0)
                continue;

            QString content = QString::fromUtf8((const char*)extf->fileData, extf->fileSize);

            if (!content.isEmpty())
            {
                map["eval"] = content;
            }

            break;
        }
    }

    if (map.contains(QLatin1String("eval")))
    {
        QString content = map[QLatin1String("eval")].toString();
        if (!content.isEmpty())
        {
            QString path; // dummy
            DDF_TryCompileAndFixJavascript(&content, path);
            map[QLatin1String("eval")] = content;
            result = std::move(map);
        }
    }

    return result;
}

DeviceDescription DDF_LoadScripts(const DeviceDescription &ddf)
{
    auto result = ddf;

    for (auto &sub : result.subDevices)
    {
        for (auto &item : sub.items)
        {
            item.parseParameters = DDF_ResolveParamScript(item.parseParameters, ddf.path);
            item.readParameters = DDF_ResolveParamScript(item.readParameters, ddf.path);
            item.writeParameters = DDF_ResolveParamScript(item.writeParameters, ddf.path);
        }
    }

    return result;
}

/*! Reads a DDF file which may contain one or more device descriptions.
    \returns Vector of parsed DDF objects.
 */
static DeviceDescription DDF_ReadDeviceFile(DDF_ParseContext *pctx)
{
    U_ASSERT(pctx->fileData);
    U_ASSERT(pctx->fileDataSize > 64);

    const QByteArray data = QByteArray::fromRawData((const char*)pctx->fileData, pctx->fileDataSize);
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(data, &error);

    if (error.error != QJsonParseError::NoError)
    {
        DBG_Printf(DBG_DDF, "DDF failed to read %s, err: %s, offset: %d\n", pctx->filePath, qPrintable(error.errorString()), error.offset);
        return { };
    }

    if (doc.isObject())
    {
        DeviceDescription ddf = DDF_ParseDeviceObject(pctx, doc.object());
        if (ddf.isValid())
        {
            return ddf;
        }
    }

    return { };
}

static int DDF_ProcessSignatures(DDF_ParseContext *pctx, std::vector<U_ECC_PublicKeySecp256k1> &publicKeys, U_BStream *bs, uint32_t *bundleHash)
{
    unsigned i;
    unsigned count;
    uint16_t pubkeyLen;
    uint16_t sigLen;

    U_ECC_PublicKeySecp256k1 pubkey;
    U_ECC_SignatureSecp256k1 sig;

    count = 0;

    for (;bs->status == U_BSTREAM_OK && bs->pos < bs->size;)
    {
        pubkeyLen = U_bstream_get_u16_le(bs);
        if (sizeof(pubkey.key) < pubkeyLen)
        {
            return 0;
        }

        for (i = 0; i < pubkeyLen; i++)
        {
            pubkey.key[i] = U_bstream_get_u8(bs);
        }

        sigLen = U_bstream_get_u16_le(bs);
        if (sizeof(sig.sig) < sigLen)
        {
            return 0;
        }

        for (i = 0; i < sigLen; i++)
        {
            sig.sig[i] = U_bstream_get_u8(bs);
        }

        if (U_ECC_VerifySignatureSecp256k1(&pubkey, &sig, (uint8_t*)bundleHash, U_SHA256_HASH_SIZE))
        {
            for (i = 0; i < publicKeys.size(); i++)
            {
                U_ECC_PublicKeySecp256k1 &pk = publicKeys[i];

                if (U_memcmp(pk.key, pubkey.key, sizeof(pk.key)) == 0)
                    break; // already known
            }

            if (i == publicKeys.size() && publicKeys.size() < DDF_MAX_PUBLIC_KEYS)
            {
                publicKeys.push_back(pubkey);
            }

            pctx->signatures |= (1 << i);
            count++;
        }
    }

    if (count)
        return 1;

    return 0;
}

/*! Merge common properties like "read", "parse" and "write" functions from generic items into DDF items.
    Only properties which are already defined in the DDF file won't be overwritten.

    \param genericItems - generic items used as source
    \param ddf - DDF object with unmerged items
    \returns The merged DDF object.
 */
static DeviceDescription DDF_MergeGenericItems(const std::vector<DeviceDescription::Item> &genericItems, const DeviceDescription &ddf)
{
    auto result = ddf;

    for (auto &sub : result.subDevices)
    {
        for (auto &item : sub.items)
        {
            const auto genItem = std::find_if(genericItems.cbegin(), genericItems.cend(),
                                              [&item](const DeviceDescription::Item &i){ return i.descriptor.suffix == item.descriptor.suffix; });
            if (genItem == genericItems.cend())
            {
                continue;
            }

            item.isImplicit = genItem->isImplicit;
            item.isManaged = genItem->isManaged;
            item.isGenericRead = 0;
            item.isGenericWrite = 0;
            item.isGenericParse = 0;

            if (!item.isStatic)
            {
                if (item.readParameters.isNull()) { item.readParameters = genItem->readParameters; item.isGenericRead = 1; }
                if (item.writeParameters.isNull()) { item.writeParameters = genItem->writeParameters; item.isGenericWrite = 1; }
                if (item.parseParameters.isNull()) { item.parseParameters = genItem->parseParameters; item.isGenericParse = 1; }
                if (item.refreshInterval == DeviceDescription::Item::NoRefreshInterval && genItem->refreshInterval != item.refreshInterval)
                {
                    item.refreshInterval = genItem->refreshInterval;
                }
            }

            if (item.descriptor.access == ResourceItemDescriptor::Access::Unknown)
            {
                item.descriptor.access = genItem->descriptor.access;
            }

            if (!item.hasIsPublic)
            {
                item.isPublic = genItem->isPublic;
            }

            if (!item.defaultValue.isValid() && genItem->defaultValue.isValid())
            {
                item.defaultValue = genItem->defaultValue;
            }
        }
    }

    return result;
}

static int DDF_MergeGenericBundleItems(DeviceDescription &ddf, DDF_ParseContext *pctx)
{
    std::vector<DeviceDescription::Item> genericItems;

    {
        // preserve parse context
        uint8_t *fileData = pctx->fileData;
        unsigned fileDataSize = pctx->fileDataSize;

        /*
         * Load generic items from bundle.
         */

        for (DDFB_ExtfChunk *extf = pctx->extChunks; extf; extf = extf->next)
        {
            U_SStream ss;
            U_sstream_init(&ss, (void*)extf->path, extf->pathLength);
            if (U_sstream_starts_with(&ss, "generic/items") == 0)
                continue;

            // temp. change where data points
            pctx->fileData = extf->fileData;
            pctx->fileDataSize = extf->fileSize;

            DeviceDescription::Item item = DDF_ReadItemFile(pctx);
            if (item.isValid())
            {
                genericItems.push_back(std::move(item));
            }
            else
            {
                U_ASSERT(0 && "failed to read bundle item file");
            }
        }

        // restore parse context
        pctx->fileData = fileData;
        pctx->fileDataSize = fileDataSize;
    }


    for (DeviceDescription::SubDevice &sub : ddf.subDevices)
    {
        for (DeviceDescription::Item &item : sub.items)
        {
            const auto genItem = std::find_if(genericItems.cbegin(), genericItems.cend(),
                                              [&item](const DeviceDescription::Item &i){ return i.descriptor.suffix == item.descriptor.suffix; });
            if (genItem == genericItems.cend())
            {
                continue;
            }

            item.isImplicit = genItem->isImplicit;
            item.isManaged = genItem->isManaged;
            item.isGenericRead = 0;
            item.isGenericWrite = 0;
            item.isGenericParse = 0;

            if (!item.isStatic)
            {
                if (item.readParameters.isNull()) { item.readParameters = genItem->readParameters; item.isGenericRead = 1; }
                if (item.writeParameters.isNull()) { item.writeParameters = genItem->writeParameters; item.isGenericWrite = 1; }
                if (item.parseParameters.isNull()) { item.parseParameters = genItem->parseParameters; item.isGenericParse = 1; }
                if (item.refreshInterval == DeviceDescription::Item::NoRefreshInterval && genItem->refreshInterval != item.refreshInterval)
                {
                    item.refreshInterval = genItem->refreshInterval;
                }

                item.parseParameters = DDF_ResolveBundleParamScript(item.parseParameters, pctx);
                item.readParameters = DDF_ResolveBundleParamScript(item.readParameters, pctx);
                item.writeParameters = DDF_ResolveBundleParamScript(item.writeParameters, pctx);
            }

            if (item.descriptor.access == ResourceItemDescriptor::Access::Unknown)
            {
                item.descriptor.access = genItem->descriptor.access;
            }

            if (!item.hasIsPublic)
            {
                item.isPublic = genItem->isPublic;
            }

            if (!item.defaultValue.isValid() && genItem->defaultValue.isValid())
            {
                item.defaultValue = genItem->defaultValue;
            }
        }
    }

    return 1;
}

uint8_t DDF_GetSubDeviceOrder(const QString &type)
{
    if (type.isEmpty() || type.startsWith(QLatin1String("CLIP")))
    {
        return SUBDEVICE_DEFAULT_ORDER;
    }

    if (_priv)
    {
        auto i = std::find_if(_priv->subDevices.cbegin(), _priv->subDevices.cend(), [&](const auto &sub)
        { return sub.name == type; });

        if (i != _priv->subDevices.cend())
        {
            return i->order;
        }
    }

#ifdef QT_DEBUG
    DBG_Printf(DBG_DDF, "DDF No subdevice for type: %s\n", qPrintable(type));
#endif

    return SUBDEVICE_DEFAULT_ORDER;
}

/*! Creates a unique Resource handle.
 */
Resource::Handle R_CreateResourceHandle(const Resource *r, size_t containerIndex)
{
    Q_ASSERT(r->prefix() != nullptr);
    Q_ASSERT(!r->item(RAttrUniqueId)->toString().isEmpty());

    Resource::Handle result;
    result.hash = qHash(r->item(RAttrUniqueId)->toString());
    result.index = containerIndex;
    result.type = r->prefix()[1];
    result.order = 0;

    Q_ASSERT(result.type == 's' || result.type == 'l' || result.type == 'd' || result.type == 'g');
    Q_ASSERT(isValid(result));

    if (result.type == 's' || result.type == 'l')
    {
        const ResourceItem *type = r->item(RAttrType);
        if (type)
        {
            result.order = DDF_GetSubDeviceOrder(type->toString());
        }
    }

    return result;
}
