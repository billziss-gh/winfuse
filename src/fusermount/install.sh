set -e

if [ "$(id -u)" != "0" ]; then
    echo "must be run as superuser" 1>&2; exit 1
fi

cd "$(dirname "$0")"

# fusermount
cmp_install()
{
    local src=""
    local dst=""
    for arg in "$@"; do src="$dst"; dst="$arg"; done
    cmp -s "$src" "$dst" || install "$@"
}
cmp_install --backup=numbered -o0 -g0 -mu=srwx,go=rx fusermount.out /usr/bin/fusermount
cmp_install --backup=numbered -o0 -g0 -mu=srwx,go=rx fusermount.out /usr/bin/fusermount3
cmp_install -o0 -g0 -m700 fusermount-helper.exe /usr/bin/fusermount-helper.exe

# /dev/fuse
#echo c /dev/fuse 0666 root root - 10:229 > /etc/tmpfiles.d/wslfuse.conf
#systemd-tmpfiles --create wslfuse.conf

echo FUSE for WSL1 user space components installed
