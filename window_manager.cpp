#include "window_manager.hpp"
extern "C" {
#include <X11/Xutil.h>
}
#include <cstring>
#include <algorithm>
#include "util.hpp"

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
    display_(display),
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
        XSelectInput(display_, root_, SubstructureRedirectMask | SubstructureNotifyMask);
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
    XQueryTree(display_, root_, &returned_root, &returned_parent,
      &top_level_window, &num_top_level_windows);
    if (XQueryTree == NULL) {
        cerr << "XQueryTree was NULL" << endl;
        exit(-1);
    }
    if (returned_root != root_) {
        cerr << "Returned root did not pass assertion" << endl;
        exit(-1);
    }

    for (unsigned int i = 0; i < num_top_level_windows; ++i) {
        Frame(top_level_window[i]);
    }

    XFree(top_level_window);
    XUngrabServer(display_);

    for(;;) {
        XEvent event;
        XNextEvent(display_, &event);
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
                while (XCheckTypedWindowEvent(display_, event.xmotion.window, MotionNotify, &event)) {}
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
    const unsigned int BORDER_WIDTH = 3;
    const unsigned long BORDER_COLOR = 0xff0000;
    const unsigned long BG_COLOR = 0x0000ff;
    if (clients_.count(w)) {
        cerr << "Aborting." << endl;
        exit(-1);
    }

    XWindowAttributes x_window_attrs;
    if (!XGetWindowAttributes(display_, w, &x_window_attrs)) {
        cerr << "Aborting." << endl;
        exit(-1);
    }
    const Window frame = XCreateSimpleWindow(display_, root_, x_window_attrs.x, x_window_attrs.y, x_window_attrs.width, x_window_attrs.height, BORDER_WIDTH, BORDER_COLOR, BG_COLOR);

    XSelectInput(display_, frame, SubstructureRedirectMask | SubstructureNotifyMask);
    XAddToSaveSet(display_, w);
    XReparentWindow(display_, w, frame, 0, 0);
    XMapWindow(display_, frame);
    
    clients_[w] = frame;
    XGrabButton(display_, Button1, Mod1Mask, w, false, ButtonPressMask | ButtonMotionMask, GrabModeAsync, GrabModeAsync, None, None);
    XGrabKey(display_, XKeysymToKeycode(display_, XK_F4), Mod1Mask, w, false, GrabModeAsync, GrabModeAsync);
    XGrabKey(display_, XKeysymToKeycode(display_, XK_Tab), Mod1Mask, w, false, GrabModeAsync, GrabModeAsync);
}

void WindowManager::Unframe(Window w) {
    if (clients_.count(w) == 0) {
        cerr << "Client count did not pass assertion" << endl;
    }
    const Window frame = clients_[w];
    XUnmapWindow(display_, frame);
    XReparentWindow(display_, w, root_, 0, 0);
    XRemoveFromSaveSet(display_, w);
    XDestroyWindow(display_, frame);
    clients_.erase(w);
}

void WindowManager::OnCreateNotify(const XCreateWindowEvent &event) {}
void WindowManager::OnDestroyNotify(const XDestroyWindowEvent &event) {}
void WindowManager::OnMapNotify(const XMapEvent &event) {}
void WindowManager::OnReparentNotify(const XReparentEvent &event) {}
void WindowManager::OnUnmapNotify(const XUnmapEvent &event) {
    if (!clients_.count(event.window)) {
        cerr << "Ignore UnmapNotify for window that isn't a client." << endl;
        return;
    }
    if (event.event == root_) {
        return;
    }
    Unframe(event.window);
}

void WindowManager::OnConfigureNotify(const XConfigureEvent &event) {}
void WindowManager::OnMapRequest(const XMapRequestEvent &event) {
    Frame(event.window);
    XMapWindow(display_, event.window);
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
    if (clients_.count(event.window)) {
        const Window frame = clients_[event.window];
        XConfigureWindow(display_, frame, event.value_mask, &changes);
        cerr << "Frame resized to " << Size<int>(event.width, event.height);
    }
    XConfigureWindow(display_, event.window, event.value_mask, &changes);
    cerr << "Window resized to " << Size<int>(event.width, event.height);
}

void WindowManager::OnButtonPress(const XButtonEvent &event) {
    //CHECK(clients_.count(event.window));
    if (clients_.count(event.window) == 0) {
        cerr << "Client count for the event's window did not pass assertion (>0)" << endl;
    }
    const Window frame = clients_[event.window];
    drag_start_pos_ = Position<int>(event.x_root, event.y_root);
    Window returned_root;
    int x, y;
    unsigned width, height, border_width, depth;
    if (!XGetGeometry(display_, frame, &returned_root, &x, &y, &width, &height, &border_width, &depth)) {
        cerr << "XGetGeometry failed for some reason" << endl;
    }
    drag_start_frame_pos_ = Position<int>(x, y);
    drag_start_frame_size_ = Size<int>(width, height);
    XRaiseWindow(display_, frame);
}

void WindowManager::OnButtonRelease(const XButtonEvent &event) {}
void WindowManager::OnMotionNotify(const XMotionEvent &event) {
    //CHECK(clients_.count(event.window));
    const Window frame = clients_[event.window];
    const Position<int> drag_pos(event.x_root, event.y_root);
    const Vector2D<int> delta = drag_pos - drag_start_pos_;
    if (event.state & Button1Mask) {
        const Position<int> dest_frame_pos = drag_start_frame_pos_ + delta;
        XMoveWindow(display_, frame, dest_frame_pos.x, dest_frame_pos.y);
    } else if (event.state & Button3Mask) {
        const Vector2D<int> size_delta(max(delta.x, -drag_start_frame_size_.width), max(delta.y, -drag_start_frame_size_.height));
        const Size<int> dest_frame_size = drag_start_frame_size_ + size_delta;
        XResizeWindow(display_, frame, dest_frame_size.width, dest_frame_size.height);
        XResizeWindow(display_, event.window, dest_frame_size.width, dest_frame_size.height);
    }
}

void WindowManager::OnKeyPress(const XKeyEvent &e) {
    if ((e.state & Mod1Mask) && (e.keycode == XKeysymToKeycode(display_, XK_F4))) {
        Atom* supported_protocols;
        int num_supported_protocols;
        if (XGetWMProtocols(display_, e.window, &supported_protocols, &num_supported_protocols) && (find(supported_protocols, supported_protocols + num_supported_protocols, WM_DELETE_WINDOW) != supported_protocols + num_supported_protocols)) {
            cerr << "Deleting window" << e.window << endl;
            XEvent msg;
            memset(&msg, 0, sizeof(msg));
            msg.xclient.type = ClientMessage;
            msg.xclient.message_type = WM_PROTOCOLS;
            msg.xclient.window = e.window;
            msg.xclient.format = 32;
            msg.xclient.data.l[0] = WM_DELETE_WINDOW;
            //CHECK(XSendEvent(display_, e.window, false, 0, &msg));
        } else {
            cerr << "Killing window " << e.window << endl;
            XKillClient(display_, e.window);
        }
    } else if ((e.state & Mod1Mask) && (e.keycode == XKeysymToKeycode(display_, XK_Tab))) {
        auto i = clients_.find(e.window);
        //CHECK(i != clients_.end());
        ++i;
        if (i == clients_.end()) {
            i = clients_.begin();
        }
        XRaiseWindow(display_, i->second);
        XSetInputFocus(display_, i->first, RevertToPointerRoot, CurrentTime);
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
    //CHECK_EQ(static_cast<int>(e->error_code), BadAccess);
    wm_detected_ = true;
    return 0;
}
