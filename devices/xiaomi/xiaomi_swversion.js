const v = (Attr.val & 0x00ff);
let s = "0.0.0_";
for (i = 0; i < (v.toString().length % 4); i++)
{
    s += "0";
}
Item.val = s + v;