/***********************************************************************\
	syntax macros
------------------------------
local i;
global f;
------------------------------
void f (...)
Begin
End
------------------------------
If true
Then	bla;
	bla;
Elsif false
Then	argl;
	argl;
Else	gulp;
	gulp;
Fi
------------------------------
Dowhile true
Do	loop;
	loop;
Done
------------------------------
Loop
	bla;
	bla;
	If false Then Exit; Fi
	argl;
	argl;
Until false
Pool
------------------------------
For i = 0 While i < len Step i ++
Do	bla;
	bla;
Done
------------------------------
Select x
Default:	bla;
When 1:		argl;
When 2:		gulp;
End
------------------------------
\***********************************************************************/

#define local static
#define global extern

#define Begin	{
#define End	}

#define If	if (
#define Then	) {
#define Else	} else {
#define Elsif	} else if (
#define Fi	}

#define Dowhile	while (
#define Do	) {
#define Done	}

#define Loop	do {
#define Until	} while (! (
#define Pool	));

#define For	for (
#define While	;
#define Step	;

#define Exit	break

#define Select	switch (
#define Default	) { default
#define When	break; case

/***********************************************************************\
	end
\***********************************************************************/
