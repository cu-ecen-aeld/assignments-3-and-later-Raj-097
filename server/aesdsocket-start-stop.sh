#!/bin/sh

### BEGIN INIT INFO
# Provides:          aesdsocket
# Required-Start:    $remote_fs $syslog
# Required-Stop:     $remote_fs $syslog
# Default-Start:     2 3 4 5
# Default-Stop:      0 1 6
# Short-Description: Start aesdsocket daemon at boot
### END INIT INFO

DAEMON=/usr/bin/aesdsocket  # Update path if needed
DAEMON_OPTS="-d"
PIDFILE=/var/run/aesdsocket.pid

case "$1" in
    start)
        echo "Starting aesdsocket..."
        start-stop-daemon --start --background --make-pidfile --pidfile "$PIDFILE" --exec "$DAEMON" -- $DAEMON_OPTS
        ;;
    stop)
        echo "Stopping aesdsocket..."
        start-stop-daemon --stop --pidfile "$PIDFILE" --retry TERM/5
        rm -f "$PIDFILE"
        ;;
    restart)
        $0 stop
        sleep 1
        $0 start
        ;;
    status)
        if [ -f "$PIDFILE" ] && kill -0 $(cat "$PIDFILE") 2>/dev/null; then
            echo "aesdsocket is running"
        else
            echo "aesdsocket is not running"
        fi
        ;;
    *)
        echo "Usage: $0 {start|stop|restart|status}"
        exit 1
        ;;
esac

exit 0
