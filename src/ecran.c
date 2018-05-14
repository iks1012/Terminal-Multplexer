/*
 * Ecran: A program that (if properly completed) supports the multiplexing
 * of multiple virtual terminal sessions onto a single physical terminal.
 */

#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <ncurses.h>
#include <sys/signal.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#include "ecran.h"
#include "debug.h"




static void initialize(int argc, char *argv[]);
static void curses_init(void);
static void curses_fini(void);
static void finalize(int);
static int check_for_cmdExec(int argc, char *argv[]);
static int errout_init(int argc, char *argv[]);
void handle_sigchld(int);

int closeError = 0;
int newArgs = 0; 

extern SESSION * fg_session;
extern int session_activity[MAX_SESSIONS];

int main(int argc, char *argv[]) {

	struct sigaction sa;
	sa.sa_handler = &handle_sigchld;
	sigaction(SIGCHLD, &sa, NULL);

	newArgs = check_for_cmdExec(argc, argv);
	errout_init(argc, argv);
	if(newArgs){
		int index = -1;
		for(int i = 0; i < argc; i++){
			if(!strcmp(argv[i], "-o")){//!0 == 1
				index = i;
			}
		}
		int fin_ind = 0;
		int init_ind = 0;
		if(index == 1){
			fin_ind = argc;
			init_ind = 3;
		}
		else{
			fin_ind = index;
			init_ind = 1;
			if(index == -1){
				fin_ind = argc;
			}
		}
		char *args[fin_ind - init_ind + 1];
		char *path = getenv("PATH");

		for(int i = 0; i < (fin_ind - init_ind) ; i++){
			args[i] = argv[i + init_ind];
		}
		args[fin_ind - init_ind] = NULL;

		initialize(argc, args);
	}
	else{
		initialize(argc, argv);
	}
	//errout_init(argc, argv);
	mainloop();
}



static int errout_init(int argc, char *argv[]){
	int fd = -1;


	if(closeError){
		for(int i = 0; i < argc; i++){
			debug("argv[%d]: %s \n", i, argv[i]);
		}
		debug("argc: %d\n", argc);
		int index = -1;
		for(int i = 0; i < argc && (index != i+1); i++){
			if((!strcmp(argv[i], "-o")) && i+1 < argc){
				index = i+1;
			}
		}
		fd = open(argv[index], O_RDWR|O_CREAT|O_TRUNC|O_APPEND , 0777);
		if(fd < 0){
			debug("%s \n","fd is -1");
			finalize(EXIT_FAILURE);
		}
		else{
			debug("%s \n","fd is valid");
		}
		dup2(fd, STDERR_FILENO);
	}
	return fd;
}

static int check_for_cmdExec(int argc, char *argv[]){
	int ch = getopt(argc, argv, "o:");
	if(ch == 'o'){
		closeError = 1;
	}
	return ((argc >= 4 && ch == 'o') || (ch != 'o' && argc >= 2));
}

/*
 * Initialize the program and launch a single session to run the
 * default shell.
 */
static void initialize(int argc, char *args[]) {
	curses_init();
	vscreen_help_init();

	for(int i = 0; i < MAX_SESSIONS; i++){
		session_activity[i] = 0;

	}
	

	if(newArgs){
		char* path = getenv("PATH");
		session_init(path, args);
	}
	else{
		char *path = getenv("SHELL");
		if(path == NULL)
			path = "/bin/bash";
		char *argv[2] = { " (ecran session)", NULL };
		session_init(path, argv);
	}
}

/*
 * Cleanly terminate the program.  All existing sessions should be killed,
 * all pty file descriptors should be closed, all memory should be deallocated,
 * and the original screen contents should be restored before terminating
 * normally.  Note that the current implementation only handles restoring
 * the original screen contents and terminating normally; the rest is left
 * to be done.
 */
static void finalize(int code) {

	int fd = 2; /* file descriptor */
	close(fd);
	//kill all of the sessions
	for(int i = 0; i < MAX_SESSIONS; i++){
		if(sessions[i] != NULL){
			session_kill(sessions[i]);
		}
	} // This line should never cross, but just incase..
	curses_fini(); //kill curses
	exit(code);
}

/*
 * Helper method to initialize the screen for use with curses processing.
 * You can find documentation of the "ncurses" package at:
 * https://invisible-island.net/ncurses/man/ncurses.3x.html
 */
static void curses_init(void) {
	initscr();
	raw();                       // Don't generate signals, and make typein
								 // immediately available.
	noecho();                    // Don't echo -- let the pty handle it.
	main_screen = stdscr;
	setup_status_bar();
	nodelay(main_screen, TRUE);  // Set non-blocking I/O on input.
	nodelay(status_bar, TRUE);  // Set non-blocking I/O on input.
	wclear(main_screen);         // Clear the screen.
	wclear(status_bar);         // Clear the screen.
	refresh();                   // Make changes visible.
}

/*
 * Helper method to finalize curses processing and restore the original
 * screen contents.
 */
void curses_fini(void) {
	endwin();
}


/*
 * Function to read and process a command from the terminal.
 * This function is called from mainloop(), which arranges for non-blocking
 * I/O to be disabled before calling and restored upon return.
 * So within this function terminal input can be collected one character
 * at a time by calling getch(), which will block until input is available.
 * Note that while blocked in getch(), no updates to virtual screens can
 * occur.
 */
void do_command() {
	// Quit command: terminates the program cleanly
	char ch = wgetch(main_screen);
	if(ch == 'q'){
	   finalize(EXIT_SUCCESS);
	}
	else if('0' <= ch && ch <= '9'){
		int index = ch - '0';
		if(sessions[index] != NULL){
			session_setfg(sessions[index]);
		}
		else{
			set_status("This Session is not yet initialized!!");
		}
	}
	else if(ch == 'n'){//create new session
		//Getting the path and args setup
		char *path = getenv("SHELL");
		if(path == NULL)
			path = "/bin/bash";
		char *argv[2] = { " (ecran session)", NULL };
		SESSION* temp = session_init(path, argv); //initializes the new session
		if(temp == NULL){
			flash();
			set_status("Maximum number of Sessions reached");
		}
		else if(temp == ((void*)-1)){
			finalize(EXIT_FAILURE);
		}
		else{
			vscreen_show(temp->vscreen);//Sets the contents of the current screen to a new session
		}
	}
	else if(ch == 'k'){ //need to get the next char to kill that index.
		char killChar = wgetch(main_screen);
		if('0' <= killChar && killChar <= '9'){
			int index = killChar - '0';
			if(sessions[index] != NULL){
				session_kill(sessions[index]);
				set_status("Session Killed");
			}
			else{
				flash();
				set_status("Session at this index is NULL, nothing to kill");
			}
		}
		else{
			flash();
			set_status("Choose a valid digit to end a session");
		}

	}
	else if(ch == 'h'){
		debug("Help Screen Requested");

		vscreen_help();

		session_setfg(fg_session);
	}
	else if(ch == 's'){
		debug("Split Screen Requested");
	}
	else{
		flash();
	}
}

void handle_sigchld(int signal){
	if(signal == SIGCHLD){
		int pid = getpid();
		for(int i = 0; i < MAX_SESSIONS; i++){
			if(sessions[i] != NULL && sessions[i]->pid == pid){
				session_fini(sessions[i]);
			}
		}
	}
}

/*
 * Function called from mainloop(), whose purpose is do any other processing
 * that has to be taken care of.  An example of such processing would be
 * to deal with sessions that have terminated.
 */
void do_other_processing() {
	
}
