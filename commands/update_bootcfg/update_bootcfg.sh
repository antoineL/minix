#!/bin/sh
set -e

# Default values can be overriden from environment
: ${BOOTCFG:=/boot.cfg}
: ${DEFAULTCFG:=/etc/boot.cfg.default}
: ${LOCALCFG:=/etc/boot.cfg.local}
: ${DIRSBASE:=/boot/minix} # Without final /, please!
: ${LATEST:=/boot/minix_latest}
: ${INHERIT:="ahci acpi no_apic"}

filter_entries()
{
	# This routine performs three tasks:
	# - substitute variables in the configuration lines;
	# - remove multiboot entries that refer to nonexistent kernels;
	# - adjust the default option for line removal and different files.
	# The last part is handled by the awk part of the routine.

	while read line
	do
		# Substitute variables like $rootdevname and $args
		line=$(eval echo \"$line\")

		if ! echo "$line" | grep -sq '^menu=.*multiboot'
		then
			echo "$line"
			continue
		fi

		# Check if the referenced kernel is present
		kernel=`echo "$line" | sed -n 's/.*multiboot[[:space:]]*\(\/[^[:space:];]*\).*/\1/p'`
		if [ ! -r "$kernel" ]
		then
			echo "Warning: config contains entry for \"$kernel\" which is missing! Entry skipped." 1>&2
			echo "menu=SKIP"
		else
			echo "$line"
		fi
	done | awk '
		BEGIN {
			count=1
			base=0
			default=0
		}
		/^menu=SKIP$/ {
			# A menu entry discarded by the kernel check above
			skip[count++]=1
			next
		}
		/^menu=/ {
			# A valid menu entry
			skip[count++]=0
			print
			next
		}
		/^BASE=/ {
			# The menu count base as passed in by count_entries()
			sub(/^.*=/,"",$1)
			base=$1+0
			next
		}
		/^default=/ {
			# The default option
			# Correct for the menu count base and store for later
			sub(/^.*=/,"",$1)
			default=$1+base
			next
		}
		{
			# Any other line
			print
		}
		END {
			# If a default was given, correct for removed lines
			# (do not bother to warn if the default itself is gone)
			if (default) {
				for (i = default; i > 0; i--)
					if (skip[i]) default--;
				print "default=" default
			}
		}
	'
}

count_entries()
{
	printf "BASE="; grep -cs '^menu=' "$1"
}

# Which device holds the root file system?
find_target_root()
{
	if test -r /etc/fstab
	then
		ROOT=$(awk '$2=="/" { print $1; }' /etc/fstab)
	# else	# ... do nothing, and hope the caller knows what it does;
		# at any rate a warning will be issued later
	fi
}

find_real_root()
{
	ROOT=$(awk '$3=="/" { print $1; }' /etc/mtab)
	if test "x$ROOT" = "x/dev/ram"
	then # The root device is the ramdisk; try to find the original file system
		ramimagename=$(sysenv ramimagename)
		if test -n "$ramimagename" -a -b "/dev/$ramimagename"
		then # Found! Now try to find the mounting point
			ROOT="/dev/$ramimagename"
			ramdisk_dir=`awk "\$1=\"\$ROOT\" {print \$3;}" /etc/mtab`
			if test -z "$ramdisk_dir"
			then # Not yet mounted; mount it temporarily to /mnt
				umount /mnt 2>/dev/null || true
				mount $ROOT /mnt
				mounted_ramdisk=/mnt
				DESTDIR=$mounted_ramdisk
			else # Already mounted; redirect the script
				DESTDIR=$ramdisk_dir
			fi
			unset ramdisk_dir
		fi
		unset ramimagename # Do not pollute variable substitution
	fi
}

# Restore /boot/minix_latest pointing at the directory with the lastest kernel
restore_latest()
{
	[ ! -h $LATEST ] || rm $LATEST
	[ -z "$DESTDIR" -o ! -h $DESTDIR$LATEST ] || rm $DESTDIR$LATEST

	target=$(ls -t $DIRSBASE/*/kernel 2>/dev/null | sed -ne '1s|.*/\([^/]*\)/kernel$|\1|p')
	if test -z "$target" -o ! -d $DIRSBASE/"$target"
	then
		echo Warning! No MINIX kernels found at $DIRSBASE, cannot set $LATEST>&2
		return
	fi
	# XXX Consider a warning if test "$target" = ".temp"
	ln -s $(basename $DIRSBASE)/"$target" $LATEST

	# Also update the real file system
	if test -n "$DESTDIR"
	then
		if test ! -d "$DESTDIR$DIRSBASE$target"
		then
			cp -pfR "$DIRSBASE$target" $DESTDIR$DIRSBASE/
		fi
		ln -s $(basename $DIRSBASE)/"$target" $DESTDIR$LATEST
	fi
}

usage()
{
	cat >&2 <<'EOF'
Usage:
  update_bootcfg [-lx] [-r rootdev] [-t dest] [-a supps_args]

  Recreates the configuration file used by MINIX boot monitor.

  Options:
     -l Restore "minix_latest" to point to lastest kernel
     -x Remove "minix_latest", for example if broken
     -r Root device name; default is current device for /
     -t New configuration file to create; default is $BOOTCFG
     -a Additional arguments to menu lines
EOF
	exit 1
}

# This script is designed to run directly against a MINIX installation
if test "x$DESTDIR" != "x"
then
	echo Warning: DESTDIR is set! Unset it or try "  chroot \$DESTDIR $0"
fi

args=""

while getopts "a:lr:t:x" c
do
	case "$c" in
	a)	args="$args $OPTARG" ;;
	l)	do_restore_latest=1 ;;
	r)	ROOT=$OPTARG ;;
	t)	dryrun=1; BOOTCFGTMP=$OPTARG ;;
	x)	do_remove_latest=1 ;;
	*)	usage ;;
	esac
done

shift `expr $OPTIND - 1`

DESTDIR=
[ -n "$ROOT" ] || find_real_root

if [ ! -b "$ROOT" ]
then
	echo root device $ROOT not found
	exit 1
fi

# Remove the directory pointed to as /boot/minix_latest
# Useful when one realizes the last release does not work...
if test -n "$do_remove_latest"
then
	rm -rf $(readlink -f $DESTDIR$LATEST)
	[ ! -h $DESTDIR$LATEST ] || rm $DESTDIR$LATEST
	# Also clean up the / file system
	rm -rf $(readlink -f $LATEST)
	[ ! -h $LATEST ] || rm $LATEST
	do_restore_latest=1
fi
[ -z "$do_restore_latest" ] || restore_latest

rootdevname=`echo $ROOT | sed 's/\/dev\///'`

# Construct a list of inherited arguments for boot options to use. Note that
# rootdevname must not be passed on this way, as it is changed during setup.
for k in $INHERIT; do
	if sysenv | grep -sq "^$k="; then
		kv=$(sysenv | grep "^$k=")
		args="$args $kv"
	fi
done

# All is in place; let's proceed
[ -n "$dryrun" ] || BOOTCFGTMP=$DESTDIR$BOOTCFG.temp

echo \# Generated file. Edit $LOCALCFG ! > $BOOTCFGTMP

if [ -r $DEFAULTCFG ]
then
	echo \# Template is "$DEFAULTCFG" >> $BOOTCFGTMP
	filter_entries < $DEFAULTCFG >> $BOOTCFGTMP
fi

if [ -d $LATEST -o -h $LATEST ]
then
	latest=$(basename $(stat -f "%Y" $DESTDIR$LATEST))
fi

[ -d $DIRSBASE ] && for i in `ls $DIRSBASE/`
do
	build_name="`basename $i`"
	if [ "$build_name" != "$latest" ]
	then
	# Avoid inserting space before commands, boot monitor dislikes it
		echo "menu=Start MINIX 3 ($build_name):load_mods" \
			"$DIRSBASE/$i/mod*;multiboot $DIRSBASE/$i/kernel" \
				"rootdevname=$rootdevname $args" >> $BOOTCFGTMP
	fi
done

if [ -r $LOCALCFG ]
then
	echo \# Local options from "$LOCALCFG" >> $BOOTCFGTMP

	# If the local config supplies a "default" option, we assume that this
	# refers to one of the options in the local config itself. Therefore,
	# we increase this default by the number of options already present in
	# the output so far. To this end, count_entries() inserts a special
	# token that is recognized and filtered out by filter_entries().
	(count_entries $BOOTCFGTMP; cat $LOCALCFG) | filter_entries >> $BOOTCFGTMP
fi

[ -n "$dryrun" ] || mv $BOOTCFGTMP $DESTDIR$BOOTCFG

sync || true
[ -z "$mounted_ramdisk" ] || umount $mounted_ramdisk
