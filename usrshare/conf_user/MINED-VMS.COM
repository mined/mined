$! On VMS, add the commands below to your login script SYS$LOGIN:LOGIN.COM
$! assuming MINED is installed in your login directory
$! (otherwise replace SYS$LOGIN below with the logical name of your 
$! binary directory, e.g. USR$EXE)
$
$! to invoke mined with MINED FILE(s):
$ MINED :== $SYS$LOGIN:MINED
$! to also invoke mined with M FILE(s):
$ M*INED :== $SYS$LOGIN:MINED
$! to invoke mined with E FILE(s):
$ E*D :== $SYS$LOGIN:MINED
$
$! The help file MINED.HLP should either be installed in the same 
$! directory as the binary, or in a help directory (e.g. USR$LIBDIR):
$ DEFINE MINED$HELP USR$LIBDIR:MINED.HLP
