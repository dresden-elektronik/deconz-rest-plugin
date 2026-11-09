/* global Item, R, ZclFrame */

// Thermostat cluster 0x0201, Get Weekly Schedule Response (cmd 0x00, server -> client).
// Payload: numberOfTransitions(u8) dayOfWeek(u8) mode(u8) [transitionTime(u16) heatSetpoint(i16)]...
// Result is merged into config/schedule using the format "W<ISO weekdays>/T<HH>:<MM>|<heatsetpoint>...",
// the same format as updateThermostatSchedule() in thermostat.cpp.
// The device answers one day per request, the response sets config/schedule_read_day to the next weekday
// for 902010_32_schedule_read.js. A request without response is retried there.
(function () {
  if (!ZclFrame.isClCmd || ZclFrame.payloadSize < 3) {
    return false
  }

  var n = ZclFrame.at(0)
  var zigbeeDays = ZclFrame.at(1)
  var mode = ZclFrame.at(2)
  var pos = 3
  var transitions = ''

  function pad2(v) { return (v < 10 ? '0' : '') + v }

  for (var i = 0; i < n; i++) {
    var need = 2 + ((mode & 0x01) ? 2 : 0) + ((mode & 0x02) ? 2 : 0)
    if (pos + need > ZclFrame.payloadSize) {
      return false // truncated frame
    }

    var minutes = ZclFrame.at(pos) | (ZclFrame.at(pos + 1) << 8)
    pos += 2

    if (mode & 0x01) {
      var heat = (ZclFrame.at(pos) | (ZclFrame.at(pos + 1) << 8)) << 16 >> 16 // int16
      pos += 2
      transitions += 'T' + pad2(Math.floor(minutes / 60)) + ':' + pad2(minutes % 60) + '|' + heat
    }
    if (mode & 0x02) {
      pos += 2 // cool setpoint, ignored (same as thermostat.cpp)
      break
    }
  }

  // Zigbee weekday bitmap (bit0 = Sunday .. bit6 = Saturday) --> ISO bitmap used by the API.
  var map = [0x01, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02]
  var newDays = 0
  for (var d = 0; d < 7; d++) {
    if (zigbeeDays & (1 << d)) {
      newDays |= map[d]
    }
  }
  if (newDays === 0) {
    return false
  }

  // Request the next weekday on the next read (0 = Sunday .. 6 = Saturday).
  for (var first = 0; first < 7 && !(zigbeeDays & (1 << first)); first++) {}
  R.item('config/schedule_read_day').val = (first + 1) % 7

  // Parse current value, drop newDays from existing entries.
  var entries = {}
  var parts = String(Item.val).split('W')
  for (var p = 0; p < parts.length; p++) {
    if (parts[p] === '') continue
    var kv = parts[p].split('/')
    var days = parseInt(kv[0], 10)
    if (isNaN(days) || kv.length < 2) {
      entries = {}
      break
    }
    days &= ~newDays & 0xFF
    if (days !== 0) {
      entries[days] = kv[1]
    }
  }

  if (transitions !== '') {
    var merged = false
    for (var k in entries) {
      if (entries[k] === transitions) {
        var combined = parseInt(k, 10) | newDays
        delete entries[k]
        entries[combined] = transitions
        merged = true
        break
      }
    }
    if (!merged) {
      entries[newDays] = transitions
    }
  }

  var keys = Object.keys(entries).map(function (x) { return parseInt(x, 10) })
  keys.sort(function (a, b) { return a - b })

  var s = ''
  for (var j = 0; j < keys.length; j++) {
    s += 'W' + keys[j] + '/' + entries[keys[j]]
  }

  Item.val = s

  return true
})()
