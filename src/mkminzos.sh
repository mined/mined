#! /bin/sh

cc -Dunix -DnoHANinfo -DNOCHARNAMES -DNODECOMPOSE -DnoCJKcharmaps -c *.c
#	-DnoHANinfo:			handescr.c
#	-DNOCHARNAMES -DNODECOMPOSE:	charprop.c
#	-DnoCJKcharmaps:		charcode.c mousemen.c
# ?	-DANSI:				io.c

(cd charmaps; cc -I.. -c *.c)

cc -o mined *.o charmaps/*.o -lcurses

