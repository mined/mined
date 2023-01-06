# ====================================================================
# Install MinEd standalone version for Windows

PATH="$PATH:."	# need regtool, cygpath

# Check whether to install
# - for all: $PROGFILES/MinEd
# - for me: $USERPROFILE/Local Settings/Application Data/MinEd
#PROGFILES=`regtool get "/HKEY_LOCAL_MACHINE/SOFTWARE/Microsoft/Windows/CurrentVersion/ProgramFilesDir"`
#LOCALAPPS=`regtool get "/HKEY_CURRENT_USER/Software/Microsoft/Windows/CurrentVersion/Explorer/Shell Folders/Local AppData"`
LOCALAPPS=$LOCALAPPDATA

if [ -n "$ProgramW6432" ]
then	ARCH=64
	# this used to be used for a dual-arch installer
	ARCHPREF=64
	PROGFILES=$ProgramW6432
	# single-mode installer
	ARCHPREF=
	PROGFILES=$PROGRAMFILES
else	ARCH=32
	ARCHPREF=
	PROGFILES=$PROGRAMFILES
fi

# Test case: installation for all not possible
if [ -n "$INSTALL_FOR_ME" ]
then	PROGFILES="/cygdrive/c/System Volume Information"
fi

# check whether installation for all users permitted
# need to check via cygpath; result would be wrong with Windows path
pf=`cygpath "$PROGFILES"`
if [ -w "$pf" ]
then	INSTDIR="$PROGFILES\\MinEd"
	for=all
else	INSTDIR="$LOCALAPPS\\MinEd"
	for=me
fi


# ====================================================================
# Create installation directory tree and copy files

echo ======== Installing MinEd into `cygpath -m "$INSTDIR"`

com=`cygpath "$COMSPEC"`
copy () {
	# cp "$1" "$2"
	# but use a copy command that does not need additional libraries
	target=`cygpath -w "$2"`
	"$com" /C copy "$1" "$target"
}
makedir () {
	# mkdir -p "$1"
	target=`cygpath -w "$1"`
	"$com" /C mkdir "$target"
}

makedir "$INSTDIR"
makedir "$INSTDIR/bin"
makedir "$INSTDIR/usr/share/locale"

copy README.txt "$INSTDIR"
copy mined.ico "$INSTDIR"

copy ${ARCHPREF}mined.exe "$INSTDIR/bin/mined.exe"
copy ${ARCHPREF}cygwin1.dll "$INSTDIR/bin/cygwin1.dll"
copy ${ARCHPREF}cygwin-console-helper.exe "$INSTDIR/bin/cygwin-console-helper.exe"
copy ${ARCHPREF}mintty.exe "$INSTDIR/bin/mintty.exe"
copy mined.hlp "$INSTDIR/bin"
#copy dash.exe "$INSTDIR/bin/sh.exe"

#copy locale.alias "$INSTDIR/usr/share/locale"

makedir "$INSTDIR/setup"
copy dash.exe "$INSTDIR/setup"
copy cygwin1.dll "$INSTDIR/setup"
copy regtool.exe "$INSTDIR/setup"
copy cygpath.exe "$INSTDIR/setup"
copy mkshortcut.exe "$INSTDIR/setup"
copy cygpopt-0.dll "$INSTDIR/setup"
copy uninstall.sh "$INSTDIR/setup"
copy deleteall.bat "$INSTDIR/setup"


# ====================================================================
# Setup command line invocation

# Create command line script for console mode invocation
echo -n '@set minedexe=' > "$INSTDIR/mined.bat"
# use DOS name of installation dir to avoid codepage issues
cygpath -d "$INSTDIR/bin/mined.exe" >> "$INSTDIR/mined.bat"
echo '@set nodosfilewarning=1' >> "$INSTDIR/mined.bat"
echo '@"%minedexe%" +eW %1 %2 %3 %4 %5 %6 %7 %8 %9' >> "$INSTDIR/mined.bat"

# Create command line script for window invocation
charset="-oLocale=C -oCharset=UTF-8"
colour="-oUseSystemColours=1 -oBoldAsBright=0"
window="-oScrollbar=0"
keyboard="-oWindowShortcuts=0 -oZoomShortcuts=0"
echo -n '@set mintty=' > "$INSTDIR/wined.bat"
# use DOS name of installation dir to avoid codepage issues
cygpath -d "$INSTDIR/bin/mintty.exe" >> "$INSTDIR/wined.bat"
echo '@set nodosfilewarning=1' >> "$INSTDIR/wined.bat"
echo '@start "" "%mintty%"' $charset $colour $window $keyboard /bin/mined +eW %1 %2 %3 %4 %5 %6 %7 %8 %9 >> "$INSTDIR/wined.bat"

if false; then
# Create link to command line script for minimized invocation
mkshortcut --arguments="$charset $colour $window $keyboard /bin/mined +eW %1 %2 %3 %4 %5 %6 %7 %8 %9" \
	--name="`cygpath $INSTDIR`/wined" \
	--icon="`cygpath $INSTDIR`/mined.ico" \
	--desc="MinEd Unicode text editor" \
	--show=min \
	--workingdir="%HOME%" \
	/bin/mintty.exe
fi

# Add script directory to PATH in registry
# registry keys
ROOTPATH="/HKEY_LOCAL_MACHINE/SYSTEM/CurrentControlSet/Control/Session Manager/Environment/Path"
USERPATH="/HKEY_CURRENT_USER/Environment/PATH"
case $for in
all)	PATHKEY="$ROOTPATH";;
me)	PATHKEY="$USERPATH";;
esac

addpath () {
PATH0=`regtool get "$1" 2> /dev/null`
PATH1=
case ";$PATH0;" in
*";$INSTDIR;"*)	# already in PATH
		echo MinEd already in PATH
		;;
";;")		# empty PATH
		echo Creating new PATH to MinEd
		PATH1="$INSTDIR";;
*)		# append
		echo Appending MinEd to PATH
		PATH1="$PATH0;$INSTDIR";;
esac

if [ -z "$PATH1" ]
then	true
else	regtool -e set "$1" "$PATH1"
fi
}

case $for in
all)	addpath "$ROOTPATH";;
me)	addpath "$USERPATH";;
esac


# ====================================================================
# Add Windows registry entries to invoke MinEd from Explorer context menu

# Name of menu entry
name=MinEd
# Context menu command to invoke
regcmd='"'"$INSTDIR"'\bin\mintty" -i/mined.ico -oLocale=C -oCharset=UTF-8 -oUseSystemColours=1 -oBoldAsBright=0 -oScrollbar=0 -oWindowShortcuts=0 -oZoomShortcuts=0 -e /bin/mined +eW "%1"'

case $for in
all)	keypre=/HKEY_CLASSES_ROOT
	;;
me)	keypre=/HKEY_CURRENT_USER/SOFTWARE/Classes
	;;
esac

# context menu for text files (Windows 7)
# need to create tree down step by step
key7=SystemFileAssociations/text/shell/$name
key71=SystemFileAssociations/text/shell
key72=SystemFileAssociations/text
key73=SystemFileAssociations
# context menu for text files (Windows 10)
key10=txtfile/shell/$name
key101=txtfile/shell
key102=txtfile


regtool add "$keypre/$key73"
regtool add "$keypre/$key72"
regtool add "$keypre/$key71"
regtool add "$keypre/$key7"
regtool -s set "$keypre/$key7/Icon" "$INSTDIR\\mined.ico"
regtool add "$keypre/$key7/command"
regtool -e set "$keypre/$key7/command/" "$regcmd"
regtool add "$keypre/$key102"
regtool add "$keypre/$key101"
regtool add "$keypre/$key10"
regtool -s set "$keypre/$key10/Icon" "$INSTDIR\\mined.ico"
regtool add "$keypre/$key10/command"
regtool -e set "$keypre/$key10/command/" "$regcmd"


# ====================================================================
# Setup Desktop and Start Menu entries

ALLEXPL="/HKEY_LOCAL_MACHINE/SOFTWARE/Microsoft/Windows/CurrentVersion/Explorer"
ALLPROGS="$ALLEXPL/Shell Folders/Common Programs"
ALLDESK="$ALLEXPL/Shell Folders/Common Desktop"
MYEXPL="/HKEY_CURRENT_USER/Software/Microsoft/Windows/CurrentVersion/Explorer"
MYPROGS="$MYEXPL/Shell Folders/Programs"
MYDESK="$MYEXPL/Shell Folders/Desktop"

case $for in
all)	lnk=MinEd-PF.lnk
	unlnk=Uninst-PF.lnk
	PROGS=`regtool get "$ALLPROGS" 2> /dev/null`
	DESK=`regtool get "$ALLDESK" 2> /dev/null`
	if [ -w "$PROGS" -a -w "$DESK" ]
	then	true
	else	PROGS=`regtool get "$MYPROGS" 2> /dev/null`
		DESK=`regtool get "$MYDESK" 2> /dev/null`
		#DESK="$Desktop"
	fi;;
me)	lnk=MinEd-AD.lnk
	unlnk=Uninst-AD.lnk
	PROGS=`regtool get "$MYPROGS" 2> /dev/null`
	DESK=`regtool get "$MYDESK" 2> /dev/null`
	#DESK="$Desktop"
	;;
esac

# Create Start Menu group
makedir "$PROGS/MinEd"
copy $lnk "$PROGS/MinEd/MinEd.lnk"
copy "Mined Web Manual.url" "$PROGS/MinEd"
copy $unlnk "$PROGS/MinEd/Uninstall....lnk"
# Create Desktop symbol
copy $lnk "$DESK/MinEd.lnk"


# ====================================================================
# End
