#!/bin/bash

curl 'http://fw.ota.homesmart.ikea.net/feed/version_info.json' | jq -r '[.[] | .fw_binary_url] | .[]'  > $HOME/files.txt

cd $HOME/otau && xargs -n 1 curl -O < $HOME/files.txt

rm $HOME/files.txt