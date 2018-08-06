#!/bin/bash

ZLLDB=""
MAINUSER=$(getent passwd 1000 | cut -d: -f1)
DECONZ_CONF_DIR="/home/$MAINUSER/.local/share"
DECONZ_PORT=

TIMEOUT=0      # main loop iteration timeout, can be controlled by SIGUSR1
LOG_LEVEL=4

LOG_EMERG=
LOG_ALERT=
LOG_CRIT=
LOG_ERROR=
LOG_WARN=
LOG_NOTICE=
LOG_INFO=
LOG_DEBUG=

[[ $LOG_LEVEL -ge 0 ]] && LOG_EMERG="<0>"
[[ $LOG_LEVEL -ge 1 ]] && LOG_ALERT="<1>"
[[ $LOG_LEVEL -ge 2 ]] && LOG_CRIT="<2>"
[[ $LOG_LEVEL -ge 3 ]] && LOG_ERROR="<3>"
[[ $LOG_LEVEL -ge 4 ]] && LOG_WARN="<4>"
[[ $LOG_LEVEL -ge 5 ]] && LOG_NOTICE="<5>"
[[ $LOG_LEVEL -ge 6 ]] && LOG_INFO="<6>"
[[ $LOG_LEVEL -ge 7 ]] && LOG_DEBUG="<7>"

function init {
    # is sqlite3 installed?
	sqlite3 --version &> /dev/null
	if [ $? -ne 0 ]; then
		[[ $LOG_WARN ]] && echo "${LOG_WARN}sqlite3 not installed"
		return
	fi

	# look for latest config in specific order
	drs=("data/dresden-elektronik/deCONZ/zll.db" "dresden-elektronik/deCONZ/zll.db" "deCONZ/zll.db")
	for i in "${drs[@]}"; do
		if [ -f "${DECONZ_CONF_DIR}/$i" ]; then
			ZLLDB="${DECONZ_CONF_DIR}/$i"
			break
		fi
	done

	if [ ! -f "$ZLLDB" ]; then
		# might have been deleted
		ZLLDB=""
	fi

	if [[ -z "$ZLLDB" ]]; then
		[[ $LOG_DEBUG ]] && echo "${LOG_DEBUG}no database file (zll.db) found"
		return
	fi
}

function addUser() {
	local json="{\"devicetype\":\"homebridge-hue#deconz\"}"
	curl --noproxy '*' -s -o /dev/null -d "$json" -X POST http://127.0.0.1:${DECONZ_PORT}/api
}

function checkHomebridge {

	local PROXY_ADDRESS=""
	local PROXY_PORT=""
	local BRIDGEID=""
	local HOMEBRIDGE=""
	local IP_ADDRESS=""

	## get database config
	params=( [0]="bridgeid" [1]="homebridge" [2]="proxyaddress" [3]="proxyport" [4]="port" [5]="ipaddress")
	values=()

	for i in {0..5}; do
		param=${params[$i]}
		value=$(sqlite3 $ZLLDB "select * from config2 where key=\"${param}\"")
		if [ $? -ne 0 ]; then
			return
		fi

		value=$(echo $value | cut -d'|' -f2)

		# basic check for non empty
		if [[ ! -z "$value" ]]; then
			values[$i]=$(echo $value | cut -d'|' -f2)
		fi
	done

	## all parameters found and valid?
	if [ -z "${values[0]}" ]; then
		[[ $LOG_DEBUG ]] && echo "${LOG_DEBUG}missing parameter 'bridgeid'"
		return
	fi

	if [ -z "${values[4]}" ]; then
		[[ $LOG_DEBUG ]] && echo "${LOG_DEBUG}missing parameter 'port'"
		return
	fi

	if [ -z "${values[5]}" ]; then
		[[ $LOG_DEBUG ]] && echo "${LOG_DEBUG}missing parameter 'ipaddress'"
		return
	fi

	BRIDGEID="${values[0]}"
	HOMEBRIDGE="${values[1]}" # disabled | managed | not-managed
	PROXY_ADDRESS="${values[2]}"
	PROXY_PORT="${values[3]}"
	DECONZ_PORT="${values[4]}"
	IP_ADDRESS="${values[5]}"

	if [[ "$HOMEBRIDGE" == "disabled" ]]; then
		systemctl -q is-active homebridge
		if [ $? -eq 0 ]; then
			systemctl stop homebridge
			systemctl disable homebridge
		fi
		pkill homebridge
		return
	fi

	## check if apikey already exist or create a new apikey for homebridge apps
	local HOMEBRIDGE_AUTH=$(sqlite3 $ZLLDB "select * from auth where devicetype like 'homebridge-hue#%' limit 1")
	if [[ $HOMEBRIDGE_AUTH == "" ]]; then
		# generate a new deconz apikey for homebridge-hue
		addUser
	else
		# homebridge-hue apikey exists
		if [ -z $(echo $HOMEBRIDGE_AUTH | grep deconz) ]; then
			if [[ "$HOMEBRIDGE" != "not-managed" ]]; then
				sqlite3 $ZLLDB "replace into config2 (key, value) values('homebridge', 'not-managed')" &> /dev/null
			fi
			[[ $LOG_INFO ]] && echo "${LOG_INFO}existing homebridge hue auth found"
		fi
	fi

	## check installed components
	hb_installed=false
	hb_hue_installed=false
	node_installed=false
	npm_installed=false
	node_ver=""

	which homebridge
	if [ $? -eq 0 ]; then
		hb_installed=true
		# look for homebridge-hue installation
		dirs=("/usr/local/lib/node_modules/" "/usr/lib/node_modules/")
		for i in "${dirs[@]}"; do
			if [ -d "$i/homebridge-hue" ]; then
				# homebridge-hue installation found
				hb_hue_installed=true
			fi
		done
	fi

	which nodejs
	if [ $? -eq 0 ]; then
		node_installed=true
		# get version and strip it to only major part: v8.11.2 -> 8
		node_ver=$(nodejs --version | cut -d. -f1 | cut -c 2-)
		echo $node_ver
	fi

	if [[ $hb_installed = false || $hb_hue_installed = false || $node_installed = false ]]; then
		[[ $LOG_INFO ]] && echo "${LOG_INFO}check inet connectivity"

		curl --head --connect-timeout 20 -k https://www.phoscon.de &> /dev/null
		if [ $? -ne 0 ]; then
			if [[ ! -z "$PROXY_ADDRESS" && ! -z "$PROXY_PORT" && "$PROXY_ADDRESS" != "none" ]]; then
				export http_proxy="http://${PROXY_ADDRESS}:${PROXY_PORT}"
				export https_proxy="http://${PROXY_ADDRESS}:${PROXY_PORT}"
				[[ $LOG_DEBUG ]] && echo "${LOG_DEBUG}set proxy: ${PROXY_ADDRESS}:${PROXY_PORT}"
			else
				[[ $LOG_WARN ]] && echo "${LOG_WARN}no internet connection. Abort homebridge installation."
				return
			fi
		fi

		# install nodejs if not installed or if version < 8
		if [[ $node_installed = false ]]; then
			curl -sL https://deb.nodesource.com/setup_8.x | bash -
			if [ $? -eq 0 ]; then
				apt-get install -y nodejs
				if [ $? -ne 0 ]; then
					[[ $LOG_WARN ]] && echo "${LOG_WARN}could not install nodejs"
					return
				fi
			else
				[[ $LOG_WARN ]] && echo "${LOG_WARN}could not download node setup."
				return
			fi
		else
		    if [ $node_ver -lt 8 ]; then
			    curl -sL https://deb.nodesource.com/setup_8.x | bash -
				if [ $? -eq 0 ]; then
					apt-get install -y nodejs
					if [ $? -ne 0 ]; then
						[[ $LOG_WARN ]] && echo "${LOG_WARN}could not install nodejs"
						return
					fi
				else
					[[ $LOG_WARN ]] && echo "${LOG_WARN}could not download node setup."
					return
				fi
		    fi
		fi

		# install npm if not installed
		which npm
		if [ $? -eq 0 ]; then
			npm_installed=true
		fi

		if [[ $npm_installed = false ]]; then
			apt-get install -y npm
			if [ $? -ne 0 ]; then
				[[ $LOG_WARN ]] && echo "${LOG_WARN}could not install npm"
				return
			fi
		fi

		# install homebridge if not installed
		if [[ $hb_installed = false ]]; then
			npm -g install npm@latest
			if [ $? -eq 0 ]; then
				npm -g install homebridge --unsafe-perm
				if [ $? -ne 0 ]; then
					[[ $LOG_WARN ]] && echo "${LOG_WARN}could not install homebridge"
					return
				fi
			else
				[[ $LOG_WARN ]] && echo "${LOG_WARN}could not update npm"
				return
			fi
		fi

		# install homebridge-hue if not installed
		if [[ $hb_hue_installed = false ]]; then
			npm -g install homebridge-hue
			if [ $? -ne 0 ]; then
				[[ $LOG_WARN ]] && echo "${LOG_WARN}could not install homebridge hue"
				return
			fi
		fi
	fi

	# create config file if not exists
	if [[ -f /home/$MAINUSER/.homebridge/config.json ]]; then
		[[ $LOG_DEBUG ]] && echo "${LOG_DEBUG}found existing homebridge config.json"
		if [ -z $(cat /home/$MAINUSER/.homebridge/config.json | grep "Phoscon Homebridge") ]; then
			# set to not-managed only if homebridge is not set up by phoscon
			if [[ "$HOMEBRIDGE" != "not-managed" ]]; then
				sqlite3 $ZLLDB "replace into config2 (key, value) values('homebridge', 'not-managed')" &> /dev/null
			fi
		fi
	else
		APIKEY=$(sqlite3 $ZLLDB "select apikey from auth where devicetype like 'homebridge-hue#%'")
		if [ -z $APIKEY ]; then
			[[ $LOG_DEBUG ]] && echo "${LOG_WARN}could not read api key from db"
			TIMEOUT=10
			return
		fi
		# create homebridge dir and add Mainuser ownership
		mkdir /home/$MAINUSER/.homebridge
		#mkdir /home/$MAINUSER/.homebridge/persist
		#mkdir /home/$MAINUSER/.homebridge/accessories
		touch /home/$MAINUSER/.homebridge/config.json
		chown -R $MAINUSER /home/$MAINUSER/.homebridge

		echo "{
\"bridge\": {
    \"name\": \"Phoscon Homebridge\",
    \"username\": \"$(echo ${BRIDGEID:4} | fold -w2 | paste -sd':' -)\",
    \"port\": 51826,
    \"pin\": \"111-11-111\"
},

\"platforms\": [
{
  \"platform\": \"Hue\",
  \"host\": \"127.0.0.1\",
  \"users\": {
    \"${BRIDGEID}\": \"${APIKEY}\"
  },
  \"sensors\": true,
  \"nativeHomeKitSensors\": false,
  \"excludeSensorTypes\": [\"CLIPPresence\", \"Geofence\"],
  \"lights\": true,
  \"hueMotionTemperatureHistory\": true
}
]
}" > /home/$MAINUSER/.homebridge/config.json
	fi

	chown $MAINUSER /home/$MAINUSER/.homebridge/config.json

	## start homebridge
	systemctl -q is-active homebridge
	if [ $? -eq 0 ]; then
		[[ $LOG_INFO ]] && echo "${LOG_INFO}another homebridge service is already running"
		return
	fi

	if [ -z $(ps -a | grep homebridge) ]; then
		[[ $LOG_INFO ]] && echo "${LOG_INFO}starting  homebridge"
		if [[ "$HOMEBRIDGE" != "managed" ]]; then
			sqlite3 $ZLLDB "replace into config2 (key, value) values('homebridge', 'managed')" &> /dev/null
		fi
		homebridge -U /home/$MAINUSER/.homebridge
	else
		#[[ $LOG_INFO ]] && echo "${LOG_INFO}homebridge is already running"
		#return
		pkill homebridge
		homebridge -U /home/$MAINUSER/.homebridge
	fi
}

# break loop on SIGUSR1
trap 'TIMEOUT=0' SIGUSR1

while [ 1 ]
do
	if [[ -z "$ZLLDB" ]] || [[ ! -f "$ZLLDB" ]]; then
		init
	fi

	while [[ $TIMEOUT -gt 0 ]]
	do
		sleep 1
		TIMEOUT=$((TIMEOUT - 1))
	done

	TIMEOUT=30

	[[ -z "$ZLLDB" ]] && continue
	[[ ! -f "$ZLLDB" ]] && continue

    checkHomebridge

done
