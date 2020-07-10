#!/bin/bash

# run from Cygwin!

gensrc() {
    echo "#if FUSE_ERRNO == 87 /* Windows */"
    echo
    vcvars="$(/mnt/c/Program\ Files\ \(x86\)/Microsoft\ Visual\ Studio/Installer/vswhere.exe -latest -find 'VC\**\vcvarsall.bat')"
    cmd /c "call" "$vcvars" "x64" "&&" cl /nologo /EP /C errno.src 2>/dev/null | sed -e '1,/beginbeginbeginbegin/d' -e 's/\r$//'
    echo
    echo "#elif FUSE_ERRNO == 67 /* Cygwin */"
    echo
    cpp -C -P errno.src | sed -e '1,/beginbeginbeginbegin/d'
    echo
    echo "#elif FUSE_ERRNO == 76 /* Linux */"
    echo
    # this produces very strange results without the intermediate file (GitHub microsoft/WSL#5063 ?)
    wsl -- cpp -C -P errno.src \| sed -e '1,/beginbeginbeginbegin/d' \> errno.out
    cat errno.out
    echo
    echo "#endif"
}

cd $(dirname "$0")
pwd

(
echo '#include <errno.h>'
echo '/*beginbeginbeginbegin*/'
awk '{ printf "case %s: return %s;\n", $1, $2 }' errno.txt
) > errno.src
gensrc > ../../src/shared/km/errno.i

(
echo '#include <errno.h>'
echo '/*beginbeginbeginbegin*/'
awk '{ printf "case %s: return \"%s\";\n", $1, $1 }' errno.txt
) > errno.src
gensrc > ../../src/shared/km/errnosym.i

rm errno.src errno.out
