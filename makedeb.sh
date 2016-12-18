#! /bin/bash
DEBEMAIL="support@cs-networks.net"
DEBFULLNAME="CS Networks"
VERSION="`cat configure.in  | grep -e "VERSION=[0-9+].[0-9+]$" | cut -d '=' -f 2`"
SRCDIR="`pwd`"

echo $VERSION
pushd .
cd ..
mkdir deb &> /dev/null
cd deb
BUILDDIR="sendsms-${VERSION}"
if [ -d "$BUILDDIR" ];
then
	rm -rf ${BUILDDIR}-old
	mv "$BUILDDIR" "$BUILDDIR"-old
fi
#mkdir sendsms-$(VERSION)
cp -a "$SRCDIR" "$BUILDDIR"
cd "$BUILDDIR"
# Only one time we need dh_make
#dh_make -c gpl2 -s  -a -p sendsms  -n
dpkg-buildpackage -uc -tc -rfakeroot -b 
ls ../*.deb
popd

