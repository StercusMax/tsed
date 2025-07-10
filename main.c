#include <stdio.h>
#include <unistd.h>
#include <termios.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <string.h>

#define ESC "\033"
#define CRTL(c) ((c) - 'A' + 1)
#define INTIALLINES 200
#define INITIALPERLINE 50

//termios
void enable_raw();
void restore_term();

void clearscreen()
{
	printf(ESC"[2J"ESC"[H");
}

void manageeditor(unsigned char ** text);

int main()
{
	printf(ESC"[?25l");
	int i, j, currentscreenline;
	struct winsize size;
	if (ioctl(STDOUT_FILENO,TIOCGWINSZ, &size) < 0) {
    	size.ws_row = 25;
    	size.ws_col = 25;
	}
	enable_raw();
	unsigned char ** text = malloc(sizeof(unsigned char*)*(INTIALLINES + 1));
	text[INTIALLINES] = NULL;
	i = 0;
	while (i < INTIALLINES) {
		text[i] = malloc(sizeof(unsigned char) * INITIALPERLINE);
		i++;
	}
	text[0][0] = '\0';
	manageeditor(text);
	clearscreen();
	i = 0;
	printf("\nHere are the lines, hoho:\n");
	while (text[i]) {
		printf("line: %d\n", i);
		j = 0;
		currentscreenline = 0;
		while (text[i][j]) {
			putchar(text[i][j]);
			j++; currentscreenline++;
			if (currentscreenline + 1 == size.ws_col) {
				currentscreenline = 0;
				putchar('\n');
			}
		}
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


//lines[currentline] = realloc(lines[currentline], sizeof(unsigned char) * (INITIALPERLINE + nullpos[currentline] + 1)); works
#define reallocateline(s, nullpos) (s = realloc(s, sizeof(unsigned char) * (INITIALPERLINE + nullpos + 1)))
/*int reallocateline(char * s, int nullpos) //doesn't work
{
	printf("Segfault1");
	s = realloc(s, sizeof(unsigned char) * (INITIALPERLINE + nullpos + 1));
	printf("Segfault2"); // this one isn't printed when realloc doesn't work
	return;
	printf(ESC"[%d;%df", 24, 1);
	printf("%lx\n",s);
	if (s == NULL)
		return -1;
	return 0;
}*/

void clearline(int ws_colsize);
void updateline(int ws_colsize, int currentcollumns, int currentchar, unsigned char * s);
void backmovline(int ws_colsize, int *currentcollumns, int currentchar, unsigned char * s); //for Left arrow and backspace
void firstpl(int ws_colsize, unsigned char s[]);

void resetx();
void resety();
void addx(int n);
void addy(int n);
void showcursor();
int getx();
int gety();

void movbackstr(unsigned char * s, int i) //i is the empty one
{
	s[i] = s[i + 1];
	++i;
	while(s[i]) {
		s[i] = s[i + 1];
		i++;
	}
}

void movstr(unsigned char * s, int i) //i is the empty one
{
	unsigned char temp = s[i], secondtemp;
	s[i++] = ' ';
	while (temp) {
		secondtemp = s[i];
		s[i++] = temp;
		temp = secondtemp;
	}
	s[i] = temp; 
	
}

void movlines(unsigned char ** lines, int i, int nlines) //i is the empty one
{
	unsigned char temp = lines[i], secondtemp;
	i++;
	while (i < nlines) {
		secondtemp = lines[i];
		lines[i++] = temp;
		temp = secondtemp;
	}
	lines[i] = temp;
}

void manageeditor(unsigned char ** lines)
{
	int * nullpos = malloc(sizeof(int) * INTIALLINES);
	nullpos[0] = 0;
	int currentchar = 0, currentline = 0, nlines = 0, currentcollumns = 0; //nlines starting with 0 like the index
	unsigned char c;
	struct winsize size;
	if (ioctl(STDOUT_FILENO,TIOCGWINSZ, &size) < 0) {
    	size.ws_row = 25;
    	size.ws_col = 25;
	}
	clearscreen();
	showcursor();
	while ((c = getchar()) != CRTL('Q')) {
		printf(ESC"[%d;%df", gety(), getx()); //for debugging
		switch (c)
		{
		case CRTL(ARROWLEFT):
			addx(-1); 
			showcursor();
			if (currentchar) {
				currentchar--;
				backmovline(size.ws_col, &currentcollumns, currentchar, lines[currentline]);
			}
			break;
		case CRTL(ARROWRIGHT):
			if (currentchar == nullpos[currentline])
				break;
			
			{ //to destroy comparison quickly
			int comparison = size.ws_col;
			if (currentcollumns)
				comparison--;
			currentchar++;
			if (getx() == comparison - 1) {
				clearline(size.ws_col);
				putchar('>');
				addx(1);
				currentcollumns++;
				putchar(lines[currentline][158 + (currentcollumns - 1) * 156]); //since updateline only update on right side
				addx(1);
				showcursor();
				updateline(size.ws_col, currentcollumns, currentchar, lines[currentline]);
				break;
			}	
			}
						
			if (currentchar != nullpos[currentline]) {
				putchar(lines[currentline][currentchar - 1]);
				addx(1);
				showcursor();
				updateline(size.ws_col, currentcollumns, currentchar, lines[currentline]);
			}
			else if (currentchar == nullpos[currentline])
			{
				putchar(lines[currentline][currentchar - 1]);
				addx(1); showcursor();
			}
			break;
		case BACKSPACE:
			if (!currentchar)
				break;
			addx(-1);
			currentchar--; nullpos[currentline]--;
			if (currentchar != nullpos[currentline]) {
				movbackstr(lines[currentline], currentchar);
				backmovline(size.ws_col, &currentcollumns, currentchar, lines[currentline]);
			}
			else {
				addx(1);
				putchar(' '); //erase old cursor
				addx(-1);
				lines[currentline][currentchar] = '\0';
				if (currentcollumns && getx() == 2)
					backmovline(size.ws_col, &currentcollumns, currentchar, lines[currentline]);
			}
			showcursor(); //erase erased char and replace it by cursor
			break;
		case '\n':
			if (!lines[nlines + 1]) {
				lines = realloc(lines, (sizeof(unsigned char*)) * (INTIALLINES + nlines + 2)); //+1 for NULL pointer
				lines[INTIALLINES + nlines + 1] = NULL;
				nullpos = realloc(nullpos, (sizeof(int)) * (INTIALLINES + nlines + 1));
			}
			nullpos[currentline + 1] = 0;
			if (currentline != nlines) {
				movlines(lines, currentline + 1, nlines);
			}
			if (currentchar != nullpos[currentline]) {
				int i, j;
				if (currentchar > INITIALPERLINE - 1)
					reallocateline(lines[currentline + 1], nullpos[currentline] - currentchar);

				for(i = currentchar, j = 0; lines[currentline][i]; i++, j++)
					lines[currentline + 1][j] = lines[currentline][i];
				
				lines[currentline + 1][j] = '\0';
				nullpos[currentline + 1] = j;
				nullpos[currentline] = currentchar;
				lines[currentline][currentchar] = '\0';
				reallocateline(lines[currentline], nullpos[currentline]);
			}
			firstpl(size.ws_col, lines[currentline]);
			resetx(); currentchar = 0, currentcollumns = 0;
			if (gety() != size.ws_row)
				addy(1); 
			else
				putchar('\n');
			currentline++; nlines++;
			showcursor();
			if (currentchar != nullpos[currentline])
				updateline(size.ws_col, currentcollumns, currentchar, lines[currentline]);
			break;
		default:
			{
			int comparison = size.ws_col; //comparison is the length of the line, since the cursor takes one char... and > another...
			if (currentcollumns)
				comparison--;
			if (getx() == comparison - 1) {
				clearline(size.ws_col);
				putchar('>');
				addx(1);
				currentcollumns++;
			}
			}
			putchar(c);
			if ((nullpos[currentline] + 1) % INITIALPERLINE == 0) //reallocate if necessary
				reallocateline(lines[currentline], nullpos[currentline]);
			if (currentchar != nullpos[currentline]) { //move the string, if currrent char is not at the end
				movstr(lines[currentline], currentchar);
			}
			else {
				lines[currentline][currentchar + 1] = '\0';
			}
			lines[currentline][currentchar] = c;
			addx(1); showcursor(); currentchar++; nullpos[currentline]++;
			if (currentchar != nullpos[currentline]) 
				updateline(size.ws_col, currentcollumns, currentchar, lines[currentline]);
			break;
		}
		/*printf("\n\n");
		printf(ESC"[%d;%df", 25, 1);
		printf("%d, %d", nullpos[currentline], currentchar);*/
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

void updateline(int ws_colsize, int currentcollumns, int currentchar, unsigned char * line)
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
	if (i < comparison) {
		putchar(' ');
	}
	addx(-1);
}

void backmovline(int ws_colsize, int *currentcollumns, int currentchar, unsigned char * line)
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
		showcursor();
	}
	else
	{
		updateline(ws_colsize,*currentcollumns, currentchar, line);
	}
}

void firstpl(int ws_colsize, unsigned char * line)
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

void showcursor()
{
	printf(ESC"[%d;%df", curpos.y, curpos.x);
	putchar('|');
	printf(ESC"[%d;%df", curpos.y, curpos.x);
}

void addx(int n)
{
	if (getx() + n < 1) {
		return;
	}
	curpos.x += n;
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
	new_tio.c_lflag &=(~ICANON & ~ECHO & ~ISIG);

	/* set the new settings immediately */
	tcsetattr(STDIN_FILENO,TCSANOW,&new_tio);
}

void restore_term()
{
    tcsetattr(STDIN_FILENO,TCSANOW,&old_tio);
}
