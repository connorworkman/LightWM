#include "winman.hpp"
extern "C" {
#include <X11/Xutil.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
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
        XSelectInput(display_handle, root_handle, SubstructureRedirectMask | SubstructureNotifyMask);
        XSync(display_handle, false);
        if (wm_detected_) {//change this reference style
            cerr << "There is already a window manager for display " << XDisplayString(display_handle);
            return;
        }
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
                OnButtonPress(event.xbutton);
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
            default:
                cerr << "Warning: Event ignored" << endl;
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
    XGrabButton(display_handle, Button1, Mod1Mask, w, false, ButtonPressMask | ButtonMotionMask, GrabModeAsync, GrabModeAsync, None, None);
    XGrabKey(display_handle, XKeysymToKeycode(display_handle, XK_Q), Mod1Mask, w, false, GrabModeAsync, GrabModeAsync);
    XGrabKey(display_handle, XKeysymToKeycode(display_handle, XK_Return), Mod1Mask, w, false, GrabModeAsync, GrabModeAsync);
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

void WindowManager::OnButtonPress(const XButtonEvent &event) {
    //CHECK(clients_handle.count(event.window));
    if (clients_handle.count(event.window) == 0) {
        cerr << "Client count for the event's window did not pass assertion (>0)" << endl;
    }
    const Window frame = clients_handle[event.window];
    drag_start_pos_ = Position<int>(event.x_root, event.y_root);
    Window returned_root;
    int x, y;
    unsigned width, height, border_width, depth;
    if (!XGetGeometry(display_handle, frame, &returned_root, &x, &y, &width, &height, &border_width, &depth)) {
        cerr << "XGetGeometry failed for some reason" << endl;
    }
    drag_start_frame_pos_ = Position<int>(x, y);
    drag_start_frame_size_ = Size<int>(width, height);
    XRaiseWindow(display_handle, frame);
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
    } 
    else if ((e.state & Mod1Mask) && (e.keycode == XKeysymToKeycode(display_handle, XK_Return)))
    {
        char xterm[6];
        strcpy(xterm, "xterm&");
        execute(xterm);

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

int WindowManager::execute(char *s)
{
    cout << "inside execute:" << endl;
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
