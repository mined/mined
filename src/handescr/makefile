all:	descriptions.h Radical_Stroke.h


# Han info pronunciations

pronunciations=Cantonese|HanyuPinlu|HanyuPinyin|JapaneseKun|JapaneseOn|Hangul|Korean|Mandarin|Tang|Vietnamese|XHC1983


# Unihan database
../Unihan_Readings.txt:	../Unihan.zip
	cd ..; unzip Unihan.zip Unihan_Readings.txt

../Unihan_RadicalStrokeCounts.txt:	../Unihan.zip
	cd ..; unzip Unihan.zip Unihan_RadicalStrokeCounts.txt

../Unihan.zip:
	cd ..; make -f mkinclud.mak Unihan.zip


# Radical/Stroke input method data

unihan.defrad:	../Unihan_Readings.txt
	egrep "kDefinition.*[Rr]adical" ../Unihan_Readings.txt > unihan.defrad

unihan.rsunicode:	../Unihan_RadicalStrokeCounts.txt seprs.sed
	egrep "^[^#].*kRSUnicode" ../Unihan_RadicalStrokeCounts.txt | LC_CTYPE=C sed -f seprs.sed > unihan.rsunicode

unihan.radicals-by-def:	unihan.defrad
	sed -e "s,\(.*\)[Rr]adical[number ]*\([0-9][0-9]*\),\2	\1radical \2," -e t -e "s,^,	," unihan.defrad | sort -n > unihan.radicals-by-def

unihan.radicals-by-rs:	unihan.rsunicode
	sed -e "s,\(.*kRSUnicode.*	\)\([0-9]*\)\([']*\)\.0,\2	\1\2\3.0," -e t -e d unihan.rsunicode | sort -n > unihan.radicals-by-rs

unihan.radicals:	unihan.radicals-by-def unihan.radicals-by-rs
	( \
	sed -e "s,	,-d	," unihan.radicals-by-def; \
	sed -e "s,	,-rs	," unihan.radicals-by-rs; \
	) | ../insutf8 -d | sort -n > unihan.radicals

unihan.check-unique:	unihan.radicals
	sed -e "s,^\([0-9][0-9]*\)-rs.* \([^ ][^ ]*\) U+\([0-9A-Fa-f]*\).*,\3	\2	\1," -e t -e d unihan.radicals | uniq -f 2 > unihan.check-unique-radicals
	sed -e "s,.*	\(.*\)	\(.*\),\2	\1," unihan.check-unique-radicals > unihan.check-unique

unihan.radicals.list:	unihan.radicals
	sed -e "s,^\([1-9][0-9]*\)[^	]*	0.* \([^ ]*\) U+\([0-9A-Fa-f]*\)	kRSUnicode	[0-9]*\([']*\)\..*,\1\4	\2	\3," -e t -e d unihan.radicals > unihan.radicals.list

radicals:	unihan.radicals.list

menu.radicals:	unihan.radicals.list mkmenu
	./mkmenu unihan.radicals.list > menu.radicals

menu1:	menu.radicals

unihan.rs.list:	unihan.rsunicode mkrs
	./mkrs > unihan.rs.list

strokemap:	Radical_Stroke.h

Radical_Stroke.h:	unihan.rs.list mkstrokemap
	./mkstrokemap > Radical_Stroke.h

menu2:	unihan.rs.list

menus:	menu1 menu2


# Han info pronunciation and description entries

descriptions.uni:	../Unihan_Readings.txt
	egrep "^U.*k($(pronunciations)|Definition)" ../Unihan_Readings.txt > descriptions.uni

descriptions.list:	descriptions.uni descriptions.sed
	sed -f descriptions.sed descriptions.uni | sort > descriptions.list

descriptions.sh:	descriptions.list descriptions.sep descriptions.seq
	sed -f descriptions.sep descriptions.list | sed -f descriptions.seq > descriptions.sh

descriptions.g:	descriptions.sh mkdescriptions
	./mkdescriptions descriptions.sh > descriptions.g

descriptions.h:	descriptions.g makefile
	sed -e 's/", "/\\000/g' descriptions.g > descriptions.h

desc:	descriptions.h

descriptions.test:	descriptions.h
	sed -e 's/^	{0x\([0-9A-Fa-f]*\), .*, "\(.*\)"},$$/U+\1	\2/' -e '/	$$/ d' descriptions.h | ../insutf8 > descriptions.test

descriptions.full:	descriptions.h
	egrep -v '""' descriptions.h | sed -e "s,{0x,U+," -e "s,},," | ../insutf8 > descriptions.full

test:	descriptions.test
