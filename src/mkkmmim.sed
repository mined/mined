# remove comments
/^;/ d
s,)[ 	]*;.*,),


# pre-transform ("code", ?ch) or ("code", ?\ch)
s/" ?\(\\.[^)]*\))/" "\1")/g
s/" ?\([^)]*\))/" "\1")/g

# join ("ban" ("办辦半般板班搬伴版拌瓣扮扳颁頒斑绊辨阪坂钣瘢癍舨姅絆鈑闆辯怑"
#         "攽昄粄湴斒鉡蝂靽魬褩虨埿岅朌秚肦螌辬㚘㩯㪵㸞㺜䉽䕰䬳"))
: checkjoin
/".*"$/ b join
b nojoin
: join
s,$,,
N
s,"[^"]*",,
b checkjoin

: nojoin

# detect ("arf" ("照煦"))
/(".* ("/ b choice

# detect ("uai"  (("uai"  "uāi"  "uái"  "uǎi"  "uài" )))
/(".* (("/ b words

b simple


# ("arfx" ("照煦中人办京"))
# -> arfx	照 煦 中 人 办 京
: choice
s,.*("\([^"]*\)" *("\([^"]*\)")).*,\1	\2,
h
s,.*	,,
s,\(.\),\1 ,g
s, $,,
x
s,	.*,	,
G
s,	.,	,
b generate


# ("uai"  (("uai"  "uāi"  "uái"  "uǎi"  "uài" )))
# -> uai	uai uāi uái uǎi uài
: words
s,.*("\([^"]*\)" *((\([^)]*\)))).*,\1	\2,
s,",,g
s,   *, ,g
s, $,,
b generate


: simple

# ("code", "seq")
# -> code	seq
s,.*("\(.*\)"  *"\(.*\)").*,\1	\2,


: generate

# generate C table entry
s/\(.*\)	\(.*\)/	{"\1", "\2"},/

# drop unmatching lines
/	/ b
d
