#include <stdio.h>
#include <unistd.h>
#include <termios.h>
#include <stdlib.h>
#include <sys/ioctl.h>

#define ESC "\033"
#define CRTL(c) ((c) - 'A' + 1)
#define INTIALLINES 10
#define INITIALPERLINE 50

//termios
void enable_raw();
void restore_term();

void clearscreen()
{
	printf(ESC"[2J"ESC"[H");
}

void manageeditor(char ** text);

int main()
{
	printf(ESC"[?25l");
	int i, j;
	struct winsize size;
	if (ioctl(STDOUT_FILENO,TIOCGWINSZ, &size) < 0) {
    	size.ws_row = 25;
    	size.ws_col = 25;
	}
	enable_raw();
	unsigned char ** text = malloc(sizeof(unsigned char*)*(INTIALLINES+1));
	text[INTIALLINES] = NULL;
	i = 0;
	while (text[i]) {
		text[i] = malloc(sizeof(unsigned char) * INITIALPERLINE);
		text[i][0] = '\0';
		i++;
	}
	manageeditor(text);
	clearscreen();
	i = 0;
	printf("\nHere are the lines, hoho:\n");
	while (text[i]) {
		printf("line: %d\n", i);
		j = 0;
		while (text[i][j]) {
			putchar(text[i][j]);
			j++;
			if (j + 1 == size.ws_col)
				putchar('\n');
		}
		printf("\\0");
		putchar('\n');
		free(text[i]);
		i++;
	}
	free(text);
	restore_term();
	return 0;
}

enum Keys {
	ARROWUP = 'E',
	ARROWDOWN = 'D',
	ARROWLEFT = 'S',
	ARROWRIGHT = 'F',
	BACKSPACE = 127
};

void clearline(int ws_colsize);
void updateline(int ws_colsize, int currentcollumns, int currentchar, char s[]);
void backmovline(int ws_colsize, int *currentcollumns, int currentchar, char s[]); //for Left arrow and backspace
void firstpl(int ws_colsize, int currentline, char s[]);

void resetx();
void resety();
void addx(int n);
void addy(int n);
int getx();
int gety();

void movbackstr(char s[], int i) //i is the empty one
{
	s[i] = s[i + 1];
	++i;
	while(s[i]) {
		s[i] = s[i + 1];
		i++;
	}
}

void movstr(char s[], int i) //i is the empty one
{
	char temp = s[i], secondtemp;
	s[i++] = ' ';
	while (temp) {
		secondtemp = s[i];
		s[i++] = temp;
		temp = secondtemp;
	}
	s[i] = temp; 
	
}

void manageeditor(char ** s)
{
	int * nullpos = calloc(INTIALLINES,sizeof(int)), currentcollumns = 0;
	int currentchar = 0, currentline = 0, nlines = 0; //nlines starting with 0 like the index
	unsigned char c;
	struct winsize size;
	if (ioctl(STDOUT_FILENO,TIOCGWINSZ, &size) < 0) {
    	size.ws_row = 25;
    	size.ws_col = 25;
	}
	clearscreen();
	putchar('|');
	resetx(); //put the cursor position on the initial |
	s[0][0] = '\0';
	while ((c = getchar()) != CRTL('Q')) {
		switch (c)
		{
		case CRTL(ARROWLEFT):
			addx(-1); 
			if (currentchar) {
				currentchar--;
				backmovline(size.ws_col, &currentcollumns, currentchar, s[currentline]);
			}
			break;
		case CRTL(ARROWRIGHT):
			if (currentchar == nullpos[currentline])
				break;
			
			currentchar++;
			{ //to define comparison without issue we reduce the scope
			int comparison = size.ws_col;
			if (currentcollumns)
				comparison--;
			if (getx() == comparison - 1) {
				clearline(size.ws_col);
				putchar('>');
				addx(1);
				currentcollumns++;
				putchar(s[currentline][158 + (currentcollumns - 1) * 156]); //since updateline only update on right side
				addx(1);
				updateline(size.ws_col, currentcollumns, currentchar, s[currentline]);
				break;
			}	
			}
			addx(1);
			
			
			if (currentchar != nullpos[currentline]) {
				addx(-1);
				putchar(s[currentline][currentchar - 1]);
				addx(1);
				addx(1);
				int i = currentchar;
				int comparison = size.ws_col - 2; //to turn into the list element
				if (currentcollumns)
					comparison += (currentcollumns) * 156;
				while (s[currentline][i] && i < comparison) {
					putchar(s[currentline][i]);
					i++;
				}
				addx(-1);
			}
			else if (currentchar == nullpos[currentline])
			{
				currentchar--;
				addx(-1);
				putchar(s[currentline][currentchar]);
				addx(1); currentchar++;
			}
			break;
		case BACKSPACE:
			if (getx() == 1)
				break;
			addx(-1);
			putchar(' ');
			addx(1);
			if (currentchar != nullpos[currentline]) {
				movbackstr(s[currentline], currentchar - 1);
				nullpos[currentline] -= 1;
				addx(-1); currentchar--;
			}
			else {
				addx(-1);
				if (nullpos[currentline] && currentchar) {
					currentchar--;
					s[currentline][currentchar] = '\0';
					nullpos[currentline] -= 1;
				}
			}
			backmovline(size.ws_col, &currentcollumns, currentchar, s[currentline]);
			break;
		case '\n':
			if (currentchar != nullpos[currentline]) {
				int i = currentchar, j = 0;
				/*/if (j > INITIALPERLINE - 1) {
					s[currentline + 1] = realloc(s[currentline + 1], sizeof(unsigned char) * (INITIALPERLINE + nullpos[currentline] + 1)); //not great but nvm
				}/*/
				while (s[currentline][i]) {
					s[currentline + 1][j] = s[currentline][i];
					i++; j++;
				}
				s[currentline + 1][j] = '\0';
				nullpos[currentline + 1] = j;
				nullpos[currentline] = currentchar;
				s[currentline][currentchar] = '\0';
			}
			firstpl(size.ws_col, currentline, s[currentline]);
			resetx(); currentchar = 0, currentcollumns = 0;
			addy(1); currentline++; nlines++;
			putchar('|');
			resetx(); //put the cursor position on the initial |
			//if (currentchar != nullpos[currentline])
			updateline(size.ws_col, currentcollumns, currentchar, s[currentline]);
			//else
				//s[currentline][0] = '\0';
			break;
		default:
			printf(ESC"[%d;%df", gety(), getx()); //for debugging
			int comparison = size.ws_col; //comparison is the length of the line, since the cursor takes one char... and > another...
			if (currentcollumns)
				comparison--;
			if (getx() == comparison - 1) {
				clearline(size.ws_col);
				putchar('>');
				addx(1);
				currentcollumns++;
			}
			putchar(c);
			if ((nullpos[currentline] + 1) % INITIALPERLINE == 0) { //reallocate if necessary
				s[currentline] = realloc(s[currentline], sizeof(unsigned char) * (INITIALPERLINE + nullpos[currentline] + 1));
				if (s[currentline] == NULL)
					printf("\nPanicking!!");
				printf(ESC"[%d;%df", 24, 1);
				printf("\n\n\n\n\n\n\nReallocating: %d", INITIALPERLINE + nullpos[currentline] + 1);
			}
			if (currentchar != nullpos[currentline]) { //move the string, if currrent char is not at the end
				movstr(s[currentline], currentchar);
			}
			else {
				s[currentline][currentchar + 1] = '\0';
			}
			s[currentline][currentchar] = c;
			addx(1); currentchar++; nullpos[currentline]++;
			if (currentchar != nullpos[currentline])
				updateline(size.ws_col, currentcollumns, currentchar, s[currentline]);
			break;
		}
		printf("\n\n");
		printf(ESC"[%d;%df", 25, 1);
		printf("%d, %d", nullpos[currentline], currentchar);
	}
	free(nullpos);
}

void clearline(int ws_colsize)
{
	resetx();
	int i = 0;
	while (i < ws_colsize)  {
		putchar(' ');
		i++;
	}
	resetx();
}

void updateline(int ws_colsize, int currentcollumns, int currentchar, char line[])
{
	int comparison, i;

	addx(1);
	comparison = ws_colsize - 2; //to turn into the list element
	if (currentcollumns)
		comparison += currentcollumns * 156;
	i = currentchar;
	while (line[i] && i < comparison) {
		putchar(line[i]);
		i++;
	}
	addx(-1);
}

void backmovline(int ws_colsize, int *currentcollumns, int currentchar, char line[])
{	
	if ((*currentcollumns) && getx() == 2) { //in the case where we exit the currentcollumn
		int comparison, i;
		(*currentcollumns)--;
		clearline(ws_colsize);
		comparison = ws_colsize - 1; //since there's |
		i = 0;
		
		if (*currentcollumns) {
			putchar('>');
			addx(1);
			comparison -= 1; //since there's >
			i = 158 + (*currentcollumns - 1) * 156;
		}
		while (getx() < comparison)  {
			putchar(line[i]);
			i++;
			addx(1);
		}
	}
	else
	{
		updateline(ws_colsize,*currentcollumns, currentchar, line);
	}
}

void firstpl(int ws_colsize, int currentline, char line[])
{
	clearline(ws_colsize);
	int comparison, i;
	comparison = ws_colsize - 1;
	i = 0;
	while (line[i] && i < comparison) {
		putchar(line[i]);
		i++;
	}
}

struct pos
{
	int x;
	int y;	
};

struct pos curpos = {.x = 1, .y = 1};

void resetx()
{
	curpos.x = 1;
	printf(ESC"[%d;%df", curpos.y, curpos.x);
}

void resety()
{
	curpos.y = 1;
	printf(ESC"[%d;%df", curpos.y, curpos.x);
}

void addx(int n)
{
	if (getx() + n < 1) {
		return;
	}
	if (n < 0)
		putchar(' ');
	curpos.x += n;
	printf(ESC"[%d;%df", curpos.y, curpos.x);
	putchar('|');
	printf(ESC"[%d;%df", curpos.y, curpos.x);
}

void addy(int n)
{
	if (curpos.y + n < 1) {
		return;
	}
	curpos.y += n;
	printf(ESC"[%d;%df", curpos.y, curpos.x);
}

int getx()
{
	return curpos.x;
}

int gety()
{
	return curpos.y;
}

struct termios old_tio, new_tio;

void enable_raw()
{
	/* get the terminal settings for stdin */
	tcgetattr(STDIN_FILENO,&old_tio);

	/* we want to keep the old setting to restore them a the end */
	new_tio=old_tio;

	/* disable canonical mode (buffered i/o) and local echo */
	new_tio.c_lflag &=(~ICANON & ~ECHO);

	/* set the new settings immediately */
	tcsetattr(STDIN_FILENO,TCSANOW,&new_tio);
}

void restore_term()
{
    tcsetattr(STDIN_FILENO,TCSANOW,&old_tio);
}
