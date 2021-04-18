# extract mapping part from file
1,/^BEGIN/ d
/^END/,$ d
# remove blank and comment lines
/^[ 	]*$/ d
/^#/ d

# remove trailing comments and white space
s,#.*,,
s,[ 	]*$,,

# normalize separating white space to TAB
s,^\([^ 	]*\)[ 	][ 	]*,\1	,

# skip choice separation if already grouped
/	.* / b grouped
/	(/ b paren

