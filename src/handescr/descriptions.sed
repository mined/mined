# for new pronunciations, check also mkdescriptions

# remove frequency supplements from HanyuPinlu entries
/	kHanyuPinlu	/ s,([^)]*),,g

# remove location reference from HanyuPinyin entries
/	kHanyuPinyin	/ s,[0-9.,]*:,,g

# replace tags with sorting weight and shortcut
# weight will be removed in descriptions.sep
s,	kMandarin,	01 M,
s,	kCantonese,	02 C,
s,	kJapaneseKun,	03 J,
s,	kJapaneseOn,	04 S,
s,	kHangul,	05 H,
s,	kKorean,	06 K,
s,	kVietnamese,	07 V,
s,	kHanyuPinlu,	08 P,
s,	kHanyuPinyin,	09 Y,
s,	kXHC1983,	10 X,
s,	kTang,	11 T,
s,	kDefinition,	99 D,
t cont
d

# U+4E0B  kHanyuPinlu     xia4(6430) xia5(249)

: cont
# format code value to uniform length for subsequent sorting
s,U+\(....\)	,U+0\1	,


# fix typographic flaws

## fix missing space around parentheses
#s,\([^	 (]\)(\([^)]* [^)]* \),\1 (\2,g
#s,)\([^ ]\),) \1,g

# insert space before '(...)' with more than 4 characters
s,\([^	 (]\)(\([^)][^)][^)][^)][^)]\),\1 (\2,g

# fix duplicate (( or ))
s,((\([^)]*)[^)]*\)$,(\1,
/([^)]*(/ b duparop
s,)),),
: duparop

# fix superfluous blanks before , or ;
s/ ,/,/g
s/ ;/;/g
