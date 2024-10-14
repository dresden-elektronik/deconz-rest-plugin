/* global Attr, Item */
// Map battery voltage from 1.9V ... 3.0V to 0% ... 100%
Item.val = Math.max(0, Math.min(Math.round(9.09090909 * (Attr.val - 19)), 100))
