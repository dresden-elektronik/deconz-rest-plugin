/*
 * Copyright (c) 2013-2019 dresden elektronik ingenieurtechnik gmbh.
 * All rights reserved.
 *
 * The software in this package is published under the terms of the BSD
 * style license a copy of which has been included with this distribution in
 * the LICENSE.txt file.
 *
 */

#include "sensor.h"
#include "json.h"

static const Sensor::ButtonMap deLightingSwitchMap[] = {
//    mode                          ep    cluster cmd   param button                                       name
    { Sensor::ModeTwoGroups,        0x01, 0x0006, 0x01, 0,    S_BUTTON_1 + S_BUTTON_ACTION_SHORT_RELEASED, "On" },
    { Sensor::ModeTwoGroups,        0x01, 0x0006, 0x00, 0,    S_BUTTON_2 + S_BUTTON_ACTION_SHORT_RELEASED, "Off" },
    { Sensor::ModeTwoGroups,        0x01, 0x0008, 0x05, 0,    S_BUTTON_1 + S_BUTTON_ACTION_HOLD,           "Dimm up" },
    { Sensor::ModeTwoGroups,        0x01, 0x0008, 0x03, 0,    S_BUTTON_1 + S_BUTTON_ACTION_LONG_RELEASED,  "Dimm stop" },
    { Sensor::ModeTwoGroups,        0x01, 0x0008, 0x01, 1,    S_BUTTON_2 + S_BUTTON_ACTION_HOLD,           "Dimm down" },
    { Sensor::ModeTwoGroups,        0x01, 0x0008, 0x03, 1,    S_BUTTON_2 + S_BUTTON_ACTION_LONG_RELEASED,  "Dimm stop" },

    { Sensor::ModeTwoGroups,        0x02, 0x0006, 0x01, 0,    S_BUTTON_3 + S_BUTTON_ACTION_SHORT_RELEASED, "On" },
    { Sensor::ModeTwoGroups,        0x02, 0x0006, 0x00, 0,    S_BUTTON_4 + S_BUTTON_ACTION_SHORT_RELEASED, "Off" },
    { Sensor::ModeTwoGroups,        0x02, 0x0008, 0x05, 0,    S_BUTTON_3 + S_BUTTON_ACTION_HOLD,           "Dimm up" },
    { Sensor::ModeTwoGroups,        0x02, 0x0008, 0x03, 0,    S_BUTTON_3 + S_BUTTON_ACTION_LONG_RELEASED,  "Dimm stop" },
    { Sensor::ModeTwoGroups,        0x02, 0x0008, 0x01, 1,    S_BUTTON_4 + S_BUTTON_ACTION_HOLD,           "Dimm down" },
    { Sensor::ModeTwoGroups,        0x02, 0x0008, 0x03, 1,    S_BUTTON_4 + S_BUTTON_ACTION_LONG_RELEASED,  "Dimm stop" },

    { Sensor::ModeScenes,           0x01, 0x0006, 0x01, 0,    S_BUTTON_1 + S_BUTTON_ACTION_SHORT_RELEASED, "On" },
    { Sensor::ModeScenes,           0x01, 0x0006, 0x00, 0,    S_BUTTON_2 + S_BUTTON_ACTION_SHORT_RELEASED, "Off" },
    { Sensor::ModeScenes,           0x01, 0x0008, 0x05, 0,    S_BUTTON_1 + S_BUTTON_ACTION_HOLD,           "Dimm up" },
    { Sensor::ModeScenes,           0x01, 0x0008, 0x03, 0,    S_BUTTON_1 + S_BUTTON_ACTION_LONG_RELEASED,  "Dimm stop" },
    { Sensor::ModeScenes,           0x01, 0x0008, 0x01, 1,    S_BUTTON_2 + S_BUTTON_ACTION_HOLD,           "Dimm down" },
    { Sensor::ModeScenes,           0x01, 0x0008, 0x03, 1,    S_BUTTON_2 + S_BUTTON_ACTION_LONG_RELEASED,  "Dimm stop" },

    { Sensor::ModeScenes,           0x01, 0x0005, 0x05, 1,    S_BUTTON_3 + S_BUTTON_ACTION_SHORT_RELEASED, "Recall scene 1" },
    { Sensor::ModeScenes,           0x01, 0x0005, 0x05, 4,    S_BUTTON_4 + S_BUTTON_ACTION_SHORT_RELEASED, "Recall scene 4" },

    { Sensor::ModeColorTemperature, 0x01, 0x0006, 0x01, 0,    S_BUTTON_1 + S_BUTTON_ACTION_SHORT_RELEASED, "On" },
    { Sensor::ModeColorTemperature, 0x01, 0x0006, 0x00, 0,    S_BUTTON_2 + S_BUTTON_ACTION_SHORT_RELEASED, "Off" },
    { Sensor::ModeColorTemperature, 0x01, 0x0008, 0x05, 0,    S_BUTTON_1 + S_BUTTON_ACTION_HOLD,           "Dimm up" },
    { Sensor::ModeColorTemperature, 0x01, 0x0008, 0x03, 0,    S_BUTTON_1 + S_BUTTON_ACTION_LONG_RELEASED,  "Dimm stop" },
    { Sensor::ModeColorTemperature, 0x01, 0x0008, 0x01, 1,    S_BUTTON_2 + S_BUTTON_ACTION_HOLD,           "Dimm down" },
    { Sensor::ModeColorTemperature, 0x01, 0x0008, 0x03, 1,    S_BUTTON_2 + S_BUTTON_ACTION_LONG_RELEASED,  "Dimm stop" },

    { Sensor::ModeColorTemperature, 0x01, 0x0300, 0x4b, 0x01FE,    S_BUTTON_3 + S_BUTTON_ACTION_SHORT_RELEASED, "Color temperature move up" },
    { Sensor::ModeColorTemperature, 0x01, 0x0300, 0x4b, 0x03FE,    S_BUTTON_4 + S_BUTTON_ACTION_SHORT_RELEASED, "Color temperature move down" },
    { Sensor::ModeColorTemperature, 0x01, 0x0300, 0x4b, 0x0128,    S_BUTTON_3 + S_BUTTON_ACTION_HOLD,           "Color temperature move up hold" },
    { Sensor::ModeColorTemperature, 0x01, 0x0300, 0x4b, 0x0328,    S_BUTTON_4 + S_BUTTON_ACTION_HOLD,           "Color temperature move down hold" },
    { Sensor::ModeColorTemperature, 0x01, 0x0300, 0x4b, 0x1028,    S_BUTTON_3 + S_BUTTON_ACTION_LONG_RELEASED,  "Color temperature move up stop" },
    { Sensor::ModeColorTemperature, 0x01, 0x0300, 0x4b, 0x3028,    S_BUTTON_4 + S_BUTTON_ACTION_LONG_RELEASED,  "Color temperature move down stop" },

    // end
    { Sensor::ModeNone,             0x00, 0x0000, 0x00, 0,    0,                                           nullptr }
};

static const Sensor::ButtonMap deSceneSwitchMap[] = {
//    mode                          ep    cluster cmd   param button                                       name
    { Sensor::ModeScenes,           0x01, 0x0006, 0x01, 0,    S_BUTTON_1 + S_BUTTON_ACTION_SHORT_RELEASED, "On" },
    { Sensor::ModeScenes,           0x01, 0x0006, 0x00, 0,    S_BUTTON_2 + S_BUTTON_ACTION_SHORT_RELEASED, "Off" },
    { Sensor::ModeScenes,           0x01, 0x0008, 0x05, 0,    S_BUTTON_1 + S_BUTTON_ACTION_HOLD,           "Dimm up" },
    { Sensor::ModeScenes,           0x01, 0x0008, 0x03, 0,    S_BUTTON_1 + S_BUTTON_ACTION_LONG_RELEASED,  "Dimm up stop" },
    { Sensor::ModeScenes,           0x01, 0x0008, 0x01, 1,    S_BUTTON_2 + S_BUTTON_ACTION_HOLD,           "Dimm down" },
    { Sensor::ModeScenes,           0x01, 0x0008, 0x03, 1,    S_BUTTON_2 + S_BUTTON_ACTION_LONG_RELEASED,  "Dimm down stop" },

    { Sensor::ModeScenes,           0x01, 0x0005, 0x05, 1,    S_BUTTON_3 + S_BUTTON_ACTION_SHORT_RELEASED, "Recall scene 1" },
    { Sensor::ModeScenes,           0x01, 0x0005, 0x05, 2,    S_BUTTON_4 + S_BUTTON_ACTION_SHORT_RELEASED, "Recall scene 2" },
    { Sensor::ModeScenes,           0x01, 0x0005, 0x05, 3,    S_BUTTON_5 + S_BUTTON_ACTION_SHORT_RELEASED, "Recall scene 3" },
    { Sensor::ModeScenes,           0x01, 0x0005, 0x05, 4,    S_BUTTON_6 + S_BUTTON_ACTION_SHORT_RELEASED, "Recall scene 4" },

    // end
    { Sensor::ModeNone,             0x00, 0x0000, 0x00, 0,    0,                                           nullptr }
};

static const Sensor::ButtonMap instaRemoteMap[] = {
//    mode                          ep    cluster cmd   param button                                       name
    { Sensor::ModeScenes,           0x01, 0x0006, 0x40, 0,    S_BUTTON_1 + S_BUTTON_ACTION_SHORT_RELEASED, "Off with effect" },
    { Sensor::ModeScenes,           0x01, 0x0006, 0x01, 0,    S_BUTTON_2 + S_BUTTON_ACTION_SHORT_RELEASED, "On" },
    { Sensor::ModeScenes,           0x01, 0x0008, 0x06, 0,    S_BUTTON_2 + S_BUTTON_ACTION_HOLD,           "Dimm up" },
    { Sensor::ModeScenes,           0x01, 0x0008, 0x03, 0,    S_BUTTON_2 + S_BUTTON_ACTION_LONG_RELEASED,  "Dimm up stop" },
    { Sensor::ModeScenes,           0x01, 0x0008, 0x02, 1,    S_BUTTON_1 + S_BUTTON_ACTION_HOLD,           "Dimm down" },
    { Sensor::ModeScenes,           0x01, 0x0008, 0x03, 1,    S_BUTTON_1 + S_BUTTON_ACTION_LONG_RELEASED,  "Dimm down stop" },

    { Sensor::ModeScenes,           0x01, 0x0005, 0x05, 0,    S_BUTTON_3 + S_BUTTON_ACTION_SHORT_RELEASED, "Recall scene 0" },
    { Sensor::ModeScenes,           0x01, 0x0005, 0x05, 1,    S_BUTTON_4 + S_BUTTON_ACTION_SHORT_RELEASED, "Recall scene 1" },
    { Sensor::ModeScenes,           0x01, 0x0005, 0x05, 2,    S_BUTTON_5 + S_BUTTON_ACTION_SHORT_RELEASED, "Recall scene 2" },
    { Sensor::ModeScenes,           0x01, 0x0005, 0x05, 3,    S_BUTTON_6 + S_BUTTON_ACTION_SHORT_RELEASED, "Recall scene 3" },
    { Sensor::ModeScenes,           0x01, 0x0005, 0x05, 4,    S_BUTTON_7 + S_BUTTON_ACTION_SHORT_RELEASED, "Recall scene 4" },
    { Sensor::ModeScenes,           0x01, 0x0005, 0x05, 5,    S_BUTTON_8 + S_BUTTON_ACTION_SHORT_RELEASED, "Recall scene 5" },

    // end
    { Sensor::ModeNone,             0x00, 0x0000, 0x00, 0,    0,                                           nullptr }
};

static const Sensor::ButtonMap philipsDimmerSwitchMap[] = {
//    mode                          ep    cluster cmd   param button                                       name
/*  { Sensor::ModeScenes,           0x01, 0x0006, 0x01, 0,    S_BUTTON_1 + S_BUTTON_ACTION_SHORT_RELEASED, "On" },
    { Sensor::ModeScenes,           0x01, 0x0006, 0x40, 0,    S_BUTTON_4 + S_BUTTON_ACTION_SHORT_RELEASED, "Off with effect" },

    { Sensor::ModeScenes,           0x01, 0x0008, 0x02, 0,    S_BUTTON_2 + S_BUTTON_ACTION_HOLD,           "Step up" },
    { Sensor::ModeScenes,           0x01, 0x0008, 0x03, 0,    S_BUTTON_2 + S_BUTTON_ACTION_LONG_RELEASED,  "Dimm stop" }, // might be button 2 as well
    { Sensor::ModeScenes,           0x01, 0x0008, 0x02, 1,    S_BUTTON_3 + S_BUTTON_ACTION_HOLD,           "Step down" },
*/
//  vendor specific
    // top button
    { Sensor::ModeScenes,           0x02, 0xfc00, 0x00, 0x10,  S_BUTTON_1 + S_BUTTON_ACTION_INITIAL_PRESS,  "initial press" },
    { Sensor::ModeScenes,           0x02, 0xfc00, 0x00, 0x11,  S_BUTTON_1 + S_BUTTON_ACTION_HOLD,           "hold" },
    { Sensor::ModeScenes,           0x02, 0xfc00, 0x00, 0x12,  S_BUTTON_1 + S_BUTTON_ACTION_SHORT_RELEASED, "short release" },
    { Sensor::ModeScenes,           0x02, 0xfc00, 0x00, 0x13,  S_BUTTON_1 + S_BUTTON_ACTION_LONG_RELEASED,  "long release" },

    // second button
    { Sensor::ModeScenes,           0x02, 0xfc00, 0x00, 0x20,  S_BUTTON_2 + S_BUTTON_ACTION_INITIAL_PRESS,  "initial press" },
    { Sensor::ModeScenes,           0x02, 0xfc00, 0x00, 0x21,  S_BUTTON_2 + S_BUTTON_ACTION_HOLD,           "hold" },
    { Sensor::ModeScenes,           0x02, 0xfc00, 0x00, 0x22,  S_BUTTON_2 + S_BUTTON_ACTION_SHORT_RELEASED, "short release" },
    { Sensor::ModeScenes,           0x02, 0xfc00, 0x00, 0x23,  S_BUTTON_2 + S_BUTTON_ACTION_LONG_RELEASED,  "long release" },

    // third button
    { Sensor::ModeScenes,           0x02, 0xfc00, 0x00, 0x30,  S_BUTTON_3 + S_BUTTON_ACTION_INITIAL_PRESS,  "initial press" },
    { Sensor::ModeScenes,           0x02, 0xfc00, 0x00, 0x31,  S_BUTTON_3 + S_BUTTON_ACTION_HOLD,           "hold" },
    { Sensor::ModeScenes,           0x02, 0xfc00, 0x00, 0x32,  S_BUTTON_3 + S_BUTTON_ACTION_SHORT_RELEASED, "short release" },
    { Sensor::ModeScenes,           0x02, 0xfc00, 0x00, 0x33,  S_BUTTON_3 + S_BUTTON_ACTION_LONG_RELEASED,  "long release" },

    // fourth button
    { Sensor::ModeScenes,           0x02, 0xfc00, 0x00, 0x40,  S_BUTTON_4 + S_BUTTON_ACTION_INITIAL_PRESS,  "initial press" },
    { Sensor::ModeScenes,           0x02, 0xfc00, 0x00, 0x41,  S_BUTTON_4 + S_BUTTON_ACTION_HOLD,           "hold" },
    { Sensor::ModeScenes,           0x02, 0xfc00, 0x00, 0x42,  S_BUTTON_4 + S_BUTTON_ACTION_SHORT_RELEASED, "short release" },
    { Sensor::ModeScenes,           0x02, 0xfc00, 0x00, 0x43,  S_BUTTON_4 + S_BUTTON_ACTION_LONG_RELEASED,  "long release" },

    // end
    { Sensor::ModeNone,             0x00, 0x0000, 0x00, 0,    0,                                           nullptr }
};

static const Sensor::ButtonMap ikeaOnOffMap[] = {
//    mode                          ep    cluster cmd   param button                                       name
    { Sensor::ModeScenes,           0x01, 0x0006, 0x01, 0,    S_BUTTON_1 + S_BUTTON_ACTION_SHORT_RELEASED, "On" },
    { Sensor::ModeScenes,           0x01, 0x0006, 0x00, 0,    S_BUTTON_2 + S_BUTTON_ACTION_SHORT_RELEASED, "Off" },
    { Sensor::ModeScenes,           0x01, 0x0008, 0x05, 0,    S_BUTTON_1 + S_BUTTON_ACTION_HOLD, "Move up (with on/off)" },
    { Sensor::ModeScenes,           0x01, 0x0008, 0x01, 1,    S_BUTTON_2 + S_BUTTON_ACTION_HOLD, "Move down" },
    { Sensor::ModeScenes,           0x01, 0x0008, 0x07, 0,    S_BUTTON_1 + S_BUTTON_ACTION_LONG_RELEASED,  "Stop (with on/off)" },
    { Sensor::ModeScenes,           0x01, 0x0008, 0x07, 1,    S_BUTTON_2 + S_BUTTON_ACTION_LONG_RELEASED,  "Stop" },
    // end
    { Sensor::ModeNone,             0x00, 0x0000, 0x00, 0,    0,                                           nullptr }
};

static const Sensor::ButtonMap ikeaOpenCloseMap[] = {
//    mode                          ep    cluster cmd   param button                                       name
    { Sensor::ModeScenes,           0x01, 0x0102, 0x00, 0,    S_BUTTON_1 + S_BUTTON_ACTION_SHORT_RELEASED, "Open" },
    { Sensor::ModeScenes,           0x01, 0x0102, 0x02, 0,    S_BUTTON_1 + S_BUTTON_ACTION_LONG_RELEASED,  "Stop" },
    { Sensor::ModeScenes,           0x01, 0x0102, 0x01, 0,    S_BUTTON_2 + S_BUTTON_ACTION_SHORT_RELEASED, "Close" },
    { Sensor::ModeScenes,           0x01, 0x0102, 0x02, 1,    S_BUTTON_2 + S_BUTTON_ACTION_LONG_RELEASED,  "Stop" },
    // end
    { Sensor::ModeNone,             0x00, 0x0000, 0x00, 0,    0,                                           nullptr }
};

static const Sensor::ButtonMap ikeaRemoteMap[] = {
//    mode                          ep    cluster cmd   param button                                       name
    // big button
    { Sensor::ModeColorTemperature, 0x01, 0x0006, 0x02, 0,    S_BUTTON_1 + S_BUTTON_ACTION_SHORT_RELEASED, "Toggle" },
    { Sensor::ModeColorTemperature, 0x01, 0x0005, 0x07, 2,    S_BUTTON_1 + S_BUTTON_ACTION_HOLD,           "Setup 10s" },
    // top button
    { Sensor::ModeColorTemperature, 0x01, 0x0008, 0x06, 0,    S_BUTTON_2 + S_BUTTON_ACTION_SHORT_RELEASED, "Step up (with on/off)" },
    { Sensor::ModeColorTemperature, 0x01, 0x0008, 0x05, 0,    S_BUTTON_2 + S_BUTTON_ACTION_HOLD,           "Move up (with on/off)" },
    { Sensor::ModeColorTemperature, 0x01, 0x0008, 0x07, 0,    S_BUTTON_2 + S_BUTTON_ACTION_LONG_RELEASED,  "Stop_ (with on/off)" },
    // bottom button
    { Sensor::ModeColorTemperature, 0x01, 0x0008, 0x02, 1,    S_BUTTON_3 + S_BUTTON_ACTION_SHORT_RELEASED, "Step down" },
    { Sensor::ModeColorTemperature, 0x01, 0x0008, 0x01, 1,    S_BUTTON_3 + S_BUTTON_ACTION_HOLD,           "Move down" },
    { Sensor::ModeColorTemperature, 0x01, 0x0008, 0x03, 1,    S_BUTTON_3 + S_BUTTON_ACTION_LONG_RELEASED,  "Stop" },
    // left button (non-standard)
    { Sensor::ModeColorTemperature, 0x01, 0x0005, 0x07, 1,    S_BUTTON_4 + S_BUTTON_ACTION_SHORT_RELEASED, "Step ct colder" },
    { Sensor::ModeColorTemperature, 0x01, 0x0005, 0x08, 1,    S_BUTTON_4 + S_BUTTON_ACTION_HOLD,           "Move ct colder" },
    { Sensor::ModeColorTemperature, 0x01, 0x0005, 0x09, 1,    S_BUTTON_4 + S_BUTTON_ACTION_LONG_RELEASED,  "Stop ct colder" },
    // right button (non-standard)
    { Sensor::ModeColorTemperature, 0x01, 0x0005, 0x07, 0,    S_BUTTON_5 + S_BUTTON_ACTION_SHORT_RELEASED, "Step ct warmer" },
    { Sensor::ModeColorTemperature, 0x01, 0x0005, 0x08, 0,    S_BUTTON_5 + S_BUTTON_ACTION_HOLD,           "Move ct warmer" },
    { Sensor::ModeColorTemperature, 0x01, 0x0005, 0x09, 0,    S_BUTTON_5 + S_BUTTON_ACTION_LONG_RELEASED,  "Stop ct warmer" },
    // end
    { Sensor::ModeNone,             0x00, 0x0000, 0x00, 0,    0,                                           nullptr }
};

static const Sensor::ButtonMap ikeaDimmerMap[] = {
//    mode                ep    cluster cmd   param button                                       name
    // on
    { Sensor::ModeDimmer, 0x01, 0x0008, 0x04, 255,  S_BUTTON_1 + S_BUTTON_ACTION_SHORT_RELEASED, "Move to level 255 (with on/off)" },
    // dim up
    { Sensor::ModeDimmer, 0x01, 0x0008, 0x05, 0,    S_BUTTON_2 + S_BUTTON_ACTION_SHORT_RELEASED, "Move up (with on/off)" },
    // { Sensor::ModeDimmer, 0x01, 0x0008, 0x05, 0,    S_BUTTON_2 + S_BUTTON_ACTION_HOLD,           "Move up (with on/off)" },
    // { Sensor::ModeDimmer, 0x01, 0x0008, 0x07, 0,    S_BUTTON_2 + S_BUTTON_ACTION_LONG_RELEASED,  "Stop_ up (with on/off)" },
    // dim down
    { Sensor::ModeDimmer, 0x01, 0x0008, 0x01, 1,    S_BUTTON_3 + S_BUTTON_ACTION_SHORT_RELEASED, "Move down" },
    // { Sensor::ModeDimmer, 0x01, 0x0008, 0x01, 1,    S_BUTTON_3 + S_BUTTON_ACTION_HOLD,           "Move down" },
    // { Sensor::ModeDimmer, 0x01, 0x0008, 0x07, 1,    S_BUTTON_3 + S_BUTTON_ACTION_LONG_RELEASED,  "Stop_ down (with on/off)" },
    // off
    { Sensor::ModeDimmer, 0x01, 0x0008, 0x04, 0,    S_BUTTON_4 + S_BUTTON_ACTION_SHORT_RELEASED, "Move to level 0 (with on/off)" },
    // end
    { Sensor::ModeNone,             0x00, 0x0000, 0x00, 0,    0,                                           nullptr }
};


static const Sensor::ButtonMap ikeaMotionSensorMap[] = {
//    mode                          ep    cluster cmd   param button                                       name
// presence event
    { Sensor::ModeScenes,           0x01, 0x0006, 0x42, 0,    S_BUTTON_1 + S_BUTTON_ACTION_SHORT_RELEASED, "On with timed off" },

    // end
    { Sensor::ModeNone,             0x00, 0x0000, 0x00, 0,    0,                                           nullptr }
};

static const Sensor::ButtonMap ikeaSoundControllerMap[] = {
//    mode                          ep    cluster cmd   param button                                       name
    // press
    { Sensor::ModeScenes,           0x01, 0x0006, 0x02, 0,    S_BUTTON_1 + S_BUTTON_ACTION_SHORT_RELEASED, "Toggle" },
    { Sensor::ModeScenes,           0x01, 0x0008, 0x02, 0,    S_BUTTON_1 + S_BUTTON_ACTION_DOUBLE_PRESS, "Step Up" },
    { Sensor::ModeScenes,           0x01, 0x0008, 0x02, 1,    S_BUTTON_1 + S_BUTTON_ACTION_TREBLE_PRESS, "Step Down" },
    // turn counter clockwise
    { Sensor::ModeScenes,           0x01, 0x0008, 0x01, 1,    S_BUTTON_2 + S_BUTTON_ACTION_HOLD, "Move Down" },
    { Sensor::ModeScenes,           0x01, 0x0008, 0x03, 1,    S_BUTTON_2 + S_BUTTON_ACTION_LONG_RELEASED, "Stop" },
    // turn clockwise
    { Sensor::ModeScenes,           0x01, 0x0008, 0x01, 0,    S_BUTTON_3 + S_BUTTON_ACTION_HOLD, "Move Up" },
    { Sensor::ModeScenes,           0x01, 0x0008, 0x03, 0,    S_BUTTON_3 + S_BUTTON_ACTION_LONG_RELEASED, "Stop" },
    // end
    { Sensor::ModeNone,             0x00, 0x0000, 0x00, 0,    0,                                           nullptr }
};

static const Sensor::ButtonMap trustZYCT202SwitchMap[] = {
//    mode                          ep    cluster cmd   param button                                       name
    { Sensor::ModeScenes,           0x01, 0x0006, 0x01, 0,    S_BUTTON_1 + S_BUTTON_ACTION_SHORT_RELEASED, "On" },
    { Sensor::ModeScenes,           0x01, 0x0008, 0x01, 0,    S_BUTTON_2 + S_BUTTON_ACTION_HOLD,           "Dimm up" },
    { Sensor::ModeScenes,           0x01, 0x0008, 0x03, 0,    S_BUTTON_2 + S_BUTTON_ACTION_LONG_RELEASED,  "Dimm up stop" },
    { Sensor::ModeScenes,           0x01, 0x0008, 0x01, 1,    S_BUTTON_3 + S_BUTTON_ACTION_HOLD,           "Dimm down" },
    { Sensor::ModeScenes,           0x01, 0x0008, 0x03, 1,    S_BUTTON_3 + S_BUTTON_ACTION_LONG_RELEASED,  "Dimm down stop" },
    { Sensor::ModeScenes,           0x01, 0x0006, 0x40, 0,    S_BUTTON_4 + S_BUTTON_ACTION_SHORT_RELEASED, "Off" },

    // end
    { Sensor::ModeNone,             0x00, 0x0000, 0x00, 0,    0,                                           nullptr }
};


static const Sensor::ButtonMap bjeSwitchMap[] = {
//    mode                          ep    cluster cmd   param button                                       name
//  1) row left button
    { Sensor::ModeScenes,           0x0A, 0x0006, 0x00, 0,    S_BUTTON_1 + S_BUTTON_ACTION_SHORT_RELEASED, "Off" },
    { Sensor::ModeScenes,           0x0A, 0x0008, 0x02, 1,    S_BUTTON_1 + S_BUTTON_ACTION_HOLD, "Step down" },
    { Sensor::ModeScenes,           0x0A, 0x0008, 0x03, 1,    S_BUTTON_1 + S_BUTTON_ACTION_LONG_RELEASED, "Stop" },
//  1) row right button
    { Sensor::ModeScenes,           0x0A, 0x0006, 0x01, 0,    S_BUTTON_2 + S_BUTTON_ACTION_SHORT_RELEASED, "On" },
    { Sensor::ModeScenes,           0x0A, 0x0008, 0x06, 0,    S_BUTTON_2 + S_BUTTON_ACTION_HOLD, "Step up (with on/off)" },
    { Sensor::ModeScenes,           0x0A, 0x0008, 0x03, 0,    S_BUTTON_2 + S_BUTTON_ACTION_LONG_RELEASED, "Stop" },
//  2) row left button
    { Sensor::ModeScenes,           0x0B, 0x0006, 0x00, 0,    S_BUTTON_3 + S_BUTTON_ACTION_SHORT_RELEASED, "Off" },
    { Sensor::ModeScenes,           0x0B, 0x0008, 0x02, 1,    S_BUTTON_3 + S_BUTTON_ACTION_HOLD, "Step down" },
    { Sensor::ModeScenes,           0x0B, 0x0008, 0x03, 1,    S_BUTTON_3 + S_BUTTON_ACTION_LONG_RELEASED, "Stop" },
    { Sensor::ModeScenes,           0x0B, 0x0005, 0x05, 3,    S_BUTTON_3 + S_BUTTON_ACTION_SHORT_RELEASED, "Recall scene 3" },
//  2) row right button
    { Sensor::ModeScenes,           0x0B, 0x0006, 0x01, 0,    S_BUTTON_4 + S_BUTTON_ACTION_SHORT_RELEASED, "On" },
    { Sensor::ModeScenes,           0x0B, 0x0008, 0x06, 0,    S_BUTTON_4 + S_BUTTON_ACTION_HOLD, "Step up (with on/off)" },
    { Sensor::ModeScenes,           0x0B, 0x0008, 0x03, 0,    S_BUTTON_4 + S_BUTTON_ACTION_LONG_RELEASED, "Stop" },
    { Sensor::ModeScenes,           0x0B, 0x0005, 0x05, 4,    S_BUTTON_4 + S_BUTTON_ACTION_SHORT_RELEASED, "Recall scene 4" },
//  3) row left button
    { Sensor::ModeScenes,           0x0C, 0x0006, 0x00, 0,    S_BUTTON_5 + S_BUTTON_ACTION_SHORT_RELEASED, "Off" },
    { Sensor::ModeScenes,           0x0C, 0x0008, 0x02, 1,    S_BUTTON_5 + S_BUTTON_ACTION_HOLD, "Step down" },
    { Sensor::ModeScenes,           0x0C, 0x0008, 0x03, 1,    S_BUTTON_5 + S_BUTTON_ACTION_LONG_RELEASED, "Stop" },
    { Sensor::ModeScenes,           0x0C, 0x0005, 0x05, 5,    S_BUTTON_5 + S_BUTTON_ACTION_SHORT_RELEASED, "Recall scene 5" },
//  3) row right button
    { Sensor::ModeScenes,           0x0C, 0x0006, 0x01, 0,    S_BUTTON_6 + S_BUTTON_ACTION_SHORT_RELEASED, "On" },
    { Sensor::ModeScenes,           0x0C, 0x0008, 0x06, 0,    S_BUTTON_6 + S_BUTTON_ACTION_HOLD, "Step up (with on/off)" },
    { Sensor::ModeScenes,           0x0C, 0x0008, 0x03, 0,    S_BUTTON_6 + S_BUTTON_ACTION_LONG_RELEASED, "Stop" },
    { Sensor::ModeScenes,           0x0C, 0x0005, 0x05, 6,    S_BUTTON_6 + S_BUTTON_ACTION_SHORT_RELEASED, "Recall scene 6" },
//  4) row left button
    { Sensor::ModeScenes,           0x0D, 0x0006, 0x00, 0,    S_BUTTON_7 + S_BUTTON_ACTION_SHORT_RELEASED, "Off" },
    { Sensor::ModeScenes,           0x0D, 0x0008, 0x02, 1,    S_BUTTON_7 + S_BUTTON_ACTION_HOLD, "Step down" },
    { Sensor::ModeScenes,           0x0D, 0x0008, 0x03, 1,    S_BUTTON_7 + S_BUTTON_ACTION_LONG_RELEASED, "Stop" },
    { Sensor::ModeScenes,           0x0D, 0x0005, 0x05, 7,    S_BUTTON_7 + S_BUTTON_ACTION_SHORT_RELEASED, "Recall scene 7" },
//  4) row right button
    { Sensor::ModeScenes,           0x0D, 0x0006, 0x01, 0,    S_BUTTON_8 + S_BUTTON_ACTION_SHORT_RELEASED, "On" },
    { Sensor::ModeScenes,           0x0D, 0x0008, 0x06, 0,    S_BUTTON_8 + S_BUTTON_ACTION_HOLD, "Step up (with on/off)" },
    { Sensor::ModeScenes,           0x0D, 0x0008, 0x03, 0,    S_BUTTON_8 + S_BUTTON_ACTION_LONG_RELEASED, "Stop" },
    { Sensor::ModeScenes,           0x0D, 0x0005, 0x05, 8,    S_BUTTON_8 + S_BUTTON_ACTION_SHORT_RELEASED, "Recall scene 8" },

/////////////////////////////////////////////////////////
//  1) row left button
    { Sensor::ModeDimmer,           0x0A, 0x0006, 0x00, 0,    S_BUTTON_1 + S_BUTTON_ACTION_SHORT_RELEASED, "Off" },
    { Sensor::ModeDimmer,           0x0A, 0x0008, 0x02, 1,    S_BUTTON_1 + S_BUTTON_ACTION_HOLD, "Step down" },
    { Sensor::ModeDimmer,           0x0A, 0x0008, 0x03, 0,    S_BUTTON_1 + S_BUTTON_ACTION_LONG_RELEASED, "Stop" },
//  1) row right button
    { Sensor::ModeDimmer,           0x0A, 0x0006, 0x01, 0,    S_BUTTON_2 + S_BUTTON_ACTION_SHORT_RELEASED, "On" },
    { Sensor::ModeDimmer,           0x0A, 0x0008, 0x06, 0,    S_BUTTON_2 + S_BUTTON_ACTION_HOLD, "Step up (with on/off)" },
//  2) row left button
    { Sensor::ModeDimmer,           0x0B, 0x0006, 0x00, 0,    S_BUTTON_3 + S_BUTTON_ACTION_SHORT_RELEASED, "Off" },
    { Sensor::ModeDimmer,           0x0B, 0x0008, 0x02, 1,    S_BUTTON_3 + S_BUTTON_ACTION_HOLD, "Step down" },
    { Sensor::ModeDimmer,           0x0B, 0x0008, 0x03, 1,    S_BUTTON_3 + S_BUTTON_ACTION_LONG_RELEASED, "Stop" },
//  2) row right button
    { Sensor::ModeDimmer,           0x0B, 0x0006, 0x01, 0,    S_BUTTON_4 + S_BUTTON_ACTION_SHORT_RELEASED, "On" },
    { Sensor::ModeDimmer,           0x0B, 0x0008, 0x06, 0,    S_BUTTON_4 + S_BUTTON_ACTION_HOLD, "Step up (with on/off)" },
    { Sensor::ModeDimmer,           0x0B, 0x0008, 0x03, 0,    S_BUTTON_4 + S_BUTTON_ACTION_LONG_RELEASED, "Stop" },

    // end
    { Sensor::ModeNone,             0x00, 0x0000, 0x00, 0,    0,                                           nullptr }
};

static const Sensor::ButtonMap xiaomiSwitchMap[] = {
//    mode                          ep    cluster cmd   param button                                       name
    // First button
    { Sensor::ModeScenes,           0x01, 0x0006, 0x0a, 0,    S_BUTTON_1 + S_BUTTON_ACTION_INITIAL_PRESS, "Normal press" },
    { Sensor::ModeScenes,           0x01, 0x0006, 0x0a, 1,    S_BUTTON_1 + S_BUTTON_ACTION_SHORT_RELEASED, "Normal release" },
    { Sensor::ModeScenes,           0x01, 0x0006, 0x0a, 1,    S_BUTTON_1 + S_BUTTON_ACTION_LONG_RELEASED, "Long release" },
    { Sensor::ModeScenes,           0x01, 0x0006, 0x0a, 2,    S_BUTTON_1 + S_BUTTON_ACTION_DOUBLE_PRESS, "Double press" },
    { Sensor::ModeScenes,           0x01, 0x0006, 0x0a, 3,    S_BUTTON_1 + S_BUTTON_ACTION_TREBLE_PRESS, "Triple press" },
    { Sensor::ModeScenes,           0x01, 0x0006, 0x0a, 4,    S_BUTTON_1 + S_BUTTON_ACTION_QUADRUPLE_PRESS, "Quad press" },
    { Sensor::ModeScenes,           0x01, 0x0006, 0x0a, 0x80, S_BUTTON_1 + S_BUTTON_ACTION_MANY_PRESS, "Many press" },

    // end
    { Sensor::ModeNone,             0x00, 0x0000, 0x00, 0,    0,                                           nullptr }
};

static const Sensor::ButtonMap xiaomiSwitchAq2Map[] = {
//    mode                          ep    cluster cmd   param button                                       name
    // First button
    { Sensor::ModeScenes,           0x01, 0x0006, 0x0a, 0,    S_BUTTON_1 + S_BUTTON_ACTION_SHORT_RELEASED, "Normal press" },
    { Sensor::ModeScenes,           0x01, 0x0006, 0x0a, 2,    S_BUTTON_1 + S_BUTTON_ACTION_DOUBLE_PRESS, "Double press" },
    { Sensor::ModeScenes,           0x01, 0x0006, 0x0a, 3,    S_BUTTON_1 + S_BUTTON_ACTION_TREBLE_PRESS, "Triple press" },
    { Sensor::ModeScenes,           0x01, 0x0006, 0x0a, 4,    S_BUTTON_1 + S_BUTTON_ACTION_QUADRUPLE_PRESS, "Quad press" },

    // end
    { Sensor::ModeNone,             0x00, 0x0000, 0x00, 0,    0,                                           nullptr }
};

static const Sensor::ButtonMap xiaomiVibrationMap[] = {
//    mode                          ep    cluster cmd   param button                                       name
    // First button
    { Sensor::ModeScenes,           0x01, 0x0101, 0x0a, 1,    S_BUTTON_1 + S_BUTTON_ACTION_SHAKE, "Shake" },
    { Sensor::ModeScenes,           0x01, 0x0101, 0x0a, 2,    S_BUTTON_1 + S_BUTTON_ACTION_TILT, "Tilt" },
    { Sensor::ModeScenes,           0x01, 0x0101, 0x0a, 3,    S_BUTTON_1 + S_BUTTON_ACTION_DROP, "Drop" },

    // end
    { Sensor::ModeNone,             0x00, 0x0000, 0x00, 0,    0,                                           nullptr }
};

static const Sensor::ButtonMap ubisysD1Map[] = {
//    mode                          ep    cluster cmd   param button                                       name
    // First button
    { Sensor::ModeScenes,           0x02, 0x0006, 0x02, 0,    S_BUTTON_1 + S_BUTTON_ACTION_SHORT_RELEASED, "Toggle" },
    { Sensor::ModeScenes,           0x02, 0x0008, 0x05, 0,    S_BUTTON_1 + S_BUTTON_ACTION_HOLD, "Move up (with on/off)" },
    { Sensor::ModeScenes,           0x02, 0x0008, 0x07, 0,    S_BUTTON_1 + S_BUTTON_ACTION_LONG_RELEASED, "Stop_ (with on/off)" },
    { Sensor::ModeScenes,           0x02, 0x0008, 0x05, 1,    S_BUTTON_1 + S_BUTTON_ACTION_HOLD, "Move down (with on/off)" },
    { Sensor::ModeScenes,           0x02, 0x0008, 0x07, 1,    S_BUTTON_1 + S_BUTTON_ACTION_LONG_RELEASED, "Stop_ (with on/off)" },
    // Second button
    { Sensor::ModeScenes,           0x03, 0x0006, 0x02, 0,    S_BUTTON_2 + S_BUTTON_ACTION_SHORT_RELEASED, "Toggle" },
    { Sensor::ModeScenes,           0x03, 0x0008, 0x05, 0,    S_BUTTON_2 + S_BUTTON_ACTION_HOLD, "Move up (with on/off)" },
    { Sensor::ModeScenes,           0x03, 0x0008, 0x07, 0,    S_BUTTON_2 + S_BUTTON_ACTION_LONG_RELEASED, "Stop_ (with on/off)" },
    { Sensor::ModeScenes,           0x03, 0x0008, 0x05, 1,    S_BUTTON_2 + S_BUTTON_ACTION_HOLD, "Move down (with on/off)" },
    { Sensor::ModeScenes,           0x03, 0x0008, 0x07, 1,    S_BUTTON_2 + S_BUTTON_ACTION_LONG_RELEASED, "Stop_ (with on/off)" },
    // end
    { Sensor::ModeNone,             0x00, 0x0000, 0x00, 0,    0,                                           nullptr }
};

static const Sensor::ButtonMap ubisysC4Map[] = {
//    mode                          ep    cluster cmd   param button                                       name
    // First button
    { Sensor::ModeScenes,           0x01, 0x0006, 0x02, 0,    S_BUTTON_1 + S_BUTTON_ACTION_SHORT_RELEASED, "Toggle" },
    { Sensor::ModeScenes,           0x01, 0x0008, 0x05, 0,    S_BUTTON_1 + S_BUTTON_ACTION_HOLD, "Move up (with on/off)" },
    { Sensor::ModeScenes,           0x01, 0x0008, 0x07, 0,    S_BUTTON_1 + S_BUTTON_ACTION_LONG_RELEASED, "Stop_ (with on/off)" },
    { Sensor::ModeScenes,           0x01, 0x0008, 0x05, 1,    S_BUTTON_1 + S_BUTTON_ACTION_HOLD, "Move down (with on/off)" },
    { Sensor::ModeScenes,           0x01, 0x0008, 0x07, 1,    S_BUTTON_1 + S_BUTTON_ACTION_LONG_RELEASED, "Stop_ (with on/off)" },
    // Second button
    { Sensor::ModeScenes,           0x02, 0x0006, 0x02, 0,    S_BUTTON_2 + S_BUTTON_ACTION_SHORT_RELEASED, "Toggle" },
    { Sensor::ModeScenes,           0x02, 0x0008, 0x05, 0,    S_BUTTON_2 + S_BUTTON_ACTION_HOLD, "Move up (with on/off)" },
    { Sensor::ModeScenes,           0x02, 0x0008, 0x07, 0,    S_BUTTON_2 + S_BUTTON_ACTION_LONG_RELEASED, "Stop_ (with on/off)" },
    { Sensor::ModeScenes,           0x02, 0x0008, 0x05, 1,    S_BUTTON_2 + S_BUTTON_ACTION_HOLD, "Move down (with on/off)" },
    { Sensor::ModeScenes,           0x02, 0x0008, 0x07, 1,    S_BUTTON_2 + S_BUTTON_ACTION_LONG_RELEASED, "Stop_ (with on/off)" },
    // Third button
    { Sensor::ModeScenes,           0x03, 0x0006, 0x02, 0,    S_BUTTON_3 + S_BUTTON_ACTION_SHORT_RELEASED, "Toggle" },
    { Sensor::ModeScenes,           0x03, 0x0008, 0x05, 0,    S_BUTTON_3 + S_BUTTON_ACTION_HOLD, "Move up (with on/off)" },
    { Sensor::ModeScenes,           0x03, 0x0008, 0x07, 0,    S_BUTTON_3 + S_BUTTON_ACTION_LONG_RELEASED, "Stop_ (with on/off)" },
    { Sensor::ModeScenes,           0x03, 0x0008, 0x05, 1,    S_BUTTON_3 + S_BUTTON_ACTION_HOLD, "Move down (with on/off)" },
    { Sensor::ModeScenes,           0x03, 0x0008, 0x07, 1,    S_BUTTON_3 + S_BUTTON_ACTION_LONG_RELEASED, "Stop_ (with on/off)" },
    // Fourth button
    { Sensor::ModeScenes,           0x04, 0x0006, 0x02, 0,    S_BUTTON_4 + S_BUTTON_ACTION_SHORT_RELEASED, "Toggle" },
    { Sensor::ModeScenes,           0x04, 0x0008, 0x05, 0,    S_BUTTON_4 + S_BUTTON_ACTION_HOLD, "Move up (with on/off)" },
    { Sensor::ModeScenes,           0x04, 0x0008, 0x07, 0,    S_BUTTON_4 + S_BUTTON_ACTION_LONG_RELEASED, "Stop_ (with on/off)" },
    { Sensor::ModeScenes,           0x04, 0x0008, 0x05, 1,    S_BUTTON_4 + S_BUTTON_ACTION_HOLD, "Move down (with on/off)" },
    { Sensor::ModeScenes,           0x04, 0x0008, 0x07, 1,    S_BUTTON_4 + S_BUTTON_ACTION_LONG_RELEASED, "Stop_ (with on/off)" },
    // end
    { Sensor::ModeNone,             0x00, 0x0000, 0x00, 0,    0,                                           nullptr }
};

static const Sensor::ButtonMap ubisysS2Map[] = {
//    mode                          ep    cluster cmd   param button                                       name
    // First button
    { Sensor::ModeScenes,           0x03, 0x0006, 0x02, 0,    S_BUTTON_1 + S_BUTTON_ACTION_SHORT_RELEASED, "Toggle" },
    { Sensor::ModeScenes,           0x03, 0x0008, 0x05, 0,    S_BUTTON_1 + S_BUTTON_ACTION_HOLD, "Move up (with on/off)" },
    { Sensor::ModeScenes,           0x03, 0x0008, 0x07, 0,    S_BUTTON_1 + S_BUTTON_ACTION_LONG_RELEASED, "Stop_ (with on/off)" },
    { Sensor::ModeScenes,           0x03, 0x0008, 0x05, 1,    S_BUTTON_1 + S_BUTTON_ACTION_HOLD, "Move down (with on/off)" },
    { Sensor::ModeScenes,           0x03, 0x0008, 0x07, 1,    S_BUTTON_1 + S_BUTTON_ACTION_LONG_RELEASED, "Stop_ (with on/off)" },
    // Second button
    { Sensor::ModeScenes,           0x04, 0x0006, 0x02, 0,    S_BUTTON_2 + S_BUTTON_ACTION_SHORT_RELEASED, "Toggle" },
    { Sensor::ModeScenes,           0x04, 0x0008, 0x05, 0,    S_BUTTON_2 + S_BUTTON_ACTION_HOLD, "Move up (with on/off)" },
    { Sensor::ModeScenes,           0x04, 0x0008, 0x07, 0,    S_BUTTON_2 + S_BUTTON_ACTION_LONG_RELEASED, "Stop_ (with on/off)" },
    { Sensor::ModeScenes,           0x04, 0x0008, 0x05, 1,    S_BUTTON_2 + S_BUTTON_ACTION_HOLD, "Move down (with on/off)" },
    { Sensor::ModeScenes,           0x04, 0x0008, 0x07, 1,    S_BUTTON_2 + S_BUTTON_ACTION_LONG_RELEASED, "Stop_ (with on/off)" },
    // end
    { Sensor::ModeNone,             0x00, 0x0000, 0x00, 0,    0,                                           nullptr }
};

static const Sensor::ButtonMap lutronLZL4BWHLSwitchMap[] = {
//    mode                          ep    cluster cmd   param button                                       name
//  vendor specific
    // top button
    { Sensor::ModeDimmer,           0x01, 0x0008, 0x04, 0xfe,  S_BUTTON_1 + S_BUTTON_ACTION_SHORT_RELEASED, "on" },
    // second button
    { Sensor::ModeDimmer,           0x01, 0x0008, 0x06, 0x00,  S_BUTTON_2 + S_BUTTON_ACTION_HOLD,           "dimm up" },
    { Sensor::ModeDimmer,           0x01, 0x0008, 0x03, 0x00,  S_BUTTON_2 + S_BUTTON_ACTION_LONG_RELEASED,  "dimm up release" },
    // third button
    { Sensor::ModeDimmer,           0x01, 0x0008, 0x02, 0x01,  S_BUTTON_3 + S_BUTTON_ACTION_HOLD,           "dimm down" },
    { Sensor::ModeDimmer,           0x01, 0x0008, 0x03, 0x01,  S_BUTTON_3 + S_BUTTON_ACTION_LONG_RELEASED,  "dimm down release" },
    // bottom button
    { Sensor::ModeDimmer,           0x01, 0x0008, 0x04, 0x00,  S_BUTTON_4 + S_BUTTON_ACTION_SHORT_RELEASED,  "off" },

    // end
    { Sensor::ModeNone,             0x00, 0x0000, 0x00, 0,    0,                                           nullptr }
};

static const Sensor::ButtonMap innrRC110Map[] = {
//    mode                          ep    cluster cmd   param button                                       name
    // Remote's switch set to Scenes.
    { Sensor::ModeScenes,           0x01, 0x0006, 0x00, 0,    S_BUTTON_1 + S_BUTTON_ACTION_SHORT_RELEASED, "OnOff" },
    { Sensor::ModeScenes,           0x01, 0x0006, 0x01, 0,    S_BUTTON_1 + S_BUTTON_ACTION_SHORT_RELEASED, "OnOff" },
    { Sensor::ModeScenes,           0x01, 0x0008, 0x01, 0,    S_BUTTON_2 + S_BUTTON_ACTION_HOLD,           "DimUp" },
    { Sensor::ModeScenes,           0x01, 0x0008, 0x02, 0,    S_BUTTON_2 + S_BUTTON_ACTION_SHORT_RELEASED, "DimUp" },
    { Sensor::ModeScenes,           0x01, 0x0008, 0x03, 0,    S_BUTTON_2 + S_BUTTON_ACTION_LONG_RELEASED,  "DimUp" },
    { Sensor::ModeScenes,           0x01, 0x0008, 0x01, 1,    S_BUTTON_3 + S_BUTTON_ACTION_HOLD,           "DimDown" },
    { Sensor::ModeScenes,           0x01, 0x0008, 0x02, 1,    S_BUTTON_3 + S_BUTTON_ACTION_SHORT_RELEASED, "DimDown" },
    { Sensor::ModeScenes,           0x01, 0x0008, 0x03, 1,    S_BUTTON_3 + S_BUTTON_ACTION_LONG_RELEASED,  "DimDown" },
    { Sensor::ModeScenes,           0x01, 0x0008, 0x04, 0x02, S_BUTTON_4 + S_BUTTON_ACTION_SHORT_RELEASED,  "1" },
    { Sensor::ModeScenes,           0x01, 0x0008, 0x04, 0x34, S_BUTTON_5 + S_BUTTON_ACTION_SHORT_RELEASED,  "2" },
    { Sensor::ModeScenes,           0x01, 0x0008, 0x04, 0x66, S_BUTTON_6 + S_BUTTON_ACTION_SHORT_RELEASED,  "3" },
    { Sensor::ModeScenes,           0x01, 0x0008, 0x04, 0x99, S_BUTTON_7 + S_BUTTON_ACTION_SHORT_RELEASED,  "4" },
    { Sensor::ModeScenes,           0x01, 0x0008, 0x04, 0xC2, S_BUTTON_8 + S_BUTTON_ACTION_SHORT_RELEASED,  "5" },
    { Sensor::ModeScenes,           0x01, 0x0008, 0x04, 0xFE, S_BUTTON_9 + S_BUTTON_ACTION_SHORT_RELEASED,  "6" },
    // Remote's switch set to Lights, after pressing 1
    { Sensor::ModeScenes,           0x03, 0x0006, 0x00, 0,         10000 + S_BUTTON_ACTION_SHORT_RELEASED, "OnOff 1" },
    { Sensor::ModeScenes,           0x03, 0x0006, 0x01, 0,         10000 + S_BUTTON_ACTION_SHORT_RELEASED, "OnOff 1" },
    { Sensor::ModeScenes,           0x03, 0x0008, 0x01, 0,         11000 + S_BUTTON_ACTION_HOLD,           "DimUp 1" },
    { Sensor::ModeScenes,           0x03, 0x0008, 0x02, 0,         11000 + S_BUTTON_ACTION_SHORT_RELEASED, "DimUp 1" },
    { Sensor::ModeScenes,           0x03, 0x0008, 0x03, 0,         11000 + S_BUTTON_ACTION_LONG_RELEASED,  "DimUp 1" },
    { Sensor::ModeScenes,           0x03, 0x0008, 0x01, 1,         12000 + S_BUTTON_ACTION_HOLD,           "DimDown 1" },
    { Sensor::ModeScenes,           0x03, 0x0008, 0x02, 1,         12000 + S_BUTTON_ACTION_SHORT_RELEASED, "DimDown 1" },
    { Sensor::ModeScenes,           0x03, 0x0008, 0x03, 1,         12000 + S_BUTTON_ACTION_LONG_RELEASED,  "DimDown 1" },
    // Remote's switch set to Lights, after pressing 2
    { Sensor::ModeScenes,           0x04, 0x0006, 0x00, 0,         13000 + S_BUTTON_ACTION_SHORT_RELEASED, "OnOff 2" },
    { Sensor::ModeScenes,           0x04, 0x0006, 0x01, 0,         13000 + S_BUTTON_ACTION_SHORT_RELEASED, "OnOff 2" },
    { Sensor::ModeScenes,           0x04, 0x0008, 0x01, 0,         14000 + S_BUTTON_ACTION_HOLD,           "DimUp 2" },
    { Sensor::ModeScenes,           0x04, 0x0008, 0x02, 0,         14000 + S_BUTTON_ACTION_SHORT_RELEASED, "DimUp 2" },
    { Sensor::ModeScenes,           0x04, 0x0008, 0x03, 0,         14000 + S_BUTTON_ACTION_LONG_RELEASED,  "DimUp 2" },
    { Sensor::ModeScenes,           0x04, 0x0008, 0x01, 1,         15000 + S_BUTTON_ACTION_HOLD,           "DimDown 2" },
    { Sensor::ModeScenes,           0x04, 0x0008, 0x02, 1,         15000 + S_BUTTON_ACTION_SHORT_RELEASED, "DimDown 2" },
    { Sensor::ModeScenes,           0x04, 0x0008, 0x03, 1,         15000 + S_BUTTON_ACTION_LONG_RELEASED,  "DimDown 2" },
    // Remote's switch set to Lights, after pressing 3
    { Sensor::ModeScenes,           0x05, 0x0006, 0x00, 0,         16000 + S_BUTTON_ACTION_SHORT_RELEASED, "OnOff 3" },
    { Sensor::ModeScenes,           0x05, 0x0006, 0x01, 0,         16000 + S_BUTTON_ACTION_SHORT_RELEASED, "OnOff 3" },
    { Sensor::ModeScenes,           0x05, 0x0008, 0x01, 0,         17000 + S_BUTTON_ACTION_HOLD,           "DimUp 3" },
    { Sensor::ModeScenes,           0x05, 0x0008, 0x02, 0,         17000 + S_BUTTON_ACTION_SHORT_RELEASED, "DimUp 3" },
    { Sensor::ModeScenes,           0x05, 0x0008, 0x03, 0,         17000 + S_BUTTON_ACTION_LONG_RELEASED,  "DimUp 3" },
    { Sensor::ModeScenes,           0x05, 0x0008, 0x01, 1,         18000 + S_BUTTON_ACTION_HOLD,           "DimDown 3" },
    { Sensor::ModeScenes,           0x05, 0x0008, 0x02, 1,         18000 + S_BUTTON_ACTION_SHORT_RELEASED, "DimDown 3" },
    { Sensor::ModeScenes,           0x05, 0x0008, 0x03, 1,         18000 + S_BUTTON_ACTION_LONG_RELEASED,  "DimDown 3" },
    // Remote's switch set to Lights, after pressing 4
    { Sensor::ModeScenes,           0x06, 0x0006, 0x00, 0,         19000 + S_BUTTON_ACTION_SHORT_RELEASED, "OnOff 4" },
    { Sensor::ModeScenes,           0x06, 0x0006, 0x01, 0,         19000 + S_BUTTON_ACTION_SHORT_RELEASED, "OnOff 4" },
    { Sensor::ModeScenes,           0x06, 0x0008, 0x01, 0,         20000 + S_BUTTON_ACTION_HOLD,           "DimUp 4" },
    { Sensor::ModeScenes,           0x06, 0x0008, 0x02, 0,         20000 + S_BUTTON_ACTION_SHORT_RELEASED, "DimUp 4" },
    { Sensor::ModeScenes,           0x06, 0x0008, 0x03, 0,         20000 + S_BUTTON_ACTION_LONG_RELEASED,  "DimUp 4" },
    { Sensor::ModeScenes,           0x06, 0x0008, 0x01, 1,         21000 + S_BUTTON_ACTION_HOLD,           "DimDown 4" },
    { Sensor::ModeScenes,           0x06, 0x0008, 0x02, 1,         21000 + S_BUTTON_ACTION_SHORT_RELEASED, "DimDown 4" },
    { Sensor::ModeScenes,           0x06, 0x0008, 0x03, 1,         21000 + S_BUTTON_ACTION_LONG_RELEASED,  "DimDown 4" },
    // Remote's switch set to Lights, after pressing 5
    { Sensor::ModeScenes,           0x07, 0x0006, 0x00, 0,         22000 + S_BUTTON_ACTION_SHORT_RELEASED, "OnOff 5" },
    { Sensor::ModeScenes,           0x07, 0x0006, 0x01, 0,         22000 + S_BUTTON_ACTION_SHORT_RELEASED, "OnOff 5" },
    { Sensor::ModeScenes,           0x07, 0x0008, 0x01, 0,         23000 + S_BUTTON_ACTION_HOLD,           "DimUp 5" },
    { Sensor::ModeScenes,           0x07, 0x0008, 0x02, 0,         23000 + S_BUTTON_ACTION_SHORT_RELEASED, "DimUp 5" },
    { Sensor::ModeScenes,           0x07, 0x0008, 0x03, 0,         23000 + S_BUTTON_ACTION_LONG_RELEASED,  "DimUp 5" },
    { Sensor::ModeScenes,           0x07, 0x0008, 0x01, 1,         24000 + S_BUTTON_ACTION_HOLD,           "DimDown 5" },
    { Sensor::ModeScenes,           0x07, 0x0008, 0x02, 1,         24000 + S_BUTTON_ACTION_SHORT_RELEASED, "DimDown 5" },
    { Sensor::ModeScenes,           0x07, 0x0008, 0x03, 1,         24000 + S_BUTTON_ACTION_LONG_RELEASED,  "DimDown 5" },
    // Remote's switch set to Lights, after pressing 6
    { Sensor::ModeScenes,           0x08, 0x0006, 0x00, 0,         25000 + S_BUTTON_ACTION_SHORT_RELEASED, "OnOff 6" },
    { Sensor::ModeScenes,           0x08, 0x0006, 0x01, 0,         25000 + S_BUTTON_ACTION_SHORT_RELEASED, "OnOff 6" },
    { Sensor::ModeScenes,           0x08, 0x0008, 0x01, 0,         26000 + S_BUTTON_ACTION_HOLD,           "DimUp 6" },
    { Sensor::ModeScenes,           0x08, 0x0008, 0x02, 0,         26000 + S_BUTTON_ACTION_SHORT_RELEASED, "DimUp 6" },
    { Sensor::ModeScenes,           0x08, 0x0008, 0x03, 0,         26000 + S_BUTTON_ACTION_LONG_RELEASED,  "DimUp 6" },
    { Sensor::ModeScenes,           0x08, 0x0008, 0x01, 1,         27000 + S_BUTTON_ACTION_HOLD,           "DimDown 6" },
    { Sensor::ModeScenes,           0x08, 0x0008, 0x02, 1,         27000 + S_BUTTON_ACTION_SHORT_RELEASED, "DimDown 6" },
    { Sensor::ModeScenes,           0x08, 0x0008, 0x03, 1,         27000 + S_BUTTON_ACTION_LONG_RELEASED,  "DimDown 6" },

    { Sensor::ModeNone,             0x00, 0x0000, 0x00, 0,    0,                                           nullptr }
};

static const Sensor::ButtonMap icasaKeypadMap[] = {
//    mode                          ep    cluster cmd   param button                                       name
    // Off button
    { Sensor::ModeScenes,           0x01, 0x0006, 0x00, 0,    S_BUTTON_1 + S_BUTTON_ACTION_SHORT_RELEASED, "Off" },
    { Sensor::ModeScenes,           0x01, 0x0008, 0x05, 1,    S_BUTTON_1 + S_BUTTON_ACTION_HOLD, "Move down (with on/off)" },
    { Sensor::ModeScenes,           0x01, 0x0008, 0x07, 1,    S_BUTTON_1 + S_BUTTON_ACTION_LONG_RELEASED, "Stop_ (with on/off)" },
    // On button
    { Sensor::ModeScenes,           0x01, 0x0006, 0x01, 0,    S_BUTTON_2 + S_BUTTON_ACTION_SHORT_RELEASED, "On" },
    { Sensor::ModeScenes,           0x01, 0x0008, 0x05, 0,    S_BUTTON_2 + S_BUTTON_ACTION_HOLD, "Move up (with on/off)" },
    { Sensor::ModeScenes,           0x01, 0x0008, 0x07, 0,    S_BUTTON_2 + S_BUTTON_ACTION_LONG_RELEASED, "Stop_ (with on/off)" },
    // Scene buttons
    { Sensor::ModeScenes,           0x01, 0x0005, 0x05, 1,    S_BUTTON_3 + S_BUTTON_ACTION_SHORT_RELEASED, "Recall scene 1" },
    { Sensor::ModeScenes,           0x01, 0x0005, 0x05, 2,    S_BUTTON_4 + S_BUTTON_ACTION_SHORT_RELEASED, "Recall scene 2" },
    { Sensor::ModeScenes,           0x01, 0x0005, 0x05, 3,    S_BUTTON_5 + S_BUTTON_ACTION_SHORT_RELEASED, "Recall scene 3" },
    { Sensor::ModeScenes,           0x01, 0x0005, 0x05, 4,    S_BUTTON_6 + S_BUTTON_ACTION_SHORT_RELEASED, "Recall scene 4" },
    { Sensor::ModeScenes,           0x01, 0x0005, 0x05, 5,    S_BUTTON_7 + S_BUTTON_ACTION_SHORT_RELEASED, "Recall scene 5" },
    { Sensor::ModeScenes,           0x01, 0x0005, 0x05, 6,    S_BUTTON_8 + S_BUTTON_ACTION_SHORT_RELEASED, "Recall scene 6" },
    // end
    { Sensor::ModeNone,             0x00, 0x0000, 0x00, 0,    0,                                           nullptr }
};

static const Sensor::ButtonMap samjinButtonMap[] = {
//    mode                          ep    cluster cmd   param button                                       name
    // First button
    { Sensor::ModeScenes,           0x01, 0x0500, 0x00, 0x01,    S_BUTTON_1 + S_BUTTON_ACTION_SHORT_RELEASED, "Single press" },
    { Sensor::ModeScenes,           0x01, 0x0500, 0x00, 0x02,    S_BUTTON_1 + S_BUTTON_ACTION_DOUBLE_PRESS, "Double press" },
    { Sensor::ModeScenes,           0x01, 0x0500, 0x00, 0x03,    S_BUTTON_1 + S_BUTTON_ACTION_HOLD, "Hold" },

    // end
    { Sensor::ModeNone,             0x00, 0x0000, 0x00, 0,    0,                                           nullptr }
};

static const Sensor::ButtonMap sunricherCCTMap[] = {
//    mode                          ep    cluster cmd   param button                                       name
    // Off button
    { Sensor::ModeScenes,           0x01, 0x0006, 0x01, 0,    S_BUTTON_1 + S_BUTTON_ACTION_SHORT_RELEASED, "On" },
    // On button
    { Sensor::ModeScenes,           0x01, 0x0006, 0x00, 0,    S_BUTTON_2 + S_BUTTON_ACTION_SHORT_RELEASED, "Off" },
    // Dim button
    { Sensor::ModeScenes,           0x01, 0x0008, 0x05, 0,    S_BUTTON_3 + S_BUTTON_ACTION_HOLD, "Move up (with on/off)" },
    { Sensor::ModeScenes,           0x01, 0x0008, 0x07, 0,    S_BUTTON_3 + S_BUTTON_ACTION_LONG_RELEASED, "Stop_ (with on/off)" },
    { Sensor::ModeScenes,           0x01, 0x0008, 0x05, 1,    S_BUTTON_3 + S_BUTTON_ACTION_HOLD, "Move down (with on/off)" },
    { Sensor::ModeScenes,           0x01, 0x0008, 0x07, 1,    S_BUTTON_3 + S_BUTTON_ACTION_LONG_RELEASED, "Stop_ (with on/off)" },
    // C/W button
    { Sensor::ModeScenes,           0x01, 0x0300, 0x0a, 0,    S_BUTTON_4 + S_BUTTON_ACTION_SHORT_RELEASED, "Move to color temperature" },
    { Sensor::ModeScenes,           0x01, 0x0300, 0x4b, 0x013C, S_BUTTON_4 + S_BUTTON_ACTION_HOLD, "Move color temperature up" },
    { Sensor::ModeScenes,           0x01, 0x0300, 0x4b, 0x033C, S_BUTTON_4 + S_BUTTON_ACTION_HOLD, "Move color temperature down" },
    { Sensor::ModeScenes,           0x01, 0x0300, 0x47, 0,    S_BUTTON_4 + S_BUTTON_ACTION_LONG_RELEASED, "Stop_" },
    // end
    { Sensor::ModeNone,             0x00, 0x0000, 0x00, 0,    0,                                           nullptr }
};

static const Sensor::ButtonMap sunricherMap[] = {
//    mode                          ep    cluster cmd   param button                                       name
    // 1st On button
    { Sensor::ModeScenes,           0x01, 0x0006, 0x01, 0,    S_BUTTON_1 + S_BUTTON_ACTION_SHORT_RELEASED, "On" },
    { Sensor::ModeScenes,           0x01, 0x0008, 0x05, 0,    S_BUTTON_1 + S_BUTTON_ACTION_HOLD, "Move up (with on/off)" },
    { Sensor::ModeScenes,           0x01, 0x0008, 0x07, 0,    S_BUTTON_1 + S_BUTTON_ACTION_LONG_RELEASED, "Stop_ (with on/off)" },
    // 1st Off button
    { Sensor::ModeScenes,           0x01, 0x0006, 0x00, 0,    S_BUTTON_2 + S_BUTTON_ACTION_SHORT_RELEASED, "Off" },
    { Sensor::ModeScenes,           0x01, 0x0008, 0x05, 1,    S_BUTTON_2 + S_BUTTON_ACTION_HOLD, "Move down (with on/off)" },
    { Sensor::ModeScenes,           0x01, 0x0008, 0x07, 1,    S_BUTTON_2 + S_BUTTON_ACTION_LONG_RELEASED, "Stop_ (with on/off)" },
    // 2nd On button
    { Sensor::ModeScenes,           0x02, 0x0006, 0x01, 0,    S_BUTTON_3 + S_BUTTON_ACTION_SHORT_RELEASED, "On" },
    { Sensor::ModeScenes,           0x02, 0x0008, 0x05, 0,    S_BUTTON_3 + S_BUTTON_ACTION_HOLD, "Move up (with on/off)" },
    { Sensor::ModeScenes,           0x02, 0x0008, 0x07, 0,    S_BUTTON_3 + S_BUTTON_ACTION_LONG_RELEASED, "Stop_ (with on/off)" },
    // 2nd Off button
    { Sensor::ModeScenes,           0x02, 0x0006, 0x00, 0,    S_BUTTON_4 + S_BUTTON_ACTION_SHORT_RELEASED, "Off" },
    { Sensor::ModeScenes,           0x02, 0x0008, 0x05, 1,    S_BUTTON_4 + S_BUTTON_ACTION_HOLD, "Move down (with on/off)" },
    { Sensor::ModeScenes,           0x02, 0x0008, 0x07, 1,    S_BUTTON_4 + S_BUTTON_ACTION_LONG_RELEASED, "Stop_ (with on/off)" },
    // 3rd On button
    { Sensor::ModeScenes,           0x03, 0x0006, 0x01, 0,    S_BUTTON_5 + S_BUTTON_ACTION_SHORT_RELEASED, "On" },
    { Sensor::ModeScenes,           0x03, 0x0008, 0x05, 0,    S_BUTTON_5 + S_BUTTON_ACTION_HOLD, "Move up (with on/off)" },
    { Sensor::ModeScenes,           0x03, 0x0008, 0x07, 0,    S_BUTTON_5 + S_BUTTON_ACTION_LONG_RELEASED, "Stop_ (with on/off)" },
    // 3rd Off button
    { Sensor::ModeScenes,           0x03, 0x0006, 0x00, 0,    S_BUTTON_6 + S_BUTTON_ACTION_SHORT_RELEASED, "Off" },
    { Sensor::ModeScenes,           0x03, 0x0008, 0x05, 1,    S_BUTTON_6 + S_BUTTON_ACTION_HOLD, "Move down (with on/off)" },
    { Sensor::ModeScenes,           0x03, 0x0008, 0x07, 1,    S_BUTTON_6 + S_BUTTON_ACTION_LONG_RELEASED, "Stop_ (with on/off)" },
    // 4th On button
    { Sensor::ModeScenes,           0x04, 0x0006, 0x01, 0,    S_BUTTON_7 + S_BUTTON_ACTION_SHORT_RELEASED, "On" },
    { Sensor::ModeScenes,           0x04, 0x0008, 0x05, 0,    S_BUTTON_7 + S_BUTTON_ACTION_HOLD, "Move up (with on/off)" },
    { Sensor::ModeScenes,           0x04, 0x0008, 0x07, 0,    S_BUTTON_7 + S_BUTTON_ACTION_LONG_RELEASED, "Stop_ (with on/off)" },
    // 4th Off button
    { Sensor::ModeScenes,           0x04, 0x0006, 0x00, 0,    S_BUTTON_8 + S_BUTTON_ACTION_SHORT_RELEASED, "Off" },
    { Sensor::ModeScenes,           0x04, 0x0008, 0x05, 1,    S_BUTTON_8 + S_BUTTON_ACTION_HOLD, "Move down (with on/off)" },
    { Sensor::ModeScenes,           0x04, 0x0008, 0x07, 1,    S_BUTTON_8 + S_BUTTON_ACTION_LONG_RELEASED, "Stop_ (with on/off)" },
    // end
    { Sensor::ModeNone,             0x00, 0x0000, 0x00, 0,    0,                                           nullptr }
};

/*! Returns a fingerprint as JSON string. */
QString SensorFingerprint::toString() const
{
    if (endpoint == 0xFF || profileId == 0xFFFF)
    {
        return QString();
    }

    QVariantMap map;
    map["ep"] = (double)endpoint;
    map["p"] = (double)profileId;
    map["d"] = (double)deviceId;

    if (!inClusters.empty())
    {
        QVariantList ls;
        for (uint  i = 0; i < inClusters.size(); i++)
        {
            ls.append((double)inClusters[i]);
        }
        map["in"] = ls;
    }

    if (!outClusters.empty())
    {
        QVariantList ls;
        for (uint  i = 0; i < outClusters.size(); i++)
        {
            ls.append((double)outClusters[i]);
        }
        map["out"] = ls;
    }

    return deCONZ::jsonStringFromMap(map);
}

/*! Parses a fingerprint from JSON string.
    \returns true on success
*/
bool SensorFingerprint::readFromJsonString(const QString &json)
{
    if (json.isEmpty())
    {
        return false;
    }

    bool ok = false;
    QVariant var = Json::parse(json, ok);

    if (!ok)
    {
        return false;
    }

    QVariantMap map = var.toMap();

    if (map.contains("ep") && map.contains("p") && map.contains("d"))
    {
        endpoint = map["ep"].toUInt(&ok);
        if (!ok) { return false; }
        profileId = map["p"].toUInt(&ok);
        if (!ok) { return false; }
        deviceId = map["d"].toUInt(&ok);
        if (!ok) { return false; }

        inClusters.clear();
        outClusters.clear();

        if (map.contains("in") && map["in"].type() == QVariant::List)
        {
            QVariantList ls = map["in"].toList();
            QVariantList::const_iterator i = ls.constBegin();
            QVariantList::const_iterator end = ls.constEnd();
            for (; i != end; ++i)
            {
                quint16 clusterId = i->toUInt(&ok);
                if (ok)
                {
                    inClusters.push_back(clusterId);
                }
            }
        }

        if (map.contains("out") && map["out"].type() == QVariant::List)
        {
            QVariantList ls = map["out"].toList();
            QVariantList::const_iterator i = ls.constBegin();
            QVariantList::const_iterator end = ls.constEnd();
            for (; i != end; ++i)
            {
                quint16 clusterId = i->toUInt(&ok);
                if (ok)
                {
                    outClusters.push_back(clusterId);
                }
            }
        }

        return true;
    }

    return false;
}

/*! Returns true if server cluster is part of the finger print.
 */
bool SensorFingerprint::hasInCluster(quint16 clusterId) const
{
    for (size_t i = 0; i < inClusters.size(); i++)
    {
        if (inClusters[i] == clusterId)
        {
            return true;
        }
    }

    return false;
}

/*! Returns true if server cluster is part of the finger print.
 */
bool SensorFingerprint::hasOutCluster(quint16 clusterId) const
{
    for (size_t i = 0; i < outClusters.size(); i++)
    {
        if (outClusters[i] == clusterId)
        {
            return true;
        }
    }

    return false;
}

/*! Constructor. */
Sensor::Sensor() :
    Resource(RSensors),
    m_deletedstate(Sensor::StateNormal),
    m_mode(ModeTwoGroups),
    m_resetRetryCount(0),
    m_buttonMap(nullptr),
    m_rxCounter(0)
{
    QDateTime now = QDateTime::currentDateTime();
    lastStatePush = now;
    lastConfigPush = now;
    durationDue = QDateTime();

    // common sensor items
    addItem(DataTypeString, RAttrName);
    addItem(DataTypeString, RAttrManufacturerName);
    addItem(DataTypeString, RAttrModelId);
    addItem(DataTypeString, RAttrType);
    addItem(DataTypeString, RAttrSwVersion);
    addItem(DataTypeBool, RConfigOn);
    addItem(DataTypeBool, RConfigReachable);
    addItem(DataTypeTime, RStateLastUpdated);

    previousDirection = 0xFF;
}

/*! Returns the sensor deleted state.
 */
Sensor::DeletedState Sensor::deletedState() const
{
    return m_deletedstate;
}

/*! Sets the sensor deleted state.
    \param deletedState the sensor deleted state
 */
void Sensor::setDeletedState(DeletedState deletedstate)
{
    m_deletedstate = deletedstate;
}

/*! Returns true if the sensor is reachable.
 */
bool Sensor::isAvailable() const
{
    const ResourceItem *i = item(RConfigReachable);
    if (i)
    {
        return i->toBool();
    }
    return true;
}

/*! Returns the sensor name.
 */
const QString &Sensor::name() const
{
   return item(RAttrName)->toString();
}

/*! Sets the sensor name.
    \param name the sensor name
 */
void Sensor::setName(const QString &name)
{
    item(RAttrName)->setValue(name);
}

/*! Returns the sensor mode.
 */
Sensor::SensorMode Sensor::mode() const
{
   return m_mode;
}

/*! Sets the sensor mode (Lighting Switch).
 * 1 = Secenes
 * 2 = Groups
 * 3 = Color Temperature
    \param mode the sensor mode
 */
void Sensor::setMode(SensorMode mode)
{
    m_mode = mode;
}

/*! Returns the sensor type.
 */
const QString &Sensor::type() const
{
    return item(RAttrType)->toString();
}

/*! Sets the sensor type.
    \param type the sensor type
 */
void Sensor::setType(const QString &type)
{
    item(RAttrType)->setValue(type);
}

/*! Returns the sensor modelId.
 */
const QString &Sensor::modelId() const
{
    return item(RAttrModelId)->toString();
}

/*! Sets the sensor modelId.
    \param mid the sensor modelId
 */
void Sensor::setModelId(const QString &mid)
{
    item(RAttrModelId)->setValue(mid.trimmed());
}

/*! Returns the resetRetryCount.
 */
uint8_t Sensor::resetRetryCount() const
{
    return m_resetRetryCount;
}

/*! Sets the resetRetryCount.
    \param resetRetryCount the resetRetryCount
 */
void Sensor::setResetRetryCount(uint8_t resetRetryCount)
{
    m_resetRetryCount = resetRetryCount;
}

/*! Returns the zdpResetSeq number.
 */
uint8_t Sensor::zdpResetSeq() const
{
    return m_zdpResetSeq;
}

/*! Sets the zdpResetSeq number.
    \param resetRetryCount the resetRetryCount
 */
void Sensor::setZdpResetSeq(uint8_t zdpResetSeq)
{
    m_zdpResetSeq = zdpResetSeq;
}

void Sensor::updateStateTimestamp()
{
    ResourceItem *i = item(RStateLastUpdated);
    if (i)
    {
        i->setValue(QDateTime::currentDateTimeUtc());
        m_rxCounter++;
    }
}

/*! Increments the number of received commands during this session. */
void Sensor::incrementRxCounter()
{
    m_rxCounter++;
}

/*! Returns number of received commands during this session. */
int Sensor::rxCounter() const
{
    return m_rxCounter;
}


/*! Returns the sensor manufacturer.
 */
const QString &Sensor::manufacturer() const
{
    return item(RAttrManufacturerName)->toString();
}

/*! Sets the sensor manufacturer.
    \param manufacturer the sensor manufacturer
 */
void Sensor::setManufacturer(const QString &manufacturer)
{
    item(RAttrManufacturerName)->setValue(manufacturer.trimmed());
}

/*! Returns the sensor software version.
    Not supported for ZGP Sensortype
 */
const QString &Sensor::swVersion() const
{
    return item(RAttrSwVersion)->toString();
}

/*! Sets the sensor software version.
    \param swVersion the sensor software version
 */
void Sensor::setSwVersion(const QString &swversion)
{
    item(RAttrSwVersion)->setValue(swversion.trimmed());
}

/*! Transfers state into JSONString.
 */
QString Sensor::stateToString()
{
    QVariantMap map;

    for (int i = 0; i < itemCount(); i++)
    {
        ResourceItem *item = itemForIndex(i);
        const ResourceItemDescriptor &rid = item->descriptor();

        if (strncmp(rid.suffix, "state/", 6) == 0)
        {
            const char *key = item->descriptor().suffix + 6;
            map[key] = item->toVariant();
        }
    }

    return Json::serialize(map);
}

/*! Transfers config into JSONString.
 */
QString Sensor::configToString()
{
    QVariantMap map;

    for (int i = 0; i < itemCount(); i++)
    {
        ResourceItem *item = itemForIndex(i);
        const ResourceItemDescriptor &rid = item->descriptor();

        if (strncmp(rid.suffix, "config/", 7) == 0)
        {
            const char *key = item->descriptor().suffix + 7;
            map[key] = item->toVariant();
        }
    }

    return Json::serialize(map);
}

/*! Parse the sensor state from a JSON string. */
void Sensor::jsonToState(const QString &json)
{
    bool ok;
    QVariant var = Json::parse(json, ok);

    if (!ok)
    {
        return;
    }

    QVariantMap map = var.toMap();

    // use old time stamp before deCONZ was started
    QDateTime dt = QDateTime::currentDateTime().addSecs(-120);
    if (map.contains("lastupdated"))
    {
        QDateTime lu = QDateTime::fromString(map["lastupdated"].toString(), QLatin1String("yyyy-MM-ddTHH:mm:ss"));
        if (lu < dt)
        {
            dt = lu;
        }
        lu.setTimeSpec(Qt::UTC);
        map["lastupdated"] = lu;
    }

    for (int i = 0; i < itemCount(); i++)
    {
        ResourceItem *item = itemForIndex(i);
        const ResourceItemDescriptor &rid = item->descriptor();

        if (strncmp(rid.suffix, "state/", 6) == 0)
        {
            const char *key = item->descriptor().suffix + 6;

            if (map.contains(QLatin1String(key)))
            {
                item->setValue(map[key]);
                item->setTimeStamps(dt);
            }
        }
    }
}

/*! Parse the sensor config from a JSON string. */
void Sensor::jsonToConfig(const QString &json)
{
    bool ok;

    QVariant var = Json::parse(json, ok);

    if (!ok)
    {
        return;
    }
    QVariantMap map = var.toMap();
    QDateTime dt = QDateTime::currentDateTime().addSecs(-120);

    for (int i = 0; i < itemCount(); i++)
    {
        ResourceItem *item = itemForIndex(i);
        const ResourceItemDescriptor &rid = item->descriptor();

        if (type().startsWith(QLatin1String("CLIP")))
        {}
        else if (item->descriptor().suffix == RConfigReachable)
        { // set only from live data
            item->setValue(false);
            continue;
        }

        if (strncmp(rid.suffix, "config/", 7) == 0 && rid.suffix != RConfigPending)
        {
            const char *key = item->descriptor().suffix + 7;

            if (map.contains(QLatin1String(key)))
            {
                QVariant val = map[key];

                if (val.isNull())
                {
                    if (rid.suffix == RConfigOn)
                    {
                        map[key] = true; // default value
                        setNeedSaveDatabase(true);
                    }
                    else
                    {
                        continue;
                    }
                }

                item->setValue(map[key]);
                item->setTimeStamps(dt);
            }
        }
    }
}

/*! Returns the sensor fingerprint. */
SensorFingerprint &Sensor::fingerPrint()
{
    return m_fingerPrint;
}

/*! Returns the sensor fingerprint. */
const SensorFingerprint &Sensor::fingerPrint() const
{
    return m_fingerPrint;
}

const Sensor::ButtonMap *Sensor::buttonMap()
{
    if (!m_buttonMap)
    {
        const QString &modelid = item(RAttrModelId)->toString();
        const QString &manufacturer = item(RAttrManufacturerName)->toString();
        if (manufacturer == QLatin1String("dresden elektronik"))
        {
            if      (modelid == QLatin1String("Lighting Switch")) { m_buttonMap = deLightingSwitchMap; }
            else if (modelid == QLatin1String("Scene Switch"))    { m_buttonMap = deSceneSwitchMap; }
        }
        else if (manufacturer == QLatin1String("Insta"))
        {
            if      (modelid.endsWith(QLatin1String("_1")))       { m_buttonMap = instaRemoteMap; }
            if      (modelid.contains(QLatin1String("Remote")))   { m_buttonMap = instaRemoteMap; }
        }
        else if (manufacturer == QLatin1String("Philips"))
        {
            if      (modelid.startsWith(QLatin1String("RWL02")))  { m_buttonMap = philipsDimmerSwitchMap; }
        }
        else if (manufacturer == QLatin1String("Busch-Jaeger"))
        {
            m_buttonMap = bjeSwitchMap;
        }
        else if (manufacturer.startsWith(QLatin1String("IKEA")))
        {
            if      (modelid.startsWith(QLatin1String("TRADFRI remote control"))) { m_buttonMap = ikeaRemoteMap; }
            else if (modelid.startsWith(QLatin1String("TRADFRI motion sensor"))) { m_buttonMap = ikeaMotionSensorMap; }
            else if (modelid.startsWith(QLatin1String("TRADFRI wireless dimmer"))) { m_buttonMap = ikeaDimmerMap; }
            else if (modelid.startsWith(QLatin1String("TRADFRI on/off switch"))) { m_buttonMap = ikeaOnOffMap; }
            else if (modelid.startsWith(QLatin1String("TRADFRI open/close remote"))) { m_buttonMap = ikeaOpenCloseMap; }
            else if (modelid.startsWith(QLatin1String("SYMFONISK"))) { m_buttonMap = ikeaSoundControllerMap; }
        }
        else if (manufacturer == QLatin1String("ubisys"))
        {
            if      (modelid.startsWith(QLatin1String("D1"))) { m_buttonMap = ubisysD1Map; }
            else if (modelid.startsWith(QLatin1String("C4"))) { m_buttonMap = ubisysC4Map; }
            else if (modelid.startsWith(QLatin1String("S1"))) { m_buttonMap = ubisysD1Map; }
            else if (modelid.startsWith(QLatin1String("S2"))) { m_buttonMap = ubisysS2Map; }
        }
        else if (manufacturer == QLatin1String("LUMI"))
        {
            if      (modelid == QLatin1String("lumi.sensor_switch"))      { m_buttonMap = xiaomiSwitchMap; }
            else if (modelid == QLatin1String("lumi.sensor_switch.aq2"))  { m_buttonMap = xiaomiSwitchAq2Map; }
            else if (modelid.startsWith(QLatin1String("lumi.vibration"))) { m_buttonMap = xiaomiVibrationMap; }
        }
        else if (manufacturer == QLatin1String("Lutron"))
        {
            if      (modelid.startsWith(QLatin1String("LZL4BWHL")))      { m_buttonMap = lutronLZL4BWHLSwitchMap; }
        }
        else if (manufacturer == QLatin1String("Trust"))
        {
            if      (modelid == QLatin1String("ZYCT-202"))      { m_buttonMap = trustZYCT202SwitchMap; }
        }
        else if (manufacturer == QLatin1String("innr"))
        {
            if      (modelid.startsWith(QLatin1String("RC 110"))) { m_buttonMap = innrRC110Map; }
        }
        else if (manufacturer == QLatin1String("icasa"))
        {
            if      (modelid.startsWith(QLatin1String("ICZB-KPD1"))) { m_buttonMap = icasaKeypadMap; }
        }
        else if (manufacturer == QLatin1String("Samjin"))
        {
            if (modelid == QLatin1String("button")) { m_buttonMap = samjinButtonMap; }
        }
        else if (manufacturer == QLatin1String("Sunricher"))
        {
            if      (modelid.startsWith(QLatin1String("ZGRC-KEY"))) { m_buttonMap = sunricherCCTMap; }
            else if (modelid.startsWith(QLatin1String("ZG2833K"))) { m_buttonMap = sunricherMap; }
        }
    }

    return m_buttonMap;
}
