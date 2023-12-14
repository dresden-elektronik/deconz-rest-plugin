/* global Attr, Item, R */

// Translate VOC index to tVOC density.
const table = [
  [0, 50, 0, 220, 'good'],
  [51, 100, 221, 660, 'moderate'],
  [101, 150, 661, 1430, 'high'],
  [151, 200, 1431, 2200, 'very high'],
  [201, 300, 2201, 3300, 'very high'],
  [301, 500, 3301, 5500, 'very high']
]

for (let i = 0; i < table.length; i++) {
  const entry = table[i]
  if (Attr.val <= entry[1]) {
    const aq = Math.round((Attr.val - entry[0]) / (entry[1] - entry[0]) * (entry[3] - entry[2]) + entry[2])
    Item.val = aq
    R.item('state/airqualityppb').val = aq // deprecated
    // R.item('state/airquality').val = entry[4] // use standard (?) num2str instead
    break
  }
}
