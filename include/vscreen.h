#ifndef VSCREEN_H
#define VSCREEN_H

/*
 * Data structure maintaining information about a virtual screen.
 */

#include <ncurses.h>
typedef struct vscreen VSCREEN;

extern WINDOW *main_screen;
extern WINDOW *status_bar;

VSCREEN *vscreen_init(void);
void setup_status_bar();
void set_status(char* status);

void vscreen_show(VSCREEN *vscreen);
void vscreen_sync(VSCREEN *vscreen);
void vscreen_putc(VSCREEN *vscreen, char c);
void vscreen_fini(VSCREEN *vscreen);
void vscreen_handleESC(VSCREEN *vscreen);
char* vscreen_collect_chars_from_window(WINDOW * window, VSCREEN * vscreen);
int charStarToNum(char* numArray);
void vscreen_help_init();
void vscreen_help();

#include "session.h"


#endif
