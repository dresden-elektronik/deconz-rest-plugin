const values = ["not fully locked","locked","unlocked","undefined"];
if (Attr.val >= 0 && Attr.val < 4)
  Item.val = values[Attr.val];
