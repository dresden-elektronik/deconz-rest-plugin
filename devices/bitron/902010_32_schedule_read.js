/* global R */

// Thermostat cluster 0x0201, Get Weekly Schedule (cmd 0x02): request ONE day per poll.
// Payload: daysToReturn(u8, Zigbee bitmap: bit0 = Sunday .. bit6 = Saturday), mode(u8, 0x01 = heat).
//
// The device only answers a request for a single day, so the week is read one day per poll (refresh.interval
// of config/schedule), a full week takes 7 polls.
//
// The day to request is kept in config/schedule_read_day: weekday (0 = Sunday .. 6 = Saturday) + 7 * unanswered requests.
// The parse script (902010_32_schedule.js) moves it to the next weekday when the response arrives.
// A day which is not answered is requested once more, after two unanswered requests the day is skipped.
(function () {
  var state = R.item('config/schedule_read_day')
  var value = Number(state.val) || 0
  var day = value % 7
  var tries = Math.floor(value / 7)

  if (tries >= 2) {
    day = (day + 1) % 7
    tries = 0
  }

  state.val = day + 7 * (tries + 1)

  var bit = 1 << day // Zigbee weekday bitmap: bit0 = Sunday .. bit6 = Saturday
  return (bit < 16 ? '0' : '') + bit.toString(16) + '01'
})()
