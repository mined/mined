#! /bin/sh

htmlents=`
sed -e 's,\(&#x*[0-9A-F][0-9A-F]*;\),\1,g' -e t -e d "$1" |
tr '\015' '\012' | sed -e 's,^&#\([^;]*\);$,\1,' -e t -e d | sort -n | uniq
`

out="$2"
sed=entities.sed

cat > "$out" <</EOROFF
.ds <- \(<-
.if c\C'u2192' \{.ds <- \(<- 
'\}
.ds -> \(->
.if c\C'u2192' \{'ds -> \(-> 
'\}
/EOROFF

n=10
for ent in $htmlents
do
	case "$ent" in
	x*)	# ensure min. 4 digits
		#echo "hex &#$ent" >& 2
		roffent=`printf '%04X' 0$ent`;;
	*)	# prevent octal interpretation
		#echo "dec &#$ent" >& 2
		decimal=`expr 0 + $ent`
		roffent=`printf '%04X' $decimal`;;
	esac
	cat >> "$out" <</EOROFF
.ds $n [U+$roffent]
.if c\[u$roffent] \{.ds $n \[u$roffent]
'\}
/EOROFF

	echo "s,&#$ent;,\\\\*($n,g"

	n=`expr $n + 1`
done > "$sed"
#ls -l "$sed" >& 2
