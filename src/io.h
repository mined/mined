/* terminal control mode */
#ifdef msdos
#define conio
#define dosmouse
#endif


/* debug options */
#define dont_debug_keytrack

#ifdef debug_keytrack
#define trace_keytrack(tag, c)	do_trace_keytrack (tag, c)
extern void do_trace_keytrack _((char * tag, unsigned long c));
#else
#define trace_keytrack(tag, c)	
#endif


#define dont_debug_mouse

#ifdef debug_mouse
#define trace_mouse(tag)	do_trace_mouse (tag)
extern void do_trace_mouse _((char * tag));
#else
#define trace_mouse(tag)	
#endif


#define dont_debug_terminal_resize


/* options */
extern void mouse_button_event_mode _((FLAG report));


/* signal handling */
extern void catch_signals _((void));
extern void handle_interrupt _((int));
extern FLAG cansuspendmyself;
extern void suspendmyself _((void));
extern FLAG tty_closed;
extern FLAG hup;	/* set when SIGHUP is caught */
extern FLAG intr_char;	/* set when SIGINT is caught */

/* i/o internal */
extern int input_fd;	/* terminal input file descriptor */
extern int output_fd;	/* terminal output file descriptor */
extern int inputreadyafter _((int msec));

/* tty handling */
extern void raw_mode _((FLAG state));
extern void get_term _((char * TERM));
extern void getwinsize _((void));

/* output handling */
extern void flush _((void));
extern void _putescape _((char * s));
extern void putescape _((char * s));

/* terminal encoding detection */
#ifdef msdos
extern unsigned int get_codepage _((void));
#endif

/* terminal screen mode handling */
#ifdef msdos
extern void set_video_lines _((int r));
	/* 0/1/2: 200/350/400 lines */
extern void set_grafmode_height _((int r, int l));
	/* 0/1/2: font height 8/14/16 ; 1/2/3/n: 14/25/43/n lines */
extern void set_fontbank _((int f));
	/* 0..7 */
#endif
extern void set_textmode_height _((int r));
	/* 0/1/2: font height 8/14/16 */
extern void set_font_height _((int r));
	/* set font height in character pixels, <= 32 */
extern void set_screen_mode _((int mode));
extern void maximize_restore_screen _((void));
extern void resize_font _((int inc));
extern void resize_the_screen _((FLAG sb, FLAG keep_columns));
extern void switch_textmode_height _((FLAG cycle));
	/* True: cycle through font heights 8/14/16 
	   False: switch between font heights 8/16 */


/* terminal screen manipulation */
extern void clear_screen _((void));
extern void clear_eol _((void));
extern void erase_chars _((int));
extern void scroll_forward _((void));
extern void scroll_reverse _((void));
extern void add_line _((int));
extern void delete_line _((int));
extern void set_cursor _((int, int));

/* terminal screen attributes handling */
extern void disp_normal _((void));
extern void disp_selected _((FLAG bg, FLAG border));
extern void disp_colour _((int, FLAG for_256_colours));
extern void disp_cursor _((FLAG));
extern void reverse_on _((void));
extern void reverse_off _((void));
extern void altcset_on _((void));
extern void altcset_off _((void));
extern void emph_on _((void));
extern void mark_on _((void));
extern void mark_off _((void));
extern void bold_on _((void));
extern void bold_off _((void));
extern void underline_on _((void));
extern void underline_off _((void));
extern void blink_on _((void));
extern void blink_off _((void));
extern void unidisp_on _((void));
extern void unidisp_off _((void));
extern void ctrldisp_on _((void));
extern void ctrldisp_off _((void));
extern void menudisp_on _((void));
extern void menudisp_off _((void));
extern void dispXML_attrib _((void));
extern void dispXML_value _((void));
extern void dispHTML_code _((void));
extern void dispHTML_jsp _((void));
extern void dispHTML_comment _((void));
extern void dispHTML_off _((void));
extern void specialdisp_on _((void));
extern void specialdisp_off _((void));
extern void combiningdisp_on _((void));
extern void combiningdisp_off _((void));
extern void diagdisp_on _((void));
extern void diagdisp_off _((void));
extern void disp_scrollbar_foreground _((void));
extern void disp_scrollbar_background _((void));
extern void disp_scrollbar_off _((void));
extern void menuheader_on _((void));
extern void menuheader_off _((void));
extern void menuborder_on _((void));
extern void menuborder_off _((void));
extern void menuitem_on _((void));
extern void menuitem_off _((void));
extern void set_colour_token _((int));

extern char set_ansi7 [];
extern char restore_ansi7 [];
extern FLAG redefined_ansi7;
extern char set_ansi2 [];
extern char restore_ansi2 [];
extern FLAG redefined_ansi2;
extern char set_ansi3 [];
extern char restore_ansi3 [];
extern FLAG redefined_ansi3;
extern char set_curscolr [];
extern char restore_curscolr [];
extern FLAG redefined_curscolr;


/* screen output */
extern void clearscreen _((void));
extern void putstring _((char *));
extern void __putchar _((character));
extern void ring_bell _((void));
extern void putblockchar _((character));

/* special attribute output support */
extern void put_menu_marker _((void));
extern void put_submenu_marker _((FLAG with_attr));

/* screen attributes: ANSI sequences */
extern char * markansi;		/* line indicator marking (dim/red) */
extern char * emphansi;		/* status line emphasis mode (red bg) */
extern char * borderansi;	/* menu border colour (red) */
extern char * selansi;		/* menu selection */
extern char * selfgansi;	/* menu selection background */
extern char * ctrlansi;		/* control character display */
extern char * uniansi;		/* Unicode character display */
extern char * specialansi;	/* Unicode (lineend) marker display */
extern char * combiningansi;	/* combining character display */
extern char * menuansi;		/* menu line */
extern char * HTMLansi;		/* HTML display */
extern char * XMLattribansi;	/* HTML/XML attribute display */
extern char * XMLvalueansi;	/* HTML/XML value display */
extern char * diagansi;		/* dialog (bottom status) line */
extern char * scrollfgansi;	/* scrollbar foreground */
extern char * scrollbgansi;	/* scrollbar background */


/* input functions */
extern int __readchar _((void));
extern int __readchar_report_winchg _((void));
extern int get_digits _((int *));
extern int get_string_nokeymap _((char *, char *, FLAG, char *));

/* basic keyboard to function mappings */
extern voidfunc key_map [256];
extern voidfunc mined_key_map [32];
extern voidfunc ws_key_map [32];
extern voidfunc pico_key_map [32];
extern voidfunc emacs_key_map [32];
extern voidfunc windows_key_map [32];
struct pc_fkeyentry {
	voidfunc fp;
	unsigned char fkeyshift;
};
extern struct pc_fkeyentry pc_xkey_map [512];
struct fkeyentry {
	char * fk;
	voidfunc fp;
	unsigned char fkeyshift;
};
extern void set_fkeymap _((char *));

/* keyboard functions */
extern unsigned long readcharacter_allbuttons _((FLAG map_keyboard));
extern unsigned long readcharacter _((void));
extern unsigned long readcharacter_mapped _((void));
extern unsigned long readcharacter_unicode _((void));
extern unsigned long readcharacter_unicode_mapped _((void));
extern unsigned long _readchar_nokeymap _((void));
extern int read1byte _((void));
extern int char_ready_within _((int msec, char * debug_tag));


/* mouse handling */
extern void DIRECTcrttool _((void));
extern void DIRECTvtlocator _((void));
extern void DIRECTxterm _((void));
extern void TRACKxterm _((void));
extern void TRACKxtermT _((void));

extern mousebutton_type mouse_button, mouse_prevbutton;
extern int mouse_xpos, mouse_ypos, mouse_prevxpos, mouse_prevypos;
extern int mouse_shift;		/* modifier status */
extern FLAG report_release;	/* selectively suppress release event */
extern FLAG window_focus;	/* does the window have mouse/keyboard focus? */
extern int mouse_buttons_pressed;	/* count pressed buttons */
extern mousebutton_type last_mouse_event;

/* auxiliary menu functions */
extern void menu_mouse_mode _((FLAG));
extern FLAG in_menu_border;

/* switch normal/alternate screen buffer */
extern void screen_buffer _((FLAG alternate));


/* terminal feature indications */
extern FLAG can_scroll_reverse;
extern FLAG can_add_line;
extern FLAG can_delete_line;
extern FLAG can_clear_eol;
extern FLAG can_clear_eos;
extern FLAG can_erase_chars;
extern FLAG can_reverse_mode;
extern FLAG can_hide_cursor;
extern FLAG can_dim;
extern FLAG can_alt_cset;
extern FLAG use_mouse_button_event_tracking;

/* terminal feature usage flags */
extern FLAG use_ascii_graphics;		/* use ASCII graphics for borders */
extern FLAG use_vga_block_graphics;	/* charset is VGA with block graphics */
extern FLAG use_pc_block_graphics;	/* alternate charset is VGA with block graphics */
extern FLAG use_vt100_block_graphics;
extern FLAG full_menu_bg;		/* full coloured background */
extern FLAG use_appl_cursor;
extern FLAG use_appl_keypad;
extern char menu_border_style;
extern int menumargin;
extern FLAG use_stylish_menu_selection;
extern FLAG bold_border;
extern int use_unicode_menubar _((void));
extern FLAG explicit_border_style;
extern FLAG explicit_scrollbar_style;
extern FLAG explicit_keymap;
extern FLAG use_script_colour;
extern FLAG use_mouse;
extern FLAG use_mouse_release;
extern FLAG use_mouse_anymove_inmenu;
extern FLAG use_mouse_anymove_always;
extern FLAG use_mouse_extended;	/* UTF-8 encoding of mouse coordinates */
extern FLAG use_mouse_1015;	/* numeric encoding of mouse coordinates */
extern FLAG use_bold;
extern FLAG avoid_reverse_colour;
extern FLAG use_bgcolor;
extern FLAG use_modifyOtherKeys;
extern int cursor_style;
extern void disable_modify_keys _((FLAG dis));
extern FLAG colours_256;
extern FLAG colours_88;
extern FLAG ansi_esc;
extern FLAG standout_glitch;


/* keyboard input special results (additional information) */
extern voidfunc keyproc;	/* function addressed by entered function key */

extern unsigned char keyshift;	/* shift state of entered function key */
/* shift state indications of escape sequences */
/*
ctrl	alt	shift	ESC code/ -1	keyshift code
0	0	0	0		000
0	0	1	2	1	001
0	1	0	3	2	010
0	1	1	4	3	011
1	0	0	5	4	100
1	0	1	6	5	101
1	1	0	7	6	110
1	1	1	8	7	111
*/
/* shift state masks (indications decremented by 1) */
#define shift_mask	0x01
#define alt_mask	0x02
#define ctrl_mask	0x04
#define ctrlshift_mask	(ctrl_mask | shift_mask)
#define altshift_mask	(alt_mask | shift_mask)
#define altctrl_mask	(alt_mask | ctrl_mask)
#define altctrlshift_mask	(alt_mask | ctrl_mask | shift_mask)
#define applkeypad_mask	0x08
/* skip 0x30 (may be left in from keyboard.c) */
#define report_mask	0x40	/* indicate CSI ? sequence */

extern int ansi_params;
extern int ansi_param [];
extern char ansi_ini;
extern char ansi_fini;


/* prompt.c: callback for file menu */
extern int is_directory _((menuitemtype * item));
extern void check_slow_hint _((void));

/* prompt.c, textfile.c: check file "modes" */
#ifndef S_ISDIR
#define S_ISDIR(mode)	(mode & S_IFMT) == S_IFDIR
#endif

#ifndef S_ISFIFO
#define S_ISFIFO(mode)	(mode & S_IFMT) == S_IFIFO
#endif

#ifndef S_ISCHR
#define S_ISCHR(mode)	(mode & S_IFMT) == S_IFCHR
#endif

#ifndef S_ISBLK
#define S_ISBLK(mode)	(mode & S_IFMT) == S_IFBLK
#endif

