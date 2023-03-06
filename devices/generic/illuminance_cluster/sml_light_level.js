const tholddark = R.item('config/tholddark').val;
const tholdoffset = R.item('config/tholdoffset').val;
const measuredValue = Attr.val;

R.item('state/dark').val = measuredValue <= tholddark;
R.item('state/daylight').val = measuredValue >= tholddark + tholdoffset;
if (measuredValue >= 0 && measuredValue < 0xffff) {
	const exp = measuredValue - 1;
	const l = Math.pow(10, exp / 10000.0);
	R.item('state/lux').val = Math.floor(l);
}
Item.val = measuredValue;
