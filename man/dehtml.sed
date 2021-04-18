# replace NEW image with NEW marker
s,<img [^>]*src="*new[^>]*>,\\(->NEW\\(-> ,g

# handle table cells like listings
s,<tr[^>]*>,<dt>,g
s,<td[^>]*>,<dd>,g

# handle man-only tags
s,<!man:,<,g

# undent headings
s,\(<h.[^>]*>\)[ 	]*,\1,

# remove class attributes
s,\(<[^>]*[^> ]\) *class=[^> ]*,\1,g

# fix no-break space
s, ,\\0,g

# handle table arrows
/<noframes>/,/<\/noframes>/ s,&larr;,\\\*(<-,g
/<noframes>/,/<\/noframes>/ s,&rarr;,\\\*(->,g

# HTML character named entity conversions
s,&amp;*,\&,g
s,&nbsp;*,\\ ,g
s,&emsp;*,\\ \\ ,g
s,&ndash;*,-,g
s,&mdash;*,\\(em,g
s,&hellip;*,...,g
s,&rarr;*,\\(->,g
s,&larr;*,\\(<-,g
s,&uarr;*,\\(ua,g
s,&darr;*,\\(da,g
#s,&lt;*,<,g
#s,&gt;*,>,g

