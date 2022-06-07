#!/bin/bash

UPDATE_VERSION_NODE="16.14.2"

# when increasing major version of node adjust downoload link
NODE_DOWNLOAD_LINK="https://deb.nodesource.com/setup_16.x"

TIMEOUT=0
LOG_LEVEL=7
ZLLDB=""
SQL_RESULT=
OWN_PID=$$
MAINUSER=$(getent passwd 1000 | cut -d: -f1)
DECONZ_CONF_DIR="/home/$MAINUSER/.local/share"
LOG_DIR=""
DECONZ_PORT=

PROXY_ADDRESS=""
PROXY_PORT=""
AUTO_UPDATE=false #auto update active status

all_installed=false

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

LOG_VER_NODEJS=
LOG_VER_NPM=
LOG_VER_HOMEBRIDGE=
LOG_VER_HOMEBRIDGE_HUE=
LOG_VER_CONFIG_UI_X=
LOGFILE_DATE=

# $1 = key $2 = value
function putHomebridgeUpdated {
	curl --noproxy '*' -s -o /dev/null -d "{\"$1\":\"$2\"}" -X PUT http://127.0.0.1:${DECONZ_PORT}/api/$OWN_PID/config/homebridge/updated
}

# $1 = application $2 = version
function logInstallVersion {

	if [[ "$LOGFILE_DATE" != $(date +%Y-%m-%d) ]]; then
		LOGFILE_DATE=$(date +%Y-%m-%d)
		echo "Logging started $(date +%Y-%m-%dT%H:%M:%S)" >> "$LOG_DIR/LOG_HOMEBRIDGE_INSTALL_$LOGFILE_DATE"
		echo "-----------------------------------" >> "$LOG_DIR/LOG_HOMEBRIDGE_INSTALL_$LOGFILE_DATE"
		echo "UPDATE_VERSION_NODE = $UPDATE_VERSION_NODE" >> "$LOG_DIR/LOG_HOMEBRIDGE_INSTALL_$LOGFILE_DATE"
		LOG_VER_NODEJS=
		LOG_VER_NPM=
		LOG_VER_HOMEBRIDGE=
		LOG_VER_HOMEBRIDGE_HUE=
		LOG_VER_CONFIG_UI_X=
	fi

	if [[ "$1" = "nodejs" ]]; then
		if [[ "$LOG_VER_NODEJS" != "$2" ]]; then
			[[ $LOG_DEBUG ]] && echo "${LOG_DEBUG} $1 installed version $2"
			echo "$1 installed version $2" >> "$LOG_DIR/LOG_HOMEBRIDGE_INSTALL_$LOGFILE_DATE"
			LOG_VER_NODEJS="$2"
		fi
	elif [[ "$1" = "npm" ]]; then
		if [[ "$LOG_VER_NPM" != "$2" ]]; then
			[[ $LOG_DEBUG ]] && echo "${LOG_DEBUG} $1 installed version $2"
			echo "$1 installed version $2" >> "$LOG_DIR/LOG_HOMEBRIDGE_INSTALL_$LOGFILE_DATE"
			LOG_VER_NPM="$2"
		fi
	elif [[ "$1" = "homebridge" ]]; then
		if [[ "$LOG_VER_HOMEBRIDGE" != "$2" ]]; then
			[[ $LOG_DEBUG ]] && echo "${LOG_DEBUG} $1 installed version $2"
			echo "$1 installed version $2" >> "$LOG_DIR/LOG_HOMEBRIDGE_INSTALL_$LOGFILE_DATE"
			LOG_VER_HOMEBRIDGE="$2"
		fi
	elif [[ "$1" = "homebridgeHue" ]]; then
		if [[ "$LOG_VER_HOMEBRIDGE_HUE" != "$2" ]]; then
			[[ $LOG_DEBUG ]] && echo "${LOG_DEBUG} $1 installed version $2"
			echo "$1 installed version $2" >> "$LOG_DIR/LOG_HOMEBRIDGE_INSTALL_$LOGFILE_DATE"
			LOG_VER_HOMEBRIDGE_HUE="$2"
		fi
	elif [[ "$1" = "config_ui_x" ]]; then
		if [[ "$LOG_VER_CONFIG_UI_X" != "$2" ]]; then
			[[ $LOG_DEBUG ]] && echo "${LOG_DEBUG} $1 installed version $2"
			echo "$1 installed version $2" >> "$LOG_DIR/LOG_HOMEBRIDGE_INSTALL_$LOGFILE_DATE"
			LOG_VER_CONFIG_UI_X="$2"
		fi
	fi
}

# $1 = queryString
function sqliteSelect() {
    if [[ -z "$ZLLDB" ]] || [[ ! -f "$ZLLDB" ]]; then
        [[ $LOG_DEBUG ]] && echo "${LOG_DEBUG}database not found"
        ZLLDB=""
        return
    fi
    [[ $LOG_SQL ]] && echo "SQLITE3 $1"

    SQL_RESULT=$(sqlite3 $ZLLDB "$1")
    if [ $? -ne 0 ]; then
    	SQL_RESULT=
	fi
    [[ $LOG_SQL ]] && echo "$SQL_RESULT"
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
			LOG_DIR="${ZLLDB:0:-7}/homebridge-install-logfiles"
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
	sqliteSelect "select value from config2 where key=\"port\""
	local value="$SQL_RESULT"

	if [[ -n "$value" ]]; then
		DECONZ_PORT=$value
	fi

	# init logging
	if [ ! -d "$LOG_DIR" ]; then
		mkdir "$LOG_DIR"
	fi
}

function installHomebridge {

	## get database config
	params=( [0]="proxyaddress" [1]="proxyport" [2]="homebridgeupdate" )
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

	PROXY_ADDRESS="${values[0]}"
	PROXY_PORT="${values[1]}"
	AUTO_UPDATE="${values[2]}"

	## check installed components
	hb_installed=false
	hb_hue_installed=false
	node_installed=false
	npm_installed=false
	config_ui_x_installed=false
	node_ver=""
	hb_hue_version=""
	config_ui_x_ver=""

	which homebridge &> /dev/null
	if [ $? -eq 0 ]; then
		hb_installed=true
		logInstallVersion "homebridge" "$(homebridge --version | sed -n 2p)"
		# look for homebridge-hue installation
		hb_hue_version=$(npm list -g homebridge-hue | grep homebridge-hue | cut -d@ -f2 | xargs)
		if [ -n "$hb_hue_version" ]; then
			# homebridge-hue installation found
			hb_hue_installed=true
			logInstallVersion "homebridgeHue" "$hb_hue_version"
			putHomebridgeUpdated "homebridgeversion" "$hb_hue_version"
		else
			logInstallVersion "homebridgeHue" "not-installed"
		fi
	else
		logInstallVersion "homebridge" "not-installed"
	fi

	# search for nodejs or node install
	which nodejs &> /dev/null
	if [ $? -ne 0 ]; then
		which node &> /dev/null
		if [ $? -eq 0 ]; then
			node_installed=true
			node_ver=$(node --version | cut -dv -f2) # strip the v
			logInstallVersion "nodejs" "$node_ver"
		else
			logInstallVersion "nodejs" "not-installed"
		fi
	else
		node_installed=true
		node_ver=$(nodejs --version | cut -dv -f2) # strip the v
		logInstallVersion "nodejs" "$node_ver"
	fi

	which npm &> /dev/null
	if [ $? -eq 0 ]; then
		npm_ver=$(npm --version)
		logInstallVersion "npm" "$npm_ver"
	else
		logInstallVersion "npm" "not-installed"
	fi

	which homebridge-config-ui-x &> /dev/null
	if [ $? -eq 0 ]; then
		config_ui_x_installed=true
		config_ui_x_ver=$(npm list -g homebridge-config-ui-x | grep config | cut -d@ -f2 | xargs)
		logInstallVersion "config_ui_x" "$config_ui_x_ver"
	else
		logInstallVersion "config_ui_x" "not-installed"
	fi

	if [[ $hb_installed = false || $config_ui_x_installed = false || $hb_hue_installed = false || $node_installed = false ]]; then
		[[ $LOG_INFO ]] && echo "${LOG_INFO}check inet connectivity"
		echo "check inet connectivity" >> "$LOG_DIR/LOG_HOMEBRIDGE_INSTALL_$LOGFILE_DATE"

		putHomebridgeUpdated "homebridge" "installing"

		curl --head --connect-timeout 20 -k https://www.phoscon.de &> /dev/null
		if [ $? -ne 0 ]; then
			if [[ ! -z "$PROXY_ADDRESS" && ! -z "$PROXY_PORT" && "$PROXY_ADDRESS" != "none" ]]; then
				export http_proxy="http://${PROXY_ADDRESS}:${PROXY_PORT}"
				export https_proxy="http://${PROXY_ADDRESS}:${PROXY_PORT}"
				[[ $LOG_DEBUG ]] && echo "${LOG_DEBUG}set proxy: ${PROXY_ADDRESS}:${PROXY_PORT}"
				echo "set proxy: ${PROXY_ADDRESS}:${PROXY_PORT}" >> "$LOG_DIR/LOG_HOMEBRIDGE_INSTALL_$LOGFILE_DATE"
			else
				[[ $LOG_WARN ]] && echo "${LOG_WARN}no internet connection. Abort homebridge installation."
				echo "no internet connection. Abort homebridge installation." >> "$LOG_DIR/LOG_HOMEBRIDGE_INSTALL_$LOGFILE_DATE"
				putHomebridgeUpdated "homebridge" "install-error"
				return
			fi
		fi

		# check correct timezone
		sysTimezone=$(timedatectl | grep zone | cut -d':' -f2 | cut -d '(' -f1 | xargs)
		[[ $LOG_DEBUG ]] && echo "${LOG_DEBUG} System TZ: $sysTimezone"
		echo "System TZ: $sysTimezone" >> "$LOG_DIR/LOG_HOMEBRIDGE_INSTALL_$LOGFILE_DATE"
		dbTimezone=$(sqliteSelect "select value from config2 where key=\"timezone\"")
		[[ $LOG_DEBUG ]] && echo "${LOG_DEBUG} TZ from db: $dbTimezone"
		echo "TZ from db: $dbTimezone" >> "$LOG_DIR/LOG_HOMEBRIDGE_INSTALL_$LOGFILE_DATE"

		if [[ "$sysTimezone" != "$dbTimezone" && "$dbTimezone" != '' ]]; then
			[[ $LOG_DEBUG ]] && echo "${LOG_DEBUG} Setting sys timezone to db timezone"
			echo "Setting sys timezone to db timezone" >> "$LOG_DIR/LOG_HOMEBRIDGE_INSTALL_$LOGFILE_DATE"
			timedatectl set-timezone "$dbTimezone"
		fi

		# install nodejs if not installed
		if [[ $node_installed = false ]]; then
		# example for getting it worked on rpi1 and 0
		# if armv6
			#wget node 8.12.0
			#cp -R to /usr/local
			#PATH=usr/local/bin/
			#link nodejs to node
		# else
			curl -sL "$NODE_DOWNLOAD_LINK" | bash -
			apt-get update
			if [ $? -eq 0 ]; then
				apt install -y nodejs | tee -a "$LOG_DIR/LOG_HOMEBRIDGE_INSTALL_$LOGFILE_DATE"
				if [ $? -ne 0 ]; then
					[[ $LOG_WARN ]] && echo "${LOG_WARN}could not install nodejs"
					echo "could not install nodejs" >> "$LOG_DIR/LOG_HOMEBRIDGE_INSTALL_$LOGFILE_DATE"
					putHomebridgeUpdated "homebridge" "install-error"
					return
				fi
			else
				[[ $LOG_WARN ]] && echo "${LOG_WARN}could not download node setup."
				echo "could not download node setup." >> "$LOG_DIR/LOG_HOMEBRIDGE_INSTALL_$LOGFILE_DATE"
				putHomebridgeUpdated "homebridge" "install-error"
				return
			fi
		# fi
		else
			echo "compare installed node version $node_ver to update version $UPDATE_VERSION_NODE" >> "$LOG_DIR/LOG_HOMEBRIDGE_INSTALL_$LOGFILE_DATE"
		    dpkg --compare-versions "$node_ver" lt "$UPDATE_VERSION_NODE"
			if [ $? -eq 0 ]; then
			    curl -sL "$NODE_DOWNLOAD_LINK" | bash -
			    apt-get update
				if [ $? -eq 0 ]; then
					apt-get install -y nodejs | tee -a "$LOG_DIR/LOG_HOMEBRIDGE_INSTALL_$LOGFILE_DATE"
					if [ $? -ne 0 ]; then
						[[ $LOG_WARN ]] && echo "${LOG_WARN}could not install nodejs"
							echo "could not install nodejs" >> "$LOG_DIR/LOG_HOMEBRIDGE_INSTALL_$LOGFILE_DATE"
							putHomebridgeUpdated "homebridge" "install-error"
						return
					fi
				else
					[[ $LOG_WARN ]] && echo "${LOG_WARN}could not download node setup."
					echo "could not download node setup." >> "$LOG_DIR/LOG_HOMEBRIDGE_INSTALL_$LOGFILE_DATE"
					putHomebridgeUpdated "homebridge" "install-error"
					return
				fi
		    fi
		fi

		# install homebridge if not installed
		if [[ $hb_installed = false ]]; then
			npm -g install npm | tee -a "$LOG_DIR/LOG_HOMEBRIDGE_INSTALL_$LOGFILE_DATE"
			if [ $? -eq 0 ]; then
				npm -g install homebridge | tee -a "$LOG_DIR/LOG_HOMEBRIDGE_INSTALL_$LOGFILE_DATE"
				if [ $? -ne 0 ]; then
					[[ $LOG_WARN ]] && echo "${LOG_WARN}could not install homebridge"
					echo "could not install homebridge" >> "$LOG_DIR/LOG_HOMEBRIDGE_INSTALL_$LOGFILE_DATE"
					putHomebridgeUpdated "homebridge" "install-error"
					return
				fi
			else
				[[ $LOG_WARN ]] && echo "${LOG_WARN}could not update npm"
				echo "could not update npm" >> "$LOG_DIR/LOG_HOMEBRIDGE_INSTALL_$LOGFILE_DATE"
				putHomebridgeUpdated "homebridge" "install-error"
				return
			fi
		fi

		# install homebridge-hue if not installed
		if [[ $hb_hue_installed = false ]]; then
			npm -g install homebridge-lib homebridge-hue | tee -a "$LOG_DIR/LOG_HOMEBRIDGE_INSTALL_$LOGFILE_DATE"
			if [ $? -ne 0 ]; then
				[[ $LOG_WARN ]] && echo "${LOG_WARN}could not install homebridge hue"
				echo "could not install homebridge hue" >> "$LOG_DIR/LOG_HOMEBRIDGE_INSTALL_$LOGFILE_DATE"
				putHomebridgeUpdated "homebridge" "install-error"
				return
			else
				hb_hue_version=$(npm list -g homebridge-hue | grep homebridge-hue | cut -d@ -f2 | xargs)
				putHomebridgeUpdated "homebridgeversion" "$hb_hue_version"
			fi
		fi

		# install homebridge-config-ui-x if not installed
		if [[ $config_ui_x_installed = false ]]; then
			npm -g install homebridge-config-ui-x | tee -a "$LOG_DIR/LOG_HOMEBRIDGE_INSTALL_$LOGFILE_DATE"
			if [ $? -ne 0 ]; then
				[[ $LOG_WARN ]] && echo "${LOG_WARN}could not install homebridge-config-ui-x"
				echo "could not install homebridge-config-ui-x" >> "$LOG_DIR/LOG_HOMEBRIDGE_INSTALL_$LOGFILE_DATE"
				putHomebridgeUpdated "homebridge" "install-error"
				return
			else
				config_ui_x_ver=$(npm list -g homebridge-config-ui-x | grep config | cut -d@ -f2 | xargs)
				logInstallVersion "config-ui-x" "$config_ui_x_ver"
			fi
		fi
	fi

	# fix missing homebridge-lib
	if [[ -n $(npm list -g homebridge-lib | grep empty) ]]; then
		npm -g install homebridge-lib | tee -a "$LOG_DIR/LOG_HOMEBRIDGE_INSTALL_$LOGFILE_DATE"
	fi

	all_installed=true
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

		TIMEOUT=30 # 1/2 minute
		[[ -z "$ZLLDB" ]] && continue
		[[ ! -f "$ZLLDB" ]] && continue
		[[ -z "$DECONZ_PORT" ]] && continue

		installHomebridge

		if [[ $all_installed == true ]]; then
			TIMEOUT=3600 # 1 hour
		fi
done
