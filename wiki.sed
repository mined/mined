# skip scripts
/<script.*>/,/<\/script>/ d

# skip lines not intended for description
/<table id=nodescr/,/^$/ d
/<p id=nodescr>/,/^$/ d

# define ranges to be extracted and handled
1,/^<h.>Introduction/ d
/^<h.>.*verview/,$ b overview

: intro
/^$/ d
s/<p>//
/^[	 ]*</ d
s,^Mined is,'''Mined''' is,
i\


# embed Wiki keywords
s,\(Unicode\),[[\1]],g
s,\(CJK\),[[\1]],g
s,\(Han\),[[\1]],g
s,\(text terminal\),[[\1]],g
s,\(xterm\),[[\1]],g
s,\(rxvt\),[[\1]],g

b

: overview

/^<h.>.*verview/ b ovbegin
b ovcont
: ovbegin
i\

i\


: ovcont

# suppress HTML only lines
s,^<p>$,,
/^[	 ]*<[^>]*>$/ d

# headers
s,^[	 ]*<h3>\([^<]*\).*,== \1 ==,
t lines

# sections
s,[	 ]*<h[^>]*>\([^<]*\).*,=== \1 ===,
t lines

# items
s,[	 ]*<li>,* ,
t lines

# subitems
s,[	 ]*<dt>,** ,
t lines

# continuation lines
s,[	 ]*,  ,

:lines

# remove embedded HTML
s,<[^>]*> *,,g
s,^ *$,,

# remove empty lines
/^$/ d

# prepend space to headings
/^==[^=]/ i\

/^=/ i\


# remove leading space
s,^  *,,

