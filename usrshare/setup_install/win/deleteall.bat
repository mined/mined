echo ======== Removing MinEd from %INSTDIR%

cd /D %INSTDIR%

del mined.bat
del wined.bat
del README.txt
del mined.ico

cd bin
del mined.exe
del cygwin1.dll
del cygwin-console-helper.exe
del mined.hlp
del mintty.exe
del sh.exe
cd ..
rmdir bin

cd setup
del dash.exe
del uninstall.sh
del cygpath.exe
del cygwin1.dll
del deleteall.bat
del regtool.exe
cd ..
rmdir setup

del usr\share\locale\locale.alias
rmdir usr\share\locale
rmdir usr\share
rmdir usr

cd ..
rmdir "%INSTDIR%"

cd /D %TEMP%
del deleteall.bat
