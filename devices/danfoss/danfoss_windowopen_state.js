switch (Attr.val) {
    case 0:
        Item.val = "quarantine";
        break;
    case 1:
        Item.val = "closed";
        break;
    case 2:
        Item.val = "hold";
        break;
    case 3:
        Item.val = "open";
        break;
    case 4:
        Item.val = "open (external), closed (internal)";
        break;
}