#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xos.h>
#include <X11/Xatom.h>
#include <stdlib.h>
#include <stdio.h>
#include "bitmaps/icon_bitmap"
#include "basicwin.h"

#define BITMAPDEPTH 1
#define TOO_SMALL 0
#define BIG_ENOUGH 1

//global variable declaration
Display* display;
int screen_num;

static char* progname;

void main(argc, argv)
int argc;
char** argv;
{
	Window win;
	unsigned int width, height;
	int x, y;
	unsigned int border_width = 4;
	unsigned int display_width, display_height;
	unsigned int icon_width, icon_height;
	char* window_name = "Basic Window Program";
	char* icon_name = "basicwin";
	Pixmap icon_pixmap;
	XSizeHints* size_hints;
	XIconSize* size_list;
	XWMHints* wm_hints;
	XClassHint* class_hints;
	XTextProperty windowName, iconName;
	int count;
	XEvent report;
	GC gc;
	XFontStruct* font_info;
	char* display_name = NULL;
	int window_size = 0;

	progname = argv[0];

	if (!(size_hints = XAllocSizeHints())) {
		fprintf(stderr, "%s: failure to allocate memory\n", progname);
		exit(0);
	}

	if (!(wm_hints = XAllocWMHints())) {
		fprintf(stderr, "%s: failure to allocate memory\n", progname);
		exit(0);
	}
	
	if (!(class_hints = XAllocClassHint())) {
		fprintf(stderr, "%s: failure to allocate memory\n", progname);
		exit(0);
	}

	if ((display=XOpenDisplay(display_name)) == NULL) {
		(void) fprintf(stderr,"%s: Cannot connect to X server display named %s\n",progname,display_name);
		exit(-1);
	}

	screen_num = DefaultScreen(display);
	display_width = DisplayWidth(display, screen_num);
	display_height = DisplayHeight(display, screen_num);
	x = y = 0;
	width = display_width/3;
	height = display_height/4;

	win = XCreateSimpleWindow(display, RootWindow(display,screen_num), x, y, width, height, border_width, BlackPixel(display, screen_num), WhitePixel(display, screen_num));
	if (XGetIconSizes(display, RootWindow(display,screen_num), &size_list, &count) == 0) {
		(void) fprintf(stderr, "%s: Window manager didn't set icon sizes - using default.\n", progname);
	} else {
		;
		//TODO search through size_list here to find an acceptable icon size and then create pixmap of that size.
		//This means we would have to hold data for different icon sizes.
	}
	
	icon_pixmap = XCreateBitmapFromData(display, win, icon_bitmap_bits, icon_bitmap_width, icon_bitmap_height);

	size_hints->flags = PPosition | PSize | PMinSize;
	size_hints->min_width = 300;
	size_hints->min_height = 200;

	if (XStringListToTextProperty(&window_name, 1, &windowName) == 0) {
		(void) fprintf(stderr, "%s: structure allocation for windowName failed.\n", progname);
		exit(-1);
	}
	if (XStringListToTextProperty(&icon_name, 1, &iconName) == 0) {
		(void) fprintf(stderr, "%s: structure allocation for iconName failed.\n", progname);
		exit(-1);
	}
	wm_hints->initial_state = NormalState;
	wm_hints->input = True;
	wm_hints->icon_pixmap = icon_pixmap;
	wm_hints->flags = StateHint | IconPixmapHint | InputHint;

	class_hints->res_name = progname;
	class_hints->res_class = "Basicwin";

	XSetWMProperties(display,win,&windowName,&iconName,argv,argc,size_hints,wm_hints,class_hints);
	
	XSelectInput(display, win, (ExposureMask | KeyPressMask | ButtonPressMask | StructureNotifyMask));
	load_font(&font_info);
	getGC(win,&gc,font_info);

	XMapWindow(display,win);
	while(1) {//main event loop
		XNextEvent(display,&report);
		switch (report.type) {
			case Expose:
				if (report.xexpose.count != 0) {
					break;
				}
				if (window_size == TOO_SMALL) {
					TooSmall(win,gc,font_info);
				} else {
					place_text(win,gc,font_info,width,height);
					place_graphics(win,gc,width,height);
				}
				break;
			case ConfigureNotify:
				width = report.xconfigure.width;
				height = report.xconfigure.height;
				if ((width < size_hints->min_width) || (height < size_hints->min_height)) {
					window_size == TOO_SMALL;
				} else {
					window_size == BIG_ENOUGH;
				}
				break;
			case ButtonPress:
				//trickle down into KeyPress case.
			case KeyPress:
				XUnloadFont(display,font_info->fid);
				XFreeGC(display,gc);
				XCloseDisplay(display);
				exit(1);
			default:
				break;
		}
	}
}

void getGC(win,gc,font_info)
Window win;
GC* gc;
XFontStruct* font_info;
{
	unsigned long valuemask = 0;
	XGCValues values;
	unsigned int line_width = 6;
	int line_style = LineOnOffDash;
	int cap_style = CapRound;
	int join_style = JoinRound;
	int dash_offset = 0;
	static char dash_list[] = {12,24};
	int list_length = 2;
	*gc = XCreateGC(display,win,valuemask,&values);
	XSetFont(display,*gc,font_info->fid);
	XSetForeground(display, *gc, BlackPixel(display,screen_num));
	XSetLineAttributes(display,*gc,line_width,line_style,cap_style,join_style);
	XSetDashes(display,*gc,dash_offset,dash_list,list_length);
}

void load_font(font_info)
XFontStruct** font_info;
{
	char* fontname = "9x15";
	if ((*font_info = XLoadQueryFont(display,fontname)) == NULL) {
		(void) fprintf(stderr,"%s: Cannot open font 9x15\n",progname);
	}

}

void place_text(win,gc,font_info,win_width,win_height)
Window win;
GC gc;
XFontStruct* font_info;
unsigned int win_width, win_height;
{
	char* string1 = "Hi, dude(s)! I'm a window, who are you?";
	char* string2 = "To terminate program, presss any key";
	char* string3 = "or button while in this window.";
	char* string4 = "Screen Dimensions:";
	int len1,len2,len3,len4;
	int width1,width2,width3;
	char cd_height[50],cd_width[50],cd_depth[50];
	int font_height;
	int initial_y_offset, x_offset;
	len1 = strlen(string1);
	len2 = strlen(string2);
	len3 = strlen(string3);
	width1 = XTextWidth(font_info,string1,len1);
	width2 = XTextWidth(font_info,string2,len2);
	width3 = XTextWidth(font_info,string3,len3);
	font_height = font_info->ascent + font_info->descent;

	XDrawString(display,win,gc,(win_width-width1)/2,font_height,string1,len1);	
	XDrawString(display,win,gc,(win_width-width2)/2,(int)(win_height-(2*font_height)),string2,len2);
	XDrawString(display,win,gc,(win_width-width3)/2,(int)(win_height-font_height),string3,len3);
	
	(void) sprintf(cd_height, " Height - %d pixels", DisplayHeight(display,screen_num));
	(void) sprintf(cd_width, " Width - %d pixels", DisplayWidth(display,screen_num));
	(void) sprintf(cd_depth, " Depth - %d plane(s)", DefaultDepth(display,screen_num));

	len4 = strlen(string4);
	len1 = strlen(cd_height);
	len2 = strlen(cd_width);
	len3 = strlen(cd_depth);

	initial_y_offset = (int) win_width/4;
	XDrawString(display,win,gc,x_offset,(int)initial_y_offset,string4,len4);
	XDrawString(display,win,gc,x_offset,(int)initial_y_offset+font_height,cd_height,len1);
	XDrawString(display,win,gc,x_offset,(int)initial_y_offset+font_height*2,cd_width,len2);
	XDrawString(display,win,gc,x_offset,(int)initial_y_offset+font_height*3,cd_depth,len3);
}

void place_graphics(win,gc,window_width,window_height)
Window win;
GC gc;
unsigned int window_width, window_height;
{
	int x,y;
	int width,height;
	height = window_height/2;
	width = 3*window_width/4;
	x = window_width/2 - width/2;
	y = window_height/2 - height/2;
	XDrawRectangle(display,win,gc,x,y,width,height);
}

void TooSmall(win,gc,font_info)
Window win;
GC gc;
XFontStruct* font_info;
{
	char* string1 = "Too Small";
	int y_offset,x_offset;
	y_offset = font_info->ascent+2;
	x_offset = 2;
	XDrawString(display,win,gc,x_offset,y_offset,string1,strlen(string1));
}
