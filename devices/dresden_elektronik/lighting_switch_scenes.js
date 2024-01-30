var i = R.item("state/buttonevent");
if (ZclFrame.cmd == 5) {
	if (ZclFrame.at(2) == 1)
		i.val = 3002;
	else
		i.val = 4002;
} else if (ZclFrame.cmd == 4) {
		if (ZclFrame.at(2) == 1)
		i.val = 3003;
	else
		i.val = 4003;
}
R.item("attr/mode").val = 1;