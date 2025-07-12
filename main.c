#include <stdio.h>
#include <unistd.h>
#include <termios.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <string.h>
#include <stdint.h>

#define ESC "\033"
#define CRTL(c) ((c) - 'A' + 1)
#define INTIALLINES 10
#define INITIALPERLINE 50

FILE *serial;

/*typedef struct heap_segment_struct{
        uint64_t magic;
        uint64_t lenght;
        struct heap_segment_struct *next;
        struct heap_segment_struct *prev;
}heap_segment;

void print_seg(heap_segment *seg){
	fprintf(serial, "magic = %lx\n",seg->magic);
	fprintf(serial, "lenght = %ld\n",seg->lenght);
	fprintf(serial, "prev = %p\n",seg->prev);
	fprintf(serial, "next = %p\n",seg->next);
}*/

//termios
void enable_raw();
void restore_term();

void clearscreen()
{
	printf(ESC"[2J"ESC"[H");
}

void manageeditor(unsigned char *** text);

int main()
{
	int i;
	size_t chartoprint;
	char * ps;
	serial = fopen("/dev/ttyS0","w");
	printf(ESC"[?25l");
	struct winsize size;
	if (ioctl(STDOUT_FILENO,TIOCGWINSZ, &size) < 0) {
    	size.ws_row = 25;
    	size.ws_col = 25;
	}
	enable_raw();
	unsigned char ** text = malloc(sizeof(unsigned char*)*(INTIALLINES));
	i = 0;
	while (i < INTIALLINES) {
		text[i] = malloc(sizeof(unsigned char) * INITIALPERLINE);
		text[i][0] = '\0';
		i++;
	}
	manageeditor(&text);
	clearscreen();
	i = 0;
	while (text[i]) {
		printf("\nline[%d] is:\n", i);
		chartoprint = sizeof(text[i]);
		ps = text[i];
		while (chartoprint) {
			if (chartoprint > (size_t) size.ws_col) {
				printf("%.*s \n", size.ws_col - 1, ps);
				ps += size.ws_col - 1;
				chartoprint -= size.ws_col - 1;
			}
			else {
				printf("%s \n", ps);
				chartoprint = 0;
			}
		}
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
//#define reallocateline(s, nullpos) (s = realloc(s, sizeof(unsigned char) * (INITIALPERLINE + nullpos + 1)))
char * reallocateline(char * s, int nullpos)
{
	s = realloc(s, sizeof(unsigned char) * (INITIALPERLINE + nullpos + 2));
	return s;
}

void scrolldown(int ws_rowsize, int currentline, unsigned char ** lines, int nlines); //incorrect usage may segfault
void scrollup(int ws_rowsize, int currentline, unsigned char ** lines, int nlines); //incorrect usage may segfault

void clearline(int ws_colsize);
void updateline(int ws_colsize, int currentcollumns, int currentchar, unsigned char * s);
void backmovline(int ws_colsize, int *currentcollumns, int currentchar, unsigned char * s); //for Left arrow and backspace
void firstpl(int ws_colsize, unsigned char * s, int b_clearline);
void gotocurrentchar(int ws_colsize, int *currentcollumns,int currentchar, int nullpos, int currentline, unsigned char * s);  //set the line and the cursor generically

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

void movlines(unsigned char ** lines, int i, int nlines)
{
	clearline(160);
	unsigned char * temp = lines[i], * secondtemp;
	lines[i++] = lines[nlines + 1];
	while (i <= nlines) {
		secondtemp = lines[i];
		lines[i++] = temp;
		temp = secondtemp;
	}
	lines[i] = temp;
	return lines;
}

void movnullpos(int * nullpos, int i, int nlines)
{
	int temp = nullpos[i], secondtemp;
	nullpos[i++] = 0;
	while (i <= nlines) {
		secondtemp = nullpos[i];
		nullpos[i++] = temp;
		temp = secondtemp;
	}
	nullpos[i] = temp;
	return nullpos;
}

void manageeditor(unsigned char *** lines)
{
	int * nullpos = calloc(INTIALLINES, sizeof(int));
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
			if (currentchar) {
				addx(-1); 
				showcursor();
				currentchar--;
				backmovline(size.ws_col, &currentcollumns, currentchar, (*lines)[currentline]);
			}
			else if (currentline) {
				currentline--; currentcollumns = 0;
				currentchar = nullpos[currentline];
				if (gety() == 1)
					scrollup(size.ws_row, currentline, *lines, nlines);
				else {
					firstpl(size.ws_col, (*lines)[currentline + 1], 1);
					addy(-1);
				}
				gotocurrentchar(size.ws_col, &currentcollumns, currentchar, nullpos[currentline], currentline, (*lines)[currentline]);
			}
			break;
		case CRTL(ARROWRIGHT):
			if (currentchar == nullpos[currentline]) {
				if (currentline < nlines) {
					currentline++; currentcollumns = 0; 
					if (gety() < size.ws_row)
						addy(1);
					currentchar = 0;
					if (gety() == size.ws_row) {
						scrolldown(size.ws_row, currentline, *lines, nlines);
					}
					else {	
						addy(-1);
						firstpl(size.ws_col, (*lines)[currentline - 1], 1);
						addy(1);
					}
					gotocurrentchar(size.ws_col, &currentcollumns, currentchar, nullpos[currentline], currentline, (*lines)[currentline]);
				}
				break;
			}
			
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
				putchar((*lines)[currentline][158 + (currentcollumns - 1) * 156]); //since updateline only update on right side
				addx(1);
				showcursor();
				updateline(size.ws_col, currentcollumns, currentchar, (*lines)[currentline]);
				break;
			}	
			}
						
			if (currentchar != nullpos[currentline]) {
				putchar((*lines)[currentline][currentchar - 1]);
				addx(1);
				showcursor();
				updateline(size.ws_col, currentcollumns, currentchar, (*lines)[currentline]);
			}
			else if (currentchar == nullpos[currentline])
			{
				putchar((*lines)[currentline][currentchar - 1]);
				addx(1); showcursor();
			}
			break;
		case CRTL(ARROWDOWN):
			//scrolldown(size.ws_row, currentline, *lines);
			break;
		case BACKSPACE:
			if (!currentchar)
				break;
			addx(-1);
			currentchar--; nullpos[currentline]--;
			if (currentchar != nullpos[currentline]) {
				movbackstr((*lines)[currentline], currentchar);
				backmovline(size.ws_col, &currentcollumns, currentchar, (*lines)[currentline]);
			}
			else {
				addx(1);
				putchar(' '); //erase old cursor
				addx(-1);
				(*lines)[currentline][currentchar] = '\0';
				if (currentcollumns && getx() == 2)
					backmovline(size.ws_col, &currentcollumns, currentchar, (*lines)[currentline]);
			}
			showcursor(); //erase erased char and replace it by cursor
			break;
		case '\n':
			if ((nlines + 1) % INTIALLINES == 0) {
				int i;
				(*lines) = realloc((*lines), (sizeof(unsigned char*)) * (INTIALLINES + nlines + 2)); //+1 to turn list into n element, +1 for next element
				nullpos = realloc(nullpos, (sizeof(int)) * (INTIALLINES + nlines + 2));
				for (i = nlines + 1; i < INTIALLINES + nlines + 2; i++) {
					(*lines)[i] = malloc(sizeof(unsigned char) * INITIALPERLINE);
					(*lines)[i][0] = '\0';
					if (i < INTIALLINES + nlines + 1)
						nullpos[i] = 0;
 				}
			}
			if (currentline != nlines) {
				movlines((*lines), currentline + 1, nlines);
				movnullpos(nullpos, currentline + 1, nlines);
			}
			if(gety() + 1 == size.ws_row) {
				scrolldown(size.ws_row, currentline, *lines, nlines);
				addy(-1);
			}
			if (currentchar != nullpos[currentline]) {
				int i, j;
				if (currentchar > INITIALPERLINE - 1)
					(*lines)[currentline + 1] = reallocateline((*lines)[currentline + 1], nullpos[currentline] - currentchar);

				for(i = currentchar, j = 0; (*lines)[currentline][i]; i++, j++)
					(*lines)[currentline + 1][j] = (*lines)[currentline][i];
				
				(*lines)[currentline + 1][j] = '\0';
				nullpos[currentline + 1] = j;
				(*lines)[currentline][currentchar] = '\0';
				(*lines)[currentline] = reallocateline((*lines)[currentline], nullpos[currentline]);
				nullpos[currentline] = currentchar;
			}
			firstpl(size.ws_col, (*lines)[currentline], 1);
			resetx(); currentchar = 0, currentcollumns = 0;
			addy(1); 
			currentline++; nlines++;
			clearline(size.ws_col);
			if (currentline != nlines) {
				int i;
				i = currentline + 1;
				while (gety() < size.ws_row - 1 && i <= nlines) {
					addy(1);
					firstpl(size.ws_col, (*lines)[i], 1);
					i++;
				}
				addy(-(i - (currentline + 1)));
			}
			showcursor();
			if (currentchar != nullpos[currentline])
				updateline(size.ws_col, currentcollumns, currentchar, (*lines)[currentline]);
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
				(*lines)[currentline] = reallocateline((*lines)[currentline], nullpos[currentline]);
			if (currentchar != nullpos[currentline]) //move the string, if currrent char is not at the end
				movstr((*lines)[currentline], currentchar);
			else
				(*lines)[currentline][currentchar + 1] = '\0';
			(*lines)[currentline][currentchar] = c;
			addx(1); showcursor(); currentchar++; nullpos[currentline]++;
			if (currentchar != nullpos[currentline]) 
				updateline(size.ws_col, currentcollumns, currentchar, (*lines)[currentline]);
			break;
		}
		/*printf("\n\n");
		printf(ESC"[%d;%df", 25, 1);
		printf("%d, %d, %d", nullpos[currentline], currentline, size.ws_row);
		*/
	}
	free(nullpos);
	int i = nlines + 1;
	if (!(i % INTIALLINES)) {
		(*lines)[nlines + 1] = NULL;
	}
	else
	{
		while (i % INTIALLINES) { //free unused ellements
			free((*lines)[i]);
			i++;
		}
		(*lines)[nlines + 1] = NULL;
	}
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
			comparison--; //since there's >
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

void firstpl(int ws_colsize, unsigned char * line, int b_clearline)
{
	size_t strsize;
	if (b_clearline)
		clearline(ws_colsize);
	//printf("%.*s", ws_colsize - 1, line); printf on stanix doesn't work with . for now
	strsize = strlen(line);
	if (strsize == 0)
		return;
	if (strsize > ws_colsize - 1)
		fwrite(line,ws_colsize - 1,1,stdout);
	else
		fwrite(line,strsize,1,stdout);
}

void gotocurrentchar(int ws_colsize, int *currentcollumns , int currentchar, int nullpos, int currentline, unsigned char * s)
{
	int i, oldi;
	clearline(ws_colsize);
	if (currentchar + 1 < ws_colsize) {
		for (i = 0; i < currentchar; i++) {
			putchar(s[i]);
			addx(1);
		}
	}
	else {
		*currentcollumns = 1;
		for (oldi = i = 158; i < currentchar; i++) {
			if (i - 156 == oldi)
				(*currentcollumns)++;
		}
		putchar('>');
		addx(1);
		i = 158 + (*currentcollumns - 1) * 156;
		while (i < currentchar)  {
			putchar(s[i]);
			i++;
			addx(1);
		}
	}
	if (currentchar != nullpos)
		updateline(ws_colsize, *currentcollumns, currentchar, s); //already showcursor
	showcursor();
}

void scrolldown(int ws_rowsize, int currentline, unsigned char ** lines, int nlines)
{
	int i;
	clearscreen(); resety(); resetx();
	if (currentline == nlines) {
		i = currentline - ws_rowsize + 2;
		while (gety() < ws_rowsize && i <= nlines) {
			firstpl(160, lines[i], 0);
			//printf("lines[%d] == %s \n", i, lines[i]);
			addy(1); resetx(); i++;
		}
	}
	else
	{
		i = currentline - ws_rowsize + 2;
		while (gety() < ws_rowsize && i <= nlines) {
			firstpl(160, lines[i], 0);
			resetx(); i++; addy(1); 
			//printf(ESC"[%d;%df", gety() - 1, getx()); //for debugging
			//printf("%d", gety());
		}
	}
	addy(-1);
}

void scrollup(int ws_rowsize, int currentline, unsigned char ** lines, int nlines)
{
	int i;
	i = currentline;
	clearscreen(); resety(); resetx();
	while (gety() < ws_rowsize) {
		firstpl(160, lines[i], 0);
		addy(1); resetx(); i++;
	}
	addy(-(i - currentline));
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
