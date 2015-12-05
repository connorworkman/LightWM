#include "winman.hpp"
extern "C" {
#include <X11/Xutil.h>
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
}
#include <cstring>
#include <algorithm>
#include "eventnames.hpp"

#define MAX_PANES 10
#define RAISE 1
#define LOWER 0
#define MOVE 1
#define RESIZE 0
#define NONE 100
#define NOTDEFINED 0
#define BLACK 1
#define WHITE 0

using namespace std;
bool WindowManager::wm_detected_;
mutex WindowManager::wm_detected_mutex_;
static char const* menu_labels[] = {"Raise window", 
    "Lower window", "Move window", "Resize window", 
    "Circulate windows (down)", "Circulate windows (up)",
    "Focus keyboard", "XTERM!", "Exit LightWM",};

XFontStruct *font_info;
int screen_num;
Window focus_window;
Window inverted_pane = NONE;
Window panes[MAX_PANES];
GC gc, rgc;
unsigned int button;
Bool owner_events;
int pointer_mode, keyboard_mode;
Window assoc_win, confine_to;
int char_count, winindex;
Window menuwin;
const char* icon_name = "Constant icon name";

unique_ptr<WindowManager> WindowManager::Create(const string& display_str) {
	const char* display_c_str = display_str.empty() ? nullptr : display_str.c_str();
	Display* display = XOpenDisplay(display_c_str);
	if (display == nullptr) {
		cerr << "Failed to open X Display" << XDisplayName(display_c_str) << endl;
		return nullptr;
	}
	return unique_ptr<WindowManager>(new WindowManager(display));
}

WindowManager::WindowManager(Display* display) : 
    display_handle(display),
    root_handle(DefaultRootWindow(display_handle)), 
    WM_PROTOCOLS(XInternAtom(display_handle, "WM_PROTOCOLS", false)),
    WM_DELETE_WINDOW(XInternAtom(display_handle, "WM_DELETE_WINDOW", false)) {
}

WindowManager::~WindowManager() {
	XCloseDisplay(display_handle);
}

void WindowManager::Run() {
    {
        lock_guard<mutex> lock(wm_detected_mutex_);
        wm_detected_ = false;
        XSetErrorHandler(&WindowManager::OnWMDetected);
        XSelectInput(display_handle, root_handle, SubstructureNotifyMask);
        XSync(display_handle, false);
        if (wm_detected_) {//change this reference style
            cerr << "There is already a window manager for display " << XDisplayString(display_handle);
            return;
        }
    }


    int menu_width, menu_height, x = 0, y = 0, border_width = 4, pane_height, direction, ascent, descent;
    char const* font_name = "9x15";
    char const* strbuf;
    
    font_info = XLoadQueryFont(display_handle, font_name);
    if (font_info == NULL)
    {
        cerr << "Failed to load font." << endl;
        exit(-1);
    }
    strbuf = menu_labels[6];
    XCharStruct overall;
    
    char_count = strlen(strbuf);
    XTextExtents(font_info, strbuf, char_count, &direction, &ascent, &descent, &overall);
    menu_width = overall.width + 4;
    pane_height = overall.ascent + overall.descent + 4;
    menu_height = pane_height + MAX_PANES;

    screen_num = DefaultScreen(display_handle);
    x = DisplayWidth(display_handle,screen_num);
    y = 0;
    menuwin = XCreateSimpleWindow(display_handle, RootWindow(display_handle, screen_num), x, y, menu_width, menu_height, border_width, BlackPixel(display_handle,screen_num), WhitePixel(display_handle, screen_num));
    for (winindex = 0; winindex < MAX_PANES; winindex++)
    {
        panes[winindex] = XCreateSimpleWindow(display_handle, menuwin, 0, menu_height, menu_width, pane_height, border_width = 1, BlackPixel(display_handle, screen_num), WhitePixel(display_handle, screen_num));
        XSelectInput(display_handle, panes[winindex], ButtonPressMask | ButtonReleaseMask | ExposureMask);
    }
    XSelectInput(display_handle, RootWindow(display_handle, screen_num), SubstructureNotifyMask);
    XMapSubwindows(display_handle, menuwin);
    focus_window = RootWindow(display_handle, screen_num);
    //Create two graphics contexts for inverting pane colors
    gc = XCreateGC(display_handle, RootWindow(display_handle, screen_num), 0, NULL);
    XSetForeground(display_handle, gc, BlackPixel(display_handle, screen_num));
    rgc = XCreateGC(display_handle, RootWindow(display_handle, screen_num), 0, NULL);
    XSetForeground(display_handle, rgc, WhitePixel(display_handle, screen_num));
    //Frame(menuwin);
    XMapWindow(display_handle, menuwin);
    //force the child process to disinherit the TCP fd (for New Xterm exec call)
    if ((fcntl(ConnectionNumber(display_handle), F_SETFD, 1)) == -1)
    {
        cerr << "Error: LightWM child cannot disinherit TCP fd" << endl;
    }

    XSetErrorHandler(&WindowManager::OnXError);
    XGrabServer(display_handle);
    Window returned_root, returned_parent;
    Window* top_level_window;
    unsigned int num_top_level_windows;
    XQueryTree(display_handle, root_handle, &returned_root, &returned_parent, &top_level_window, &num_top_level_windows);
    if (XQueryTree == NULL) {
        cerr << "XQueryTree was NULL" << endl;
        exit(-1);
    }
    if (returned_root != root_handle) {
        cerr << "Returned root did not pass assertion" << endl;
        exit(-1);
    }

    for (unsigned int i = 0; i < num_top_level_windows; ++i) {
        Frame(top_level_window[i]);
    }

    XFree(top_level_window);
    XUngrabServer(display_handle);

    while(1) {
        XEvent event;
        cout << "Waiting for next event" << endl;
        XNextEvent(display_handle, &event);
        cout << "Event: \"" << ToString(event) << "\" occurred." << endl;
        switch (event.type) {
            case CreateNotify:
                OnCreateNotify(event.xcreatewindow);
                break;
            case DestroyNotify:
                OnDestroyNotify(event.xdestroywindow);
                break;
            case ReparentNotify:
                OnReparentNotify(event.xreparent);
                break;
            case MapNotify:
                OnMapNotify(event.xmap);
                break;
            case UnmapNotify:
                OnUnmapNotify(event.xunmap);
                break;
            case ConfigureNotify:
                OnConfigureNotify(event.xconfigure);
                break;
            case MapRequest:
                OnMapRequest(event.xmaprequest);
                break;
            case ConfigureRequest:
                OnConfigureRequest(event.xconfigurerequest);
                break;
            case ButtonPress:
                OnButtonPress(event);
                break;
            case MotionNotify:
                while (XCheckTypedWindowEvent(display_handle, event.xmotion.window, MotionNotify, &event)) {}
                OnMotionNotify(event.xmotion);
                break;
            case KeyPress:
                OnKeyPress(event.xkey);
                break;
            case KeyRelease:
                OnKeyRelease(event.xkey);
                break;
            case Expose:
                //if (isIcon(event.xexpose.window, event.xexpose.x, event.xexpose.y, &assoc_win, icon_name, False))
                //{
                    XDrawString(display_handle, event.xexpose.window, gc, 2, ascent+2, icon_name, strlen(icon_name));
                //}
                //else
                //{
                    if (inverted_pane == event.xexpose.window)
                    {
                        paint_pane(event.xexpose.window, panes, gc, rgc,BLACK);
                    }
                    else
                    {
                        paint_pane(event.xexpose.window, panes, gc, rgc, WHITE);
                    }
                //}
                break;
            default:
                cerr << "Warning: Event ignored" << endl;
                break;
        }
    }
}

void WindowManager::Frame(Window w) {
    const unsigned int BORDER_WIDTH = 1;
    const unsigned long BORDER_COLOR = 0xffffaa;
    const unsigned long BG_COLOR = 0xffffff;
    if (clients_handle.count(w)) {
        cerr << "Aborting." << endl;
        exit(-1);
    }

    XWindowAttributes x_window_attrs;
    if (!XGetWindowAttributes(display_handle, w, &x_window_attrs)) {
        cerr << "Aborting." << endl;
        exit(-1);
    }
    const Window frame = XCreateSimpleWindow(display_handle, root_handle, x_window_attrs.x, x_window_attrs.y, x_window_attrs.width, x_window_attrs.height, BORDER_WIDTH, BORDER_COLOR, BG_COLOR);

    XSelectInput(display_handle, frame, SubstructureRedirectMask | SubstructureNotifyMask);
    XAddToSaveSet(display_handle, w);
    XReparentWindow(display_handle, w, frame, 0, 0);
    
    XMapWindow(display_handle, frame);
    
    clients_handle[w] = frame;
    /* Hotkeys defined */
    XGrabButton(display_handle, Button1, Mod1Mask, w, false, ButtonPressMask | ButtonReleaseMask | ButtonMotionMask, GrabModeAsync, GrabModeAsync, None, None);
    XGrabButton(display_handle, Button3, Mod1Mask, w, false, ButtonPressMask | ButtonReleaseMask | ButtonMotionMask, GrabModeAsync, GrabModeAsync, None, None);
    XGrabKey(display_handle, XKeysymToKeycode(display_handle, XK_Q), Mod1Mask, w, false, GrabModeAsync, GrabModeAsync);
    XGrabKey(display_handle, XKeysymToKeycode(display_handle, XK_Tab), Mod1Mask, w, false, GrabModeAsync, GrabModeAsync);
}

void WindowManager::Unframe(Window w) {
    if (clients_handle.count(w) == 0) {
        cerr << "Client count did not pass assertion" << endl;
    }
    const Window frame = clients_handle[w];
    XUnmapWindow(display_handle, frame);
    XReparentWindow(display_handle, w, root_handle, 0, 0);
    XRemoveFromSaveSet(display_handle, w);
    XDestroyWindow(display_handle, frame);
    clients_handle.erase(w);
}

void WindowManager::OnCreateNotify(const XCreateWindowEvent &event) {}
void WindowManager::OnDestroyNotify(const XDestroyWindowEvent &event) {}
void WindowManager::OnMapNotify(const XMapEvent &event) {}
void WindowManager::OnReparentNotify(const XReparentEvent &event) {}
void WindowManager::OnUnmapNotify(const XUnmapEvent &event) {
    if (clients_handle.count(event.window) == 0) {
        cerr << "Ignore UnmapNotify for window that isn't a client." << endl;
        return;
    }
    if (event.event == root_handle) {
        return;
    }
    Unframe(event.window);
}

void WindowManager::OnConfigureNotify(const XConfigureEvent &event) {}
void WindowManager::OnMapRequest(const XMapRequestEvent &event) {
    Frame(event.window);
    XMapWindow(display_handle, event.window);
}

void WindowManager::OnConfigureRequest(const XConfigureRequestEvent &event) {
    XWindowChanges changes;
    changes.x = event.x;
    changes.y = event.y;
    changes.width = event.width;
    changes.height = event.height;
    changes.border_width = event.border_width;
    changes.sibling = event.above;
    changes.stack_mode = event.detail;
    if (clients_handle.count(event.window)) {
        const Window frame = clients_handle[event.window];
        XConfigureWindow(display_handle, frame, event.value_mask, &changes);
        cerr << "Frame resized to " << Size<int>(event.width, event.height);
    }
    XConfigureWindow(display_handle, event.window, event.value_mask, &changes);
    cerr << "Window resized to " << Size<int>(event.width, event.height);
}

void WindowManager::OnButtonPress(XEvent &event) {
    if (clients_handle.count(event.xbutton.window) == 0) {
        cerr << "Client count for the event's window did not pass assertion (>0)" << endl;
    }
    const Window frame = clients_handle[event.xbutton.window];
    drag_start_pos_ = Position<int>(event.xbutton.x_root, event.xbutton.y_root);
    Window returned_root;
    int x, y;
    unsigned width, height, border_width, depth;
    if (!XGetGeometry(display_handle, frame, &returned_root, &x, &y, &width, &height, &border_width, &depth)) {
        cerr << "XGetGeometry failed for some reason" << endl;
    }
    drag_start_frame_pos_ = Position<int>(x, y);
    drag_start_frame_size_ = Size<int>(width, height);
    XRaiseWindow(display_handle, frame);
    paint_pane(event.xbutton.window, panes, gc, rgc, BLACK);
    button = event.xbutton.button;
    inverted_pane = event.xbutton.window;
    while (1)
    {
        while(XCheckTypedEvent(display_handle, ButtonPress, &event));
        XMaskEvent(display_handle, ButtonReleaseMask, &event);
        if (event.xbutton.button == button)
        {
            break;
        }
    }
    owner_events = true;
    pointer_mode = GrabModeAsync;
    keyboard_mode = GrabModeAsync;
    confine_to = None;
    XGrabPointer(display_handle, menuwin, owner_events, ButtonPressMask | ButtonReleaseMask, pointer_mode, keyboard_mode, confine_to, None, CurrentTime);
    if (inverted_pane == event.xbutton.window)
    {
        for (winindex = 0; inverted_pane != panes[winindex]; winindex++)
        {
            ;
        }
        char* xterm;
        strcpy(xterm, "xterm&");
        switch(winindex) {
            case 0:
                raise_lower(menuwin, RAISE);
                break;
            case 1:
                raise_lower(menuwin, LOWER);
                break;
            case 2:
                //move_resize(menuwin, MOVE);
                break;
            case 3:
                //move_resize(menuwin, RESIZE);
                break;
            case 4:
                circup(menuwin);
                break;
            case 5:
                circdown(menuwin);
                break;
            case 6:
                //iconify(menuwin);
                break;
            case 7:
                //focus_window = focus(menuwin);
                break;
            case 8:
                execute(xterm);
                break;
            case 9:
                //exit
                XSetInputFocus(display_handle, RootWindow(display_handle, screen_num), RevertToPointerRoot, CurrentTime);
                XClearWindow(display_handle, RootWindow(display_handle, screen_num));
                XFlush(display_handle);
                XCloseDisplay(display_handle);
                exit(1);
            default:
                cerr << "Something bad happened." << endl;
                break;
        }//end switch
    }//end if inverted_pane
    paint_pane(event.xbutton.window, panes, gc, rgc, WHITE);
    inverted_pane = NONE;
    //draw_focus_frame();
    XUngrabPointer(display_handle, CurrentTime);
    XFlush(display_handle);
}

void WindowManager::OnButtonRelease(const XButtonEvent &event) {}
void WindowManager::OnMotionNotify(const XMotionEvent &event) {
    //CHECK(clients_handle.count(event.window));
    if (clients_handle.count(event.window) == 0) {
        cerr << "Client count for the event's window did not pass assertion (>0)" << endl;
    }
    const Window frame = clients_handle[event.window];
    const Position<int> drag_pos(event.x_root, event.y_root);
    const Vector2D<int> delta = drag_pos - drag_start_pos_;
    if (event.state & Button1Mask) {
        const Position<int> dest_frame_pos = drag_start_frame_pos_ + delta;
        XMoveWindow(display_handle, frame, dest_frame_pos.x, dest_frame_pos.y);
    } else if (event.state & Button3Mask) {
        const Vector2D<int> size_delta(max(delta.x, -drag_start_frame_size_.width), max(delta.y, -drag_start_frame_size_.height));
        const Size<int> dest_frame_size = drag_start_frame_size_ + size_delta;
        XResizeWindow(display_handle, frame, dest_frame_size.width, dest_frame_size.height);
        XResizeWindow(display_handle, event.window, dest_frame_size.width, dest_frame_size.height);
    }
}

void WindowManager::OnKeyPress(const XKeyEvent &e) {
    if ((e.state & Mod1Mask) && (e.keycode == XKeysymToKeycode(display_handle, XK_Q))) {
        Atom* supported_protocols;
        int num_supported_protocols;
        if (XGetWMProtocols(display_handle, e.window, &supported_protocols, &num_supported_protocols) && (find(supported_protocols, supported_protocols + num_supported_protocols, WM_DELETE_WINDOW) != supported_protocols + num_supported_protocols)) {
            cerr << "Deleting window" << e.window << endl;
            XEvent msg;
            memset(&msg, 0, sizeof(msg));
            msg.xclient.type = ClientMessage;
            msg.xclient.message_type = WM_PROTOCOLS;
            msg.xclient.window = e.window;
            msg.xclient.format = 32;
            msg.xclient.data.l[0] = WM_DELETE_WINDOW;
            if (XSendEvent(display_handle, e.window, false, 0, &msg))
            {
                cerr << "Failed to send event to X (delete message)" << endl;
            }
        } else {
            cerr << "Killing window " << e.window << endl;
            XKillClient(display_handle, e.window);
        }
    /* example key press handle
    } 
    else if ((e.state & Mod1Mask) && (e.keycode == XKeysymToKeycode(display_handle, XK_SOMEKEY)))
    {
        auto i = clients_handle.find(e.window);
        if (i == clients_handle.end())
        {
            cerr << "Assertion failed, window not found." << endl;
        }
        ++i;//go to next window
        if (i == clients_handle.end()) {
            i = clients_handle.begin();
        }
        XRaiseWindow(display_handle, i->second);
        XSetInputFocus(display_handle, i->first, RevertToPointerRoot, CurrentTime);
    
    */
    } else if ((e.state & Mod1Mask) && (e.keycode == XKeysymToKeycode(display_handle, XK_Tab))) {
        auto i = clients_handle.find(e.window);
        //CHECK(i != clients_handle.end());
        if (i == clients_handle.end())
        {
            cerr << "Assertion failed, window not found" << endl;
        }
        ++i;
        if (i == clients_handle.end()) {
            i = clients_handle.begin();
        }
        XRaiseWindow(display_handle, i->second);
        XSetInputFocus(display_handle, i->first, RevertToPointerRoot, CurrentTime);
    }
}

void WindowManager::OnKeyRelease(const XKeyEvent &e) {}

int WindowManager::OnXError(Display* display, XErrorEvent *e) {
    const int MAX_ERROR_TEXT_LENGTH = 1024;
    char error_text[MAX_ERROR_TEXT_LENGTH];
    XGetErrorText(display, e->error_code, error_text, sizeof(error_text));
    cerr << "Received X error: " << endl;
    cerr << "   Request code: " << int(e->request_code) << endl;
    cerr << "   " << XRequestCodeToString(e->request_code) << endl;
    cerr << "   " << error_text << endl;
    cerr << "   ResourceID: " << e->resourceid << endl;
    return 0;
}

int WindowManager::OnWMDetected(Display* display, XErrorEvent *e) {
    if (static_cast<int>(e->error_code) != BadAccess)
    {
        cerr << "OnWMDetected: unable to check error code" << endl;
    }
    wm_detected_ = true;
    return 0;
}

void WindowManager::paint_pane(Window window, Window panes[], GC ngc, GC rgc, int mode)
{
    int win;
    int x = 2;
    int y;
    GC gc;
    if (mode == BLACK)
    {
        XSetWindowBackground(display_handle, window, BlackPixel(display_handle, screen_num));
        gc = rgc;
    }
    else
    {
        XSetWindowBackground(display_handle, window, WhitePixel(display_handle, screen_num));
        gc = ngc;
    }
    XClearWindow(display_handle, window);
    for (win = 0; window != panes[win]; win++)
    {
        ;
    }
    y = font_info->max_bounds.ascent;
    XDrawString(display_handle, window, gc, x, y, menu_labels[win], strlen(menu_labels[win]));
}

void WindowManager::circup(Window menuwin)
{
    XCirculateSubwindowsUp(display_handle, RootWindow(display_handle, screen_num));
    XRaiseWindow(display_handle, menuwin);
}

void WindowManager::circdown(Window menuwin)
{
    XCirculateSubwindowsDown(display_handle, RootWindow(display_handle, screen_num));
    XRaiseWindow(display_handle, menuwin);
}

void WindowManager::raise_lower(Window menuwin, Bool raise_or_lower)
{
    XEvent report;
    int root_x,root_y;
    Window child, root;
    int win_x, win_y;
    unsigned int mask;
    unsigned int button;

    XMaskEvent(display_handle, ButtonPressMask, &report);
    button = report.xbutton.button;
    XQueryPointer(display_handle, RootWindow(display_handle, screen_num), &root, &child, &root_x, &root_y, &win_x, &win_y, &mask);
    if (child != 0)
    {
        if (raise_or_lower == RAISE)
        {
            XRaiseWindow(display_handle, child);
        }
        else
        {
            XLowerWindow(display_handle, child);
        }
        XRaiseWindow(display_handle, menuwin);
    }
    while(1)
    {
        XMaskEvent(display_handle, ButtonReleaseMask, &report);
        if (report.xbutton.button == button)
        {
            break;
        }
    }
    while(XCheckMaskEvent(display_handle, ButtonReleaseMask | ButtonPressMask, &report)) {
        ;
    }
    
}

int WindowManager::execute(char *s)
{
    int status, pid, w;
    void (*istat)(int), (*qstat)(int);
    if ((pid = vfork()) == 0) {
        signal(SIGINT, SIG_DFL);
        signal(SIGQUIT, SIG_DFL);
        signal(SIGHUP, SIG_DFL);
        execl("/bin/sh", "sh", "-c", s, 0);
        _exit(127);
    }
    istat = signal(SIGINT, SIG_IGN);
    qstat = signal(SIGQUIT, SIG_IGN);
    while ((w = wait(&status)) != pid && w != -1)
    {
        ;
    }
    if (w == -1)
    {
        status = -1;
    }
    signal(SIGINT, istat);
    signal(SIGQUIT, qstat);
    return(status);
}
