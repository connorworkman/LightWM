#ifndef BASICWIN_H_
#define BASICWIN_H_
#include <X11/Xlib.h>
#include <X11/Xos.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>


void getGC(Window win, GC* gc, XFontStruct* font_info);
void load_font(XFontStruct** font_info);
void place_graphics(Window win, GC gc, unsigned int window_width, unsigned int window_height);
void TooSmall(Window win, GC gc, XFontStruct* font_info);
void place_text(Window win, GC gc, XFontStruct* font_info, unsigned int win_width,unsigned int win_height);
#endif
