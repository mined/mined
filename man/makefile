#############################################################################
# man page generator
HTMLtoMAN=./g.html2man
callHTMLtoMAN=perl $(HTMLtoMAN)

# version
VER=$(shell sed -e 's,^[^0-9]*,,' -e 's,[^0-9][^0-9]*,.,g' -e q ../VERSION )
date=$(shell LC_ALL=C date +"%B %Y" )


#############################################################################
# header and footer lines

header=sed -e 's,<meta name="*manheader"* content="*\([^"]*\)">.*,\1,' -e t -e d
footer.mined=mined $(VER)
footer.uterm=uterm


#############################################################################
# mined manual page

man:	mined.1 uterm.1

%.1:	../doc/%.html $(HTMLtoMAN) dehtml.sed entities.sh *.add makefile
	LC_ALL=C sed -e '/<span id=frames>/,/<\/span id=frames>/ d' -e 's,\\,\\\\,g' -e 's,^\([	 ]*\.\),\\ .,' -f dehtml.sed $< > $*
	LC_ALL=C $(callHTMLtoMAN) $* $*-0 1
	./entities.sh $< $@
	LC_ALL=C sed -f entities.sed $*-0 | LC_ALL=C sed -e "s,__header__,$(shell $(header) $< )," -e "s,__footer__,$(footer.$*)," -e "s,__date__,$(date)," -e "/^$$/ d" > $*-1
	rm -f entities.sed
	LC_ALL=C sed -e "s,^', '," -e '/^.SH DESCRIPTION/ r note1.add' $*-1 >> $@
	rm -f $* $*-0 $*-1


#############################################################################
# test target
date:
	echo $(date)


#############################################################################
# end
