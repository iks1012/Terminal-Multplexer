#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "vscreen.h"
#include "debug.h"

#define STATUS_LINES 1

  // Table of existing sessions

/*
 * Functions to implement a virtual screen that can be multiplexed
 * onto a physical screen.  The current contents of a virtual screen
 * are maintained in an in-memory buffer, from which they can be
 * used to initialize the physical screen to correspond.
 */

WINDOW *main_screen;
WINDOW *status_bar;
int max_status_len;
int save_row = -1;
int save_col = -1;

struct vscreen {
	int num_lines;
	int num_cols;
	int cur_line;
	int cur_col;
	char **lines;
	char *line_changed;
};

VSCREEN * help_menu = NULL;


static void update_line(VSCREEN *vscreen, int l);

/*
 * Create a new virtual screen of the same size as the physical screen.
 */
VSCREEN *vscreen_init() {
	VSCREEN *vscreen = calloc(sizeof(VSCREEN), 1);
	vscreen->num_lines = LINES-STATUS_LINES; //reserve one for the status line.
	vscreen->num_cols = COLS;
	vscreen->cur_line = 0;
	vscreen->cur_col = 0;
	vscreen->lines = calloc(sizeof(char *), vscreen->num_lines);
	vscreen->line_changed = calloc(sizeof(char), vscreen->num_lines);
	for(int i = 0; i < vscreen->num_lines; i++)
		vscreen->lines[i] = calloc(sizeof(char), vscreen->num_cols);
	return vscreen;
}

void setup_status_bar(){
	status_bar = newwin(STATUS_LINES, COLS, LINES-STATUS_LINES, 0);
	max_status_len = COLS;
}

void set_status(char* status){
	wmove(status_bar, LINES-STATUS_LINES, 0);
	wclear(status_bar);
	if(strlen(status) > max_status_len){
		max_status_len = strlen(status);
	}
	for(int i = 0; i < strlen(status); i++) {
		waddch(status_bar, status[i]);
	}
	wrefresh(status_bar);
}

/*
 * Erase the physical screen and show the current contents of a
 * specified virtual screen.
 */
void vscreen_show(VSCREEN *vscreen) {
	wclear(main_screen);
	for(int l = 0; l < vscreen->num_lines; l++) {
		if(vscreen->line_changed[l]) {
			update_line(vscreen, l);
			vscreen->line_changed[l] = 0;
		}
	}
	wmove(main_screen, vscreen->cur_line, vscreen->cur_col);
	refresh();
	for(int i = 0; i < (vscreen->num_lines); i++){
		memcpy(vscreen->lines[i], vscreen->lines[i], vscreen->num_cols);
	}
	for(int i = 0; i < (vscreen->num_lines); i++){
		update_line(vscreen, i);
	}
}

/*
 * Function to be called after a series of state changes,
 * to cause changed lines on the physical screen to be refreshed
 * and the cursor position to be updated.
 * Although the same effect could be achieved by calling vscreen_show(),
 * the present function tries to be more economical about what is displayed,
 * by only rewriting the contents of lines that have changed.
 */
void vscreen_sync(VSCREEN *vscreen) {
	for(int l = 0; l < vscreen->num_lines; l++) {
		if(vscreen->line_changed[l]) {
			update_line(vscreen, l);
			vscreen->line_changed[l] = 0;
		}
	}
	wmove(main_screen, vscreen->cur_line, vscreen->cur_col);
	refresh();
}

/*
 * Helper function to clear and rewrite a specified line of the screen.
 */
static void update_line(VSCREEN *vscreen, int l) {
	char *line = vscreen->lines[l];
	wmove(main_screen, l, 0);
	wclrtoeol(main_screen);
	for(int c = 0; c < vscreen->num_cols; c++) {
		char ch = line[c];
		if(isprint(ch))
			waddch(main_screen, line[c]);
	}
	wmove(main_screen, vscreen->cur_line, vscreen->cur_col);
	refresh();
}

/*
 * Output a character to a virtual screen, updating the cursor position
 * accordingly.  Changes are not reflected to the physical screen until
 * vscreen_show() or vscreen_sync() is called.  The current version of
 * this function emulates a "very dumb" terminal.  Each printing character
 * output is placed at the cursor position and the cursor position is
 * advanced by one column.  If the cursor advances beyond the last column,
 * it is reset to the first column.  The only non-printing characters
 * handled are carriage return, which causes the cursor to return to the
 * beginning of the current line, and line feed, which causes the cursor
 * to advance to the next line and clear from the current column position
 * to the end of the line.  There is currently no scrolling: if the cursor
 * advances beyond the last line, it wraps to the first line.
 */
void vscreen_putc(VSCREEN *vscreen, char ch) {
	
	int l = vscreen->cur_line;
	int c = vscreen->cur_col;
	if(isprint(ch)) {
		vscreen->lines[l][c] = ch;
		if(vscreen->cur_col + 1 < vscreen->num_cols)
			vscreen->cur_col++;
	} 
	else if(ch == '\n') {
		if((vscreen->cur_line + 1) >= (vscreen->num_lines)){
			for(int i = 0; i < (vscreen->num_lines-1); i++){
				memcpy(vscreen->lines[i], vscreen->lines[i+1], vscreen->num_cols);
			}
			for(int i = 0; i < (vscreen->num_lines-1); i++){
				vscreen->line_changed[i] =  (vscreen->line_changed[i+1]);
			}
			free(vscreen->lines[vscreen->num_lines-1]);
			vscreen->lines[vscreen->num_lines-1] = calloc(sizeof(char*), vscreen->num_cols);
			vscreen->line_changed[vscreen->num_lines-1] = 1;
			vscreen->cur_line--;
			for(int i = 0; i < (vscreen->num_lines); i++){
				update_line(vscreen, i);
			}
			set_status("Scroll Success!");
		}

		l = vscreen->cur_line = (vscreen->cur_line + 1) % vscreen->num_lines;
		memset(vscreen->lines[l], 0, vscreen->num_cols);
	} 
	else if(ch == '\r') {
		vscreen->cur_col = 0;
	}
	else if (ch == '\a'){ // ctrl + g
		flash();
	}
	/*
	else if (ch == 27){//escape key
		nodelay(main_screen, FALSE);

		vscreen_handleESC(vscreen);

		nodelay(main_screen, TRUE);
	}
	*/

	vscreen->line_changed[l] = 1;
}



void vscreen_handleESC(VSCREEN *vscreen){

	char bracket = wgetch(main_screen);
	if(bracket == '['){

		debug("%s \n","Reached Here!");
		set_status("Reached Here!");

		//char after 'ESC->['
		char ch = wgetch(main_screen);
		if(ch == 's'){//save cursor position
			save_row = vscreen->cur_line;
			save_col = vscreen->cur_col;
		}
		else if(ch == 'u'){
			if(save_row != -1){
				vscreen->cur_line = save_row;
				vscreen->cur_col = save_col;
				save_row = -1;
				save_col = -1;
			}
		}
		else if(ch == '2' && (wgetch(main_screen) == 'J')){
			for(int i = 0; i < vscreen->num_lines; i++){
				free(vscreen->lines[i]);
				vscreen->lines[i] = calloc(sizeof(char*), vscreen->num_cols);
			}
			vscreen->cur_line = 0;
			vscreen->cur_col = 0;
		}
		else if(ch == 'K'){
			int i = vscreen->cur_line;
			free(vscreen->lines[i]);
			vscreen->lines[i] = calloc(sizeof(char*), vscreen->num_cols);
		}
		else {
			//its a number




			char* escNumData = vscreen_collect_chars_from_window(main_screen, vscreen);

			char delim = *escNumData;

			char* numChars = malloc(sizeof(char) * (vscreen->num_cols));

			memcpy(escNumData+1, numChars, (sizeof(char) * (vscreen->num_cols)));

			free(escNumData);

			//convert numData to number;
			int num = charStarToNum(numChars);
			//flash();
		}
	}
	else{

		return;
	}

	


	for(int i = 0; i < (vscreen->num_lines); i++){
		update_line(vscreen, i);
	}

}

//collects numbers from keyboard input
char* vscreen_collect_chars_from_window(WINDOW *window, VSCREEN * vscreen){
	char* chars = malloc(sizeof(char) * (vscreen->num_cols + 1));
	char ch = wgetch(window);
	int i = 0;
	for(; ('0' <= ch && ch <= '9') &&  i < (vscreen->num_cols) ;i++){
		chars[i+1] = ch;
		set_status(&ch);
		ch = wgetch(window);
	}
	chars[0] = ch;

	chars[i] = '\0';
	return chars;
}



int charStarToNum(char* numArray){
	int len = strlen(numArray);

	int number = 0;

	for(int i = 0; i < len;i++){
		char ch = *( numArray + len - i - 1 );
		debug("ch: %c\n",ch);

	}

	return -1;
}


void vscreen_help_init(){
	help_menu = vscreen_init();
}


void vscreen_help(){
	int i = 0;
	char* line0 = "This is the Help Menu for the Terminal Multiplexer - In order to ESC, just press ESC.";
	char* line1 = "Controls:  CMD KEY = CTRL + A";
	char* line2 = "CMD + N = New Session (Max is 10)";
	char* line3 = "CMD + i = Changes to the ith Session if it is initialized";
	char* line4 = "CMD + k + i = Kills the ith Session if it is initialized";
	char* line5 = "CMD + h = Help Screen";
	char* line6 = "----------------------------------------------------------------------------------------------------";
	char* line7 = "Made By Ishan Sethi for HW 4";
	char* line8 = "CSE 320 - 110941217";
	char* line9 = "----------------------------------------------------------------------------------------------------";

	char* line10 = (getActivity(i)) ? ("Session 0: Active") : ("Session 0: Inactive"); i++;
	char* line11 = (getActivity(i)) ? ("Session 1: Active") : ("Session 1: Inactive"); i++;
	char* line12 = (getActivity(i)) ? ("Session 2: Active") : ("Session 2: Inactive"); i++;
	char* line13 = (getActivity(i)) ? ("Session 3: Active") : ("Session 3: Inactive"); i++;
	char* line14 = (getActivity(i)) ? ("Session 4: Active") : ("Session 4: Inactive"); i++;
	char* line15 = (getActivity(i)) ? ("Session 5: Active") : ("Session 5: Inactive"); i++;
	char* line16 = (getActivity(i)) ? ("Session 6: Active") : ("Session 6: Inactive"); i++;
	char* line17 = (getActivity(i)) ? ("Session 7: Active") : ("Session 7: Inactive"); i++;
	char* line18 = (getActivity(i)) ? ("Session 8: Active") : ("Session 8: Inactive"); i++;
	char* line19 = (getActivity(i)) ? ("Session 9: Active") : ("Session 9: Inactive"); i++;

	
	for(int l = 0; l < help_menu->num_lines; l++){
		memset(help_menu->lines[l], 0, help_menu->num_cols);
	}
	i = 0;
	memcpy(help_menu->lines[i], line0, strlen(line0)); i++;
	memcpy(help_menu->lines[i], line1, strlen(line1)); i++;
	memcpy(help_menu->lines[i], line2, strlen(line2)); i++;
	memcpy(help_menu->lines[i], line3, strlen(line3)); i++;
	memcpy(help_menu->lines[i], line4, strlen(line4)); i++;
	memcpy(help_menu->lines[i], line5, strlen(line5)); i++;
	memcpy(help_menu->lines[i], line6, strlen(line6)); i++;
	memcpy(help_menu->lines[i], line7, strlen(line7)); i++;
	memcpy(help_menu->lines[i], line8, strlen(line8)); i++;
	memcpy(help_menu->lines[i], line9, strlen(line9)); i++;
	memcpy(help_menu->lines[i], line10, strlen(line10)); i++;
	memcpy(help_menu->lines[i], line11, strlen(line11)); i++;
	memcpy(help_menu->lines[i], line12, strlen(line12)); i++;
	memcpy(help_menu->lines[i], line13, strlen(line13)); i++;
	memcpy(help_menu->lines[i], line14, strlen(line14)); i++;
	memcpy(help_menu->lines[i], line15, strlen(line15)); i++;
	memcpy(help_menu->lines[i], line16, strlen(line16)); i++;
	memcpy(help_menu->lines[i], line17, strlen(line17)); i++;
	memcpy(help_menu->lines[i], line18, strlen(line18)); i++;
	memcpy(help_menu->lines[i], line19, strlen(line19)); i++;

	for(int i = 0; i < help_menu->num_lines; i++){
		update_line(help_menu, i);
	}

	while(wgetch(main_screen) != 27);
}



/*
 * Deallocate a virtual screen that is no longer in use.
 */
void vscreen_fini(VSCREEN *vscreen) {
	for(int i = 0; i < vscreen->num_lines; i++){
		free(vscreen->lines[i]);
	}
	//free the both the arrays for the vscreen
	free(vscreen->line_changed);
	free(vscreen->lines);
}
