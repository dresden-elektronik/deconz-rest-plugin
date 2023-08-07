/* global Attr, R */

const x = Attr.val << 16 >> 16
const y = Attr.val >> 16
const z = (Attr.val / 0x10000) >> 16
R.item('state/orientation_x').val = Math.round(Math.atan(x / Math.sqrt(z * z + y * y)) * 180 / Math.PI)
R.item('state/orientation_y').val = Math.round(Math.atan(y / Math.sqrt(x * x + z * z)) * 180 / Math.PI)
R.item('state/orientation_z').val = Math.round(Math.atan(z / Math.sqrt(x * x + y * y)) * 180 / Math.PI)
