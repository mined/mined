/*======================================================================*\
|*		Editor mined						*|
|*		MSDOS video mode switching table			*|
\*======================================================================*/

struct {
	int mode, cols, lins;
} modetab [] =
{ /* list of available modes, unsorted, terminated by a '-1' mode */
  /* the entries contain:
	a video mode number (restriction to text modes preferable),
	the numbers of columns and of lines which can be displayed 
		in that mode if a font of height 16 is used */
  {42, 100, 40},
  {38, 80, 60},
  {36, 132, 28},
  {35, 132, 25},
  {34, 132, 44},
  { 3, 80, 25},
  { 1, 40, 25},
  {-1, 0, 0}
};


/*======================================================================*\
|*				End					*|
\*======================================================================*/
