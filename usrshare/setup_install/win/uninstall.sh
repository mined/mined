# ====================================================================
# Uninstall MinEd standalone version for Windows

echo -n "Really uninstall and remove MinEd? (y/n) "
read a
case "$a" in
y|Y|yes|YES)	true;;
*)	exit;;
esac

PATH=".:$PATH"	# need regtool, cygpath

# Check whether installed for all or for current user ("me")
# - for all: $PROGFILES/MinEd
# - for me: $USERPROFILE/Local Settings/Application Data/MinEd
PROGFILES=`regtool get "/HKEY_LOCAL_MACHINE/SOFTWARE/Microsoft/Windows/CurrentVersion/ProgramFilesDir"`
LOCALAPPS=`regtool get "/HKEY_CURRENT_USER/Software/Microsoft/Windows/CurrentVersion/Explorer/Shell Folders/Local AppData"`

case "$*" in
"-for all")	INSTDIR="$PROGFILES\\MinEd"
		for=all;;
"-for me")	INSTDIR="$LOCALAPPS\\MinEd"
		for=me;;
*)	if [ -w "$PROGFILES" -a -d "$PROGFILES\\MinEd" ]
	then	INSTDIR="$PROGFILES\\MinEd"
		for=all
	elif [ -d "$LOCALAPPS\\MinEd" ]
	then	INSTDIR="$LOCALAPPS\\MinEd"
		for=me
	else	echo Cannot find MinEd installation directory
		exit
	fi;;
esac

echo ======== Removing MinEd, installed in `cygpath -m "$INSTDIR"`

com=`cygpath "$COMSPEC"`
copy () {
	target=`cygpath -w "$2"`
	# use a copy command that does not need additional libraries
	"$com" /C copy "$1" "$target"
}
delete () {
	target=`cygpath -w "$1"`
	"$com" /C del "$target"
}
rmdir () {
	target=`cygpath -w "$1"`
	"$com" /C rmdir "$target"
}


# ====================================================================
# Uninstall setup for command line invocation

# Remove script directory from PATH in registry
# registry keys
ROOTPATH="/HKEY_LOCAL_MACHINE/SYSTEM/CurrentControlSet/Control/Session Manager/Environment/Path"
USERPATH="/HKEY_CURRENT_USER/Environment/PATH"
case $for in
all)	PATHKEY="$ROOTPATH";;
me)	PATHKEY="$USERPATH";;
esac

doublebackslash () {
parse="$*"
dbl=
while	case "$parse" in
	*\\*)	pre="${parse%%\\*}"
		post="${parse#*\\}"
		parse="$post"
		true;;
	*)	false;;
	esac
do	dbl="$dbl$pre\\\\"
done
dbl="$dbl$post"
}

removepath () {
PATH0=`regtool get "$1" 2> /dev/null`
PATH1=";$PATH0;"
case "$PATH1" in
*";$INSTDIR;"*)	# found in PATH
		echo Removing MinEd from PATH
		doublebackslash "$INSTDIR"
		pre=${PATH1%;$dbl;*}
		post=${PATH1##*;$dbl;}
		PATH1="$pre;$post"
		PATH1=${PATH1#;}
		PATH1=${PATH1%;}
		regtool -e set "$1" "$PATH1"
		true;;
*)		# not in PATH
		false;;
esac
}

if removepath "$ROOTPATH"
then	true
else	removepath "$USERPATH"
fi


# ====================================================================
# Remove Windows registry entries to invoke MinEd from Explorer context menu

# Name of menu entry
name=MinEd
# Context menu command to invoke
regcmd='"'"$INSTDIR"'\bin\mintty" -oLocale=C -oCharset=UTF-8 -oUseSystemColours=1 -oBoldAsBright=0 -oScrollbar=0 -oWindowShortcuts=0 -oZoomShortcuts=0 -e /bin/mined +eW "%1"'

ROOTKEY=/HKEY_CLASSES_ROOT/SystemFileAssociations/text/shell/$name
USERKEY=/HKEY_CURRENT_USER/Software/Classes/SystemFileAssociations/text/shell/$name

case $for in
all)	MENUKEY="$ROOTKEY";;
me)	MENUKEY="$USERKEY";;
esac

regtool remove "$MENUKEY/command"
regtool remove "$MENUKEY"


# ====================================================================
# Remove Desktop and Start Menu entries

ALLEXPL="/HKEY_LOCAL_MACHINE/SOFTWARE/Microsoft/Windows/CurrentVersion/Explorer"
ALLPROGS="$ALLEXPL/Shell Folders/Common Programs"
ALLDESK="$ALLEXPL/Shell Folders/Common Desktop"
MYEXPL="/HKEY_CURRENT_USER/Software/Microsoft/Windows/CurrentVersion/Explorer"
MYPROGS="$MYEXPL/Shell Folders/Programs"
MYDESK="$MYEXPL/Shell Folders/Desktop"

case $for in
all)	lnk=MinEd-PF.lnk
	PROGS=`regtool get "$ALLPROGS" 2> /dev/null`
	DESK=`regtool get "$ALLDESK" 2> /dev/null`
	if [ -w "$PROGS" -a -w "$DESK" ]
	then	true
	else	PROGS=`regtool get "$MYPROGS" 2> /dev/null`
		DESK=`regtool get "$MYDESK" 2> /dev/null`
	fi;;
me)	lnk=MinEd-AD.lnk
	PROGS=`regtool get "$MYPROGS" 2> /dev/null`
	DESK=`regtool get "$MYDESK" 2> /dev/null`
	;;
esac

# Remove Start Menu group
delete "$PROGS/MinEd/MinEd.lnk"
delete "$PROGS/MinEd/Mined Web Manual.url"
delete "$PROGS/MinEd/Uninstall....lnk"
rmdir "$PROGS/MinEd"
# Remove Desktop symbol
delete "$DESK/MinEd.lnk"


# ====================================================================
# Remove files from installation directory tree

echo ======== Preparing to remove MinEd from `cygpath -m "$INSTDIR"`

# Decouple from file locks on dash.exe and . for complete deletion
copy deleteall.bat "$TEMP"
cd "$TEMP"
export INSTDIR
"$com" /C start "" "$COMSPEC" /C deleteall.bat


# ====================================================================
# End
