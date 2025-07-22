#include <stdio.h>
#include <unistd.h>
#include <termios.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>

#define ESC "\033"
#define CRTL(c) ((c) - 'A' + 1)
#define INTIALLINES 10
#define INITIALPERLINE 50
#define TABLENGTH 4

struct winsize size;

//termios
void enable_raw();
void restore_term();

void clearscreen()
{
	printf(ESC"[2J"ESC"[H");
}


int manageeditor(unsigned char *** text, char * filename);


int main(int argc, char ** argv)
{
	printf(ESC"[?25l"); // hide cursor
	if (argc != 2) {
		printf("You need to put one file as argument");
		putchar('\n'); //idk i can't \n from printf
		return 0;
	}
	
	int i;
	size_t chartoprint;
	unsigned char * ps;
	
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
	manageeditor(&text,argv[1]);
	clearscreen();
	i = 0;
	while (text[i]) {
		free(text[i]);
		i++;
	}
	free(text);
	restore_term();
	return 0;
}

enum Keys {
	BACKSPACE = 127,
	ARROWLEFT = 1000,
	ARROWRIGHT,
	ARROWDOWN,
	ARROWUP
};

void scrolldown(int currentline, int nlines, unsigned char ** lines); //incorrect usage may segfault
void scrollup(int currentline, unsigned char ** lines); //incorrect usage may segfault

void clearline();
void updateline(int currentcollumns, int currentchar, unsigned char * s);
void backmovline(int *currentcollumns, int currentchar, unsigned char * s); //for Left arrow and backspace
void firstpl(unsigned char * s, int b_clearline);
void gotocurrentchar(int *currentcollumns, int currentchar, int nullpos, unsigned char * s);  //set the line and the cursor generically
void redrawscreen(int currentline, unsigned char ** lines);

void resetx();
void resety();
void addx(int n);
void addy(int n);
void showcursor();
int getx();
int gety();

int ex_tab = 0;

int getkey()
{
	if (ex_tab) {
		ex_tab--;
		return ' ';
	}
	int c;
	c = getchar();
	if (c == '\033') {
		int i;
		unsigned char seq[2];
		for (i = 0; i < 2; i++)
			seq[i] = getchar();
		if (seq[0] == '[')
			switch (seq[1])
			{
			case 'C':
				return ARROWRIGHT;
			case 'D':
				return ARROWLEFT;
			case 'A':
				return ARROWUP;
			case 'B':
				return ARROWDOWN;
			}
		return -1; //unrecognized esc sequence
	}
	else
	{
		if (c == '\t') {
			ex_tab = TABLENGTH - 1;
			return ' ';
		}
		return c;
	}
}

void reallocateline(unsigned char ** s, int reallocator)
{
	*s = realloc(*s, sizeof(unsigned char) * (reallocator));
}

void movbackstr(unsigned char * s, int i) //i is the empty one
{
	s[i] = s[i + 1];
	i++;
	while(s[i]) {
		s[i] = s[i + 1];
		i++;
	}
}

void movstr(unsigned char * s, int i) //i is the empty one
{
	unsigned char temp = s[i], secondtemp;
	s[i] = ' ';
	i++;
	while (temp) {
		secondtemp = s[i];
		s[i] = temp;
		temp = secondtemp;
		i++;
	}
	s[i] = temp; 
	
}

void movbacklines(unsigned char ** lines, int i, int nlines) //i is the empty one
{
	unsigned char * temp;
	temp = lines[i];
	lines[i] = lines[i + 1];
	i++;
	while(i < nlines) {
		lines[i] = lines[i + 1];
		i++;
	}
	lines[i] = temp;
	lines[i][0] = '\0';
}

void movlines(unsigned char ** lines, int i, int nlines)
{
	unsigned char * temp = lines[i], * secondtemp;
	lines[i] = lines[nlines + 1];
	i++;
	while (i <= nlines) {
		secondtemp = lines[i];
		lines[i] = temp;
		temp = secondtemp;
		i++;
	}
	lines[i] = temp; 
}

void movbacknullpos(int * nullpos, int i, int nlines) //i is the empty one
{
	int temp;
	temp = nullpos[i];
	nullpos[i] = nullpos[i + 1];
	i++;
	while(i < nlines) {
		nullpos[i] = nullpos[i + 1];
		i++;
	}
	nullpos[i] = temp;
	nullpos[i] = 0;
}

void movnullpos(int * nullpos, int i, int nlines)
{
	int temp = nullpos[i], secondtemp;
	nullpos[i] = 0;
	i++;
	while (i <= nlines) {
		secondtemp = nullpos[i];
		nullpos[i] = temp;
		temp = secondtemp;
		i++;
	}
	nullpos[i] = temp;
}

int manageeditor(unsigned char *** lines, char * filename)
{
	clearscreen();
	int * nullpos = calloc(INTIALLINES, sizeof(int));
	int currentchar = 0, currentline = 0, nlines = 0, currentcollumns = 0; //nlines starting with 0 like the index
	int lasthighernullpos_updown = 0; //for up and down arrows
	int key;
	int edited = 0;
	FILE * fptr = fopen(filename, "r");
	if (fptr) { //put the content of the file into the editor
		int c, oldy, i;
		while ((c = fgetc(fptr)) != EOF) {
			switch(c) {
				case '\n':
					if ((nlines + 1) % INTIALLINES == 0) {
						int i;
						(*lines) = realloc((*lines), (sizeof(unsigned char*)) * (INTIALLINES + nlines + 1)); //+1 for next element
						nullpos = realloc(nullpos, (sizeof(int)) * (INTIALLINES + nlines + 1));
						for (i = nlines + 1; i < INTIALLINES + nlines + 1; i++) {
							(*lines)[i] = malloc(sizeof(unsigned char) * INITIALPERLINE);
							(*lines)[i][0] = '\0';
							nullpos[i] = 0;
 						}
					}
					nlines++; currentline++; currentchar = 0, currentcollumns = 0;
					break;
				case '\t':
					int tablength = TABLENGTH;
					c = ' ';
					while (tablength) {
						if ((nullpos[currentline] + 1) % INITIALPERLINE == 0) //reallocate if necessary
							reallocateline(&((*lines)[currentline]), INITIALPERLINE + nullpos[currentline] + 1);
						(*lines)[currentline][currentchar] = c;
						(*lines)[currentline][currentchar + 1] = '\0';
						currentchar++; nullpos[currentline]++;
						tablength--;
					}
					break;
				default:
					if ((nullpos[currentline] + 1) % INITIALPERLINE == 0) //reallocate if necessary
						reallocateline(&((*lines)[currentline]), INITIALPERLINE + nullpos[currentline] + 1);
					(*lines)[currentline][currentchar] = c;
					(*lines)[currentline][currentchar + 1] = '\0';
					currentchar++; nullpos[currentline]++;
					break;
			}
		}
		currentline = 0; currentchar = 0;
		fclose(fptr);
		redrawscreen(currentline, *lines);
		oldy = gety();
		i = currentline + 1;
		while (gety() + 1 < size.ws_row && i <= nlines) {
			addy(1);
			firstpl((*lines)[i], 0);
			i++;
		}
		addy(-(gety() - oldy));
		gotocurrentchar(&currentcollumns, currentchar, *nullpos, (*lines)[currentline]);
	}
	else if(errno != ENOENT) {
		free(nullpos);
		return -1; //unknown error that prevent us from reading/writing
	}
	showcursor();
	while (1) {
		key = getkey();
		if (key == -1) //unrecognized ESC sequence
			continue;
		switch (key)
		{
		case ARROWLEFT:
			if (currentchar) {
				addx(-1); 
				showcursor();
				currentchar--;
				backmovline(&currentcollumns, currentchar, (*lines)[currentline]);
			}
			else if (currentline) {
				currentline--; currentcollumns = 0;
				currentchar = nullpos[currentline];
				if (gety() == 1)
					scrollup(currentline, *lines);
				else {
					firstpl((*lines)[currentline + 1], 1);
					addy(-1);
				}
				gotocurrentchar(&currentcollumns, currentchar, nullpos[currentline], (*lines)[currentline]);
			}
			break;
		case ARROWRIGHT:
			if (currentchar == nullpos[currentline]) {
				if (currentline < nlines) {
					currentline++; currentchar = currentcollumns = 0; 
					if (gety() < size.ws_row)
						addy(1);
					if (gety() == size.ws_row) {
						scrolldown(currentline, nlines, *lines);
					}
					else {	
						addy(-1);
						firstpl((*lines)[currentline - 1], 1);
						addy(1);
					}
					gotocurrentchar(&currentcollumns, currentchar, nullpos[currentline], (*lines)[currentline]);
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
				updateline(currentcollumns, currentchar, (*lines)[currentline]);
				break;
			}	
			}
						
			if (currentchar != nullpos[currentline]) {
				putchar((*lines)[currentline][currentchar - 1]);
				addx(1);
				showcursor();
				updateline(currentcollumns, currentchar, (*lines)[currentline]);
			}
			else if (currentchar == nullpos[currentline])
			{
				putchar((*lines)[currentline][currentchar - 1]);
				addx(1); showcursor();
			}
			break;
		case ARROWDOWN:
			if (currentline == nlines)
				break;
			if (!lasthighernullpos_updown)
				lasthighernullpos_updown = currentchar;
			if (nullpos[currentline + 1] < lasthighernullpos_updown)
				currentchar = nullpos[currentline + 1];
			else
				currentchar = lasthighernullpos_updown;

			currentline++; currentcollumns = 0;
			if (gety() < size.ws_row)
				addy(1);
			if (gety() == size.ws_row) {
				scrolldown(currentline, nlines, *lines);
			}
			else {	
				addy(-1);
				firstpl((*lines)[currentline - 1], 1);
				addy(1);
			}
			gotocurrentchar(&currentcollumns, currentchar, nullpos[currentline], (*lines)[currentline]);
			break;
		case ARROWUP:
			if (!currentline)
				break;
			if (!lasthighernullpos_updown)
				lasthighernullpos_updown = currentchar;
			if (nullpos[currentline - 1] < lasthighernullpos_updown)
				currentchar = nullpos[currentline - 1];
			else
				currentchar = lasthighernullpos_updown;
			currentline--; currentcollumns = 0;
			if (gety() == 1)
				scrollup(currentline, *lines);
			else {
				firstpl((*lines)[currentline + 1], 1);
				addy(-1);
			}
			gotocurrentchar(&currentcollumns, currentchar, nullpos[currentline], (*lines)[currentline]);
			break;
		case BACKSPACE:
			edited = 1;
			if (currentchar) {
				addx(-1);
				currentchar--; nullpos[currentline]--;
				if (currentchar != nullpos[currentline]) {
					movbackstr((*lines)[currentline], currentchar);
					backmovline(&currentcollumns, currentchar, (*lines)[currentline]);
				}
				else {
					addx(1);
					putchar(' '); //erase old cursor
					addx(-1);
					(*lines)[currentline][currentchar] = '\0';
					if (currentcollumns && getx() == 2)
						backmovline(&currentcollumns, currentchar, (*lines)[currentline]);
				}
				showcursor(); //erase erased char and replace it by cursor
			}
			else if (currentline) {
				int i, j, lastnullpos, oldy;
				if (nullpos[currentline]) {
					for (j = 0, i = nullpos[currentline - 1]; i - nullpos[currentline - 1] < nullpos[currentline]; i++)
						if (i % INITIALPERLINE == 0)
							j++;
					if (j)
						reallocateline(&((*lines)[currentline - 1]), INITIALPERLINE*(j));
					for (j = currentchar, i = nullpos[currentline - 1]; j < nullpos[currentline]; i++, j++)
						(*lines)[currentline - 1][i] = (*lines)[currentline][j];
					(*lines)[currentline - 1][i] = '\0'; lastnullpos = nullpos[currentline - 1]; nullpos[currentline - 1] = i;
				}
				else
				{
					lastnullpos = nullpos[currentline - 1];
				}
				movbacklines(*lines, currentline, nlines);
				movbacknullpos(nullpos, currentline, nlines);
				nlines--; currentline--; currentchar = lastnullpos; addy(-1);
				redrawscreen(currentline, *lines);
				oldy = gety();
				i = currentline + 1;
				while (gety() + 1 < size.ws_row && i <= nlines) {
					addy(1);
					firstpl((*lines)[i], 0);
					i++;
				}
				addy(-(gety() - oldy));
				gotocurrentchar(&currentcollumns, currentchar, *nullpos, (*lines)[currentline]);
				showcursor();
			}
			break;
		case '\n':
			edited = 1;
			if ((nlines + 1) % INTIALLINES == 0) {
				int i;
				(*lines) = realloc((*lines), (sizeof(unsigned char*)) * (INTIALLINES + nlines + 1)); //+1 for next element
				nullpos = realloc(nullpos, (sizeof(int)) * (INTIALLINES + nlines + 1));
				for (i = nlines + 1; i < INTIALLINES + nlines + 1; i++) {
					(*lines)[i] = malloc(sizeof(unsigned char) * INITIALPERLINE);
					(*lines)[i][0] = '\0';
					nullpos[i] = 0;
 				}
			}
			if (currentline != nlines) {
				movlines((*lines), currentline + 1, nlines);
				movnullpos(nullpos, currentline + 1, nlines);
			}
			if (currentchar != nullpos[currentline]) {
				int i, j, reallocsize;
				for (j = 0, i = currentchar; i < nullpos[currentline];i++)
						if (i % INITIALPERLINE == 0)
							j++;
				reallocsize = j;
				if (reallocsize)
					reallocateline(&((*lines)[currentline + 1]), INITIALPERLINE*(reallocsize));

				for(i = currentchar, j = 0; (*lines)[currentline][i]; i++, j++)
					(*lines)[currentline + 1][j] = (*lines)[currentline][i];
				
				(*lines)[currentline + 1][j] = '\0';
				nullpos[currentline + 1] = j;
				(*lines)[currentline][currentchar] = '\0';
				nullpos[currentline] = currentchar;
				if (reallocsize) {
					for (j = 0, i = 0; i < nullpos[currentline];i++)
						if (i % INITIALPERLINE == 0)
							j++;
					reallocateline(&((*lines)[currentline]), INITIALPERLINE*(j + 1));
				}
			}
			
			resetx(); currentchar = 0, currentcollumns = 0; addy(1);
			currentline++; nlines++;
			if(gety() == size.ws_row) {
				scrolldown(currentline, nlines, *lines);
			}
			else
			{
				int i, oldy;
				addy(-1);
				firstpl((*lines)[currentline - 1], 1);
				addy(1);
				if (currentline != nlines) {
					redrawscreen(currentline, *lines);
					oldy = gety();
					i = currentline + 1;
					while (gety() + 1 < size.ws_row && i <= nlines) {
						addy(1);
						firstpl((*lines)[i], 0);
						i++;
					}
					addy(-(gety() - oldy));
				}
				else
				{
					clearline(size.ws_col);
				}
			}
			showcursor();
			if (currentchar != nullpos[currentline])
				updateline(currentcollumns, currentchar, (*lines)[currentline]);
			break;
		case CRTL('S'):
			int i;
			FILE * fptr = fopen(filename, "w");
			if (fptr == NULL)
				return -1;
			for (i = 0; i <= nlines; i++)
				fprintf(fptr, "%s\n", (*lines)[i]);
			fclose(fptr);
			edited = 0;
			break;
		case CRTL('Q'):
			if (edited) {
				clearscreen();
				printf("The file wasn't save, are you sure to leave without saving you changes? \n Save your changes? (Y/N/C)");
				while (1) {
					key = getkey();
					switch (key)
					{
					case 'y': case 'Y':
						int i;
						FILE * fptr = fopen(filename, "w");
						if (fptr == NULL)
							return -1;
						for (i = 0; i <= nlines; i++)
							fprintf(fptr, "%s \n", (*lines)[i]);
						fclose(fptr);
						goto end;
					case 'n': case 'N':
						goto end;
					case 'c': case 'C':
						redrawscreen(currentline, *lines);
						int oldy = gety();
						i = currentline + 1;
						while (gety() + 1 < size.ws_row && i <= nlines) {
							addy(1);
							firstpl((*lines)[i], 0);
							i++;
						}
						addy(-(gety() - oldy));
						gotocurrentchar(&currentcollumns, currentchar, *nullpos, (*lines)[currentline]);
						break;
					default:
						break;
					}
					if (key == 'Y' || key == 'y' || key == 'N' || key == 'n' || key == 'C' || key == 'c')
						break;
				}
			}
			else
				goto end;
			break;
		default:
			edited = 1;
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
			putchar(key);
			if ((nullpos[currentline] + 1) % INITIALPERLINE == 0) //reallocate if necessary
				reallocateline(&((*lines)[currentline]), INITIALPERLINE + nullpos[currentline] + 1);
			if (currentchar != nullpos[currentline]) //move the string, if currrent char is not at the end
				movstr((*lines)[currentline], currentchar);
			else
				(*lines)[currentline][currentchar + 1] = '\0';
			(*lines)[currentline][currentchar] = key;
			addx(1); 
			showcursor(); currentchar++; nullpos[currentline]++;
			if (currentchar != nullpos[currentline]) 
				updateline(currentcollumns, currentchar, (*lines)[currentline]);
			break;
		}
		if (key != ARROWDOWN && key != ARROWUP)
			lasthighernullpos_updown = 0;
	}
	end:
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
	return 0; //everything worked
}

void clearline()
{
	char s[size.ws_col + 1];
	s[size.ws_col] = '\0';
	memset(s, ' ', size.ws_col - 1);
	
	resetx();
	printf("%s", s);
	resetx();
}

void updateline(int currentcollumns, int currentchar, unsigned char * line)
{
	int comparison, i;

	addx(1);
	comparison = size.ws_col - 2; //to turn into the list element
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

void backmovline(int *currentcollumns, int currentchar, unsigned char * line)
{	
	if ((*currentcollumns) && getx() == 2) { //in the case where we exit the currentcollumn
		int comparison, i;
		(*currentcollumns)--;
		clearline(size.ws_col);
		comparison = size.ws_col - 1; //since there's |
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
		updateline(*currentcollumns, currentchar, line);
	}
}

void firstpl(unsigned char * line, int b_clearline)
{
	size_t strsize;
	if (b_clearline)
		clearline(size.ws_col);
	//printf("%.*s", size.ws_col - 1, line); printf on stanix doesn't work with . for now
	strsize = strlen((char *) line);
	if (strsize == 0)
		return;
	if ((int) strsize > size.ws_col - 1)
		fwrite(line,size.ws_col - 1,1,stdout);
	else
		fwrite(line,strsize,1,stdout);
}

void gotocurrentchar(int *currentcollumns , int currentchar, int nullpos, unsigned char * s)
{
	int i, oldi;
	clearline(size.ws_col);
	if (currentchar + 1 < size.ws_col) {
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
		updateline(*currentcollumns, currentchar, s); //already showcursor
	showcursor();
}

void scrolldown(int currentline, int nlines, unsigned char ** lines)
{
	int i;
	clearscreen(); resety(); resetx();
	i = currentline - size.ws_row + 2;
	while (gety() < size.ws_row && i <= nlines) {
		firstpl(lines[i], 0);
		//printf("lines[%d] == %s \n", i, lines[i]);
		addy(1); resetx(); i++;
	}
	addy(-1);
}

void scrollup(int currentline, unsigned char ** lines)
{
	int i;
	i = currentline;
	clearscreen(); resety(); resetx();
	while (gety() < size.ws_row) {
		firstpl(lines[i], 0);
		addy(1); resetx(); i++;
	}
	addy(-(i - currentline));
}

void redrawscreen(int currentline, unsigned char ** lines)
{
	int i, j, oldy;
	clearscreen(); //since clearline current implementation is too slow
	for (i = currentline, j = gety(); j > 1; i--, j--)
		;
	oldy = gety();
	resetx(); resety();
	while (gety() < oldy) {
		firstpl(lines[i], 0);
		addy(1); i++;
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
