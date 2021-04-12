#!/bin/bash

ZLLDB=""
SQL_RESULT=
MAINUSER=$(getent passwd 1000 | cut -d: -f1)
OWN_PID=$$
DECONZ_CONF_DIR="/home/$MAINUSER/.local/share"
DECONZ_DATA_DIR=""
DECONZ_PORT=
BRIDGEID=
LAST_MAX_TIMESTAMP=0
TIMEOUT=0      # main loop iteration timeout, can be controlled by SIGUSR1
LOG_LEVEL=3

LOG_EMERG=
LOG_ALERT=
LOG_CRIT=
LOG_ERROR=
LOG_WARN=
LOG_NOTICE=
LOG_INFO=
LOG_DEBUG=
LOG_SQL=

[[ $LOG_LEVEL -ge 0 ]] && LOG_EMERG="<0>"
[[ $LOG_LEVEL -ge 1 ]] && LOG_ALERT="<1>"
[[ $LOG_LEVEL -ge 2 ]] && LOG_CRIT="<2>"
[[ $LOG_LEVEL -ge 3 ]] && LOG_ERROR="<3>"
[[ $LOG_LEVEL -ge 4 ]] && LOG_WARN="<4>"
[[ $LOG_LEVEL -ge 5 ]] && LOG_NOTICE="<5>"
[[ $LOG_LEVEL -ge 6 ]] && LOG_INFO="<6>"
[[ $LOG_LEVEL -ge 7 ]] && LOG_DEBUG="<7>"
[[ $LOG_LEVEL -ge 8 ]] && LOG_SQL="<8>"

# $1 = queryString
function sqliteSelect() {
    if [[ -z "$ZLLDB" ]] || [[ ! -f "$ZLLDB" ]]; then
        [[ $LOG_DEBUG ]] && echo "${LOG_DEBUG}database file not found"
        ZLLDB=""
        return
    fi
    [[ $LOG_SQL ]] && echo "SQLITE3 $1"

    SQL_RESULT=$(sqlite3 $ZLLDB "$1")
    if [ $? -ne 0 ]; then
    	SQL_RESULT="error"
	fi
    [[ $LOG_SQL ]] && echo "$SQL_RESULT"
}

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
			DECONZ_DATA_DIR="/home/$MAINUSER/.local/share/${i::-7}"
            [[ $LOG_DEBUG ]] && echo "${LOG_DEBUG}deconz data dir: $DECONZ_DATA_DIR"
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

	# get deCONZ REST-API port
	sqliteSelect "select value from config2 where key=\"port\""
	local value="$SQL_RESULT"
	if [ -z "$value" ] || [[ "$value" == "error" ]]; then
		[[ $LOG_DEBUG ]] && echo "${LOG_DEBUG}no deCONZ port found in database"
		return
	fi

	if [[ -n "$value" ]]; then
		DECONZ_PORT=$value
		[[ $LOG_INFO ]] && echo "${LOG_INFO}use deCONZ port $DECONZ_PORT"
	fi

	# get bridgeid
	sqliteSelect "select value from config2 where key=\"bridgeid\""
	local value="$SQL_RESULT"
	if [ -z "$value" ] || [[ "$value" == "error" ]]; then
		[[ $LOG_DEBUG ]] && echo "${LOG_DEBUG}no bridgeid found in database"
		return
	fi

	if [[ -n "$value" ]]; then
		if [[ ! "$value" == 00212E* ]]; then
			[[ $LOG_DEBUG ]] && echo "${LOG_DEBUG}no valid bridge id"
		    return
		fi
		BRIDGEID=$value
		[[ $LOG_INFO ]] && echo "${LOG_INFO}use bridgeid $BRIDGEID"
	fi
}

function addUser() {
	local json="{\"devicetype\":\"homebridge-hue#deconz\"}"
	curl --noproxy '*' -s -o /dev/null -d "$json" -X POST http://127.0.0.1:${DECONZ_PORT}/api
}

# $1 = key, $2 = value
function putHomebridgeUpdated {
	curl --noproxy '*' -s -o /dev/null -d "{\"$1\":\"$2\"}" -X PUT http://127.0.0.1:${DECONZ_PORT}/api/$OWN_PID/config/homebridge/updated
}

function checkNewDevices() {
	local max_timestamp=
	local proceed=false
	local changed=false

	while [[ $proceed == false ]]
	do
		sqliteSelect "select timestamp from devices order by timestamp DESC limit 1"
		max_timestamp="$SQL_RESULT"

		if [ -z "$max_timestamp" ] || [[ "$max_timestamp" == "error" ]]; then
			[[ $LOG_DEBUG ]] && echo "${LOG_DEBUG}timestamp from db is empty - skip check for new devices"
			return
		fi
		if [ $LAST_MAX_TIMESTAMP -eq 0 ]; then
			# skip first run
			LAST_MAX_TIMESTAMP=$max_timestamp
			[[ $LOG_DEBUG ]] && echo "${LOG_DEBUG}set last max timestamp to $max_timestamp - skip check"
			return
		fi
		if [[ "$max_timestamp" -ne "$LAST_MAX_TIMESTAMP" ]]; then
			# there is a new light discovered by deCONZ
			[[ $LOG_INFO ]] && echo "${LOG_INFO} new light discovered by deCONZ - wait 30 sec for more lights"
			if [ -n "$max_timestamp" ]; then
				LAST_MAX_TIMESTAMP=$max_timestamp
				changed=true
			fi
			sleep 30
		else
			proceed=true
		fi
	done

	if [[ $changed == true ]]; then
		[[ $LOG_INFO ]] && echo "${LOG_INFO} all new lights discovered - restarting homebridge"
		pkill homebridge
		homebridge -U /home/$MAINUSER/.homebridge &
	fi
}

function checkHomebridge {

	local HOMEBRIDGE=""
	local IP_ADDRESS=""
	local HOMEBRIDGE_PIN=""
	local hb_hue_version=$(npm list -g homebridge-hue | grep homebridge-hue | cut -d@ -f2 | xargs)

	#hostline used in config
	local hostline="\"hosts\": [\"127.0.0.1\"],"
	if [ $(echo "$hb_hue_version" | cut -d'.' -f 2) -le 13 ]; then
		if [ $(echo "$hb_hue_version" | cut -d'.' -f 3) -lt 2 ]; then
			hostline="\"host\": \"127.0.0.1\","
		fi
	fi

	## get database config
	params=( [0]="homebridge" [1]="ipaddress" [2]="homebridge-pin")
	values=()

	for i in {0..2}; do
		param=${params[$i]}

		sqliteSelect "select value from config2 where key=\"${param}\""
		value="$SQL_RESULT"			

		# basic check for non empty
		if [[ ! -z "$value" ]]; then
			values[$i]=$(echo $value | cut -d'|' -f2)
		fi
	done

	# any database errors?
	if [[ "${values[0]}" == "error" ]] || [[ "${values[1]}" == "error" ]] || [[ "${values[2]}" == "error" ]]; then
		TIMEOUT=10
		return
	fi

	## all parameters found and valid?
	if [ -z "${values[0]}" ]; then
		[[ $LOG_DEBUG ]] && echo "${LOG_DEBUG}missing parameter 'homebridge'"
		return
	fi
	if [ -z "${values[1]}" ]; then
		[[ $LOG_DEBUG ]] && echo "${LOG_DEBUG}missing parameter 'ipaddress'"
		return
	fi

	HOMEBRIDGE="${values[0]}" # disabled | managed | not-managed | reset
	IP_ADDRESS="${values[1]}"
	HOMEBRIDGE_PIN="${values[2]}"

	if [[ "$HOMEBRIDGE" == "disabled" ]]; then
		systemctl -q is-active homebridge
		if [ $? -eq 0 ]; then
			systemctl stop homebridge
			systemctl disable homebridge
		fi
		systemctl -q is-active deconz-homebridge-install
		if [ $? -eq 0 ]; then
			systemctl stop deconz-homebridge-install
		fi
		pkill homebridge
		return
	fi

	if [[ "$HOMEBRIDGE" == "reset" ]]; then
		systemctl -q is-active homebridge
		if [ $? -eq 0 ]; then
			systemctl stop homebridge
			systemctl disable homebridge
		fi
		pkill homebridge
		sleep 5
		rm -rf /home/$MAINUSER/.homebridge/persist
		rm -rf /home/$MAINUSER/.homebridge/config.json
	fi

	if [[ "$HOMEBRIDGE" == "updated" ]]; then
		pkill homebridge
		sleep 5
	fi

	## check if apikey already exist or create a new apikey for homebridge apps
	sqliteSelect "select * from auth where devicetype like 'homebridge-hue#%' limit 1"
	
	if [[ "$SQL_RESULT" == "error" ]]; then
		TIMEOUT=5
		return
	fi

	local HOMEBRIDGE_AUTH="$SQL_RESULT"

	if [[ -z "$HOMEBRIDGE_AUTH" ]]; then
		# generate a new deconz apikey for homebridge-hue
		addUser
		if [[ "$HOMEBRIDGE" != "managed" ]]; then
			putHomebridgeUpdated "homebridge" "managed"
			HOMEBRIDGE="managed"
		fi
	else
		# homebridge-hue apikey exists
		if [ -z $(echo $HOMEBRIDGE_AUTH | grep deconz) ]; then
			# homebridge-hue apikey is not made by this script
			if [[ "$HOMEBRIDGE" != "not-managed" ]]; then
				putHomebridgeUpdated "homebridge" "not-managed"
				HOMEBRIDGE="not-managed"
			fi
			[[ $LOG_INFO ]] && echo "${LOG_INFO}existing homebridge hue auth found"
		else
			if [[ "$HOMEBRIDGE" != "managed" && "$HOMEBRIDGE" != "installing" && "$HOMEBRIDGE" != "install-error" ]]; then
				putHomebridgeUpdated "homebridge" "managed"
				HOMEBRIDGE="managed"
			fi
        fi
	fi
	if [[ -z "$HOMEBRIDGE_PIN" ]]; then
		if [[ -f /home/$MAINUSER/.homebridge/config.json ]]; then
			local p=$(cat /home/$MAINUSER/.homebridge/config.json | grep "pin" | cut -d'"' -f4)
			local pin="${p:0:3}${p:4:2}${p:7:3}"
			# write pin from config.json in db
			putHomebridgeUpdated "homebridgepin" "$pin"
		else
			# or create new pin and write it in db
			local pin=$((1000 + RANDOM % 8999))$((1000 + RANDOM % 8999))
			putHomebridgeUpdated "homebridgepin" "$pin"
		fi
	fi

	if [[ "$HOMEBRIDGE" != "disabled" && "$HOMEBRIDGE" != "not-managed" ]]; then
		systemctl -q is-active deconz-homebridge-install
		if [ $? -ne 0 ]; then
			systemctl restart deconz-homebridge-install
		fi
	fi

	## check installed components
	hb_installed=false
	hb_hue_installed=false
	node_installed=false
	npm_installed=false
	node_ver=""

	which npm &> /dev/null
	if [ $? -eq 0 ]; then
		npm_installed=true
	fi

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

	if [[ $hb_installed = false || $hb_hue_installed = false || $node_installed = false || $npm_installed = false ]]; then
		[[ $LOG_DEBUG ]] && echo "${LOG_DEBUG}Not everything installed yet. Waiting..."
		return
	fi

	sleep 5

	sqliteSelect "select apikey from auth where devicetype like 'homebridge-hue#%' limit 1"

	if [[ "$SQL_RESULT" == "error" ]]; then
		TIMEOUT=5
		return
	fi

	APIKEY="$SQL_RESULT"

	# if config file exists check if parameters are still valid
	# to prevent this skript from overwrite config file set name parameter to something different then Phoscon Homebridge
	# else create config file if not exists
	if [[ -f /home/$MAINUSER/.homebridge/config.json ]]; then
		# existing config found
		[[ $LOG_DEBUG ]] && echo "${LOG_DEBUG}found existing homebridge config.json"
		if [ -z "$(cat /home/$MAINUSER/.homebridge/config.json | grep "Phoscon Homebridge")" ]; then
			# set to not-managed only if homebridge is not set up by phoscon
			if [[ "$HOMEBRIDGE" != "not-managed" ]]; then
				putHomebridgeUpdated "homebridge" "not-managed"
			fi
		else
			local updated=false
			# config created by deCONZ - check if bridgeid is present
			if [ -z "$(cat /home/$MAINUSER/.homebridge/config.json | grep "$BRIDGEID")" ]; then
				# bridgeid not found - update config
				[[ $LOG_DEBUG ]] && echo "${LOG_DEBUG}update bridgeid in config file with $BRIDGEID"
				local line="$(grep -n users /home/$MAINUSER/.homebridge/config.json | cut -d: -f 1)"
				line=$((line + 1))
				sed -i "${line}s/.*/    \"${BRIDGEID}\": \"$APIKEY\"/" /home/$MAINUSER/.homebridge/config.json
				updated=true
			fi
			# check if apikey is still correct
			if [ -z "$(cat /home/$MAINUSER/.homebridge/config.json | grep "$APIKEY")" ]; then
				# apikey not found - update config
				[[ $LOG_DEBUG ]] && echo "${LOG_DEBUG}update apikey in config file with $APIKEY"
				sed -i "/\"${BRIDGEID}\":/c\    \"${BRIDGEID}\": \"$APIKEY\"" /home/$MAINUSER/.homebridge/config.json
				updated=true
			fi
			# check if bridgeid is correct
			if [ -n "$(cat /home/$MAINUSER/.homebridge/config.json | grep "00000000")" ]; then
				# bridgeid has faulty value replace it with correct value from db
				[[ $LOG_DEBUG ]] && echo "${LOG_DEBUG}update bridgeid in config file with ${BRIDGEID}"
				sed -i "/\"00000000/c\    \"${BRIDGEID}\": \"$APIKEY\"" /home/$MAINUSER/.homebridge/config.json
				updated=true
			fi
			# check if pin is still correct
			local HB_PIN="${HOMEBRIDGE_PIN:0:3}-${HOMEBRIDGE_PIN:3:2}-${HOMEBRIDGE_PIN:5:3}"
			if [ -z "$(cat /home/$MAINUSER/.homebridge/config.json | grep "$HB_PIN")" ]; then
				# pin not found - update config
				[[ $LOG_DEBUG ]] && echo "${LOG_DEBUG}update pin in config file with $HOMEBRIDGE_PIN"
				sed -i "/\"pin\":/c\    \"pin\": \"${HB_PIN}\"" /home/$MAINUSER/.homebridge/config.json
				updated=true
			fi
			# check if hostline is still correct
			local hostline2="\"host\": \"127.0.0.1\""
			if [[ $hostline == "\"hosts\": [\"127.0.0.1\"]," ]]; then
                hostline2="\"hosts\": \[\"127.0.0.1\"\],"
            fi
            # hostline2 only needed for grep
			if [ -z "$(cat /home/$MAINUSER/.homebridge/config.json | grep "$hostline2")" ]; then
				# hostline is wrong format for this hb hue version
				[[ $LOG_DEBUG ]] && echo "${LOG_DEBUG}update hostline format in config file for this hb hue version"
				sed -i "/\"host/c\  $hostline" /home/$MAINUSER/.homebridge/config.json
				updated=true
			fi
			if [[ $updated = true ]]; then
				putHomebridgeUpdated "homebridge" "updated"
			else
				putHomebridgeUpdated "homebridge" "managed"
			fi
		fi
	else
		sqliteSelect "select value from config2 where key='homebridge-pin'"

		if [[ "$SQL_RESULT" == "error" ]]; then
			TIMEOUT=5
			return
		fi
	
		HOMEBRIDGE_PIN="$SQL_RESULT"

		if [ -z "$HOMEBRIDGE_PIN" ]; then
			[[ $LOG_DEBUG ]] && echo "${LOG_DEBUG}homebridge-pin is empty. Trying to get new one."
			TIMEOUT=2
			return
		fi

		# create homebridge dir and config and add Mainuser ownership
		mkdir /home/$MAINUSER/.homebridge
		touch /home/$MAINUSER/.homebridge/config.json
		chown -R $MAINUSER /home/$MAINUSER/.homebridge

		local HB_PIN="${HOMEBRIDGE_PIN:0:3}-${HOMEBRIDGE_PIN:3:2}-${HOMEBRIDGE_PIN:5:3}"
		echo "{
\"bridge\": {
    \"name\": \"Phoscon Homebridge\",
    \"username\": \"$(echo ${BRIDGEID:4} | fold -w2 | paste -sd':' -)\",
    \"port\": 51826,
    \"pin\": \"${HB_PIN}\"
},

\"platforms\": [
{
  \"platform\": \"Hue\",
  ${hostline}
  \"users\": {
    \"${BRIDGEID}\": \"${APIKEY}\"
  },
  \"sensors\": true,
  \"excludeSensorTypes\": [\"CLIP\", \"Geofence\"],
  \"lights\": true,
  \"hueMotionTemperatureHistory\": true
}
]
}" > /home/$MAINUSER/.homebridge/config.json

		chown $MAINUSER /home/$MAINUSER/.homebridge/config.json
	fi

	## check for backuped homebridge data before homebridge starts
	local restart=false
	for filename in $DECONZ_DATA_DIR/*; do
	    if [ -f "$filename" ]; then
            file="${filename##*/}"
            if [[ "$file" == AccessoryInfo* ]]; then
                [[ $LOG_DEBUG ]] && echo "${LOG_DEBUG}found accessoryInfo - copy it to homebridge persist dir"
                mkdir -p "/home/$MAINUSER/.homebridge/persist"
                mv "$DECONZ_DATA_DIR/$file" "/home/$MAINUSER/.homebridge/persist/$file"
                restart=true
            fi
            if [[ "$file" == IdentifierCache* ]]; then
                [[ $LOG_DEBUG ]] && echo "${LOG_DEBUG}found IdentifierCache - copy it to homebridge persist dir"
                mkdir -p "/home/$MAINUSER/.homebridge/persist"
                mv "$DECONZ_DATA_DIR/$file" "/home/$MAINUSER/.homebridge/persist/$file"
                restart=true
            fi
	    fi
	done

	## start homebridge
	systemctl -q is-active homebridge
	if [ $? -eq 0 ]; then
		[[ $LOG_INFO ]] && echo "${LOG_INFO}another homebridge service is already running"
		return
	fi

	if [[ $restart = true ]]; then
        pkill homebridge
        sleep 5
    fi

	process=$(ps -ax | grep " homebridge$")
	if [ -z "$process" ]; then
		[[ $LOG_INFO ]] && echo "${LOG_INFO}starting homebridge"
		putHomebridgeUpdated "homebridge" "managed"

		homebridge -U /home/$MAINUSER/.homebridge &
	fi

}

# break loop on SIGUSR1
trap 'TIMEOUT=0' SIGUSR1

while [ 1 ]
do
	if [[ -z "$ZLLDB" ]] || [[ ! -f "$ZLLDB" ]] || [[ -z "$DECONZ_PORT" ]] || [[ -z "$BRIDGEID" ]]; then
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
	[[ -z "$DECONZ_PORT" ]] && continue
	[[ -z "$BRIDGEID" ]] && continue

    checkHomebridge
	checkNewDevices
done
