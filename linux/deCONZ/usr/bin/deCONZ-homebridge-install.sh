#!/bin/bash

TIMEOUT=0
LOG_LEVEL=3
ZLLDB=""
OWN_PID=$$
MAINUSER=$(getent passwd 1000 | cut -d: -f1)
DECONZ_CONF_DIR="/home/$MAINUSER/.local/share"
DECONZ_PORT=

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

# $1 = key $2 = value
function putHomebridgeUpdated {
	curl --noproxy '*' -s -o /dev/null -d "{\"$1\":\"$2\"}" -X PUT http://127.0.0.1:${DECONZ_PORT}/api/$OWN_PID/config/homebridge/updated
}

function init {
    # is sqlite3 installed?
	sqlite3 --version &> /dev/null
	if [ $? -ne 0 ]; then
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
		return
	fi

	# get deCONZ REST-API port
	local value=$(sqlite3 $ZLLDB "select value from config2 where key=\"port\"")
	if [ $? -ne 0 ]; then
		return
	fi

	if [[ -n $value ]]; then
		DECONZ_PORT=$value
	fi
}

function installHomebridge {

	local PROXY_ADDRESS=""
	local PROXY_PORT=""

	## get database config
	params=( [0]="proxyaddress" [1]="proxyport" )
	values=()

	for i in {0..1}; do
		param=${params[$i]}
		RC=1
		while [ $RC -ne 0 ]; do
			value=$(sqlite3 $ZLLDB "select value from config2 where key=\"${param}\"")
			RC=$?
			if [ $RC -ne 0 ]; then
				[[ $LOG_DEBUG ]] && echo "${LOG_DEBUG}Error reading parameter ${param} from db"
				sleep 2
			fi
		done

		# basic check for non empty
		if [[ ! -z "$value" ]]; then
			values[$i]=$(echo $value | cut -d'|' -f2)
		fi
	done

	PROXY_ADDRESS="${values[0]}"
	PROXY_PORT="${values[1]}"

	## check installed components
	hb_installed=false
	hb_hue_installed=false
	node_installed=false
	npm_installed=false
	node_ver=""

	which homebridge &> /dev/null
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

	which nodejs &> /dev/null
	if [ $? -eq 0 ]; then
		node_installed=true
		# get version and strip it to only major part: v8.11.2 -> 8
		node_ver=$(nodejs --version | cut -d. -f1 | cut -c 2-)
		[[ $LOG_DEBUG ]] && echo "${LOG_DEBUG} nodejs ver. $node_ver"
	fi

	if [[ $hb_installed = false || $hb_hue_installed = false || $node_installed = false ]]; then
		[[ $LOG_INFO ]] && echo "${LOG_INFO}check inet connectivity"

		putHomebridgeUpdated "homebridge" "installing"

		curl --head --connect-timeout 20 -k https://www.phoscon.de &> /dev/null
		if [ $? -ne 0 ]; then
			if [[ ! -z "$PROXY_ADDRESS" && ! -z "$PROXY_PORT" && "$PROXY_ADDRESS" != "none" ]]; then
				export http_proxy="http://${PROXY_ADDRESS}:${PROXY_PORT}"
				export https_proxy="http://${PROXY_ADDRESS}:${PROXY_PORT}"
				[[ $LOG_DEBUG ]] && echo "${LOG_DEBUG}set proxy: ${PROXY_ADDRESS}:${PROXY_PORT}"
			else
				[[ $LOG_WARN ]] && echo "${LOG_WARN}no internet connection. Abort homebridge installation."
				putHomebridgeUpdated "homebridge" "install-error"
				return
			fi
		fi

		# install nodejs if not installed or if version < 10
		if [[ $node_installed = false ]]; then
		# example for getting it worked on rpi1 and 0
		# if armv6
			#wget node 8.12.0
			#cp -R to /usr/local
			#PATH=usr/local/bin/
			#link nodejs to node
		# else
			curl -sL https://deb.nodesource.com/setup_10.x | bash -
			if [ $? -eq 0 ]; then
				apt-get install -y nodejs
				if [ $? -ne 0 ]; then
					[[ $LOG_WARN ]] && echo "${LOG_WARN}could not install nodejs"
					putHomebridgeUpdated "homebridge" "install-error"
					return
				fi
			else
				[[ $LOG_WARN ]] && echo "${LOG_WARN}could not download node setup."
				putHomebridgeUpdated "homebridge" "install-error"
				return
			fi
		# fi
		else
		    if [ $node_ver -lt 10 ]; then
			    curl -sL https://deb.nodesource.com/setup_10.x | bash -
				if [ $? -eq 0 ]; then
					apt-get install -y nodejs
					if [ $? -ne 0 ]; then
						[[ $LOG_WARN ]] && echo "${LOG_WARN}could not install nodejs"
							putHomebridgeUpdated "homebridge" "install-error"
						return
					fi
				else
					[[ $LOG_WARN ]] && echo "${LOG_WARN}could not download node setup."
					putHomebridgeUpdated "homebridge" "install-error"
					return
				fi
		    fi
		fi

		# install npm if not installed
		which npm &> /dev/null
		if [ $? -eq 0 ]; then
			npm_installed=true
		fi

		if [[ $npm_installed = false ]]; then
			apt-get install -y npm
			if [ $? -ne 0 ]; then
				[[ $LOG_WARN ]] && echo "${LOG_WARN}could not install npm"
				putHomebridgeUpdated "homebridge" "install-error"
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
					putHomebridgeUpdated "homebridge" "install-error"
					return
				fi
			else
				[[ $LOG_WARN ]] && echo "${LOG_WARN}could not update npm"
				putHomebridgeUpdated "homebridge" "install-error"
				return
			fi
		fi

		# install homebridge-hue if not installed
		if [[ $hb_hue_installed = false ]]; then
			npm -g install homebridge-hue
			if [ $? -ne 0 ]; then
				[[ $LOG_WARN ]] && echo "${LOG_WARN}could not install homebridge hue"
				putHomebridgeUpdated "homebridge" "install-error"
				return
			fi
		fi
	fi
}

function checkUpdate {

	[[ $LOG_INFO ]] && echo "${LOG_INFO}check for homebridge updates"
	hb_version=""
	hb_hue_version=""

	hb_version=$(homebridge --version)
	if [ -f "/usr/lib/node_modules/homebridge-hue/package.json" ]; then
		hb_hue_version=$(cat /usr/lib/node_modules/homebridge-hue/package.json | grep \"version\": | cut -d'"' -f 4)
		if [[ ! "$hb_hue_version" =~ "^[0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3}$" ]]
			hb_hue_version=""
		fi
	fi
	#TODO: check node vesion
	#TODO: check npm version

	latest_hb_version=$(npm show homebridge version)
	if [ $? -ne 0 ]; then
		[[ $LOG_WARN ]] && echo "${LOG_WARN}could not query latest homebridge version"
		return
	fi

	latest_hb_hue_version=$(npm show homebridge-hue version)
	if [ $? -ne 0 ]; then
		[[ $LOG_WARN ]] && echo "${LOG_WARN}could not query latest homebridge-hue version"
		return
	fi

	# update homebridge and npm
	if [[ "$hb_version" != "$latest_hb_version" ]]; then
		[[ $LOG_INFO ]] && echo "${LOG_INFO}installed homebridge version: $hb_version - latest: $latest_hb_version"
		[[ $LOG_INFO ]] && echo "${LOG_INFO}update homebridge"

		npm -g install npm@latest
		if [ $? -eq 0 ]; then
			npm -g install homebridge --unsafe-perm
			if [ $? -ne 0 ]; then
				[[ $LOG_WARN ]] && echo "${LOG_WARN}could not update homebridge"
			else
				putHomebridgeUpdated "homebridge" "updated"
			fi
		else
			[[ $LOG_WARN ]] && echo "${LOG_WARN}could not update npm"
		fi
	fi

	# update homebridge hue
	if [[ "$hb_hue_version" != "" && "$hb_hue_version" != "$latest_hb_hue_version" ]]; then
		[[ $LOG_INFO ]] && echo "${LOG_INFO}installed homebridge-hue version: $hb_version - latest: $latest_hb_version"
		[[ $LOG_INFO ]] && echo "${LOG_INFO}update homebridge-hue"

		npm -g install homebridge-hue
		if [ $? -ne 0 ]; then
			[[ $LOG_WARN ]] && echo "${LOG_WARN}could not update homebridge hue"
		else
			putHomebridgeUpdated "homebridge" "updated"
		fi
	fi
}

# break loop on SIGUSR1
trap 'TIMEOUT=0' SIGUSR1

COUNTER=0

while [ 1 ]
do
	if [[ -z "$ZLLDB" ]] || [[ ! -f "$ZLLDB" ]] || [[ -z "$DECONZ_PORT" ]]; then
		init
	fi

	while [[ $TIMEOUT -gt 0 ]]
	do
		sleep 1
		TIMEOUT=$((TIMEOUT - 1))
	done

	TIMEOUT=60

	[[ -z "$ZLLDB" ]] && continue
	[[ ! -f "$ZLLDB" ]] && continue
	[[ -z "$DECONZ_PORT" ]] && continue

    installHomebridge

    COUNTER=$((COUNTER + 1))
	if [ $COUNTER -ge 60 ]; then
		# check for updates every hour
		COUNTER=0
		checkUpdate
	fi
done
