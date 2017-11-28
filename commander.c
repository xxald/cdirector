/*
 * commander.c
 *
 * Description: Daily Commander File Manager
 * Copyright (c) 2017 Aleksander Djuric. All rights reserved.
 * Distributed under the GNU Lesser General Public License (LGPL).
 * The complete text of the license can be found in the LICENSE
 * file included in the distribution.
 *
 */
#include "commander.h"
#include "interface.h"
#include "browser.h"

int dot_filter(const struct dirent *ent) {
	return strcmp(ent->d_name, ".");
}

int main() {
	WINDOW **dirwin, *status, *menu, *help, *execw;
	WINDOW *actwin1, *actwin2, *errwin;
	char srcbuf[MAX_STR];
	char dstbuf[MAX_STR];
	wstate dirstate[2];
	int i, active, cmd = 0;
	bool exitflag = false;
	bool updtflag = false;

	setlocale(LC_ALL, "");

	dirwin = calloc(2, sizeof(WINDOW *));
        if (!dirwin) return -1;

	ncstart();

	memset(dirstate, 0, sizeof(dirstate));
	dirstate[0].width = COLS/2 - 1;
	dirstate[0].height = LINES - 2;
	dirstate[0].items = NULL;
	dirstate[1].width = COLS - (COLS/2 - 1);
	dirstate[1].height = LINES - 2;
	dirstate[1].items = NULL;

	// Status bar
	status = newwin(1, COLS, 0, 0);
	draw_statbar(status, "");

	// Bottom bar
	menu = newwin(1, COLS, LINES-1, 0);
	draw_menubar(menu, COLS);

	// Panels
	dirwin[0] = newwin(dirstate[0].height, dirstate[0].width, 1, 0);
	wbkgd(dirwin[0], COLOR_PAIR(1));
	box(dirwin[0], 0, 0);
	dirwin[1] = newwin(dirstate[1].height, dirstate[1].width, 1, COLS/2 - 1);
	wbkgd(dirwin[1], COLOR_PAIR(1));
	box(dirwin[1], 0, 0);

	// Help window
	help = newwin(12, POPUP_SIZE, LINES/2-7, COLS/2-POPUP_SIZE/2);

	// Action 1 window
	actwin1 = newwin(8, POPUP_SIZE, LINES/2-5, COLS/2-POPUP_SIZE/2);

	// Action 2 window
	actwin2 = newwin(10, POPUP_SIZE, LINES/2-6, COLS/2-POPUP_SIZE/2);

	// Error window
	errwin = newwin(8, POPUP_SIZE, LINES/2-5, COLS/2-POPUP_SIZE/2);

	// Exec window
	execw = newwin(LINES-2, COLS, 1, 0);

	refresh();
	wrefresh(status);
	wrefresh(dirwin[0]);
	wrefresh(dirwin[1]);
	wrefresh(menu);

	if (getcwd(dirstate[0].path, MAX_STR-1) == NULL)
		snprintf(dirstate[0].path, MAX_STR-1, "/");
	snprintf(dirstate[1].path, MAX_STR-1, "/");

	dirstate[0].count = scandir(dirstate[0].path, &(dirstate[0].items), dot_filter, alphasort);
	dirstate[1].count = scandir(dirstate[1].path, &(dirstate[1].items), dot_filter, alphasort);

	browser(dirwin[0], &dirstate[0], 9, 1);
	browser(dirwin[1], &dirstate[1], 9, 0);

	active = 0;

	do {
		struct stat st;
		char *s, *p;
		pid_t pid;
		int x, y, result;

		cmd = getch();
		switch (cmd) {
		case KEY_RESIZE:
			wresize(status, 1, COLS);
			draw_statbar(status, "");
			wresize(menu, 1, COLS);
			mvwin(menu, LINES-1, 0);
			draw_menubar(menu, COLS);

			dirstate[0].width = COLS/2 - 1;
			dirstate[0].height = LINES - 2;
			dirstate[1].width = COLS - (COLS/2 - 1);
			dirstate[1].height = LINES - 2;

			wresize(dirwin[0], dirstate[0].height, dirstate[0].width);
			wresize(dirwin[1], dirstate[1].height, dirstate[1].width);
			wresize(execw, LINES-2, COLS);

			mvwin(dirwin[1], 1, COLS/2 - 1);
			mvwin(help, LINES/2-7, COLS/2-POPUP_SIZE/2);
			mvwin(actwin1, LINES/2-5, COLS/2-POPUP_SIZE/2);
			mvwin(actwin2, LINES/2-6, COLS/2-POPUP_SIZE/2);
			mvwin(errwin, LINES/2-5, COLS/2-POPUP_SIZE/2);

			dirstate[0].choice = dirstate[0].start;
			dirstate[1].choice = dirstate[1].start;

			browser(dirwin[active], &dirstate[active], cmd, 1);
			browser(dirwin[active ^ 1], &dirstate[active ^ 1], 0, 0);

			wrefresh(dirwin[0]);
			wrefresh(dirwin[1]);
			wrefresh(status);
			wrefresh(menu);
			
			break;
		case KEY_F(1):
			draw_help(help);
			wrefresh(help);
			do { cmd = getch(); }
				while (cmd < 7 && 128 > cmd); // press any key
			updtflag = true;
			break;
		case KEY_F(5):
			draw_actwin2(actwin2, "Copy", dirstate[active].items[dirstate[active].choice]->d_name,
				dirstate[active ^ 1].path);
			wrefresh(actwin2);
			do { cmd = getch(); }
				while (cmd != 10 && 27 != cmd); // enter or esc

			if (cmd != 27) {
				p = dirstate[active].items[dirstate[active].choice]->d_name;
				snprintf(srcbuf, MAX_STR-1, "%s/%s", dirstate[active].path, p);
				snprintf(dstbuf, MAX_STR-1, "%s/%s", dirstate[active ^ 1].path, p);

				result = 0;
				if (!(pid = fork()))
					return execl("/bin/cp", "cp", "-Rfp", srcbuf, dstbuf, (char *)0);
				waitpid(pid, &result, 0);

				if (result < 0) {
					draw_errwin(errwin, "Couldn't copy file or directory", p);
					wrefresh(errwin);
					do { cmd = getch(); }
						while (cmd < 7 && 128 > cmd); // press any key
					break;
				}
			}

			dirstate[active].count = scandir(dirstate[active].path, &(dirstate[active].items), dot_filter, alphasort);
			dirstate[active ^ 1].count = scandir(dirstate[active ^ 1].path, &(dirstate[active ^ 1].items), dot_filter, alphasort);
			updtflag = true;
			break;
		case KEY_F(6):
			draw_actwin2(actwin2, "Move", dirstate[active].items[dirstate[active].choice]->d_name,
				dirstate[active ^ 1].path);
			wrefresh(actwin2);
			do { cmd = getch(); }
				while (cmd != 10 && 27 != cmd); // enter or esc

			if (cmd != 27) {
				p = dirstate[active].items[dirstate[active].choice]->d_name;
				snprintf(srcbuf, MAX_STR-1, "%s/%s", dirstate[active].path, p);
				snprintf(dstbuf, MAX_STR-1, "%s/%s", dirstate[active ^ 1].path, p);

				result = 0;
				if (!(pid = fork()))
					return execl("/bin/mv", "mv", "-f", srcbuf, dstbuf, (char *)0);
				waitpid(pid, &result, 0);

				if (result < 0) {
					draw_errwin(errwin, "Couldn't move file or directory", p);
					wrefresh(errwin);
					do { cmd = getch(); }
						while (cmd < 7 && 128 > cmd); // press any key
					break;
				}
			}

			if (dirstate[active].choice > 0) dirstate[active].choice--;
			dirstate[active].count = scandir(dirstate[active].path, &(dirstate[active].items), dot_filter, alphasort);
			dirstate[active ^ 1].count = scandir(dirstate[active ^ 1].path, &(dirstate[active ^ 1].items), dot_filter, alphasort);
			updtflag = true;
			break;
		case KEY_F(7):
			draw_actwin1(actwin1, "Create directory", "");
			wrefresh(actwin1);
			i = snprintf(dstbuf, MAX_STR-1, "%s/", dirstate[active].path);
			wattron(actwin1, COLOR_PAIR(1));

			s = p = dstbuf + i;
			getyx(actwin1, y, x);
			while (p - dstbuf < MAX_STR) {
				cmd = getchar();
				if (cmd == 13 || 27 == cmd) break;
				else if (cmd == KEY_BACKSPACE || cmd == 127) { // del
					if (p == s) continue;
					p--, x--;
					mvwaddch(actwin1, y, x, ' ');
					wmove(actwin1, y, x);
					wrefresh(actwin1);
				} else {
					wprintw(actwin1, "%c", cmd);
					*p = cmd, x++, p++;
					wrefresh(actwin1);
				}
			}
			*p = '\0';

			if (cmd != 27) {
				if (lstat(dstbuf, &st) == 0 || mkdir(dstbuf, 0700) == -1) {
					p = dstbuf + i;
					draw_errwin(errwin, "Couldn't create directory", p);
					wrefresh(errwin);
					do { cmd = getch(); }
						while (cmd < 7 && 128 > cmd); // press any key
					break;
				}
			}

			dirstate[active].count = scandir(dirstate[active].path, &(dirstate[active].items), dot_filter, alphasort);
			dirstate[active ^ 1].count = scandir(dirstate[active ^ 1].path, &(dirstate[active ^ 1].items), dot_filter, alphasort);
			updtflag = true;
			break;
		case KEY_F(8):
			draw_actwin1(actwin1, "Delete", dirstate[active].items[dirstate[active].choice]->d_name);
			wrefresh(actwin1);
			do { cmd = getch(); }
				while (cmd != 10 && 27 != cmd); // enter or esc

			if (cmd != 27) {
				p = dirstate[active].items[dirstate[active].choice]->d_name;
				snprintf(dstbuf, MAX_STR-1, "%s/%s", dirstate[active].path, p);

				result = 0;
				if (!(pid = fork()))
					return execl("/bin/rm", "rm", "-rf", dstbuf, (char *)0);
				waitpid(pid, &result, 0);

				if (result < 0) {
					draw_errwin(errwin, "Couldn't delete file or directory", p);
					wrefresh(errwin);
					do { cmd = getch(); }
						while (cmd < 7 && 128 > cmd); // press any key
					break;
				}
				if (dirstate[active].choice > 0) dirstate[active].choice--;
			}

			dirstate[active].count = scandir(dirstate[active].path, &(dirstate[active].items), dot_filter, alphasort);
			dirstate[active ^ 1].count = scandir(dirstate[active ^ 1].path, &(dirstate[active ^ 1].items), dot_filter, alphasort);
			updtflag = true;
			break;
		case KEY_F(10):
			exitflag = true;
			break;
		case KEY_STAB:
		case 9:
			browser(dirwin[active], &dirstate[active], cmd, 0);
			wrefresh(dirwin[active]);
			active ^= 1;
			break;
		case KEY_ENTER:
		case 10:
			p = dirstate[active].items[dirstate[active].choice]->d_name;
			snprintf(dstbuf, MAX_STR-1, "%s/%s", dirstate[active].path, p);
			if (stat(dstbuf, &st) < 0) break;

			if (S_ISDIR(st.st_mode) && chdir(dstbuf) == 0) {
				if (lstat(dstbuf, &st) < 0) break;
				if (getcwd(dirstate[active].path, MAX_STR-1) == NULL) break;
				dirstate[active].count = scandir(dirstate[active].path, &(dirstate[active].items), dot_filter, alphasort);
				if (*p == '.' && *(p + 1) == '.') {
					dirstate[active].choice = dirstate[active].prev;
					dirstate[active].prev = 0;
				} else if (S_ISLNK(st.st_mode)) {
					dirstate[active].prev = 0;
					dirstate[active].choice = 0;
				} else {
					dirstate[active].prev = dirstate[active].choice;
					dirstate[active].choice = 0;
				}
			} else if (st.st_mode & S_IXUSR) {
				if (draw_execwin(execw, dstbuf, 1, p) < 0) {
					draw_errwin(errwin, "Couldn't exec", p);
					wrefresh(errwin);
				} else wrefresh(execw);

				do { cmd = getch(); }
					while (cmd < 7 && 128 > cmd); // press any key
			}

			updtflag = true;
			break;
		default:
			browser(dirwin[active], &dirstate[active], cmd, 1);
			wrefresh(dirwin[active]);
			break;
		}

		if (updtflag) {
			browser(dirwin[active ^ 1], &dirstate[active ^ 1], 0, 0);
			wrefresh(dirwin[active ^ 1]);
			updtflag = false;
		}

	} while (!exitflag);

	free(dirstate[0].items);
	free(dirstate[1].items);
	delwin(dirwin[0]);
	delwin(dirwin[1]);
	delwin(status);
	delwin(menu);
	delwin(errwin);
	endwin();

	return 0;
}