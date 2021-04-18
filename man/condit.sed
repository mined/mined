# obsolete

# HTML character numeric entity conversions
#s,&#x\([0-9A-F]*\);,\\[u\1],g

#s,&#x\([0-9A-F]*\);,\
#'ie c\\[u\1] \\[u\1]\
#'el [U+\1]\
#,g

#s,&#x\([0-9A-F]*\);,\
#'ie c\\[u\1] \\[u\1]\
#'el [U+\1]\
#,g

# HTML character arrow width adjustments
/<noframes>/,/<\/noframes>/ b tbl
/^.DS/,/^.DE/ b tbl
b notab
: tbl
/&[lr]arr;/ b arr
/\\(<-/ b arr
/\\(->/ b arr
b notab
: arr
# fix spacing to fix indentation
s, ,\\ ,g
# remember unexpanded line
h
# expand arrows by one space
s,\(&[lr]arr;\),\1 ,g
s,\(\\(<-\),\1 ,g
s,\(\\(->\),\1 ,g
# prepend conditional
s,^,.ie c\\C'u2192' ,
x
# prepend else
s,^,.el ,
x
G

: notab

