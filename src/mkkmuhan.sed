# split multiple mappings of the form
# shang3(12308), shang4(12308), shang5(392)	4E0A
# (with or without parenthesised additions, with or without commas)
# into separate lines of the form
# shang3	4E0A
# shang4	4E0A
# shang5	4E0A

s/([^)]*)//g
: loop
s/\([^, ]*\),*  *\([^, ]*\)	\(.*\)/\1	\3\
\2	\3/g
t loop
