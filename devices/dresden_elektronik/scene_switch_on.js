var i = R.item("state/buttonevent");
if (ZclFrame.cmd == 1) {
	i.val = 1002;
} else if (ZclFrame.cmd == 0) {
	i.val = 2002;
}
