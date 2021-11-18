const value = Attr.val;
const x = value & 0xffff;
const y = (value >> 16) & 0xffff;
const z = (value >> 32) & 0xffff;
console.log('raw orientation: ', value, x, y, z);
const X = 0.0 + x;
const Y = 0.0 + y;
const Z = 0.0 + z;
const angleX = Math.round(Math.atan(X / Math.sqrt(Z * Z + Y * Y)) * 180 / Math.PI);
const angleY = Math.round(Math.atan(Y / Math.sqrt(X * X + Z * Z)) * 180 / Math.PI);
const angleZ = Math.round(Math.atan(Z / Math.sqrt(X * X + Y * Y)) * 180 / Math.PI);

R.item('state/orientation_x').val = angleX;
R.item('state/orientation_y').val = angleY;
R.item('state/orientation_z').val = angleZ;
