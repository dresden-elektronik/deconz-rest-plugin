/* global Attr, Item, R */

const v = Attr.val
const tholddark = R.item('config/tholddark').val
const tholdoffset = R.item('config/tholdoffset').val

Item.val = v
R.item('state/dark').val = v <= tholddark
R.item('state/daylight').val = v > tholddark + tholdoffset
R.item('state/lux').val = Math.round(Math.pow(10, (Math.max(0, Math.min(v, 60001)) - 1) / 10000))
