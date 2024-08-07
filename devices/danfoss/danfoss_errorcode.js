var error = [];

if (Attr.val == 0)
{
    error.push("none");
}
else
{
    for (i = 0; i < 15; i++)
    {
        if (Attr.val >> i & 0x01)
        {
            error.push("E" + (i + 1));
        }
    }
}

var res = error.join(",");
Item.val = res.toString(16);
