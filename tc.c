#include <stdio.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <string.h>

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


#define VT100_COMPAT_MODE 0
#define clear() printf("\033[H\033[J");
#define locate(x, y) printf("\033[%d;%dH", (y), (x));
#define plot_vt100_char(c) printf("%s%s%s", PRE, (c), SUF);

struct winsize get_terminal_size()
{
	struct winsize w;
	ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
	return w;
}

void plot_outer_border(int lines, int columns)
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

void plot_inner_border(int lines, int columns)
{
	int half_cols = columns / 2;
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

void plot_right_horiz_border(int lines, int columns)
{
	int line_offset = lines / 6;
	int half_cols = columns / 2;
	locate(half_cols, line_offset);
	plot_vt100_char(T_RIGHT);
	for (int i = half_cols + 1; i < half_cols * 2; i++)
	{
		printf("%i", i);
		locate(i, line_offset);
		plot_vt100_char(HORIZ_LINE);
	}
	plot_vt100_char(T_LEFT);
}


int main(int argc, char **argv)
{
	struct winsize w = get_terminal_size();
	int lines = w.ws_row;
	int columns = w.ws_col;
	if (lines < 24 || columns < 80)
	{
		printf("Terminal Height/Width must be greater than 80x24\n");
		printf("But yours is only %ix%i\n", columns, lines);
		return 1;
	}
	clear();
	plot_outer_border(lines, columns);
	plot_inner_border(lines, columns);
	plot_right_horiz_border(lines, columns);
	printf("\n");
	return 0;
}
