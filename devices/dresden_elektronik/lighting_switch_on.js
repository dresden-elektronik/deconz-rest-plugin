var i = R.item("state/buttonevent");
if (ZclFrame.cmd == 1) {
	i.val = (SrcEp == 1) ? 1002 : 3002;
} else if (ZclFrame.cmd == 0) {
	i.val = (SrcEp == 1) ? 2002 : 4002;
}
if (SrcEp == 2) {
	R.item("attr/mode").val = 2;
}
