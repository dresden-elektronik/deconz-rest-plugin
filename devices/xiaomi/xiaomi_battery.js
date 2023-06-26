/* global Attr, Item */

const vmin = 2700
const vmax = 3000
const v = Math.max(vmin, Math.min(Attr.val, vmax))
const bat = Math.round(((v - vmin) / (vmax - vmin)) * 100)
Item.val = Math.max(0, Math.min(bat, 100))
