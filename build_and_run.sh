set -e
scons lightwm 
XEPHYR=$(whereis -b Xephyr | cut -f2 -d' ')
xinit ./xinitrc -- \
    "$XEPHYR" \
        :99 \
        -screen 800x700 \
        -ac \
        -host-cursor
