#############################################################################
# default: generate all
all:	mined help manual mnemodoc quotesdoc


#############################################################################
# generation
mined:
	sh ./makemined

mansi:
	sh ./makemined mansi

minced:
	sh ./makemined minced

help:	usrshare/package_doc/VERSION
	sh ./makemined help

mnemodoc:
	sh ./makemined mnemodoc

quotesdoc:
	sh ./makemined quotesdoc

generate:
	sh ./makemined generate

clean:
	sh ./makemined clean

manual:
	cd man; make

VER=$(shell sed -e 's,^[^0-9]*,,' -e 's,[^0-9][^0-9]*,.,g' -e q VERSION )
UNIVER=$(shell sed -e '/^\# Blocks-/ b ver' -e d -e ': ver' -e 's,^[^0-9]*\([0-9].*[0-9]\).*,\1,g' -e 's,\.0$$,,' src/Blocks.txt )
#VMSVER=$(shell echo $(VER) | sed -e 's,\.,_,g' )
VMSVER=$(shell sed -e 's,^[^0-9]*,,' -e 's,[^0-9][^0-9]*,.,g' -e 's,\.,_,g' -e q VERSION )

ver:
	echo mined $(VER)
	echo Unicode $(UNIVER)
	echo VMS $(VMSVER)

versionh:	# src/Blocks.txt # VERSION
	echo '#define VERSION "$(VER)"' > src/version.h.new
	echo '#define UNICODE_VERSION "$(UNIVER)"' >> src/version.h.new
	cmp src/version.h src/version.h.new || mv -f src/version.h.new src/version.h
	rm -f src/version.h.new

src/Blocks.txt:
	cd src; $(MAKE) -f mkinclud.mak Blocks.txt

usrshare/package_doc/VERSION:	VERSION
	echo $(VER) > usrshare/package_doc/VERSION

# "creole" file for sourceforge download page
CREOLE=readme.creole.$(VER)
creole:	$(CREOLE)

$(CREOLE):	CHANGES VERSION
	# title
	echo "== MinEd $(VER)" > $(CREOLE).new
	# subtitle
	sed -e "1,/href=#$(VER)/ d" -e "/href=#/,$$ d" -e "s,^	*<[^/]*>.ighlights [(-]*\([^)<:]*\).*,//\1//," -e t -e d CHANGES >> $(CREOLE).new
	# prefix
	(echo; echo ----; echo; echo "=== Highlights"; echo "// //") >> $(CREOLE).new
	# extract highlights
	sed -e "1,/href=#$(VER)/ d" -e "/href=#/,$$ d" -e "s,	<p[^/]*>\([^<]*\)</.*,==== \1," -e t -e "s,^		<li>,** ," -e t -e "s,^	<li>,* ," -e t -e "/^[ 	]*</ d" -e "s,^[ 	]*,," CHANGES | sed -e "s,~,~~,g" >> $(CREOLE).new
	# suffix
	(echo ----; echo) >> $(CREOLE).new
	(echo === Full documentation; echo "See [[http://mined.sf.net/|mined home page]], including:"; echo) >> $(CREOLE).new
	(echo ==== Further package download and build options; echo "Click **Download**."; echo) >> $(CREOLE).new
	(echo ==== Full changelog; echo "Click **Change Log**."; echo) >> $(CREOLE).new
	# move in place
	mv -f $(CREOLE).new $(CREOLE)
	rm -f readme.creole
	ln $(CREOLE) readme.creole


#############################################################################
# description
#SUMMARY=Powerful text editor with extensive Unicode and CJK support.
SUMMARY=$(shell sed -e 's,<meta *name="*description"* content="*\([^"]*\)"* *>,\1,' -e t -e d doc/overview.html )

descriptions:	description.summary description.medium description.long

description.summary:	doc/overview.html makefile
	echo $(SUMMARY) > description.summary

description.medium:	DESCR
	sed -n -e "/Overview/ q" -e p DESCR > description.medium
	echo "For an overview and further information see http://mined.sourceforge.net/" >> description.medium

description.long:	DESCR
	cp DESCR description.long


wiki:	mined.wiki

wikisrc=doc/overview.html doc/design.html
DATE=` sed -e "s,.*-> mined $(VER) (\(.*\)),\1," -e t -e d doc/changes.html `

mined.wiki:	$(wikisrc) wiki.sed wiki.add makefile
	sed -e "/^$$/,$$ d" -e "s,VERSION,$(VER)," -e "s,DATE,$(DATE)," wiki.add > mined.wiki
	cat $(wikisrc) | tidy -utf8 -o -wrap 999 2> /dev/null | sed -f wiki.sed >> mined.wiki
	sed -e "/^$$/,$$ b" -e d wiki.add >> mined.wiki

ann:	announcements

announcements:	DESCR doc/changes.html
	DATE=$(DATE) sh -c "cd announcement; ./makeann > announcement-update"
	DATE=$(DATE) sh -c "cd announcement; ./makeann cygwin > cygwin-announce"
	#cd announcement; ./makeupload > cygwin-upload


#############################################################################
# installation
install:
	sh ./makemined install root=$(root) prefix=$(prefix) lprefix=$(lprefix) rundir=$(rundir) lrundir=$(lrundir)

localinstall:
	sh ./makemined localinstall root=$(root) prefix=$(prefix) lprefix=$(lprefix) rundir=$(rundir) lrundir=$(lrundir)

optinstall:
	sh ./makemined optinstall root=$(root) prefix=$(prefix) lprefix=$(lprefix) rundir=$(rundir) lrundir=$(lrundir)


#############################################################################
# source distribution and private archives
beta:	all
	if egrep -e "^(printf|#define debug)" src/*.[hc]; then false; fi
	./makearch -

LSM=release/mined.lsm
SPEC=release/mined.spec


lsm:	$(LSM)

rpm:	$(SPEC)

$(SPEC):	DESCR makefile
	echo $(VER)
	echo "# partial spec file for mined" > $(SPEC)
	echo "Name:         mined" >> $(SPEC)
	echo "Group:        Productivity/Editors/Other" >> $(SPEC)
	echo "Version:      $(VER)" >> $(SPEC)
	echo "URL:          http://towo.net/mined/" >> $(SPEC)
	echo "Summary:      $(SUMMARY)" >> $(SPEC)
	echo "" >> $(SPEC)
	echo "%description" >> $(SPEC)
	sed -e "s,^,	," DESCR >> $(SPEC)
	echo "" >> $(SPEC)


distribution:	release lsm

.PHONY: release

dist:	release

release:	versionh check src/Blocks.txt tar

check:
	sh ./makemined checkall

tar:
	$(MAKE) generate
	$(MAKE) all
	$(MAKE) DESCR
	$(MAKE) release/mined-$(VER).tar.gz

release/mined-$(VER).tar.gz:	usrshare/help/mined.hlp doc/mnemodoc.html doc/quotesdoc.html man/*.1 mined
	$(MAKE) archive

archive:
	#sh ./makemined pc
	sh ./makemined
	sh ./makemined allchartabs
	sh ./makemined vms.com
	cmp usrshare/conf_user/minedrc usrshare/doc_user/.minedrc
	./makearch


#############################################################################
# specific distribution packages

TARUSER=--owner=mined --group=cygwin

packages:	cygwin

REL=$(shell sed -e 's,.*package *\([0-9]*\).*,\1,' -e 's,^$$,0,' -e q README.cygwin )

#ARCH=$(shell uname -m)
ARCH=$(shell uname -m | sed -e "s,i686,x86," )
#ARCHSUF=-$(ARCH)	# not an accepted convention for binary archive
ARCHSUF=

rel:
	echo cygwin package release $(REL)

cygrel=release/cygwin/$(ARCH)
setuphint=release/cygwin/setup.hint
#cygportfn=mined-$(VER)-$(REL).cygport
cygportfn=mined.cygport
cygport=release/cygwin/$(cygportfn)

cygwin:	release/mined-$(VER).tar.gz
	$(MAKE) cygwinbin
	# make src archive only once (cuts by failing on x86_64):
	uname -m | grep i686
	$(MAKE) cygwinsrc

#compress=bzip2
#compressuf=bz2
compress=xz -fv
compressuf=xz

cygwinsrc:	$(cygport) cygwinsrc-$(ARCH)

cygwinsrc-x86:
	mkdir -p $(cygrel)/
	echo making cygwin source package release $(REL) of mined $(VER)
	cd release/cygwin; tar xvfz ../mined-$(VER).tar.gz
	cd release/cygwin; tar cvf mined-$(VER)-$(REL)-src.tar $(TARUSER) mined.cygport mined-$(VER)
	cd release/cygwin; $(compress) mined-$(VER)-$(REL)-src.tar
	# move 32 bit source package into arch dir
	cd release/cygwin; uname -m | grep x86_64 || mv mined-$(VER)-$(REL)-src.tar.$(compressuf) $(ARCH)/

SRCREADME=release/cygwin/mined-x86_64-src.readme

cygwinsrc-x86_64:	$(SRCREADME)
	mkdir -p $(cygrel)/
	cd release/cygwin; tar cvf mined-$(VER)-$(REL)-src.tar $(TARUSER) mined.cygport mined-x86_64-src.readme
	cd release/cygwin; $(compress) mined-$(VER)-$(REL)-src.tar
	# move dummy source package into arch dir
	cd release/cygwin; mv mined-$(VER)-$(REL)-src.tar.$(compressuf) $(ARCH)/

$(SRCREADME):
	echo "For the source archive, see the 32 bit distribution." > $(SRCREADME)
	echo "This reference note saves precious upload bandwidth " >> $(SRCREADME)
	echo "as both source archive files would be identical." >> $(SRCREADME)

cygwinbin:	$(setuphint) cygwinexe
	mkdir -p $(cygrel)
	echo making cygwin package release $(REL) of mined $(VER)
	cd src; rm -fr tmp.cyg; mkdir -p tmp.cyg
	cd src; $(MAKE) -f makefile.cygwin install root=tmp.cyg
	cd src/tmp.cyg; tar cf mined-$(VER)-$(REL)$(ARCHSUF).tar $(TARUSER) *
	cd src/tmp.cyg; $(compress) mined-$(VER)-$(REL)$(ARCHSUF).tar
	mv src/tmp.cyg/mined-$(VER)-$(REL)$(ARCHSUF).tar.$(compressuf) $(cygrel)/

cygwinexe:
	cd src; $(MAKE) -f makefile.cygwin mined.exe

#cygport:	$(cygport) release/mined-$(VER).tar.gz cygport-build

#cygport-build:
#	ln release/mined-$(VER).tar.gz $(cygrel)/ || cp release/mined-$(VER).tar.gz $(cygrel)/
#	cd $(cygrel); cygport $(cygportfn) prep
#	cd $(cygrel); cygport $(cygportfn) compile
#	cd $(cygrel); cygport $(cygportfn) install
#	cd $(cygrel); cygport $(cygportfn) package
#	cd $(cygrel); cygport $(cygportfn) finish

$(cygport):	description.medium makefile
	mkdir -p $(cygrel)
	echo 'NAME="mined"' > $(cygport)
	echo 'VERSION="$(VER)"' >> $(cygport)
	echo 'RELEASE=$(REL)' >> $(cygport)
	echo 'CATEGORY="Editors"' >> $(cygport)
	echo 'SUMMARY="$(SUMMARY)"' >> $(cygport)
	echo 'DESCRIPTION="' | tr -d '\012' >> $(cygport)
	cat description.medium >> $(cygport)
	echo '"' >> $(cygport)
	echo '' >> $(cygport)
	echo 'HOMEPAGE="http://mined.sf.net/"' >> $(cygport)
	echo 'SRC_URI="http://towo.net/mined/download/mined-$${PV}.tar.gz"' >> $(cygport)
	echo '' >> $(cygport)
	echo '#DEPEND="libncurses-devel"	# does not depend technically' >> $(cygport)
	echo '#DEPEND="libncurses-devel zip subversion gettext" # to build stand-alone version' >> $(cygport)
	echo '' >> $(cygport)
	echo 'src_compile() {' >> $(cygport)
	echo '  lndirs' >> $(cygport)
	echo '  cd $${B}' >> $(cygport)
	echo '  cygmake' >> $(cygport)
	echo '}' >> $(cygport)

$(setuphint):	description.medium makefile
	mkdir -p $(cygrel)
	echo '# cygwin setup file' > $(setuphint)
	echo 'sdesc: $(SUMMARY)' >> $(setuphint)
	echo 'category: Editors' >> $(setuphint)
	echo 'requires: cygwin' >> $(setuphint)
	echo 'ldesc: "' >> $(setuphint)
	cat description.medium >> $(setuphint)
	echo '"' >> $(setuphint)

$(LSM):	DESCR makefile # release/mined-$(VER).tar.gz
	mkdir -p release
	echo $(VER)
	echo "Begin4" > $(LSM)
	echo "Title:		mined" >> $(LSM)
	echo "Version:	$(VER)" >> $(LSM)
	echo "Entered-date:	`date +%Y-%m-%d`" >> $(LSM)
	echo "Description:	$(SUMMARY)" >> $(LSM)
	sed -e "s,^,	," -e "s,:,;,g" DESCR >> $(LSM)
	echo "Keywords:	text editor, Unicode editor, UTF-8 editor, CJK editor" >> $(LSM)
	echo "Author:		mined@towo.net (Thomas Wolff)" >> $(LSM)
	echo "Primary-site:	http://towo.net/mined/" >> $(LSM)
	(cd release; ls -l mined-$(VER).tar.gz) | sed -e "s,[^ ]* *[^ ]* *[^ ]* *[^ ]* *\([^ ]*\).* \([^ ]*\)$$,		\1 \2," >> $(LSM)
	echo "Alternate-site:	http://www.ibiblio.org/pub/Linux/apps/editors/mined-$(VER).tar.gz" >> $(LSM)
	echo "Platforms:	Unix (Linux/Sun/HP/BSD/Mac and more), DOS (djgpp), Windows (cygwin)" >> $(LSM)
	echo "Copying-policy:	GPL with comments" >> $(LSM)
	echo "End" >> $(LSM)

# NetBSD pkgsrc entry
DESCR:	doc/overview.html descr.sed
	sed -f descr.sed doc/overview.html | sed -e "/./ b" -e "$$ d" > DESCR


# PC binary archives (development target, doesn't currently work locally)
bin:	win dos

win:	release/MinEd-$(VER)-install.exe

dos:	src/dj/mined.exe release/mined-$(VER)-dos.zip

src/dj/mined.exe:
	mkdir src/dj || true
	cp src/MINED.EXE src/dj/mined.exe

vms:	src/vms/vax/mined.exe src/vms/alpha/mined.exe src/vms/ia64/mined.exe release/MINED-$(VMSVER)-VMS.ZIP

release/MINED-$(VMSVER)-VMS.ZIP:
	mkdir -p src/vms
	cp README.vms src/vms/mined.readme
	cp usrshare/help/mined.hlp src/vms/
	cp usrshare/conf_user/MINED-VMS.COM src/vms/mined-vms.com
	cd src/vms; zip ../../release/MINED-$(VMSVER)-VMS.ZIP mined.readme mined.hlp mined-vms.com */mined.exe

release/MinEd-$(VER)-install.exe:	release/mined-$(VER)-windows.zip
	uname -m | grep i686
	rm -fr release/tmp.win
	mkdir -p release/tmp.win
	sed -e "s,%version%,$(VER)," makewinx.cfg > release/tmp.win/mined.SED
	cd release/tmp.win; unzip ../mined-$(VER)-windows.zip
	cd release/tmp.win; cp /bin/mkshortcut.exe .
	cd release/tmp.win; cp /bin/cygpopt-0.dll .
	cd release/tmp.win; cp /bin/cygstart.exe .
	cd release/tmp.win; cp /bin/cygpath.exe .
	cd release/tmp.win; cp /bin/regtool.exe .
	cd release/tmp.win; cp /bin/dash.exe .
	cd release/tmp.win; iexpress /n mined.SED
	mv -f release/tmp.win/MinEd-$(VER)-install.exe release/

winbin:
	cd src; $(MAKE) win

release/mined-$(VER)-windows.zip:	winbin usrshare/help/mined.hlp README.windows
	mkdir -p release
	cd bin/win; chmod +x *.exe *.dll
	cd bin/win; zip mined-$(VER)-windows.zip *.exe *.dll
	mv -f bin/win/mined-$(VER)-windows.zip release/
	cd usrshare/help; zip ../../release/mined-$(VER)-windows.zip mined.hlp
	cd usrshare/setup_install; zip ../../release/mined-$(VER)-windows.zip mined.ico
	cd usrshare/setup_install/win; zip ../../../release/mined-$(VER)-windows.zip *.* -x *~
	cp -fp README.windows release/README.txt
	cp -fp /usr/share/locale/locale.alias release/
	cd release; zip mined-$(VER)-windows.zip README.txt locale.alias
	cd release; rm -f README.txt locale.alias

release/mined-$(VER)-dos.zip:	src/dj/mined.exe usrshare/help/mined.hlp README.dos
	mkdir -p release
	cd src/dj; chmod +x mined.exe
	cd src/dj; zip mined-$(VER)-dos.zip mined.exe
	mv -f src/dj/mined-$(VER)-dos.zip release/
	cd usrshare/help; zip ../../release/mined-$(VER)-dos.zip mined.hlp
	cp -fp README.dos release/mined.txt
	cd release; zip mined-$(VER)-dos.zip mined.txt
	rm -f release/mined.txt

# Development repository archive (development target)
TARDIR=$(shell basename ${PWD})

tarup:
	cd ..; tar cvfz $(TARDIR).tgz $(TARUSER) --exclude=$(TARDIR)/attic --exclude=$(TARDIR)/src/etc/unidata --exclude=$(TARDIR)/src/etc/charinput --exclude="*.o" --exclude="*/mined" $(TARDIR)


#############################################################################
# end
