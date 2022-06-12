#!/usr/bin/env python
"""
Snipped to download current IKEA ZLL OTA files into ~/otau
compatible with python 3.
"""

import logging
import os
import json
try:
	from urllib.request import urlopen, urlretrieve
except ImportError:
	from urllib2 import urlopen
	from urllib import urlretrieve

@service
def update_ikea_zll_ota_files():
	f = await hass.async_add_executor_job(urlopen,"http://fw.ota.homesmart.ikea.net/feed/version_info.json")
	data = f.read()

	arr = json.loads(data.decode('utf-8'))

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
				await hass.async_add_executor_job(urlretrieve, url, path)
				logging.info("updated: " + path)
			else:
			    logging.info('already had file: ' + fname)




