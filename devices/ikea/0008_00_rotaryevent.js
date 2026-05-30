/* global Item, R, ZclFrame */

const rotation = 30
const duration = 1000
const delta = new Date().getTime() - new Date(R.item('state/lastupdated').val).getTime()
const level = ZclFrame.at(0)
const up = level > R.item('state/bri').val || level === 255
const changed = (up ? rotation : -rotation) !== R.item('state/expectedrotation').val
Item.val = (changed || delta > duration) ? 1 : 2
R.item('state/expectedeventduration').val = duration
R.item('state/expectedrotation').val = up ? rotation : -rotation
R.item('state/bri').val = level
