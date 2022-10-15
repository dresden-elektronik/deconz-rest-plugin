/*
 * Handle Hue-specific FC03 cluster.
 */

#include <QString>
// #include <QVariantMap>
#include "de_web_plugin.h"
#include "de_web_plugin_private.h"
// #include "colorspace.h"
// #include "device_descriptions.h"

#define HUE_CLUSTER_ID 0xFC03

const QStringList RStateEffectValuesHue({
    "none", "colorloop", "candle", "fireplace", "loop"
});

/*! Add a Hue dynamic effect task to the queue.

   \param task - the task item
   \param scene - the dynamic scene (0: none, 1: candle, 2: fireplace, 3: loop)
   \return true - on success
           false - on error
 */
bool DeRestPluginPrivate::addTaskHueDynamicScene(TaskItem &task, quint8 scene)
{
    task.taskType = TaskSetColorLoop;
    task.req.setClusterId(HUE_CLUSTER_ID);
    task.req.setProfileId(HA_PROFILE_ID);

    task.zclFrame.payload().clear();
    task.zclFrame.setSequenceNumber(zclSeq++);
    task.zclFrame.setCommandId(0x00);
    task.zclFrame.setManufacturerCode(VENDOR_PHILIPS);
    task.zclFrame.setFrameControl(deCONZ::ZclFCClusterCommand |
                                  deCONZ::ZclFCManufacturerSpecific |
                                  deCONZ::ZclFCDirectionClientToServer |
                                  deCONZ::ZclFCDisableDefaultResponse);

    { // payload
        QDataStream stream(&task.zclFrame.payload(), QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);

        if (scene == 0) {
            stream << (quint16) 0x0020; // clear dynamic scene
            stream << (quint8) 0; // off
        } else {
            stream << (quint16) 0x0021; // set dynamic scene (with on/off)
            stream << (quint8) 1; // on
            stream << scene;
        }
    }

    { // ZCL frame
        task.req.asdu().clear(); // cleanup old request data if there is any
        QDataStream stream(&task.req.asdu(), QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);
        task.zclFrame.writeToStream(stream);
    }

    return addTask(task);
}
