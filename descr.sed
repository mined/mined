# skip lines not intended for description
/<table id=nodescr/,/^$/ d
/<p id=nodescr>/,/^$/ d

# skip header before introduction
1,/^<h.>Introduction/ d

# separate and handle overview
/<h.>.*verview/ i\

/<h.>.*verview/ i\

/<h.>.*verview/,$ b overview


# introduction
/^$/ d
s/<p>//
/^[	 ]*</ d
b


: overview

# suppress HTML only lines
/^[	 ]*<[^>]*>$/ d

# headers
s,^[	 ]*<h3>\([^<]*\).*,\1,
t lines

# sections
s,[	 ]*<h[^>]*>\([^<]*\).*,\1,
t lines

# items
s,		 *<li>,  - ,
s,[	 ]*<li>,* ,
t lines

# continuation lines
s,[	 ]*,  ,

:lines
# remove embedded HTML
s,<[^>]*> *,,g
s,^ *$,,

# convert HTML character mnemos
s,&nbsp;, ,g
s,&ndash;,-,g
s,&mdash;,--,g
s,&hellip;,...,g

