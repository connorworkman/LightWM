#include "window_manager.hpp"
extern "C" {
#include <X11/Xutil.h>
}
#include <cstring>
#include <algorithm>
#include <util.hpp>

using namespace std;
bool WindowManager::wm_detected_;
mutex WindowManager::wm_dected_mutex_;

unique_ptr<WindowManager> WindowManager::Create(const string& display_str) {
	const char* display_c_str = display_str.empty() ? nullptr : display_str.c_str();
	Display* display = XOpenDisplay(display_c_str);
	if (display == nullptr) {
		cerr << "Failed to open X Display" << XDisplayName(display_c_str) << endline;
		return nullptr;
	}
	return unique_ptr<WindowManager>(new WindowManager(display));
}

WindowManager::WindowManager(Display* display) : 
    display_(CHECK_NOTNULL(display)), 
    root_(DefaultRootWindow(display_)), 
    WM_PROTOCOLS(XInternAtom(display_, "WM_PROTOCOLS", false)),
    WM_DELETE_WINDOW(XInternAtom(display_, "WM_DELETE_WINDOW", false)) {
}

WindowManager::~WindowManager() {
	XCloseDisplay(display_);
}

void WindowManager::Run() {
    {
        lock_guard<mutex> lock(wm_detected_mutex_);
        wm_detected_ = false;
        XSetErrorHandler(&WindowManager::OnWMDetected);
        XSelectInput(display_,root_,SubstructureRedirectMask | SubsstructureNotifyMask);
        XSync(display_, false);
        if (wm_detected_) {//change this reference style
            cerr << "There is already a window manager for display " << XDisplayString(display_);
            return;
        }
    }

    XSetErrorHandler(&WindowManager::OnXError);
    XGrabServer(display_);
    Window returned_root, returned_parent;
    Window* top_level_window;
    unsigned int num_top_level_windows;
    CHECK(XQueryTree(display_, root_, &returned_root, &returned_parent,
                &top_level_windows, &num_top_level_windows));
    CHECK_EQ(returned_root, root_);

    for (unsigned int i = 0; i < num_top_level_windows; ++i) {
        Frame(top_level_windows[i]);
    }

    XFree(top_level_windows);
    XUngrabServer(display_);

    for(;;) {
        XEvent e;
        XNextEvent(display_, &e);
        cout << "Event: \"" << ToString(e) << "\" occurred." << endl;
        switch (e.type) {
            case CreateNotify:
                OnCreateNotify(e.xcreatewindow);
                break;
            case DestroyNotify:
                OnDestroyNotify(e.xdestroywindow);
                break;
            case ReparentNotify:
                OnReparentNotify(e.xreparent);
                break;
            case MapNotify:
                OnReparentNotify(e.xmap);
                break;
            case UnmapNotify:
                OnMapNotify(e.xmap);
                break;
            case ConfigureNotify:
                OnConfigureNotify(e.xunmap);
                break;
            case MapRequest:
                OnMapRequest(e.xmaprequest);
                break;
            case ConfigureRequest:
                OnConfigureRequest(e.xconfigurerequest);
                break;
            case ButtonPress:
                OnButtonPress(e.xbutton);
                break;
            case MotionNotify:
                while (XCheckTypeWindowEvent(display_, e.xmotion.window, MotionNotify, &e)) {}
                OnMotionNotify(e.xmotion);
                break;
            case KeyPress:
                OnKeyPress(e.xkey);
                break;
            case KeyRelease:
                OnKeyRelease(e.xkey);
                break;
            default:
                cerr << "Warning: Event ignored" << endl;
        }
    }
}

void WindowManager::Frame(Window w) {
    const unsigned int BORDER_WIDTH = 3;
    const unsigned long BORDER_COLOR = 0xff0000;
    const unsigned long BG_COLOR = 0x0000ff;
    CHECK(!clients_.count(w));
    XWindowAttributes x_window_attrs;
    CHECK(XGetWindowAttributes(display_, w, &window_attrs));
    const Window frame = XCreateSimpleWindow(display_, root_, x_window_attrs.x, x_window_attrs.y, x_window_attrs.width, x_window_attrs.height, BORDER_WIDTH, BORDER_COLOR, BG_COLOR);

    XSelectInpput(display_, frame, SubstructureRedirectMask | SubstructureNotifyMask);
    XAddToSaveSet(display_, w);
    XReparentWindow(display_, w, frame, 0, 0);
    XMapWindow(display_, frame);
    
    clients_[w] = frame;
    XGrabButton(display_, Button1, ModlMask, w, false, ButtonPressMask | Button





