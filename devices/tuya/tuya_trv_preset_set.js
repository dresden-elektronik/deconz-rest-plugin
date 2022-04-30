const modes = ["holiday","auto","manual","comfort","eco","boost","complex"];
v = modes.indexOf(Attr.val);
if (v >= 0) { Item.val = modes.indexOf(Attr.val); }