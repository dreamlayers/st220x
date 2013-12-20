#!/bin/bash

#    ST2205U: interactively patch firmware
#    Copyright (C) 2008 Jeroen Domburg <jeroen@spritesmods.com>
#
#    This program is free software: you can redistribute it and/or modify
#    it under the terms of the GNU General Public License as published by
#    the Free Software Foundation, either version 3 of the License, or
#    (at your option) any later version.
#
#    This program is distributed in the hope that it will be useful,
#    but WITHOUT ANY WARRANTY; without even the implied warranty of
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#    GNU General Public License for more details.
#
#    You should have received a copy of the GNU General Public License
#    along with this program.  If not, see <http://www.gnu.org/licenses/>.

if [ ! -f firmware.bin ]; then
    echo "Dump firmware to firmware.bin before running this script."
    exit 1
fi

echo "Making a working copy..."
rm -f hackedfw.bin
cp firmware.bin hackedfw.bin || exit 1

cd hack || exit 1
./assembleme || exit 1
cd .. || exit 1

match=false;
echo "Looking for a known device profile..."
for x in hack/m_*; do
    echo "$x ..."
    em=`cat $x/spec | grep '^EMPTY_AT' | cut -d '$' -f 2 | tr 'A-Z' 'a-z'`
    pa=`cat $x/spec | grep '^PATCH_AT' | cut -d '$' -f 2 | tr 'A-Z' 'a-z'`
    if ./bgrep hackedfw.bin $x/lookforme.bin -h | grep -q $pa; then
	if ./bgrep hackedfw.bin $x/empty.bin -h | grep -q $em; then
	    echo "We have a match!"
	    match=true
	    break;
	fi
    fi
    echo "...nope."
done
if [ $match = false ]; then
    echo "Sorry, I couldn't find a matching device profile. If you want to give "
    echo "creating it yourself a shot, please read ./hack/newhack.txt for more"
    echo "info."
    echo "(Btw: this can also mean your device already has a hacked firmware. If"
    echo "you want to upgrade your device using this script, please flash back"
    echo "the fwimage.bak the previous version saved first.)"
    exit 1
fi

echo "Patching fw..."
./splice hackedfw.bin $x/hack_jmp.bin 0x$pa >/dev/null || exit 1
./splice hackedfw.bin $x/hack.bin 0x$em >/dev/null || exit 1
echo "Success!"
exit 0
