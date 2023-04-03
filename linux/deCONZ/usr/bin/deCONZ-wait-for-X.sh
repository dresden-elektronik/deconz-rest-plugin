#!/bin/bash
# Wait for X server to start.

LOG_ERROR="<3>"
LOG_INFO="<6>"

for ((i = 0; i < 120; i++)) ; do
  if [ -S /tmp/.X11-unix/X0 ] ; then
    echo "${LOG_INFO}X server started."
    sleep 3
    exit 0
  fi
  if ((i % 5 == 0)) ; then
    echo "${LOG_INFO}Waiting for X server to start..."
  fi
  sleep 1
done
echo "${LOG_ERROR}X server not started after ${i} seconds. Giving up."
exit 1
