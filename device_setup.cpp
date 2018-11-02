/*
 * device_setup.cpp
 *
 * Configures ubisys manufacturer specific Device_Setup Cluster 0xFC00 on Endpoint 232 (0xE8).
 *
 * It can be used to control the behavior of inputs (i.e. permanent switches, push-button switches,
 * normally open vs. normally closed, on/off, level control or scene selection, etc.)
 * Example usage:
 *   -X PUT -d '{ "mode": "momentary" }' /sensor/xx/config
 *   -X PUT -d '{ "mode": "rocker" }' /sensor/xx/config
 *
 * In addition to predefined configurations for momentary switches and rocker switches,
 * this implementation allows custom configurations for a more complex move or recall scene command.
 * Example usage:
 *   -X PUT -d '{ "mode": "custom_41020006000D0306000206010D04060002" }' /sensor/xx/config
 *   writes ZCL raw data "41020006000D0306000206010D04060002" to attribute 0x0001 on cluster 0xFC00
 *
 * Implements support for:
 * - ubisys S1 power switch
 * - ubisys S2 power switch
 * - ubisys J1 shutter control
 *
 */

#include "de_web_plugin.h"
#include "de_web_plugin_private.h"

#define UBISYS_J1  0x01
#define UBISYS_S1  0x02
#define UBISYS_S2  0x03

static bool addPayloadJ1(TaskItem &task, ResourceItem *item)
{ // payload
	QDataStream stream(&task.zclFrame.payload(), QIODevice::ReadWrite);
	stream.setByteOrder(QDataStream::LittleEndian);

	if (item->toString() == QLatin1String("momentary"))  // default configuration for J1
	{
		stream << (quint16)0x0001; // attribute id InputActions
		stream << (quint8)0x48;    // attribute type array

		stream << (quint8)0x41;    // attribute datatype: octed string
		stream << (quint16)0x0004; // element count: 0x0004 (4 entries)

		stream << (quint8)0x06;    // element #1: length: 6
		stream << (quint8)0x00;    // InputAndOptions: 0x00 = first physical input
		stream << (quint8)0x0D;    // transition: released -> pressed
		stream << (quint8)0x02;    // Source: Endpoint #2 (hosts window covering client cluster on J1)
		stream << (quint16)0x0102; // Cluster ID: 0x0102 – window covering
		stream << (quint8)0x00;    // ZCL command template: Move up/open

		stream << (quint8)0x06;    // element #2
		stream << (quint8)0x00;
		stream << (quint8)0x07;    // transition: pressed -> released
		stream << (quint8)0x02;
		stream << (quint16)0x0102;
		stream << (quint8)0x02;    // ZCL command template: Stop

		stream << (quint8)0x06;    // element #3
		stream << (quint8)0x01;    // InputAndOptions: 0x01 = second physical input
		stream << (quint8)0x0D;    // transition: released -> pressed
		stream << (quint8)0x02;
		stream << (quint16)0x0102;
		stream << (quint8)0x01;    // ZCL command template: Move down/close

		stream << (quint8)0x06;    // element #4
		stream << (quint8)0x01;
		stream << (quint8)0x07;    // transition: pressed -> released
		stream << (quint8)0x02;
		stream << (quint16)0x0102;
		stream << (quint8)0x02;    // ZCL command template: Stop

	}
	else if (item->toString() == QLatin1String("rocker"))
	{
		stream << (quint16)0x0001; // attribute id InputActions
		stream << (quint8)0x48;    // attribute type array

		stream << (quint8)0x41;    // attribute datatype: octed string
		stream << (quint16)0x0004; // element count: 0x0004 (4 entries)

		stream << (quint8)0x06;    // element #1: length 6
		stream << (quint8)0x00;    // InputAndOptions: 0x00
		stream << (quint8)0x0D;    // transition: released -> pressed
		stream << (quint8)0x02;    // Source: Endpoint #2 (hosts window covering client cluster on J1)
		stream << (quint16)0x0102; // Cluster ID: 0x0102 – window covering
		stream << (quint8)0x00;    // ZCL command template: Move up/open

		stream << (quint8)0x06;    // element #2
		stream << (quint8)0x00;    // InputAndOptions: 0x00
		stream << (quint8)0x03;    // transition: any state -> released
		stream << (quint8)0x02;
		stream << (quint16)0x0102;
		stream << (quint8)0x02;    // ZCL command template: Stop

		stream << (quint8)0x06;    // element #3
		stream << (quint8)0x01;    // InputAndOptions: 0x01
		stream << (quint8)0x0D;    // transition: released -> pressed
		stream << (quint8)0x02;
		stream << (quint16)0x0102;
		stream << (quint8)0x01;    // ZCL command template: Move down/close

		stream << (quint8)0x06;    // element #4
		stream << (quint8)0x01;
		stream << (quint8)0x03;    // transition: any state -> released
		stream << (quint8)0x02;
		stream << (quint16)0x0102;
		stream << (quint8)0x02;    // ZCL command template: Stop

	}
	else if (item->toString().startsWith("custom_"))
	{
		stream << (quint16)0x0001; // attribute id InputActions
		stream << (quint8)0x48;    // attribute type array

		// Example J1 momentary: "custom_41040006000d020201000600070202010206010d0202010106010702020102"
		// Example J1 rocker:    "custom_41040006000d020201000600030202010206010d0202010106010302020102"
		//                                                       ^^                          ^^
		QString subStr("custom");
		QByteArray command = item->toString().toUtf8();
		command.remove(0, subStr.size() + 1);  // remove custom_
		QByteArray payload = QByteArray::fromHex(command);
		stream.writeRawData(payload.data(), payload.size());

		item->setValue(subStr); // setValue "custom"
	}
	else
	{
		return false;  // do nothing
	}

	return true;
}

static bool addPayloadS1(TaskItem &task, ResourceItem *item)
{ // payload
	QDataStream stream(&task.zclFrame.payload(), QIODevice::ReadWrite);
	stream.setByteOrder(QDataStream::LittleEndian);

	if (item->toString() == QLatin1String("momentary"))
	{
		stream << (quint16)0x0001; // attribute id InputActions
		stream << (quint8)0x48;    // attribute type array

		stream << (quint8)0x41;    // attribute datatype: octed string
		stream << (quint16)0x0002; // element count: 0x0001 (1 entries)

		stream << (quint8)0x06;    // element #1: length: 6
		stream << (quint8)0x00;    // InputAndOptions: 0x00 = first physical input
		stream << (quint8)0x0D;    // transition: released -> pressed
		stream << (quint8)0x02;    // Source: Endpoint #2 (hosts primary on/off client cluster on S1)
		stream << (quint16)0x0006; // Cluster ID: 0x0006 - on/off
		stream << (quint8)0x02;    // ZCL command template: Toggle
	}
	else if (item->toString() == QLatin1String("rocker"))  // default configuration for S1
	{
		stream << (quint16)0x0001; // attribute id InputActions
		stream << (quint8)0x48;    // attribute type array

		stream << (quint8)0x41;    // attribute datatype: octed string
		stream << (quint16)0x0004; // element count: 0x0002 (2 entries)

		stream << (quint8)0x06;    // element #1: length: 6
		stream << (quint8)0x00;    // InputAndOptions: 0x00 = first physical input
		stream << (quint8)0x0D;    // transition: released -> pressed
		stream << (quint8)0x02;    // Source: Endpoint #2 (hosts primary on/off client cluster on S1)
		stream << (quint16)0x0006; // Cluster ID: 0x0006 - on/off
		stream << (quint8)0x02;    // ZCL command template: Toggle

		stream << (quint8)0x06;    // element #2: length: 6
		stream << (quint8)0x00;    // InputAndOptions: 0x00 = first physical input
		stream << (quint8)0x03;    // transition: any state -> released
		stream << (quint8)0x02;    // Source: Endpoint #2 (hosts primary on/off client cluster on S1)
		stream << (quint16)0x0006; // Cluster ID: 0x0006 - on/off
		stream << (quint8)0x02;    // ZCL command template: Toggle
	}
	else if (item->toString().startsWith("custom_"))
	{
		stream << (quint16)0x0001; // attribute id InputActions
		stream << (quint8)0x48;    // attribute type array

		QString subStr("custom");
		QByteArray command = item->toString().toUtf8();
		command.remove(0, subStr.size() + 1);  // remove custom_
		QByteArray payload = QByteArray::fromHex(command);
		stream.writeRawData(payload.data(), payload.size());

		item->setValue(subStr); // setValue "custom"
	}
	else
	{
		return false;  // do nothing
	}

	return true;
}

static bool addPayloadS2(TaskItem &task, ResourceItem *item)
{ // payload
	QDataStream stream(&task.zclFrame.payload(), QIODevice::ReadWrite);
	stream.setByteOrder(QDataStream::LittleEndian);

	if (item->toString() == QLatin1String("momentary"))  // default configuration for S2
	{
		stream << (quint16)0x0001; // attribute id InputActions
		stream << (quint8)0x48;    // attribute type array

		stream << (quint8)0x41;    // attribute datatype: octed string
		stream << (quint16)0x0002; // element count: 0x0002 (2 entries)

		stream << (quint8)0x06;    // element #1: length: 6
		stream << (quint8)0x00;    // InputAndOptions: 0x00 = first physical input
		stream << (quint8)0x0D;    // transition: released -> pressed
		stream << (quint8)0x03;    // Source: Endpoint #3 (hosts primary on/off client cluster on S2)
		stream << (quint16)0x0006; // Cluster ID: 0x0006 - on/off
		stream << (quint8)0x02;    // ZCL command template: Toggle

		stream << (quint8)0x06;    // element #2
		stream << (quint8)0x01;    // InputAndOptions: 0x01 = second physical input
		stream << (quint8)0x0D;    // transition: released -> pressed
		stream << (quint8)0x04;    // Endpoint #4 (hosts secondary on/off client cluster on S2)
		stream << (quint16)0x0006;
		stream << (quint8)0x02;    // ZCL command template: Toggle

	}
	else if (item->toString() == QLatin1String("rocker"))
	{
		stream << (quint16)0x0001; // attribute id InputActions
		stream << (quint8)0x48;    // attribute type array

		stream << (quint8)0x41;    // attribute datatype: octed string
		stream << (quint16)0x0004; // element count: 0x0004 (4 entries)

		stream << (quint8)0x06;    // element #1: length: 6
		stream << (quint8)0x00;    // InputAndOptions: 0x00 = first physical input
		stream << (quint8)0x0D;    // transition: released -> pressed
		stream << (quint8)0x03;    // Source: Endpoint #3 (hosts primary on/off client cluster on S2)
		stream << (quint16)0x0006; // Cluster ID: 0x0006 - on/off
		stream << (quint8)0x02;    // ZCL command template: Toggle

		stream << (quint8)0x06;    // element #2
		stream << (quint8)0x01;    // InputAndOptions: 0x01 = second physical input
		stream << (quint8)0x0D;    // transition: released -> pressed
		stream << (quint8)0x04;    // Endpoint #4 (hosts secondary on/off client cluster on S2)
		stream << (quint16)0x0006;
		stream << (quint8)0x02;    // ZCL command template: Toggle

		stream << (quint8)0x06;    // element #3: length: 6
		stream << (quint8)0x00;    // InputAndOptions: 0x00 = first physical input
		stream << (quint8)0x03;    // transition: any state -> released
		stream << (quint8)0x03;    // Source: Endpoint #3 (hosts primary on/off client cluster on S2)
		stream << (quint16)0x0006; // Cluster ID: 0x0006 - on/off
		stream << (quint8)0x02;    // ZCL command template: Toggle

		stream << (quint8)0x06;    // element #4
		stream << (quint8)0x01;    // InputAndOptions: 0x01 = second physical input
		stream << (quint8)0x03;    // transition: any state -> released
		stream << (quint8)0x04;    // Endpoint #4 (hosts secondary on/off client cluster on S2)
		stream << (quint16)0x0006;
		stream << (quint8)0x02;    // ZCL command template: Toggle

	}
	else if (item->toString().startsWith("custom_"))
	{
		stream << (quint16)0x0001; // attribute id InputActions
		stream << (quint8)0x48;    // attribute type array

		// Example S2 momentary: "custom_41020006000D0306000206010D04060002"
		// Example S2 rocker:    "custom_41040006000D0306000206010D040600020600030306000206010304060002"
		QString subStr("custom");
		QByteArray command = item->toString().toUtf8();
		command.remove(0, subStr.size() + 1);  // remove custom_
		QByteArray payload = QByteArray::fromHex(command);
		stream.writeRawData(payload.data(), payload.size());

		item->setValue(subStr); // setValue "custom"
	}
	else
	{
		return false;  // do nothing
	}

	return true;
}

/*! Configures ubisys switch on ubisys S1, ubisys S2, ubisys J1
 *
 * Using the device setup cluster 0xFC00 on endpoint 0xE8.
 *
 * ubisys S1: you can configure endpont 0x02
 *   either to be used with a push-button (momentary switch, one stable position)
 *   or a rocker switch (two stable positions)
 *   or any custom configuration.
 *
 *   http://www.ubisys.de/downloads/ubisys-s1-technical-reference.pdf
 *   7.7.5.1. InputConfigurations Attribute, Page 28
 *
 * ubisys S2: you can configure endpoints 0x03 and 0x04
 *   either to be used with a push-button (momentary switch, one stable position)
 *   or a rocker switch (two stable positions)
 *   or any custom configuration.
 *
 *   http://www.ubisys.de/downloads/ubisys-s2-technical-reference.pdf
 *   7.7.5.2. InputActions Attribute, Page 30
 *
 * ubisys J1:  you can configure endpoint 0x02
 *   either to be used with two push-buttons (momentary switches, one stable position)
 *   or two rocker switches (two stable positions)
 *   or any custom configuration.
 *
 *   dual push-button operation (momentary, one stable position) default configuration:
 *    - A short press will move up/down and stop when released,
 *    - while a long press will move up/down without stopping
 *
 *   two rocker switches (two stable positions):
 *    - The blind moves as long as either switch is turned on.
 *    - As soon as it is turned off, motion stops.
 *
 *   http://www.ubisys.de/downloads/ubisys-j1-technical-reference.pdf
 *   7.5.5.2. InputActions Attribute, Page 30

   \param task - the task item
   \return true - on success
           false - on error
 */
bool DeRestPluginPrivate::addTaskUbisysConfigureSwitch(TaskItem &task)
{
	int device = 0;

	Sensor *sensor = getSensorNodeForAddressAndEndpoint(task.req.dstAddress(), 0x02);  // Endpoint 0x02

	if (sensor)
	{
		if (sensor->modelId().startsWith(QLatin1String("J1")))
		{
			device = UBISYS_J1;
		}
		else if (sensor->modelId().startsWith(QLatin1String("S1")))
		{
			device = UBISYS_S1;
		}
		else
		{
			return false;
		}

	}

	if (device == 0)
	{
		sensor = getSensorNodeForAddressAndEndpoint(task.req.dstAddress(), 0x03);   // Endpoint 0x03
		if (sensor && sensor->modelId().startsWith(QLatin1String("S2")))
		{
			device = UBISYS_S2;
		}
	}

	if (device == 0)
	{
		return false;
	}

    ResourceItem *item = 0;

    item = sensor->item(RConfigMode);
    if (!item)
    {
        return false;
    }

    task.taskType = TaskWindowCovering;
    task.req.setProfileId(HA_PROFILE_ID);
    task.zclFrame.payload().clear();

    task.req.setClusterId(UBISYS_DEVICE_SETUP_CLUSTER_ID);
    task.req.setDstEndpoint(0xE8);

    task.zclFrame.setSequenceNumber(zclSeq++);
    task.zclFrame.setCommandId(deCONZ::ZclWriteAttributesId);
    task.zclFrame.setFrameControl(deCONZ::ZclFCProfileCommand |
                             deCONZ::ZclFCDirectionClientToServer |
                             deCONZ::ZclFCDisableDefaultResponse);

    bool addPayloadResult = false;

    switch(device)
    {
    case UBISYS_J1:
    	addPayloadResult = addPayloadJ1(task, item);
    	break;

    case UBISYS_S1:
    	addPayloadResult = addPayloadS1(task, item);
    	break;

    case UBISYS_S2:
    	addPayloadResult = addPayloadS2(task, item);
    	break;

    default:
    	return false;
    }

    if (!addPayloadResult)
    {
    	return false;
    }

    { // ZCL frame
        task.req.asdu().clear(); // cleanup old request data if there is any
        QDataStream stream(&task.req.asdu(), QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);
        task.zclFrame.writeToStream(stream);
    }

    if (!addTask(task))
    {
    	return false;
    }

	return true;
}
