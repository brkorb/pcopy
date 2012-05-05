#! /bin/sh

declare -r prog=$(basename $0 .sh)
declare -r progpid=$$

die() {
    echo "$prog fatal error:  $*"
    kill -TERM $progpid
    exit 1
}

set -e
trap "die could not perform \$_ command" EXIT
eval $(sed -n "/^version/s/[ ;']//gp" ${EXE}-opts.def)

test -d ${EXE}-${version} && rm -rf ${EXE}-${version}
mkdir -p ${EXE}-${version}/doc/html || \
    die "cannot make ${EXE}-${version} directory"
ln ${EXE}-opts.def *.[ch] ${EXE}.1 ${EXE}.texi ${EXE}.info \
   ${EXE}-${version}/.
ln doxy-${EXE}/html/* ${EXE}-${version}/doc/html/.

sed '/^stamp/,$d
     /^all /,/^-include.*opts/d' Makefile \
  > ${EXE}-${version}/Makefile

tar  -cvJf ${EXE}-${version}.tar.xz ${EXE}-${version}
rm   -rf ${EXE}-${version}
tar  -xJf ${EXE}-${version}.tar.xz
test -d ${EXE}-${version}
cd   ${EXE}-${version}
make >/dev/null 2>&1
cd   -
rm   -rf ${EXE}-${version}
trap '' EXIT
