$ write sys$output "MinEd VMS make script"

$ sys = f$getsyi("node_swtype")
$ ver = f$getsyi("version")
$ arch = f$getsyi("arch_name")
$ hwname = f$getsyi("hw_name")
$ nodename = f$getsyi("nodename")
$ write sys$output "@ ''nodename':	''sys' ''ver' running on ''hwname' (''arch')"

$! Check whether CC calls this warning PTRMISMATCH or PTRMISMATCH1
$ ptrmismatch := "PTRMISMATCH"
$ on error then ptrmismatch := "PTRMISMATCH1"
$ comptest := CC/DECC/WARN=(ERRORS=PTRMISMATCH1)
$ define/user sys$error nl:	! suppress test error message
$ define/user sys$output nl:	! and compiler summary
$ comptest sys$input
unsigned char * s1 = "";
$! Set the compile command with appropriate parameters
$ compile := "cc/decc/warn=(disable=(''ptrmismatch',promotmatchw)) /obj=[.''arch']"

$ made :== "false"
$ noerr :== "true"
$ cre/dir [.'arch']
$ execre = ""
$ execre = f$cvtime(f$file_attributes ("[.''arch']mined.exe", "cdt"))

$! Alternatives:
$! call make keymaps /define=NOSTRINGTABLES/include_dir=[.keymaps0]
$! call make keymaps /define=use_concatenated_keymaps/include_dir=[.keymaps]
$! call make keymaps /include_dir=[.keymaps1]
$! BEGIN sources
$ call make io
$ call make keyboard
$ call make minedaux
$ call make legacy
$ call make mined1
$ call make textfile
$ call make mousemen
$ call make edit
$ call make pastebuf
$ call make textbuf
$ call make justify
$ call make search
$ call make charprop
$ call make output
$ call make prompt
$ call make compose
$ call make charcode
$ call make keymaps
$ call make keydefs
$ call make dispatch
$ call make termprop
$ call make width
$ call make encoding
$ call make handescr
$ call make timestmp
$! END sources

$ libcre = ""
$ libcre = f$cvtime(f$file_attributes ("[.''arch']charmaps.olb", "cdt"))
$ if libcre .eqs. "" then @vms-complib CHARMAPS

$ if made .eqs. "true" .and. noerr .eqs. "true" then goto link
$ exit
$ link:
$  write sys$output "linking MINED"
$  set default [.'arch']
$  on error then goto back
$  link [-]vms-link/opt,[]charmaps/libr /exe=mined
$ back:
$  set default [-]
$  exit

$ make: subroutine
$!  write sys$output "checking ''p1'"
$  srcmod = f$cvtime(f$file_attributes ("''p1'.c", "cdt"))
$  objcre = ""
$  objcre = f$cvtime(f$file_attributes ("[.''arch']''p1'.obj", "cdt"))
$!  write sys$output "''srcmod' ''p1'.c"
$!  write sys$output "''objcre' [.''arch']''p1'.obj"
$  if objcre .gts. execre then made :== "true"	! link pending
$  if srcmod .gts. objcre then goto compile
$  goto endsub
$ compile:
$  write sys$output "compiling ''p1'"
$  on error then noerr :== "false"
$  compile 'p2 'p1
$  made :== "true"
$ endsub:
$  endsub

