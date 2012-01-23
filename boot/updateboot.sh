#!/bin/sh
set -e

MDEC=/usr/mdec
BOOT=/boot
BOOTXX=bootxx_minixfs3
ROOT=`printroot -r`

if [ ! -b "$ROOT" ]
then	echo root device $ROOT not found
	exit 1
fi

if [ ! -b "$ROOT" ]
then	echo root device $ROOT not found
	exit 1
fi

echo -n "Install boot as $BOOT on current root and patch into $ROOT? (y/N) "
read ans

if [ ! "$ans" = y ]
then	echo Aborting.
	exit 1
fi

if [ ! "$CC" ]
then	export CC=clang
fi
make install || true

echo Installing boot loader into $BOOT.
cp $MDEC/boot $BOOT

echo Patching 1st stage $BOOTXX into $ROOT.
installboot "$ROOT" $MDEC/$BOOTXX
sync
