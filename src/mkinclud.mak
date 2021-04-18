#############################################################################
# mined text editor make actions (make include file for make targets)


#############################################################################
# components to be compiled

# Mined modules
PROGOBJS = \
	$(OBJDIR)/minedaux.o $(OBJDIR)/legacy.o \
	$(OBJDIR)/mined1.o $(OBJDIR)/textfile.o \
	$(OBJDIR)/mousemen.o \
	$(OBJDIR)/edit.o $(OBJDIR)/pastebuf.o $(OBJDIR)/textbuf.o \
	$(OBJDIR)/justify.o $(OBJDIR)/search.o $(OBJDIR)/charprop.o \
	$(OBJDIR)/output.o $(OBJDIR)/prompt.o $(OBJDIR)/compose.o \
	$(OBJDIR)/charcode.o \
	$(OBJDIR)/keymaps.o $(OBJDIR)/keydefs.o $(OBJDIR)/dispatch.o \
	$(OBJDIR)/termprop.o $(OBJDIR)/width.o $(OBJDIR)/encoding.o \
	$(OBJDIR)/handescr.o
ALLPROGOBJS = $(PROGOBJS) $(OBJDIR)/timestmp.o
#OBJS = $(PROGOBJS) $(CHARMAPS)

# make target parameters that may be overriden in makefile
# (could be set to default with the ?= syntax but that doesn't work 
# in all versions of make)

SCREENOBJ=$(OBJDIR)/io.o $(OBJDIR)/keyboard.o
SCREENOBJANSI=$(OBJDIR)/ioansi.o $(OBJDIR)/keyboard.o
SCREENOBJCURS=$(OBJDIR)/iocurses.o $(OBJDIR)/keycurs.o

MOUSELIB=

#KEYMAPSDEP?=keymaps.t


#############################################################################
# commands

#WGET=curl -R -O --connect-timeout 55
WGET=wget -N -t 1 --timeout=55

SH=sh
#SH=${SHELL}


#############################################################################
# target properties, default target, auxiliary targets

# functional or abbreviated (non-file) targets:
.PHONY:	mkcharmaps mkkeymaps mnemodoc quotesdoc help man bin install localinstall optinstall links clean clear debianclean update all clean_unidata clean_unihan clean_udata vni viqr vtelex

# Default make target:
all:	$(MAKEMAPS) mined

DOC=../usrshare/doc_user


#############################################################################
# Unicode data tables:

# With Unicode 7.0, there is no UCD.zip anymore, so downloaded separately
#UCD.zip:
#	echo Trying to retrieve Unicode data file via Internet
#	$(WGET) http://unicode.org/Public/UNIDATA/UCD.zip
#UnicodeData.txt:	UCD.zip
#	unzip UCD UnicodeData.txt
#Scripts.txt:	UCD.zip
#	unzip UCD Scripts.txt
#Blocks.txt:	UCD.zip
#	unzip UCD Blocks.txt
#SpecialCasing.txt:	UCD.zip
#	unzip UCD SpecialCasing.txt
#PropList.txt:	UCD.zip
#	unzip UCD PropList.txt
#EastAsianWidth.txt:	UCD.zip
#	unzip UCD EastAsianWidth.txt
#PropertyValueAliases.txt:	UCD.zip
#	unzip UCD PropertyValueAliases.txt
#DerivedBidiClass.txt:	UCD.zip
#	unzip -j UCD extracted/DerivedBidiClass.txt
#NameAliases.txt:	UCD.zip
#	unzip UCD NameAliases.txt

%.txt:
	$(WGET) http://unicode.org/Public/UNIDATA/$@

Unihan.zip:
	echo Trying to retrieve Unicode data file via Internet
	$(WGET) http://unicode.org/Public/UNIDATA/Unihan.zip

BIG5.TXT:
	echo Trying to retrieve Unicode data file via Internet
	$(WGET) http://unicode.org/Public/MAPPINGS/OBSOLETE/EASTASIA/OTHER/BIG5.TXT

check_ccc:
	grep "^ccc; *230;.*; *Above$$" PropertyValueAliases.txt

clean_unidata:	UnicodeData.txt SpecialCasing.txt Scripts.txt PropList.txt PropertyValueAliases.txt check_ccc
	rm -f casetabl.t casespec.t softdot.t combin.t
	rm -f scripts.t scriptdf.t charname.t charseqs.t categors.sed categors.t
	rm -f UniWITH

clean_unihan:	Unihan.zip
	echo "Regenerate Han info data (may take long time)? (yes/n) " | tr -d '\n' ; read a; [ "$$a" != yes ] || rm -f handescr.t handescr/descriptions.uni
	echo "Regenerate Radical/Stroke input method (may take very long time)? (yes/n) " | tr -d '\n' ; read a; [ "$$a" != yes ] || rm -f keymaps?/Radical_Stroke.h

clean_udata:
	rm -f udata_* udata-*

uniset:	uniset.tar.gz
	gzip -dc uniset.tar.gz | tar xvf - uniset

uniset.tar.gz:
	$(WGET) http://www.cl.cam.ac.uk/~mgk25/download/uniset.tar.gz

#WIDTH-A:	uniset.tar.gz
#	tar xvzf uniset.tar.gz WIDTH-A

WIDTH-A:	EastAsianWidth.txt
	$(SH) ./mkwidthA


udata:	udata_combining.t udata_spacingcombining.t udata_ambiguous.t udata_assigned.t
	cat udata_combining.t udata_spacingcombining.t udata_assigned.t > width.t.add

univer = `sed -e '/^\# Blocks-/ s,[^0-9],,g' -e t -e d Blocks.txt`
univer_prev = `sed -e '/^\# Blocks-/ s,[^0-9],,g' -e t -e d unicode-previous/Blocks.txt`

# (Blocks.txt is a dummy dependency of uniset)

# Hangul JUNGSEONG and JONGSEONG combining characters:
#Unicode < 5.2: extracomb=+1160-11FF
extracomb=+1160-11FF +D7B0-D7C6 +D7CB-D7FB

udata_combining.t:	UnicodeData.txt Blocks.txt
	echo > udata_combining.t
	echo "static struct interval" >> udata_combining.t
	echo "combining_$(univer) [] =" >> udata_combining.t
	uniset +cat=Me +cat=Mn +cat=Cf -00AD $(extracomb) +200B c >> udata_combining.t

udata_ambiguous.t:	UnicodeData.txt Blocks.txt WIDTH-A
	echo > udata_ambiguous.t
	echo "static struct interval" >> udata_ambiguous.t
	echo "ambiguous_$(univer) [] =" >> udata_ambiguous.t
	uniset +WIDTH-A -cat=Me -cat=Mn -cat=Cf c >> udata_ambiguous.t

udata_spacingcombining.t:	UnicodeData.txt Blocks.txt
	echo > udata_spacingcombining.t
	echo "static struct interval" >> udata_spacingcombining.t
	echo "spacing_combining_$(univer) [] =" >> udata_spacingcombining.t
	uniset +cat=Mc c >> udata_spacingcombining.t

# not strictly needed for current Unicode version; 
# same range of characters as listed in scripts.t
udata_assigned.t:	UnicodeData.txt Blocks.txt
	echo > udata_assigned.t
	echo "static struct interval" >> udata_assigned.t
	echo "assigned_$(univer) [] =" >> udata_assigned.t
	uniset +0000-10FFFF clean c >> udata_assigned.t

udata-update:	Blocks.txt udata-add_assigned udata-add_combining WIDTH-A udata-add_ambiguous udata-add_right-to-left

unicode-previous/WIDTH-A:
	(cd unicode-previous; ${PWD}/mkwidthA)

udata-add_assigned:
	(cd unicode-previous >&2; uniset +0000-10FFFF clean table) > udata-old_assigned
	uniset +0000-10FFFF clean table > udata-new_assigned
	uniset +udata-new_assigned -udata-old_assigned ucs table > udata-add_assigned

udata-add_combining:
	(cd unicode-previous >&2; uniset +cat=Me +cat=Mn +cat=Cf -00AD +1160-11FF +200B table) > udata-old_combining
	uniset +cat=Me +cat=Mn +cat=Cf -00AD +1160-11FF +200B table > udata-new_combining
	uniset +udata-new_combining -udata-old_combining ucs table > udata-add_combining

udata-add_ambiguous:	WIDTH-A unicode-previous/WIDTH-A
	(cd unicode-previous >&2; uniset +WIDTH-A -cat=Me -cat=Mn -cat=Cf table) > udata-old_ambiguous
	uniset +WIDTH-A -cat=Me -cat=Mn -cat=Cf table > udata-new_ambiguous
	uniset +udata-new_ambiguous -udata-old_ambiguous ucs table > udata-add_ambiguous

udata-add_right-to-left:
	(cd unicode-previous >&2; sed -e 's-^\([^;]*\);\([^;]*\);\([^;]*\);\([^;]*\);R;.*-	\1-' -e t -e 's-^\([^;]*\);\([^;]*\);\([^;]*\);\([^;]*\);AL;.*-	\1-' -e t -e d UnicodeData.txt | uniset +- table) > udata-old_right-to-left
	sed -e 's-^\([^;]*\);\([^;]*\);\([^;]*\);\([^;]*\);R;.*-	\1-' -e t -e 's-^\([^;]*\);\([^;]*\);\([^;]*\);\([^;]*\);AL;.*-	\1-' -e t -e d UnicodeData.txt | uniset +- table > udata-new_right-to-left
	uniset +udata-new_right-to-left -udata-old_right-to-left ucs table > udata-add_right-to-left


u/full-unicode.txt:	UnicodeData.txt Blocks.txt
	(uniset +0000-FFFF utf8-list ; uniset +10000..10FFFF clean utf8-list) > u/full-unicode.txt


# Vietnamese input methods:

UniWITH:	UnicodeData.txt
	sed -e "s,^\([^;]*\);\([^;]* LETTER [^;]* WITH [^;]*\);.*,U+\1	\2 AND," -e t -e d UnicodeData.txt | CC=$(CC) $(SH) ./insutf8 > UniWITH

vni:	keymaps/VNI.h

categors.sed:	mkcategs # PropertyValueAliases.txt
	$(SH) ./mkcategs -sed

categors.t:	mkcategs # PropertyValueAliases.txt
	$(SH) ./mkcategs -h

keymaps/VNI.h:	UniWITH vni.sev vni.sed vni.seh
	mkdir -p keymaps
	(sed -f vni.sev UniWITH ; cat UniWITH) | LC_ALL=C sed -f vni.sed | sed -f vni.seh > keymaps/VNI.h

viqr:	keymaps/VIQR.h

keymaps/VIQR.h:	keymaps/VNI.h
	mkdir -p keymaps
	sed -e "s,VNI,VIQR," keymaps/VNI.h | LC_ALL=C tr "6879125340" "^(+d\'\`.?~-" > keymaps/VIQR.h

vtelex:	keymaps/Vtelex.h

keymaps/Vtelex.h:	keymaps/VNI.h
	mkdir -p keymaps
	sed -e "s,VNI,Vtelex," -e "s,\([Aa]\)6,\1a," -e "s,\([Ee]\)6,\1e," -e "s,\([Oo]\)6,\1o," keymaps/VNI.h | LC_ALL=C tr "879125340" "wwdsfjrxz" > keymaps/Vtelex.h

# Case conversion tables:

casetabl.t:	mkcasetb # UnicodeData.txt
	$(SH) ./mkcasetb

casespec.t:	mkcasesp # SpecialCasing.txt
	$(SH) ./mkcasesp

# Unicode character information tables:
wide.t:	mkwidthW # EastAsianWidth.txt
	$(SH) ./mkwidthW

softdot.t:	mkpropl # PropList.txt
	$(SH) ./mkpropl Soft_Dotted > softdot.t

combin.t:	mkcombin # UnicodeData.txt
	CC=$(CC) $(SH) ./mkcombin

typoprop.t:	mkpropl # PropList.txt
	$(SH) ./mkpropl Quotation_Mark > typoprop.t
	$(SH) ./mkpropl Dash >> typoprop.t
	$(SH) ./mkpropl Ps >> typoprop.t

#scriptdf.t and scripts.t are made together by mkscript2
scriptdf.t:	scripts.t	# mkscript2 scripts.tt

scripts.t:	mkscript mkscript2 categors.sed # Scripts.txt
	CC=$(CC) $(SH) ./mkscript
	$(SH) ./mkscript2
	rm -f scripts.tt

charname.t:	mkchname # UnicodeData.txt
	CC=$(CC) $(SH) ./mkchname

charseqs.t:	mkchseqs # UnicodeData.txt
	CC=$(CC) $(SH) ./mkchseqs

decompos.t:	mkdecompose # UnicodeData.txt
	CC=$(CC) $(SH) ./mkdecompose

# from:		Greek		124 64
# generate:		{"Greek", 124, 64},
colours.t:	colours.cfg # mkinclud.mak
	sed -e 's/^[ 	"]*\([A-Z][A-Za-z_]*\)[ 	,"]*\([0-9][0-9]*\)[	 ][	 ]*\([0-9][0-9]*\).*/	{"\1", \2, \3},/' -e t -e 's/^[ 	"]*\([A-Z][A-Za-z_]*\)[ 	,"]*\([0-9][0-9]*\).*/	{"\1", \2, -1},/' -e t -e d colours.cfg | sort > colours.t

# quotation marks styles:
quotes.t:	quotes.cfg mkquotes
	$(SH) ./mkquotes quotes.cfg > quotes.t
	# to keep web doc consistent:
	$(SH) ./mkquotesdoc quotes.cfg > $(DOC)/quotesdoc.html

# Unihan database dependent information:
unihan:	handescr.t radical_stroke
	echo "mkkbmap Cangjie [+]"

# Unihan character description table:
handescr.t:	# handescr/mkdescriptions handescr/descriptions.sed handescr/descriptions.uni # Unihan.zip
	cd handescr && $(MAKE) descriptions.h
	ln handescr/descriptions.h handescr.t || cp handescr/descriptions.h handescr.t
	ln handescr/Radical_Stroke.h keymaps0/ || cp handescr/Radical_Stroke.h keymaps0/

# Radical/Stroke input method table:
radical_stroke:	keymaps/Radical_Stroke.h

keymaps/Radical_Stroke.h:	# Unihan.zip
	cd handescr && $(MAKE) Radical_Stroke.h
	ln handescr/Radical_Stroke.h keymaps/ || cp handescr/Radical_Stroke.h keymaps/

# Radical/Stroke input method table:
cangjie:	keymaps/Cangjie.h

keymaps/Cangjie.h:	# Unihan.zip etc/charmaps/hkscs/hkscs-2004-cj.txt
	$(SH) ./mkkbmap cj +

# Keyboard mapping table configuration:

# source lines: Greek	GreekMonotonic	gr	(Y)
# check input method tag ("gr", ...) for uniqueness
getkeymapstags = sed -e '/^\#/ d' -e '/^--/ d' -e t -e 's/^[ 	"]*\([^	]*\)		*\([A-Za-z0-9_][^ 	]*\)		*\([^ 	][^ 	]\).*/\3/' -e t -e d keymaps.cfg
checkkeymapstags:	keymaps.cfg
	LC_ALL=C $(getkeymapstags) | sort | uniq -d | sed -e "s,^,[41m," -e "s,$$,[0m," | if grep .; then echo ERROR: duplicate tags in keymaps.cfg; false; else true; fi

filterCJK=-e "/^-.*Japanese/,/^$$/ b cjk" -e "/^-.*Chinese/,/^$$/ b cjk" -e "/^-.*Korean/,/^$$/ b cjk" -e "b nocjk" -e ": cjk" -e "/^-/ s,.*,\#ifdef use_CJKkeymaps," -e "/^$$/ s,,\#endif," -e t -e ": nocjk"

# from:		Greek	GreekMonotonic	gr	(Y)
# generate:		{"Greek", select_keymap_entry, "gr", keymapon, "Y"},
# from:		---- Group
# generate:		{"Group", separator, ""},
keymapsm.t:	keymaps.cfg # mkinclud.mak
	$(MAKE) -f mkinclud.mak checkkeymapstags
	LC_ALL=C sed -e '/^#/ d' -e 's/^--* *\(.*\) */	{"\1", separator, ""},/' -e t -e 's/^[ 	"]*\([^	]*\)		*\([A-Za-z0-9_][^ 	]*\)		*\([^ 	][^ 	]\)		*(*\([^)	 ]*\))*.*/	{"\1", select_keymap_entry, "\3", keymapon, "\4"},/' -e t -e 's/^[ 	"]*\([^	]*\)		*\([A-Za-z0-9_][^ 	]*\).*\([^ 	][^ 	]\).*/	{"\1", select_keymap_entry, "\3", keymapon},/' -e t -e d keymaps.cfg > keymapsm.t

# from:		Greek	GreekMonotonic	gr	(Y)
# generate:		{keymap_GreekMonotonic, "gr"},
keymapsk.t:	keymaps.cfg # mkinclud.mak
	LC_ALL=C sed $(filterCJK) -e '/^[-#]/ d' -e 's/^[ 	"]*\([^	]*\)		*\([A-Za-z0-9_][^ 	]*\)		*\([^ 	][^ 	]\).*/	{keymap_\2, "\3"},/' -e t -e d keymaps.cfg > keymapsk.t

# from:		Greek	GreekMonotonic	gr	(Y)
# generate:	#include "GreekMonotonic.h"
keymapsi.h:	keymaps.cfg # mkinclud.mak
	LC_ALL=C sed $(filterCJK) -e '/^[-#]/ d' -e 's/^[ 	"]*\([^	]*\)		*\([A-Za-z0-9_][^ 	]*\)		*\([^ 	][^ 	]\).*/#include "\2.h"/' -e t -e d keymaps.cfg > keymapsi.h

# from:		Greek	GreekMonotonic	gr	(Y)
# generate:	#include "GreekMonotonic.c"
keymapsc.h:	keymaps.cfg # mkinclud.mak
	LC_ALL=C sed $(filterCJK) -e '/^[-#]/ d' -e 's/^[ 	"]*\([^	]*\)		*\([A-Za-z0-9_][^ 	]*\)		*\([^ 	][^ 	]\).*/#include "\2.c"/' -e t -e d keymaps.cfg > keymapsc.h

# from:		Greek	GreekMonotonic	gr	(Y)
# generate:	extern keymaptableelem keymap_GreekMonotonic [];
keymapsx.h:	keymaps.cfg # mkinclud.mak
	LC_ALL=C sed $(filterCJK) -e '/^[-#]/ d' -e 's/^[ 	"]*\([^	]*\)		*\([A-Za-z0-9_][^ 	]*\)		*\([^ 	][^ 	]\).*/extern keymaptableelem keymap_\2 [];/' -e t -e d keymaps.cfg > keymapsx.h

# Character mapping configuration:

# check input method tag ("gr", ...) for uniqueness
checkcharmapstags:	charmaps.cfg mkchrcfg
	$(SH) ./mkchrcfg checktags

charmaps.t:	charmaps.cfg mkchrcfg
	$(MAKE) -f mkinclud.mak checkcharmapstags
	$(SH) ./mkchrcfg charmaps.t

charmaps.h:	charmaps.cfg mkchrcfg
	$(SH) ./mkchrcfg charmaps.h

charemen.t:	charmaps.cfg mkchrcfg
	$(SH) ./mkchrcfg charemen.t

charesub.t:	charmaps.cfg mkchrcfg
	$(SH) ./mkchrcfg charesub.t

# Mapping table locale (substring) -> charmap (encoding name)
# from:
#	ar_IN	UTF-8
#	ar	ISO-8859-6	MacArabic
# generate:
#		{"ar", "ISO-8859-6", "MacArabic"},
#		{"ar_IN", "UTF-8"},
locales.t:	locales.cfg # mkinclud.mak
	sed -e '/^$$/ d' -e '/^#/ d' -e 's/\([^ 	][^ 	]*\)[ 	][ 	]*\([^ 	][^ 	]*\)[ 	]*\([^ 	]*\)/	{"\1", "\2", "\3"},/' -e 's/, ""//' locales.cfg | LC_ALL=C sort > locales.t

# Find supplements for locales.cfg:
localealiases:	/usr/share/locale/locale.alias locales.cfg
	sed -e "/^#/ d" -e "/8859-1$$/ d" -e "s,\([^ 	]*\)[ 	][ 	]*\([^ 	]*\),\1	\2," -e "s,	[^.]*\.,	," -e "s,	euc,	EUC-," /usr/share/locale/locale.alias | fgrep -a -v -x -f locales.cfg

# HTML character mnemonics:
zeichen.htm:
	$(WGET) http://de.selfhtml.org/html/referenz/zeichen.htm

mnemos.com:	mkaccent accents.cfg # UnicodeData.txt
	CC=$(CC) $(SH) ./mkaccent -

mnemos.ara:	mkaccent accents.cfg # UnicodeData.txt
	CC=$(CC) $(SH) ./mkaccent Arabic

mnemos.cyr:	mkaccent accents.cfg # UnicodeData.txt
	CC=$(CC) $(SH) ./mkaccent Cyrillic

mnemos.grk:	mkaccent accents.cfg # UnicodeData.txt
	CC=$(CC) $(SH) ./mkaccent Greek

mnemos.heb:	mkaccent accents.cfg # UnicodeData.txt
	CC=$(CC) $(SH) ./mkaccent Hebrew

mnemos.lat:	mkaccent accents.cfg # UnicodeData.txt
	CC=$(CC) $(SH) ./mkaccent Latin

mnemos.www:	mnemos.tex mkmnhtml # zeichen.htm
	CC=$(CC) $(SH) ./mkmnhtml zeichen.htm | fgrep -v -x -f mnemos.tex > mnemos.www
	$(SH) ./mkmnemocheck

mnemos.rof:
	CC=$(CC) $(SH) ./mkmnemoroff > mnemos.rof


#############################################################################
# generate tables for character encodings and keyboard mappings

# rules for non-GNU make:

# generate makefile to deal with variable list of targets,
# invoke it to generate and compile character mapping tables:
mkcharmaps:
	echo SH=$(SH) > charmaps/makefile
	CC=$(CC) OPT="$(OPT)" $(SH) ./mkmakchr -O $(OBJDIR) >> charmaps/makefile
	$(MAKE) -f charmaps/makefile OBJDIR=$(OBJDIR)

# extract list of configured keyboard mappings from keymaps.cfg:
getKEYMAPS=`$(SH) ./mkkmlist`

# generate makefile to deal with variable list of dependencies,
# invoke it to generate cumulative keymaps file:
# (note, this does not work on Ultrix: test -d keymaps || mkdir keymaps)
mkkeymaps:
	if [ ! -d keymaps ]; then mkdir keymaps; fi
	echo SH=$(SH) > keymaps/makefile
	echo keymaps.t: keymaps.cfg $(getKEYMAPS) mkkmincl >> keymaps/makefile
	echo "	CC=$(CC) $(SH) ./mkkmincl $(getKEYMAPS) > keymaps.t" >> keymaps/makefile
	$(MAKE) -f keymaps/makefile


# extract configured keyboard mappings from keymaps.cfg
listkeymaps=LC_ALL=C sed -e '/^[-\#]/ d' -e 's,^[ 	"]*\([^	]*\)		*\([A-Za-z0-9_][^ 	]*\).*\([^ 	][^ 	]\).*,keymaps1/\2,' -e t -e d keymaps.cfg

keymaps1/makefile:	keymaps.cfg
	echo SH=$(SH) > keymaps1/makefile
	echo keymapsc:	`$(listkeymaps) | sed -e "s,$$,.c,"` >> keymaps1/makefile
	echo '' >> keymaps1/makefile
	echo keymapso:	`$(listkeymaps) | sed -e "s,$$,.o,"` >> keymaps1/makefile
	echo '' >> keymaps1/makefile
	echo 'CC=$(CC)' >> keymaps1/makefile
	echo '' >> keymaps1/makefile
	echo 'keymaps1/%.o:	keymaps1/%.c' >> keymaps1/makefile
	echo '	$$(CC) $(OPT) -c $$< -o keymaps1/$$*.o' >> keymaps1/makefile
	echo '' >> keymaps1/makefile
	echo 'keymaps1/%.c:	keymaps0/%.h mkkmincl' >> keymaps1/makefile
	echo '	CC=$$(CC) $(SH) ./mkkmincl $$< > keymaps1/$$*.c' >> keymaps1/makefile

# rules for GNU make:

# KEYMAPS is extracted list of configured keyboard mappings from keymaps.cfg:
# (note, this does not work on Ultrix: test -d keymaps || mkdir keymaps)
keymaps.t:	$(KEYMAPS) keymaps.cfg mkkmincl # keymaps1/makefile
	#$(MAKE) -f keymaps1/makefile keymapsc
	if [ ! -d keymaps ]; then mkdir keymaps; fi
	CC=$(CC) $(SH) ./mkkmincl $(KEYMAPS) > keymaps.t

# define generic rule to generate and compile character mapping tables:
#$(OBJDIR)/charmaps/%.o:	charmaps/%.map charcode.h mkchrtab
#	CC=$(CC) OPT="$(OPT)" $(SH) ./mkchrtab -O $(OBJDIR) $<

# rather do it in 2 steps to facilitate generation with non-Unix make 
# after manual copying cross-generated .c files:
# generate character mapping tables:
charmaps/%.c:	charmaps/%.map charcode.h mkchrtab
	CC=$(CC) OPT="$(OPT)" $(SH) ./mkchrtab -S $<

# compile character mapping tables:
$(OBJDIR)/charmaps/%.o:	charmaps/%.c
	$(CC) $(OPT) -I. -c $< -o $(OBJDIR)/charmaps/$*.o


#############################################################################
# Character input support tables:

MNEMOS=mnemos.???
#only understood by gmake:
#MNEMOS=mnemos.??[!~]

mnemos.t:	$(MNEMOS)
#	cat $(MNEMOS) > mnemos.t
	ls -1 $(MNEMOS) | sed -e 's,^,#include ",' -e 's,$$,",' > mnemos.t

# Documentation page for mnemonics (listing mined additional mnemos)
mnemodoc:	$(DOC)/mnemodoc.html
$(DOC)/mnemodoc.html:	mnemos.sup $(DOC)/mined.html mkmnemodoc
	CC=$(CC) $(SH) ./mkmnemodoc > $(DOC)/mnemodoc.html.new
	/bin/mv $(DOC)/mnemodoc.html.new $(DOC)/mnemodoc.html
	$(SH) ./mkmnemocheck


#############################################################################
# Smart quotes

# Documentation page for quotes styles
quotesdoc:	$(DOC)/quotesdoc.html
$(DOC)/quotesdoc.html:	quotes.cfg mkquotesdoc
	$(SH) ./mkquotesdoc quotes.cfg > $(DOC)/quotesdoc.html


#############################################################################
# Location of online help file:

DHELP=-DRUNDIR=\"$(rundir)\" -DLRUNDIR=\"$(lrundir)\"

# Version/About text:
#VERSION=`sed -e 's,^[^0-9]*,,' -e 's,[^0-9][^0-9]*,.,g' -e q ../VERSION`
#ABOUT=\"MinEd $(VERSION) - http://mined.sourceforge.net/\"


#############################################################################
# Source compilation:

$(OBJDIR)/mined1.o:	version.h mined1.c textfile.h encoding.h locales.t quotes.t mined.h io.h termprop.h
	$(CC) $(CFLAGS) $(PROTOFLAGS) $(DHELP) -c mined1.c -o $(OBJDIR)/mined1.o
$(OBJDIR)/minedaux.o:	version.h minedaux.c mined.h io.h
	$(CC) $(CFLAGS) $(PROTOFLAGS) $(DHELP) -c minedaux.c -o $(OBJDIR)/minedaux.o
$(OBJDIR)/textfile.o:	textfile.c textfile.h mined.h charprop.h termprop.h
	$(CC) $(CFLAGS) $(PROTOFLAGS) -c textfile.c -o $(OBJDIR)/textfile.o
$(OBJDIR)/textbuf.o:	textbuf.c mined.h
	$(CC) $(CFLAGS) $(PROTOFLAGS) -c textbuf.c -o $(OBJDIR)/textbuf.o
$(OBJDIR)/justify.o:	justify.c mined.h
	$(CC) $(CFLAGS) $(PROTOFLAGS) -c justify.c -o $(OBJDIR)/justify.o
$(OBJDIR)/edit.o:	edit.c mined.h io.h charprop.h termprop.h
	$(CC) $(CFLAGS) $(PROTOFLAGS) -c edit.c -o $(OBJDIR)/edit.o
$(OBJDIR)/charprop.o:	charprop.c charprop.h casespec.t casetabl.t softdot.t combin.t wide.t charname.t charseqs.t scripts.t scriptdf.t # mined.h
	$(CC) $(CFLAGS) $(PROTOFLAGS) -c charprop.c -o $(OBJDIR)/charprop.o
$(OBJDIR)/pastebuf.o:	pastebuf.c mined.h charprop.h io.h
	$(CC) $(CFLAGS) $(PROTOFLAGS) -c pastebuf.c -o $(OBJDIR)/pastebuf.o
$(OBJDIR)/legacy.o:	legacy.c
	$(CC) $(CFLAGS) -c legacy.c -o $(OBJDIR)/legacy.o
$(OBJDIR)/search.o:	search.c mined.h io.h
	$(CC) $(CFLAGS) $(PROTOFLAGS) -c search.c -o $(OBJDIR)/search.o
$(OBJDIR)/mousemen.o:	mousemen.c mined.h io.h charcode.h keymapsm.t charemen.t charesub.t termprop.h
	$(CC) $(CFLAGS) $(PROTOFLAGS) -c mousemen.c -o $(OBJDIR)/mousemen.o
$(OBJDIR)/output.o:	output.c colours.t mined.h io.h termprop.h
	$(CC) $(CFLAGS) $(PROTOFLAGS) -c output.c -o $(OBJDIR)/output.o
$(OBJDIR)/prompt.o:	prompt.c mined.h io.h termprop.h
	$(CC) $(CFLAGS) $(PROTOFLAGS) -c prompt.c -o $(OBJDIR)/prompt.o
$(OBJDIR)/charcode.o:	charcode.c charcode.h charmaps.t charmaps.h typoprop.t termprop.h
	$(CC) $(CFLAGS) $(PROTOFLAGS) -c charcode.c -o $(OBJDIR)/charcode.o
$(OBJDIR)/handescr.o:	handescr.c handescr.t # mined.h for type only
	$(CC) $(CFLAGS) $(PROTOFLAGS) -c handescr.c -o $(OBJDIR)/handescr.o
$(OBJDIR)/compose.o:	compose.c mined.h mnemos.t io.h
	$(CC) $(CFLAGS) $(PROTOFLAGS) -c compose.c -o $(OBJDIR)/compose.o
$(OBJDIR)/keymaps.o:	keymaps.c keymapsk.t $(KEYMAPSDEP)
	$(CC) $(CFLAGS) $(PROTOFLAGS) -Duse_concatenated_keymaps $(KEYMAPSFLAGS) -c keymaps.c -o $(OBJDIR)/keymaps.o
#$(OBJDIR)/keymaps.o:	keymaps.c keymapsk.t keymaps1/makefile keymapsc.h
#	$(MAKE) -f keymaps1/makefile keymapsc
#	$(CC) $(CFLAGS) $(PROTOFLAGS) -Ikeymaps1 $(KEYMAPSFLAGS) -c keymaps.c -o $(OBJDIR)/keymaps.o
$(OBJDIR)/keydefs.o:	keydefs.c mined.h io.h
	$(CC) $(CFLAGS) $(PROTOFLAGS) -c keydefs.c -o $(OBJDIR)/keydefs.o
$(OBJDIR)/dispatch.o:	dispatch.c mined.h charprop.h io.h
	$(CC) $(CFLAGS) $(PROTOFLAGS) -c dispatch.c -o $(OBJDIR)/dispatch.o
$(OBJDIR)/encoding.o:	encoding.c encoding.h
	$(CC) $(CFLAGS) $(PROTOFLAGS) -c encoding.c -o $(OBJDIR)/encoding.o
$(OBJDIR)/termprop.o:	termprop.c termprop.h
	$(CC) $(CFLAGS) $(PROTOFLAGS) -c termprop.c -o $(OBJDIR)/termprop.o
$(OBJDIR)/width.o:	width.c width.t termprop.h
	$(CC) $(CFLAGS) $(PROTOFLAGS) -c width.c -o $(OBJDIR)/width.o
$(OBJDIR)/keyboard.o:	keyboard.c mined.h termprop.h io.h
	$(CC) $(CFLAGS) $(PROTOFLAGS) -c keyboard.c -o $(OBJDIR)/keyboard.o
$(OBJDIR)/keycurs.o:	keyboard.c mined.h termprop.h
	$(CC) $(CFLAGS) -DCURSES $(PROTOFLAGS) -c keyboard.c $(ICURSES) -o $(OBJDIR)/keycurs.o
$(OBJDIR)/io.o:	io.c io.h mined.h $(MOUSELIB) dosvideo.t termprop.h
	$(CC) $(CFLAGS) $(PROTOFLAGS) -c io.c -o $(OBJDIR)/io.o
$(OBJDIR)/ioansi.o:	io.c io.h mined.h $(MOUSELIB) dosvideo.t termprop.h
	$(CC) $(CFLAGS) $(PROTOFLAGS) -DANSI -c io.c -o $(OBJDIR)/ioansi.o
$(OBJDIR)/iocurses.o:	io.c io.h mined.h termprop.h
	$(CC) $(CFLAGS) -DCURSES -DSETLOCALE $(PROTOFLAGS) -c io.c $(ICURSES) -o $(OBJDIR)/iocurses.o

#$(OBJDIR)/timestmp.o:
#	$(CC) -c timestmp.c -o $(OBJDIR)/timestmp.o


#############################################################################
# Mined generation:

green=echo -e '\033[30;42m' | sed -e 's,-e,,' >&2
yellow=echo -e '\033[30;43m' | sed -e 's,-e,,' >&2

##if $(SLIB) exists (e.g. -ltermcap):
##LINKOBJ=$(SCREENOBJ)
##LINKIO=$(SCREENOBJ) $(SLIB)
##else:
##LINKOBJ=$(SCREENOBJANSI)
##LINKIO=$(SCREENOBJANSI)
#LINKOBJ=`echo 'int main () {}' > .dumain.c; if $(CC) .dumain.c $(SLIB) -o /dev/null; then $(green); echo "Building with termcap API[0m" >&2; echo $(SCREENOBJ); else $(yellow); echo "Building with ANSI terminal control[0m" >&2; echo $(SCREENOBJANSI); fi`
#LINKIO=`echo 'int main () {}' > .dumain.c; if $(CC) .dumain.c $(SLIB) -o /dev/null; then $(green); echo "Building with termcap API[0m" >&2; echo $(SCREENOBJ) $(SLIB); else $(yellow); echo "Building with ANSI terminal control[0m" >&2; echo $(SCREENOBJANSI); fi`

$(OBJDIR)/charmaps:
	mkdir -p $(OBJDIR)/charmaps

$(OBJDIR)/charmaps.a:	$(CHARDEPS)
	ar ruv $(OBJDIR)/charmaps.a $(CHAROBJS)

#CHARDEPEND=$(CHARDEPS)
#CHARLIB=$(CHAROBJS)
CHARDEPEND=$(OBJDIR)/charmaps.a
CHARLIB=$(OBJDIR)/charmaps.a

#$(OBJDIR)/mined:	$(OBJDIR)/charmaps $(PROGOBJS) $(CHARDEPEND) $(LINKOBJ)
#	$(CC) -c timestmp.c -o $(OBJDIR)/timestmp.o
#	$(CC) $(ALLPROGOBJS) $(CHARLIB) $(LINKOPTS) ${LDFLAGS} $(LINKIO) $(EXTRALIBS) -o $(OBJDIR)/mined

mined:	$(OBJDIR)/mined

$(OBJDIR)/mined:	$(OBJDIR)/charmaps $(PROGOBJS) $(CHARDEPEND) $(SCREENOBJ)
	$(CC) -c timestmp.c -o $(OBJDIR)/timestmp.o
	$(CC) $(ALLPROGOBJS) $(CHARLIB) $(SCREENOBJ) $(LINKOPTS) ${LDFLAGS} $(SLIB) $(EXTRALIBS) -o $(OBJDIR)/mined

mansi:	$(OBJDIR)/mansi

$(OBJDIR)/mansi:	$(OBJDIR)/charmaps $(PROGOBJS) $(CHARDEPEND) $(SCREENOBJANSI)
	$(CC) -c timestmp.c -o $(OBJDIR)/timestmp.o
	$(CC) $(ALLPROGOBJS) $(CHARLIB) $(SCREENOBJANSI) $(LINKOPTS) ${LDFLAGS} $(EXTRALIBS) -o $(OBJDIR)/mansi

minced:	$(OBJDIR)/minced

$(OBJDIR)/minced:	$(OBJDIR)/charmaps $(PROGOBJS) $(CHARDEPEND) $(SCREENOBJCURS)
	$(CC) -c timestmp.c -o $(OBJDIR)/timestmp.o
	$(CC) $(ALLPROGOBJS) $(CHARLIB) $(SCREENOBJCURS) $(LINKOPTS) ${LDFLAGS} -lncursesw $(LDL) $(EXTRALIBS) -o $(OBJDIR)/minced
	$(yellow); echo "Made a curses version, but it is deprecated[0m" >&2
	false


#############################################################################
# additional files, installation, cleanup

# Online help:
help:	../usrshare/help/mined.hlp

../usrshare/help/mined.hlp:	help/* mkhelp
	$(SH) ./mkhelp > ../usrshare/help/mined.hlp

# Manual page:
man:	../man/mined.1 ../man/uterm.1

../man/%.1:	$(DOC)/%.html
	cd `/bin/pwd`/../man && $(MAKE)


# Install aliases; may be suppressed with make ...install makelinks=
makelinks=$(MAKE) -f mkinclud.mak links EXE=$(EXE)

links:	$(linkdir)/xmined $(linkdir)/minmacs$(EXE) $(linkdir)/mpico$(EXE) $(linkdir)/mstar$(EXE)

$(linkdir)/xmined:
	cd $(linkdir) && ln -s umined xmined || ln umined xmined

$(linkdir)/%:
	cd $(linkdir) && ln -s mined$(EXE) $* || ln mined$(EXE) $*


# Install uterm and umined scripts;
# may be suppressed with make ...install installscript=echo
installscript=$(INSTALLBIN)
#installscript=echo not installing

# Install desktop context menu entry
# may be suppressed with make ...install installdesktop=echo
installdesktop=$(INSTALL)
desktop=../usrshare/setup_install/mined.desktop
icon=../usrshare/setup_install/mined.xpm
rootmenudir=$(DESTDIR)$(root)/usr/share/applications
rooticondir=$(DESTDIR)$(root)/usr/share/pixmaps
localmenudir=$(DESTDIR)$(root)/usr/local/share/applications
localicondir=$(DESTDIR)$(root)/usr/local/share/pixmaps
usermenudir=${HOME}/.local/share/applications
usericondir=${HOME}/.local/share/pixmaps
rootmenu=mkdir $(rootmenudir) && $(installdesktop) $(desktop) $(rootmenudir)
rooticon=mkdir $(rooticondir) && $(installdesktop) $(icon) $(rooticondir)
# enforce local or user desktop integration with mkdir -p
localmenu=mkdir -p $(localmenudir) && $(installdesktop) $(desktop) $(localmenudir)
localicon=mkdir -p $(localicondir) && $(installdesktop) $(icon) $(localicondir)
usermenu=mkdir -p $(usermenudir) && $(installdesktop) $(desktop) $(usermenudir)
usericon=mkdir -p $(usericondir) && $(installdesktop) $(icon) $(usericondir)


# Installation:
# or installation to system-specific dir with make install root=...
install:	mined help man
	# ==== install binary and scripts
	#don't strip -p $(OBJDIR)/mined$(EXE) || true
	mkdir -p $(DESTDIR)$(bindir)
	$(INSTALLBIN) $(OBJDIR)/mined$(EXE) $(DESTDIR)$(bindir)
	$(installscript) ../usrshare/bin/uterm $(DESTDIR)$(bindir)
	$(installscript) ../usrshare/bin/umined $(DESTDIR)$(bindir)
	$(makelinks) linkdir=$(DESTDIR)$(bindir)
	# ==== install runtime support library
	mkdir -p $(DESTDIR)$(rundir)
	/bin/cp -pr ../usrshare/* $(DESTDIR)$(rundir)
	rm -fr $(DESTDIR)$(rundir)/doc_user/fonts
	# ==== install manual page
	mkdir -p $(DESTDIR)$(mandir)/man1
	$(INSTALL) ../man/*.1 $(DESTDIR)$(mandir)/man1
	# zip man if mans are zipped in actual mandir (not DESTDIR)
	if (ls $(mandir)/man1/*.gz && type gzip) > /dev/null 2>&1; then gzip -f $(DESTDIR)$(mandir)/man1/mined.1; fi
	# ==== install desktop context menu item ("Open With")
	( $(rootmenu) && $(rooticon) ) || ( $(usermenu) && $(usericon) ) || echo Could not install desktop menu entry
	# ==== install system-specific stuff (e.g. wined, README.cygwin)
	$(extrainstall)

# Local installation:
# or installation to system-specific local dir with make localinstall root=...
localinstall:	mined help man
	# ==== install binary and scripts
	#don't strip -p $(OBJDIR)/mined$(EXE) || true
	mkdir -p $(DESTDIR)$(lbindir)
	$(INSTALLBIN) $(OBJDIR)/mined$(EXE) $(DESTDIR)$(lbindir)
	$(installscript) ../usrshare/bin/uterm $(DESTDIR)$(lbindir)
	$(installscript) ../usrshare/bin/umined $(DESTDIR)$(lbindir)
	$(makelinks) linkdir=$(DESTDIR)$(lbindir)
	# ==== install runtime support library
	mkdir -p $(DESTDIR)$(lrundir)
	/bin/cp -pr ../usrshare/* $(DESTDIR)$(lrundir)
	rm -fr $(DESTDIR)$(lrundir)/doc_user/fonts
	# ==== install manual page
	mkdir -p $(DESTDIR)$(lmandir)/man1
	$(INSTALL) ../man/*.1 $(DESTDIR)$(lmandir)/man1
	# zip man if mans are zipped in actual mandir (not DESTDIR)
	if (ls $(lmandir)/man1/*.gz && type gzip) > /dev/null 2>&1; then gzip -f $(DESTDIR)$(lmandir)/man1/mined.1; fi
	# ==== install desktop context menu item ("Open With")
	( $(localmenu) && $(localicon) ) || ( $(usermenu) && $(usericon) ) || echo Could not install desktop menu entry
	# ==== install system-specific stuff (e.g. wined, README.cygwin)
	$(extrainstall)

# /opt installation:
# or installation to system-specific opt subdir with make optinstall root=...
optinstall:	mined help man
	# ==== install binary and scripts
	#don't strip -p $(OBJDIR)/mined$(EXE) || true
	mkdir -p $(DESTDIR)$(root)/opt/mined/bin
	$(INSTALLBIN) $(OBJDIR)/mined$(EXE) $(DESTDIR)$(root)/opt/mined/bin
	$(installscript) ../usrshare/bin/uterm $(DESTDIR)$(root)/opt/mined/bin
	$(installscript) ../usrshare/bin/umined $(DESTDIR)$(root)/opt/mined/bin
	$(makelinks) linkdir=$(DESTDIR)$(root)/opt/mined/bin
	# ==== install runtime support library
	mkdir -p $(DESTDIR)$(root)/opt/mined/share
	/bin/cp -pr ../usrshare/* $(DESTDIR)$(root)/opt/mined/share
	rm -fr $(DESTDIR)$(root)/opt/mined/share/doc_user/fonts
	# ==== install manual page
	mkdir -p $(DESTDIR)$(root)/opt/mined/man/man1
	$(INSTALL) ../man/*.1 $(DESTDIR)$(root)/opt/mined/man/man1
	# ==== install desktop context menu item ("Open With")
	( $(rootmenu) && $(rooticon) ) || ( $(usermenu) && $(usericon) ) || echo Could not install desktop menu entry
	# ==== install system-specific stuff (e.g. wined, README.cygwin)
	$(extrainstall)


# Cleanup after compilation:
clear:
	rm -f $(OBJDIR)/*.o $(OBJDIR)/charmaps/*.o colours.t keymaps?.t keymaps.t core
	rm -f charmaps/makefile keymaps/makefile
	rmdir keymaps || true

debianclean:
	rm -f mined semantic.cache ../debian/semantic.cache

clean:	clear debianclean


# Include file to configure binary target directory:
m-objdir.inc:
	echo OBJDIR=../bin/$(SYS).$(ARCH) > m-objdir.inc


#############################################################################
# pre-generation of tables

generate:	# keymaps1/Radical_Stroke.h

keymaps1/Radical_Stroke.h:	keymaps0/Radical_Stroke.h
	CC=$(CC) $(SH) ./mkkmincl keymaps0/Radical_Stroke.h > keymaps1/Radical_Stroke.h

#keymaps0/Radical_Stroke.h:	keymaps/Radical_Stroke.h
#	cp keymaps/Radical_Stroke.h keymaps0/Radical_Stroke.h

# Cross-generation of charmaps files for MSDOS/VMS compilation
NONCJKCHARMAPS=$(shell echo charmaps/*.map | sed -e "s, charmaps/cjk-[^ ]*.map,,g" )
#PCCHARTABS=$(shell echo $(NONCJKCHARMAPS) | sed -e "s,\.map,.c,g" )

# DOSBox make complains about the following rule ("multiple target patterns")
# so keep it away from makefile.dj
#pc:	$(PCCHARTABS)

#pc:
#	CC=$(CC) $(SH) ./mkchrtab -S $(NONCJKCHARMAPS)

allchartabs:	$(CHARTABS)


#############################################################################
# development targets

# Tags file:
tags:	*.c *.h
	ctags -w *.c *.h
	cat tags | grep -a -v 'v.......\.c' > tags1
	/bin/mv tags1 tags

# generated doc files (targets also included in main release target):
doc:	mnemodoc quotesdoc

# Source archives:
zipvms:	mined-src.zip
	zip -d mined-src handescr.t charname.t charseqs.t charmaps/cjk*

zip:	mined-src.zip

mined-src.zip:
	zip mined-src *.h *.c *.t charmaps/*.map charmaps/*.c keymaps?/*.h mnemos.??[!~] makefile.* mki* vms-* -x *~

update:
	cd ../man && $(MAKE)
	zip -y mined-src-update *.h *.c makefile.* mk* doc/changes.html doc/mined.html man/*.1 -x handescr.t

# Check function valid_cjk:
$(OBJDIR)/validcjk:	validcjk.c charcode.c $(OBJDIR)/termprop.o $(OBJDIR)/width.o $(OBJDIR)/charmaps/*.o
	$(CC) $(CFLAGS) -o $(OBJDIR)/validcjk validcjk.c $(OBJDIR)/termprop.o $(OBJDIR)/width.o $(OBJDIR)/charmaps/*.o

$(OBJDIR)/verichar:	verichar.c charprop.c $(OBJDIR)/charcode.o $(OBJDIR)/termprop.o $(OBJDIR)/width.o
	$(CC) $(CFLAGS) -o $(OBJDIR)/verichar verichar.c $(OBJDIR)/charcode.o $(OBJDIR)/termprop.o $(OBJDIR)/width.o $(OBJDIR)/handescr.o $(OBJDIR)/charmaps/*.o

# VMS build scripts:
vms.com:
	# vms-make.com
	sed -e "1,/BEGIN sources/ p" -e d vms-make.com > vms-make.new
	echo $(SCREENOBJ) $(ALLPROGOBJS) | tr ' ' '\012' | sed -e "s,.*/,," -e "s,\.o,," -e "s,^,$$ call make ," >> vms-make.new
	sed -e "/END sources/,$$ p" -e d vms-make.com >> vms-make.new
	cmp vms-make.com vms-make.new || mv -f vms-make.new vms-make.com
	rm -f vms-make.new
	# vms-complib.com
	echo "$$ arch := 'f\$$getsyi(\"arch_name\")'" > vms-complib.new
	echo "$$ compile := cc/decc/nolist/include_dir=[]" >> vms-complib.new
	echo "$$" >> vms-complib.new
	echo "$$ if arch .eqs. \"VAX\" then goto skipCJK" >> vms-complib.new
	echo charmaps/cjk-*.map | tr ' ' '\012' | sed -e "s,.*/\(.*\)\.map,$$ compile /obj=[.charmaps] [.charmaps]\1," >> vms-complib.new
	echo "$$" >> vms-complib.new
	echo "$$ skipCJK:" >> vms-complib.new
	echo $(NONCJKCHARMAPS) | tr ' ' '\012' | sed -e "s,.*/\(.*\)\.map,$$ compile /obj=[.charmaps] [.charmaps]\1," >> vms-complib.new
	echo "$$" >> vms-complib.new
	echo "$$ cre/dir [.'arch']" >> vms-complib.new
	echo "$$ libr/create [.'arch']charmaps [.charmaps]*.obj" >> vms-complib.new
	cmp vms-complib.com vms-complib.new || mv -f vms-complib.new vms-complib.com
	rm -f vms-complib.new
	# vms-link.opt
	echo $(SCREENOBJ) $(ALLPROGOBJS) | tr ' ' '\012' | sed -e "s,.*/,," -e "s,\.o,," > vms-link.new
	cmp vms-link.opt vms-link.new || mv -f vms-link.new vms-link.opt
	rm -f vms-link.new

# Check correctness, consistency, uniqueness...
checkchar:	$(OBJDIR)/validcjk
	$(OBJDIR)/validcjk
	$(SH) ./mkmnemocheck
	if grep -e 'WITH.*"' mnemos.??[!~]; then false; fi

checksrc:
	# source code checks:
	if egrep -a -e "} *if" *.c; then false; fi
	# check for remains of debug stuff:
	if egrep -a -e "^(printf|#define debug)" *.[hc]; then false; fi
	# check for debug statements at line beginning:
	#if sed -e "/^{/,/^}/ b" -e d *.c | grep -a "^[0-9a-zA-Z].*;"; then false; fi
	if sed -e "/^{/,/^}/ b" -e d *.c | grep -a "^[^ 	/].*;"; then false; fi
	# check for remaining test entries:
	if sed -e "/-- test/,$$ b" -e d keymaps.cfg | egrep -a '^[^#]'; then false; else true; fi

checklint:
	if splint *.c 2> /dev/null | grep -a Fall; then false; fi

checkhelp:
	if egrep -a -e "&" help/*; then false; fi

checkall:	checksrc checklint checkchar checkhelp

check:	checksrc

# Check makefile function
ldl:
	echo $(ldl_libs)
	echo $(LDL)

arch:
	echo `uname`.$(ARCH)

y=echo -e '\033[30;43m'
n=echo "[0m"

checkdirs:
	$(y); echo install:; echo $(prefix); echo $(bindir); echo $(mandir); echo $(rundir); $(n)
	$(y); echo localinstall:; echo $(lprefix); echo $(lbindir); echo $(lmandir); echo $(lrundir); $(n)

checkmaps:
	echo MAKEMAPS:	$(MAKEMAPS)
	echo CHARMAPS:	$(CHARMAPS)
	echo OBJDIR:	$(OBJDIR)
	echo CHARDEPS:	$(CHARDEPS)
	echo SCREENOBJ:	$(SCREENOBJ)
	echo CHAROBJS:	$(CHAROBJS) $(SCREENOBJ)


#############################################################################
# end
