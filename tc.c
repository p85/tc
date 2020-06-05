#include <stdio.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <time.h>
#include <dirent.h>
#include <signal.h>

#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <termios.h>


static const char PRE[] = "\x1b(0"; // Prefix, to enable special "Drawing Characters"
static const char SUF[] = "\x1b(B"; // Disable given special "Drawing Characters"
// list of drawing characters from the vt100 spec
static const char BOTTOM_RIGHT[2] = {0x6a};
static const char TOP_RIGHT[2] = {0x6b};
static const char TOP_LEFT[2] = {0x6c};
static const char BOTTOM_LEFT[2] = {0x6d};
static const char CROSSING[2] = {0x6e};
static const char HORIZ_LINE[2] = {0x71};
static const char T_RIGHT[2] = {0x74};
static const char T_LEFT[2] = {0x75};
static const char T_UP[2] = {0x76};
static const char T_DOWN[2] = {0x77};
static const char VERT_LINE[2] = {0x78};

static int current_page = 1;
static int max_pages = -1;
static int files_per_page = -1;
static int cursor_position = 1;

struct termios orig_termios;


#define clear() printf("\033[H\033[J");
#define locate(x, y) printf("\033[%d;%dH", (y), (x));
#define plot_vt100_char(c) printf("%s%s%s", PRE, (c), SUF);

#define ACTIVE_COLOR "\e[7;49;36m"
#define INACTIVE_COLOR "\e[7;49;34m"
#define DEFAULT_COLOR INACTIVE_COLOR

#define MAX_FILES 1024
#define MAX_FILE_LENGTH 32


struct winsize get_terminal_size()
{
	struct winsize w;
	ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
	return w;
}

void plot_outer_border(const int lines, const int columns)
{
	locate(1, 1);
	plot_vt100_char(TOP_LEFT);
	for (int i = 2; i < columns; i++)
	{
		locate(i, 0);
		plot_vt100_char(HORIZ_LINE);
	}
	plot_vt100_char(TOP_RIGHT);
	for (int i = 2; i < lines-1; i++)
	{
		locate(1, i);
		plot_vt100_char(VERT_LINE);
		locate(columns, i);
		plot_vt100_char(VERT_LINE);
	}
	locate(0, lines-1);
	plot_vt100_char(BOTTOM_LEFT);
	locate(columns, lines-1);
	plot_vt100_char(BOTTOM_RIGHT);
	for (int i = 2; i < columns; i++)
	{
		locate(i, lines-1);
		plot_vt100_char(HORIZ_LINE);
	}
}

void plot_inner_border(const int lines, const int columns)
{
	const int half_cols = columns / 2;
	locate(half_cols, 0);
	plot_vt100_char(T_DOWN);
	for (int i = 2; i < lines-1; i++)
	{
		locate(half_cols, i);
		plot_vt100_char(VERT_LINE);
	}
	locate(half_cols, lines-1);
	plot_vt100_char(T_UP);
}

void plot_right_horiz_border(const int lines, const int columns)
{
	const int line_offset = lines / 6;
	const int half_cols = columns / 2;
	locate(half_cols, line_offset);
	plot_vt100_char(T_RIGHT);
	for (int i = half_cols + 1; i < half_cols * 2; i++)
	{
		locate(i, line_offset);
		plot_vt100_char(HORIZ_LINE);
	}
	plot_vt100_char(T_LEFT);
}

void print_logo(const int lines, const int columns)
{
	const int half_cols = (columns / 2) + (columns / 2 / 2);
	const int max_line_offset = lines / 6;
	const int start_at_line = 2;
	const char app_name[] = "Telecommander v1.3";
	locate(half_cols - strlen(app_name), start_at_line);
	printf("%s", app_name);
	const char author[] = "by archer";
	locate(half_cols - strlen(author), start_at_line+1);
	printf("%s", author);
	const char usage[] = "Usage:";
	locate(half_cols - strlen(usage), start_at_line+2);
	printf("%s", usage);
	const char hotkeys[] = "w - up, s - down";
	locate(half_cols - strlen(hotkeys), start_at_line+3);
	printf("%s", hotkeys);
	const char hotkeys2[] = "q to quit";
	locate(half_cols - strlen(hotkeys2), start_at_line+4);
	printf("%s", hotkeys2);
}

void plot_status_bar(const int lines, const int columns)
{
	locate(1, lines - 3);
	plot_vt100_char(T_RIGHT);
	const int half_cols = columns / 2;
	for (int i = 2; i < half_cols; i++)
	{
		locate(i, lines - 3);
		plot_vt100_char(HORIZ_LINE);
	}
	locate(half_cols, lines - 3);
	plot_vt100_char(T_LEFT);
}

void print_status_bar_text(const int lines, const int columns)
{
	// user
	char user[32];
	getlogin_r(user, sizeof(user)-1);
	locate(3, lines - 2);
	printf("%s", user);
	const time_t t = time(NULL);
	struct tm tm = *localtime(&t);
	// month
	char month[3];
	sprintf(month, "%d", tm.tm_mon + 1);
	if (strlen(month) == 1)
	{
		char *tmp = strdup(month);
		strcpy(month, "0");
		strcat(month, tmp);
		free(tmp);
	}
	// day
	char day[3];
	sprintf(day, "%d", tm.tm_mday);
	if (strlen(day) == 1)
	{
		char *tmp = strdup(day);
		strcpy(day, "0");
		strcat(day, tmp);
		free(tmp);
	}
	// hour
	char hour[3];
	sprintf(hour, "%d", tm.tm_hour);
	if (strlen(hour) == 1)
	{
		char *tmp = strdup(hour);
		strcpy(hour, "0");
		strcat(hour, tmp);
		free(tmp);
	}
	// minute
	char minute[3];
	sprintf(minute, "%d", tm.tm_min);
	if (strlen(minute) == 1)
	{
		char *tmp = strdup(minute);
		strcpy(minute, "0");
		strcat(minute, tmp);
		free(tmp);
	}
	// second
	char second[3];
	sprintf(second, "%d", tm.tm_sec);
	if (strlen(second) == 1)
	{
		char *tmp = strdup(second);
		strcpy(second, "0");
		strcat(second, tmp);
		free(tmp);
	}

	locate(strlen(user) + 4, lines - 2);
	printf("%d-%s-%s %s:%s:%s", tm.tm_year + 1900, month, day, hour, minute, second);
	// on page/max pages
	printf(" %i/%i", current_page, max_pages);
}

void create_file_list(char *files[MAX_FILES][MAX_FILE_LENGTH], int *size)
{
	DIR *d;
	struct dirent *dir;
	d = opendir(".");
	int current_index = 0;
	if (d)
	{
		while ((dir = readdir(d)) != NULL)
		{
			if (strcmp(dir->d_name, "..") != 0 && strcmp(dir->d_name, ".") != 0)
			{
				strcpy(files[current_index++], dir->d_name);
			}
		}
		closedir(d);
	}
	free(dir);
	*size = current_index;
}

void print_file_list(const int lines, const int columns, char *files[MAX_FILES][MAX_FILE_LENGTH], int total_files)
{
	for (int i = 0; i < total_files; i++)
	{
		locate(3, i + 2);
		if (i + 1 == cursor_position)
		{
			printf("%s", ACTIVE_COLOR);
		}
		else
		{
			printf("%s", INACTIVE_COLOR);
		}
		printf(" %s ", files[i]);
	}
}

void calculate_files_per_page(int lines)
{
	files_per_page = lines - 5;
}

void calculate_max_pages(int total_files)
{
	max_pages = total_files / files_per_page + 1;
}

void terminate_program()
{
	clear();
	// Disable VT100 Char Mode, incase enabled
	printf("%s", SUF);
	tcsetattr(0, TCSANOW, &orig_termios);
}

void set_conio_terminal_mode()
{
	struct termios new_termios;
	tcgetattr(0, &orig_termios);
	memcpy(&new_termios, &orig_termios, sizeof(new_termios));
	atexit(terminate_program);
	cfmakeraw(&new_termios);
	tcsetattr(0, TCSANOW, &new_termios);
}

int kbhit()
{
	struct timeval tv = { 1L, 0L };
	fd_set fds;
	FD_ZERO(&fds);
	FD_SET(0, &fds);
	return select(1, &fds, NULL, NULL, &tv);
}

int getch()
{
	int r;
	unsigned char c;
	if ((r = read(0, &c, sizeof(c))) < 0)
	{
		return r;
	}
	else
	{
		return c;
	}
}

void set_colors(const int lines, const int columns)
{
	clear();
	printf("%s", DEFAULT_COLOR);
	for (int i = 0; i < columns; i++)
	{
		for (int ii = 0; ii < lines; ii++)
		{
			locate(i, ii);
			printf(" ");
		}
	}
}

void process_input(char *files[MAX_FILES][MAX_FILE_LENGTH])
{
	int key = getch();
	switch (key)
	{
		case 113: // q
		case 81: // Q
			exit(0);
			break;
                case 115: // s
		case 83: // S
			cursor_position = cursor_position + 1;
                        if (cursor_position > files_per_page)
                        {
                        	cursor_position = 1;
                        }
                        break;
                case 119: // w
                case 87: // W
                        cursor_position = cursor_position - 1;
                        if (cursor_position < 1)
                        {
                        	cursor_position = 1;
                        }
                        break;
                case 111:; // o
                case 79:; // O
                        // TODO: open file and display
                        char filename[MAX_FILE_LENGTH];
                        strcpy(filename, files[cursor_position - 1]);
                        printf("\n%s\n", filename);
                        sleep(3);
                        break;
                default:
                	break;
	}
}


int main(int argc, char **argv)
{
	struct winsize w;
	char *files[MAX_FILES][MAX_FILE_LENGTH];
	int *total_files;
	set_conio_terminal_mode();
	for (;;)
	{
		w = get_terminal_size();
		const int lines = w.ws_row;
		const int columns = w.ws_col;
		if (lines < 24 || columns < 80)
		{
			printf("Terminal Height/Width must be greater than 80x24\n");
			printf("But yours is only %ix%i\n", columns, lines);
			return 1;
		}
		set_colors(lines, columns);
		plot_outer_border(lines, columns);
		plot_inner_border(lines, columns);
		plot_right_horiz_border(lines, columns);
		print_logo(lines, columns);
		// create the file list
		create_file_list(files, total_files);
		calculate_files_per_page(lines);
		calculate_max_pages(*total_files);
		plot_status_bar(lines, columns);
		print_status_bar_text(lines, columns);
		print_file_list(lines, columns, files, *total_files);
		fflush(stdout);

		if (kbhit())
		{
			process_input(files);
		}
	}
	return 0;
}
