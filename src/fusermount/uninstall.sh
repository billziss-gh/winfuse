#set -x

if [ "$(id -u)" != "0" ]; then
    echo "must be run as superuser" 1>&2; exit 1
fi

cd "$(dirname "$0")"

# /dev/fuse
#rm -f /etc/tmpfiles.d/wslfuse.conf
#rm -f /dev/fuse

# fusermount
uninstall()
{
    local src="$1"
    local dst="$2"
    if cmp -s "$src" "$dst"; then
        local bak=$(ls -1rv "$dst".~*~ 2>/dev/null | head -1)
        if [ -n "$bak" ]; then
            mv -f "$bak" "$dst"
        else
            rm -f "$dst"
        fi
    fi
}
uninstall fusermount.out /usr/bin/fusermount
uninstall fusermount.out /usr/bin/fusermount3
rm -f /usr/bin/fusermount-helper.exe

echo FUSE for WSL1 user space components uninstalled
