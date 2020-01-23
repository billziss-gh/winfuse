///usr/bin/gcc -o /tmp/ioc.$$ "$0"; /tmp/ioc.$$ "$@"; rm /tmp/ioc.$$; exit

// usage: sh ioc.c dir typ num siz

#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>

int main(int argc, char *argv[])
{
    unsigned dir, typ, num, siz;

    if (5 != argc)
    {
        fprintf(stderr, "usage: sh ioc.c dir typ num siz\n");
        exit(2);
    }

    dir = strtoul(argv[1], 0, 0);
    typ = strtoul(argv[2], 0, 0);
    num = strtoul(argv[3], 0, 0);
    siz = strtoul(argv[4], 0, 0);

    printf(
        "dir=%u typ=%u num=%u siz=%u\n"
        "ioc=0x%08x\n",
        dir, typ, num, siz,
        _IOC(dir, typ, num, siz));

    return 0;
}
