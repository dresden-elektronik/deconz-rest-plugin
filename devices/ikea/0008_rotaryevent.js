/* global Item, R, ZclFrame */

const rotation = 30
const duration = 1000
const delta = new Date().getTime() - new Date(R.item('state/lastupdated').val).getTime()
const expectedrotation = ZclFrame.at(0) === 0x00 ? rotation : -rotation
const changed = expectedrotation !== R.item('state/expectedrotation').val
Item.val = (changed || delta > duration) ? 1 : 2
R.item('state/expectedeventduration').val = duration
R.item('state/expectedrotation').val = ZclFrame.at(0) === 0x00 ? rotation : -rotation
