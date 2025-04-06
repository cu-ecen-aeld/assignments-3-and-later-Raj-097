#!/bin/sh
### BEGIN INIT INFO
# Provides:          aesdchar
# Required-Start:    $local_fs
# Required-Stop:
# Default-Start:     2 3 4 5
# Default-Stop:      0 1 6
# Short-Description: Load aesdchar driver
### END INIT INFO

case "$1" in
  start)
    echo "Loading aesdchar kernel module..."
    /usr/bin/aesdchar_load || echo "Failed to load aesdchar"
    ;;
  stop)
    echo "Unloading aesdchar kernel module..."
    /usr/bin/aesdchar_unload || echo "Failed to unload aesdchar"
    ;;
  restart)
    $0 stop
    sleep 1
    $0 start
    ;;
  *)
    echo "Usage: $0 {start|stop|restart}"
    exit 1
    ;;
esac
exit 0

