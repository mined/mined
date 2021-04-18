# separate ambiguous radical.strokes entries from Unihan database
#U+553E	kRSUnicode	30.8 30.9
#->
#U+553E	kRSUnicode	30.8
#U+553E	kRSUnicode	30.9

: loop
s,\(.*	kRSUnicode	\)\(.*\) \([^ ]*\)$,\1\2\
\1\3,
t loop
