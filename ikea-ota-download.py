#!/usr/bin/env python
"""
Snipped to download current IKEA ZLL OTA files into ~/otau
compatible with python 3.
"""

import os
import json
try:
	from urllib.request import urlopen, urlretrieve
except ImportError:
	from urllib2 import urlopen
	from urllib import urlretrieve


f = urlopen("http://fw.ota.homesmart.ikea.net/feed/version_info.json")
data = f.read()

arr = json.loads(data)

otapath = '%s/otau' % os.path.expanduser('~')

if not os.path.exists(otapath):
	os.makedirs(otapath)

for i in arr:
	if 'fw_binary_url' in i:
		url = i['fw_binary_url']
		ls = url.split('/')
		fname = ls[len(ls) - 1]
		path = '%s/%s' % (otapath, fname)

		if not os.path.isfile(path):
			urlretrieve(url, path)
			print(path)
		else:
		    print('%s already exists' % fname)




