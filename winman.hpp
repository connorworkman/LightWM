#ifndef WINMAN_HPP
#define WINMAN_HPP
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include "eventnames.hpp"
extern "C" {
#include <X11/Xlib.h>
}
using namespace std;
class WindowManager {
	public:
		static unique_ptr<WindowManager> Create(const string& display_str = string());
		~WindowManager();
		void Run();

	private:
		WindowManager(Display* display);
		void Frame(Window window);
		void Unframe(Window window);
		
        
        void OnCreateNotify(const XCreateWindowEvent& event);
		void OnDestroyNotify(const XDestroyWindowEvent& event);
		void OnReparentNotify(const XReparentEvent& event);
		void OnKeyPress(const XKeyEvent& event);
		void OnKeyRelease(const XKeyEvent& event);
		void OnMapNotify(const XMapEvent& event);
		void OnUnmapNotify(const XUnmapEvent& event);
		void OnConfigureNotify(const XConfigureEvent& event);
		void OnMapRequest(const XMapRequestEvent& event);
		void OnConfigureRequest(const XConfigureRequestEvent& event);
		void OnButtonPress(const XButtonEvent& event);
		void OnButtonRelease(const XButtonEvent& event);
		void OnMotionNotify(const XMotionEvent& event);
		static int OnXError(Display* display, XErrorEvent* event);
		static int OnWMDetected(Display* display, XErrorEvent* event);
		static bool wm_detected_;
        static mutex wm_detected_mutex_;

		Display* display_handle;
		const Window root_handle;
		unordered_map<Window, Window> clients_handle;
		Position<int> drag_start_pos_;
		Position<int> drag_start_frame_pos_;
		Size<int> drag_start_frame_size_;


		const Atom WM_PROTOCOLS;
		const Atom WM_DELETE_WINDOW;
};
#endif
