#############################################################################
# mined text editor compilation options (make include file for cc options)
# also non-gnu make options


#############################################################################
# binary target directory

SYS=` uname | sed -e 's,_.*,,' `
#ARCH=` uname -p 2> /dev/null | sed -e "s,unknown,," | grep . || uname -m | sed -e "s,/,_,g" `
ARCH=` uname -m | sed -e "s,/,_,g" `

#this doesn't work due to the $(OBJDIR)/... targets:
#OBJDIR=../bin/$(SYS).$(ARCH)

#call make OBJDIR=... to override:
OBJDIR=../bin

#or generate this:
include m-objdir.inc


#############################################################################
# compilation source options

# code checking
# gcc only: checking for function signatures
PROTOFLAGS=

# suppress prototype stuff for the sake of compilers and/or include files 
# that cannot handle it; keep that as a gcc-only feature for source checking
NOPROTOFLAG	= -DNOPROTO


#############################################################################
# compilation mode options (optimisation and debug)

# Optimization flag/level:
OPT	= -O

# Debugging option
DEBUG	= 


#############################################################################
# compilation setup

# Collection of compilation parameters:
CFLAGS	= $(SYSFLAGS) $(NOPROTOFLAG) $(OPT) $(DEBUG) ${CCFLAGS}


#############################################################################
# link options

LINKOPTS=$(LDOPTS)
# for newer ncursesw?
#LDL=-ldl	# if libdl library exists:
LDL=` echo "/usr/lib ${LDFLAGS} " | sed -e "s, -L, ,g" -e "s,\([^ ][^ ]*\),ls -1 \1/libdl.*;,g" | sh | sed -e "s,.*/libdl\..*,-ldl," -e 1q `


#############################################################################
# dynamic make targets
# this version for non-GNU make

# character maps dependencies and link objects
# for this makefile, the dependency is resolved by a generated makefile
CHARDEPS=
CHAROBJS=$(OBJDIR)/charmaps/*.o

# this dependency is ensured by a dynamically generated makefile
KEYMAPS=

# explicit make targets for dynamically .generating and invoking makefiles
MAKEMAPS=mkcharmaps mkkeymaps


#############################################################################
# end
