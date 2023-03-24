const tholddark = R.item('config/tholddark').val;
const tholdoffset = R.item('config/tholdoffset').val;
const lux = Attr.val;
var ll = 0;

if (lux > 0 && lux < 0xffff) {
    ll = Math.round(10000 * Math.log10(lux) + 1);
}

R.item('state/lightlevel').val = ll;
R.item('state/dark').val = ll <= tholddark;
R.item('state/daylight').val = ll >= tholddark + tholdoffset;

Item.val = lux;
