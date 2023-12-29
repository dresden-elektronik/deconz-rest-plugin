var i = R.item("state/buttonevent");
var scene = ZclFrame.at(2);
if (ZclFrame.cmd == 5) {
	i.val = ((scene + 2) * 1000) + 2;
} else if (ZclFrame.cmd == 4) {
	i.val = ((scene + 2) * 1000) + 3;
}
