s:::
/^[ 	]*"/ b map
d

: map
# transform comments
s:// *\(.*\):/* \1 */:
# transform key mapping equation into printf with table entry
#s:"\([^=]*[^ ]\) *= *\([^"]*\)" *,*[ 	]*\(.*\):	{\\\"\1\\\", \\\"\2\\\"},	\3:
# normalise key mapping equation, separate with ^S
s:"\([^=]*[^ ]\) *= *\([^"]*\)" *,*[ 	]*\(.*\):\1	\2	\3:

# escape \ and "
s:\(["\\]\):\\\1:g

# transform into table entry, embed into printf, append newline
s:\([^	]*\)	\([^	]*\)	\(.*\):	b = 0; printf ("	{\\\"\1\\\", "); b = 1; printf ("\\\"\2\\\"},	\3\\n");:

# transform 4-digit hex into utf-8 output statement
s: *\(0x[0-9a-fA-F][0-9a-fA-F][0-9a-fA-F][0-9a-fA-F]\):"); printutf8 (\1); prints (":g

# transform 2-digit hex into char output statement
s: *\(0x[0-9a-fA-F][0-9a-fA-F]\)\([^0-9a-fA-F]\):"); printchar (\1); prints ("\2:g

