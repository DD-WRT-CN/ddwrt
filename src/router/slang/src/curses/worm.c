/*

	 @@@        @@@    @@@@@@@@@@     @@@@@@@@@@@    @@@@@@@@@@@@
	 @@@        @@@   @@@@@@@@@@@@    @@@@@@@@@@@@   @@@@@@@@@@@@@
	 @@@        @@@  @@@@      @@@@   @@@@           @@@@ @@@  @@@@
	 @@@   @@   @@@  @@@        @@@   @@@            @@@  @@@   @@@
	 @@@  @@@@  @@@  @@@        @@@   @@@            @@@  @@@   @@@
	 @@@@ @@@@ @@@@  @@@        @@@   @@@            @@@  @@@   @@@
	  @@@@@@@@@@@@   @@@@      @@@@   @@@            @@@  @@@   @@@
	   @@@@  @@@@     @@@@@@@@@@@@    @@@            @@@  @@@   @@@
	    @@    @@       @@@@@@@@@@     @@@            @@@  @@@   @@@

				 Eric P. Scott
			  Caltech High Energy Physics
				 October, 1980

		Hacks to turn this into a test frame for cursor movement:
			Eric S. Raymond <esr@snark.thyrsus.com>
				January, 1995

		July 1995 (esr): worms is now in living color! :-)

Options:
	-f			fill screen with copies of 'WORM' at start.
	-l <n>			set worm length
	-n <n>			set number of worms
	-t			make worms leave droppings
	-T <start> <end>	set trace interval
	-S			set single-stepping during trace interval
	-N			suppress cursor-movement optimization

  This program makes a good torture-test for the ncurses cursor-optimization
  code.  You can use -T to set the worm move interval over which movement
  traces will be dumped.  The program stops and waits for one character of
  input at the beginning and end of the interval.
*/

#include <curses.h>
#include <stdlib.h>
#include <signal.h>

#define cursor(col,row) move(row,col)

short *ref[128];
static chtype flavor[]={
    'O' , '*', '#', '$', '%', '0', '@',
};
#define MAXWORMS	(sizeof(flavor)/sizeof(chtype))
static short xinc[]={
     1,  1,  1,  0, -1, -1, -1,  0
}, yinc[]={
    -1,  0,  1,  1,  1,  0, -1, -1
};
static struct worm {
    int orientation, head;
    short *xpos, *ypos;
} worm[40];

static char *field;
static int length=16, number=3;
static chtype trail=' ';

#ifdef TRACE
int generation, trace_start, trace_end, singlestep;
#endif /* TRACE */
static struct options {
    int nopts;
    int opts[3];
} normal[8]={
    { 3, { 7, 0, 1 } },
    { 3, { 0, 1, 2 } },
    { 3, { 1, 2, 3 } },
    { 3, { 2, 3, 4 } },
    { 3, { 3, 4, 5 } },
    { 3, { 4, 5, 6 } },
    { 3, { 5, 6, 7 } },
    { 3, { 6, 7, 0 } }
}, upper[8]={
    { 1, { 1, 0, 0 } },
    { 2, { 1, 2, 0 } },
    { 0, { 0, 0, 0 } },
    { 0, { 0, 0, 0 } },
    { 0, { 0, 0, 0 } },
    { 2, { 4, 5, 0 } },
    { 1, { 5, 0, 0 } },
    { 2, { 1, 5, 0 } }
}, left[8]={
    { 0, { 0, 0, 0 } },
    { 0, { 0, 0, 0 } },
    { 0, { 0, 0, 0 } },
    { 2, { 2, 3, 0 } },
    { 1, { 3, 0, 0 } },
    { 2, { 3, 7, 0 } },
    { 1, { 7, 0, 0 } },
    { 2, { 7, 0, 0 } }
}, right[8]={
    { 1, { 7, 0, 0 } },
    { 2, { 3, 7, 0 } },
    { 1, { 3, 0, 0 } },
    { 2, { 3, 4, 0 } },
    { 0, { 0, 0, 0 } },
    { 0, { 0, 0, 0 } },
    { 0, { 0, 0, 0 } },
    { 2, { 6, 7, 0 } }
}, lower[8]={
    { 0, { 0, 0, 0 } },
    { 2, { 0, 1, 0 } },
    { 1, { 1, 0, 0 } },
    { 2, { 1, 5, 0 } },
    { 1, { 5, 0, 0 } },
    { 2, { 5, 6, 0 } },
    { 0, { 0, 0, 0 } },
    { 0, { 0, 0, 0 } }
}, upleft[8]={
    { 0, { 0, 0, 0 } },
    { 0, { 0, 0, 0 } },
    { 0, { 0, 0, 0 } },
    { 0, { 0, 0, 0 } },
    { 0, { 0, 0, 0 } },
    { 1, { 3, 0, 0 } },
    { 2, { 1, 3, 0 } },
    { 1, { 1, 0, 0 } }
}, upright[8]={
    { 2, { 3, 5, 0 } },
    { 1, { 3, 0, 0 } },
    { 0, { 0, 0, 0 } },
    { 0, { 0, 0, 0 } },
    { 0, { 0, 0, 0 } },
    { 0, { 0, 0, 0 } },
    { 0, { 0, 0, 0 } },
    { 1, { 5, 0, 0 } }
}, lowleft[8]={
    { 3, { 7, 0, 1 } },
    { 0, { 0, 0, 0 } },
    { 0, { 0, 0, 0 } },
    { 1, { 1, 0, 0 } },
    { 2, { 1, 7, 0 } },
    { 1, { 7, 0, 0 } },
    { 0, { 0, 0, 0 } },
    { 0, { 0, 0, 0 } }
}, lowright[8]={
    { 0, { 0, 0, 0 } },
    { 1, { 7, 0, 0 } },
    { 2, { 5, 7, 0 } },
    { 1, { 5, 0, 0 } },
    { 0, { 0, 0, 0 } },
    { 0, { 0, 0, 0 } },
    { 0, { 0, 0, 0 } },
    { 0, { 0, 0, 0 } }
};

void onsig(int sig);
float ranf(void);

int
main(int argc, char *argv[])
{
int x, y;
int n;
struct worm *w;
struct options *op;
int h;
short *ip;
int last, bottom;

    for (x=1;x<argc;x++) {
		register char *p;
		p=argv[x];
		if (*p=='-') p++;
		switch (*p) {
		case 'f':
		    field="WORM";
		    break;
		case 'l':
		    if (++x==argc) goto usage;
		    if ((length=atoi(argv[x]))<2||length>1024) {
				fprintf(stderr,"%s: Invalid length\n",*argv);
				exit(1);
		    }
		    break;
		case 'n':
		    if (++x==argc) goto usage;
		    if ((number=atoi(argv[x]))<1||number>40) {
				fprintf(stderr,"%s: Invalid number of worms\n",*argv);
				exit(1);
		    }
		    break;
		case 't':
		    trail='.';
		    break;
#ifdef TRACE
		case 'S':
		    singlestep = TRUE;
		    break;
		case 'T':
		    trace_start = atoi(argv[++x]);
		    trace_end   = atoi(argv[++x]);
		    break;
		case 'N':
		    no_optimize = TRUE;		/* declared by ncurses */
		    break;
#endif /* TRACE */
		default:
		usage:
		    fprintf(stderr, "usage: %s [-field] [-length #] [-number #] [-trail]\n",*argv);
		    exit(1);
		    break;
		}
    }

    signal(SIGINT, onsig);
    initscr();
#ifdef TRACE
    noecho();
    cbreak();
#endif /* TRACE */
    nonl();
    bottom = LINES-1;
    last = COLS-1;

#ifdef A_COLOR
    if (has_colors())
    {
	start_color();

	init_pair(COLOR_GREEN, COLOR_GREEN, COLOR_BLACK);
	init_pair(COLOR_RED, COLOR_RED, COLOR_BLACK);
	init_pair(COLOR_CYAN, COLOR_CYAN, COLOR_BLACK);
	init_pair(COLOR_WHITE, COLOR_WHITE, COLOR_BLACK);
	init_pair(COLOR_MAGENTA, COLOR_MAGENTA, COLOR_BLACK);
	init_pair(COLOR_BLUE, COLOR_BLUE, COLOR_BLACK);
	init_pair(COLOR_YELLOW, COLOR_YELLOW, COLOR_BLACK);

	flavor[0] |= COLOR_PAIR(COLOR_GREEN) | A_BOLD;
	flavor[1] |= COLOR_PAIR(COLOR_RED) | A_BOLD;
	flavor[2] |= COLOR_PAIR(COLOR_CYAN) | A_BOLD;
	flavor[3] |= COLOR_PAIR(COLOR_WHITE) | A_BOLD;
	flavor[4] |= COLOR_PAIR(COLOR_MAGENTA) | A_BOLD;
	flavor[5] |= COLOR_PAIR(COLOR_BLUE) | A_BOLD;
	flavor[6] |= COLOR_PAIR(COLOR_YELLOW) | A_BOLD;
    }
#endif /* A_COLOR */

    ip=(short *)malloc(LINES*COLS*sizeof (short));

    for (n=0;n<LINES;) {
		ref[n++]=ip; ip+=COLS;
    }
    for (ip=ref[0],n=LINES*COLS;--n>=0;) *ip++=0;

#ifdef BADCORNER
    /* if addressing the lower right corner doesn't work in your curses */
    ref[bottom][last]=1;
#endif /* BADCORNER */

    for (n=number, w= &worm[0];--n>=0;w++) {
		w->orientation=w->head=0;
		if (!(ip=(short *)malloc((length+1)*sizeof (short)))) {
		    fprintf(stderr,"%s: out of memory\n",*argv);
		    exit(1);
		}
		w->xpos=ip;
		for (x=length;--x>=0;) *ip++ = -1;
		if (!(ip=(short *)malloc((length+1)*sizeof (short)))) {
		    fprintf(stderr,"%s: out of memory\n",*argv);
		    exit(1);
		}
		w->ypos=ip;
		for (y=length;--y>=0;) *ip++ = -1;
    }
    if (field) {
		register char *p;
		p=field;
		for (y=bottom;--y>=0;) {
		    for (x=COLS;--x>=0;) {
				addch((chtype)(*p++));
				if (!*p) p=field;
		    }
            addch('\n');
        }
    }
   refresh();
   napms(100);

    for (;;) {
#ifdef TRACE
		if (trace_start || trace_end) {
		    if (generation == trace_start) {
			trace(TRACE_CALLS);
			getch();
		    } else if (generation == trace_end) {
			trace(0);
			getch();
		    }

		    if (singlestep && generation > trace_start && generation < trace_end)
			getch();

		    generation++;
		}
#endif /* TRACE */

		for (n=0,w= &worm[0];n<number;n++,w++) {
		    if ((x=w->xpos[h=w->head])<0) {
				cursor(x=w->xpos[h]=0,y=w->ypos[h]=bottom);
				addch(flavor[n % MAXWORMS]);
				ref[y][x]++;
		    }
		    else y=w->ypos[h];
		    if (++h==length) h=0;
		    if (w->xpos[w->head=h]>=0) {
				register int x1, y1;
				x1=w->xpos[h]; y1=w->ypos[h];
				if (--ref[y1][x1]==0) {
				    cursor(x1,y1); addch(trail);
				}
		    }
            op= &(x==0 ? (y==0 ? upleft : (y==bottom ? lowleft : left)) :
                (x==last ? (y==0 ? upright : (y==bottom ? lowright : right)) :
			(y==0 ? upper : (y==bottom ? lower : normal))))[w->orientation];
		    switch (op->nopts) {
		    case 0:
				refresh();
				endwin();
				exit(0);
		    case 1:
				w->orientation=op->opts[0];
				break;
		    default:
				w->orientation=op->opts[(int)(ranf()*(float)op->nopts)];
		    }
		    cursor(x+=xinc[w->orientation], y+=yinc[w->orientation]);

		    if (y < 0 ) y = 0;
		    addch(flavor[n % MAXWORMS]);
		    ref[w->ypos[h]=y][w->xpos[h]=x]++;
		}
       napms(100);
		refresh();
    }
}

void
onsig(int sig)
{
	standend();
	refresh();
	endwin();
	exit(sig);
}

float
ranf(void)
{
float rv;
long r = rand();

    r &= 077777;
    rv =((float)r/32767.);
    return rv;
}
