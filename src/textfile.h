/***************************************************************************\
	mined text editor
	include file for file handling functions
\***************************************************************************/

extern int panicwrite _((void));

extern FLAG checkoverwrite _((char *));
extern FLAG do_backup _((char *));
extern FLAG dont_modify _((void));
extern void grab_lock _((void));
extern void ignore_lock _((void));
extern void unlock_file _((void));
extern void relock_file _((void));

extern char * * fnamv;			/* parameter list of files */
extern int fnami_min, fnami_max;	/* index into list of files */
extern int fnami_cur;			/* index of current file name */

extern FLAG overwriteOK;	/* Set if current file is OK for overwrite */
extern char * default_text_encoding;

extern lineend_type newfile_lineend;
extern lineend_type default_lineend;

extern void view_help _((char * helpfile, char * item));

extern void load_wild_file _((char * file, FLAG from_pipe, FLAG with_display));
extern int write_text _((void));
extern int save_text_load_file _((char * filename));
extern void filelist_add _((char * fn, FLAG allowdups));
extern char * filelist_get _((int i));
extern void filelist_set_coord _((short line, int left, int right));
extern char * filelist_search _((short line, int col));
extern int filelist_count _((void));
extern void edit_nth_file _((int));

extern lineend_type extract_lineend_type _((char *, int));

extern int line_gotten _((int ret));
extern void reset_get_line _((FLAG from_text_file /* consider UTF-16 ? */));
extern void save_text_info _((void));
extern void restore_text_info _((void));
extern int get_line _((int fd, char * buffer, int * len, FLAG do_auto_detect));
extern void show_get_l_errors _((void));
/*extern long long file_position;*/
extern void clear_filebuf _((void));
extern int flush_filebuf _((int fd));

extern int writechar _((int fd, char));
extern int write_lineend _((int fd, lineend_type return_type, FLAG handle_utf16));


/* options */
extern char * preselect_quote_style;

extern FLAG lineends_LFtoCRLF;
extern FLAG lineends_CRLFtoLF;
extern FLAG lineends_CRtoLF;
extern FLAG lineends_detectCR;

extern FLAG groom_info_files;	/* groom once per dir */
extern FLAG groom_info_file;	/* groom once ... */
extern FLAG groom_stat;		/* stat files for grooming */


/* state */
extern FLAG pasting;

extern lineend_type got_lineend;


/***************************************************************************\
	end
\***************************************************************************/
