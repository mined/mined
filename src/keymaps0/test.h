struct keymap keymap_test [] = {
	{"§", "§ € x y a b 中文 文 ee f g h i j"},
	{"0", "  § "},
	{"1", "  § € x y a b 中文 文 d ee f g h i j"},
	{"2", "  § €   x y a b 中文 文 d ee f g h i j"},
	{"3", "  § € x y a b 中文 文 d ee f g h i j  "},
	{"4", "  § €  x y a    b 中文 文 d ee f g h i j "},
	{"a", "A"},
	{"abcdx", "ABCDX"},
	{"b", "B"},
	{"bcx", "BCX"},
	{"e", "E"},
	{"o", "Õ"},
	{"Õ", "O~"},
	{"n"},
	{NIL_PTR}
};
