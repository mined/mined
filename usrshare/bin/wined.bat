@echo off

rem Start MinEd editor (in Windows keyboard emulation mode)
rem embedded in a mintty terminal with Windows look-and-feel

rem Note: mintty 0.6 or higher required for -o option

rem Set appropriate parameters

set charset=-oLocale=C -oCharset=UTF-8
set colour=-oUseSystemColours=1 -oBoldAsBright=0
set window=-oScrollbar=0
set keyboard=-oWindowShortcuts=0 -oZoomShortcuts=0

rem Start terminal

mintty %charset% %colour% %window% %keyboard% /bin/mined +eW %1 %2 %3 %4 %5 %6 %7 %8 %9

rem End
