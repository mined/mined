#! /bin/sh

# Add/Remove Windows registry entries 
# to invoke MinEd from Explorer context menu

# Name of menu entry
#name="MinEd (cygwin)"
name="MinEd"

# Command to invoke
root=`cygpath -w /`
# Note: mintty 0.6 or higher required for -o option
command="\"$root\\bin\\mintty\" -oLocale=C -oCharset=UTF-8 -oUseSystemColours=1 -oBoldAsBright=0 -oScrollbar=0 -oWindowShortcuts=0 -oZoomShortcuts=0 -e /bin/mined \"%1\""

# Registry keys to use
rootkey='HKEY_CLASSES_ROOT\SystemFileAssociations\text\shell\'"$name"
userkey='HKEY_CURRENT_USER\Software\Classes\SystemFileAssociations\text\shell\'"$name"

# Make sure 'reg' is available
PATH="$PATH:`cygpath $SYSTEMROOT`/system32"


# Either add or remove
# - use Windows reg command (rather than regtool) for its simpler usage

case "$1" in
-)	# Deregister MinEd (cygwin) from Windows Explorer context menu

	# Delete backup key(s) of MinEd cygwin package
	reg delete "$rootkey\\command-cygwin" /f
	reg delete "$userkey\\command-cygwin" /f

	# Restore key(s) of MinEd stand-alone package; 
	# if none exist, delete the entry
	reg copy "$rootkey\\command-stand-alone" "$rootkey\\command" /f ||
		reg delete "$rootkey" /f
	reg copy "$userkey\\command-stand-alone" "$userkey\\command" /f ||
		reg delete "$userkey" /f

	# If last command failed, maybe it was the previous key 
	# to be removed? No error
	true
	;;
*)	# Register MinEd (cygwin) for Windows Explorer context menu

	# Add key to HKEY_CLASSES_ROOT;
	# if that fails, add it to HKEY_CURRENT_USER\Software\Classes
	reg add "$rootkey\\command" /ve /d "$command" /f ||
	reg add "$userkey\\command" /ve /d "$command" /f

	# Copy to backup key(s) for alignment with stand-alone package
	reg copy "$rootkey\\command" "$rootkey\\command-cygwin" /f
	reg copy "$userkey\\command" "$userkey\\command-cygwin" /f

	# If last command failed, maybe it was the previous key 
	# that has been entered, then copied? No error
	true
	;;
esac


# End
