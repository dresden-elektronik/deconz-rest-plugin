/*
 * lumi.cpp
 *
 * Implementation of Lumi cluster.
 *
 */

#include <regex>
#include "de_web_plugin.h"
#include "de_web_plugin_private.h"

//***********************************************************************************


//******************************************************************************************

/*! Helper to generate a new task with new task and req id based on a reference */
static void copyTaskReq(TaskItem &a, TaskItem &b)
{
    b.req.dstAddress() = a.req.dstAddress();
    b.req.setDstAddressMode(a.req.dstAddressMode());
    b.req.setSrcEndpoint(a.req.srcEndpoint());
    b.req.setDstEndpoint(a.req.dstEndpoint());
    b.req.setRadius(a.req.radius());
    b.req.setTxOptions(a.req.txOptions());
    b.req.setSendDelay(a.req.sendDelay());
    b.zclFrame.payload().clear();
}

/*! Handle packets related to Lumi 0xFCC0 cluster.
    \param ind the APS level data indication containing the ZCL packet
    \param zclFrame the actual ZCL frame which holds the scene cluster reponse
 */

void DeRestPluginPrivate::handleLumiClusterIndication(const deCONZ::ApsDataIndication &ind, deCONZ::ZclFrame &zclFrame)
{
    if (zclFrame.isDefaultResponse())
    {
        return;
    }

    bool update = false;

    LightNode *lightNode = getLightNodeForAddress(ind.srcAddress(), ind.srcEndpoint());

    if (!lightNode)
    {
        return;
    }
    
    // DBG_Printf(DBG_INFO, "Tuya debug 4 : Address 0x%016llX, Command 0x%02X, Payload %s\n", ind.srcAddress().ext(), zclFrame.commandId(), qPrintable(zclFrame.payload().toHex()));

    if (update)
    {
        if (lightNode)
        {
            // Update Node light
            updateLightEtag(&*lightNode);
            lightNode->setNeedSaveDatabase(true);
            saveDatabaseItems |= DB_LIGHTS;
        }
    }

}
