#!/bin/sh
set -e

# Default values can be overriden from environment
: ${d:=} # where are currently the boot monitor files?
# Filepaths as seen from the boot monitor, or from $d here:
: ${BOOTCFG:=/boot.cfg}
: ${DIRSBASE:=/boot/minix} # Without final /, please!
: ${LATEST:=/boot/minix_latest}
# Files used by this utility:
: ${ARGSCFG:=/etc/boot.cfg.args}
: ${DEFAULTCFG:=/etc/boot.cfg.default}
: ${LOCALCFG:=/etc/boot.cfg.local}

# This script aims at being portable among any POSIX shell, in order to be
# run in cross-building cases (with chroot or jails.)
# XXX Currently makes use of readlink(1) and stat(1)

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
		if [ ! -r "$d$kernel" ]
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

usage()
{
	cat >&2 <<'EOF'
Usage:
  update_bootcfg [-lx] [-r rootdev] [-b bootdir] [-t dest]

  Recreates the configuration file used by MINIX boot monitor.

  Options:
     -x Remove "minix_latest/*", for example if broken; then fix the link
     -l Restore "minix_latest" to point to lastest kernel
     -r Root device name; default is current device for /
     -b Boot directory (where boot_monitor is); default is /
     -t New configuration file to create; default is $BOOTCFG (in bootdir)
EOF
	exit 1
}

while getopts "b:lr:t:x" c
do
	case "$c" in
	b)	d="$OPTARG" ;;
	l)	do_restore_latest=1 ;;
	r)	ROOT="$OPTARG" ;;
	t)	keepexist=1; BOOTCFGTMP="$OPTARG" ;;
	x)	do_remove_latest=1 ;;
	*)	usage ;;
	esac
done
shift `expr $OPTIND - 1`

# Sanity check
if test -n "$d" -a ! -d "$d$DIRSBASE"
then
	echo -b$d option used but does not seem to point at some MINIX system!?
	usage
fi

# Let see if ROOT was initialized by setup:
if test -z "$ROOT" -a -r "$ARGSCFG"
then
	if grep -q "^ROOT=" $ARGSCFG
	then
		: ${ROOT:-$(sed -n "s/^ROOT=//p" $ARGSCFG)}
	fi
fi
# Last chance: search which device holds the current / file system:
if test -z "$ROOT"
then
	ROOT=$(awk '$3=="/" { print $1; }' /etc/mtab)
	if test "x$ROOT" = "x/dev/ram"
	then # The root device is the ramdisk!
		# Avoid $(sysenv ramimagename) as being MINIX-specific
		echo Warning: \$ROOT is a ramdisk; consider -r option
	fi
fi

if [ ! -b "$ROOT" ]
then
	echo root device $ROOT not found
	exit 1
fi

rootdevname=`echo $ROOT | sed 's/\/dev\///'`

# Construct a list of additional arguments for boot options to use, based
# on a user-editable file /etc/boot.cfg.args initialized during setup.
# Note that rootdevname is not be passed on this way, as it is handled
# manually by this script to allow menu lines like
#   menu=Root FS in RAM: [...] ;multiboot /boot/minix_latest/kernel \
#           rootdevname=ram ramimagename=$rootdevname $args
# Also, exclude the ROOT variable which is handled above.
if test -r "$ARGSCFG"
then
	args=$( egrep -v "^ROOT=|^#" $ARGSCFG | xargs echo )
fi

# Remove the directory pointed to as /boot/minix_latest
# Useful when one realizes the last release does not work...
if test -n "$do_remove_latest"
then
	rm -rf $(readlink -f $d$LATEST)
	[ ! -h $d$LATEST ] || rm $d$LATEST

	do_restore_latest=1
fi

# Restore /boot/minix_latest pointing at the directory with the lastest kernel
if test -n "$do_restore_latest"
then
	[ ! -h $d$LATEST ] || rm $d$LATEST

	target=$(ls -t $d$DIRSBASE/*/kernel 2>/dev/null \
		| sed -ne '1s|.*/\([^/]*\)/kernel$|\1|p')
	if test -z "$target" -o ! -d $d$DIRSBASE/"$target"
	then
		echo Warning! No MINIX kernels found at $d$DIRSBASE, cannot set $LATEST>&2
		return
	fi
	# XXX Consider a warning if test "$target" = ".temp"

	# XXX Blindly assume $DIRSBASE and $LATEST are sharing the same base
	ln -s $(basename $DIRSBASE)/"$target" $d$LATEST
fi

# All is in place; let's proceed
[ -n "$keepexist" ] || BOOTCFGTMP=$d$BOOTCFG.temp

echo "# Generated file. Edit $LOCALCFG and run $0 !" > $BOOTCFGTMP

if test -r "$DEFAULTCFG"
then
	echo \# Template is "$DEFAULTCFG" >> $BOOTCFGTMP
	filter_entries < $DEFAULTCFG >> $BOOTCFGTMP
fi

if [ -d $d$LATEST -o -h $d$LATEST ]
then
	latest=$(basename $(stat -f "%Y" $d$LATEST))
fi

[ -d $d$DIRSBASE ] && for i in `ls -tr $d$DIRSBASE/`
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

if test -r "$LOCALCFG"
then
	echo \# Local options from "$LOCALCFG" >> $BOOTCFGTMP

	# If the local config supplies a "default" option, we assume that this
	# refers to one of the options in the local config itself. Therefore,
	# we increase this default by the number of options already present in
	# the output so far. To this end, count_entries() inserts a special
	# token that is recognized and filtered out by filter_entries().
	(count_entries $BOOTCFGTMP; cat $LOCALCFG) | filter_entries >> $BOOTCFGTMP
fi

if test -z "$keepexist"
then
	mv $BOOTCFGTMP $d$BOOTCFG
	# Be nice and also update the / file system
	[ -z "$d" ] || cp -p $d$BOOTCFG $BOOTCFG
fi

sync || true
[ -z "$mounted_ramdisk" ] || umount $mounted_ramdisk
