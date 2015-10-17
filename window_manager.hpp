#ifndef WINDOW_MANAGER_HPP
#define WINDOW_MANAGER_HPP

#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include "util.hpp"
extern "C" {
#include <X11/Xlib.h>
}
using namespace std;
class WindowManager {
	public:
		static std::unique_ptr<WindowManager> Create(const std::string& display_str = std::string());
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
		void OnUnmapRequest(const XUnmapRequestEvent& event);
		void OnButtonPress(const XButtonEvent& event);
		void OnButtonRelease(const XButtonEvent& event);
		void OnMotionNotify(const XMotionEvent& event);
		static int OnXError(Display* display, XErrorEvent* event);
		static int OnWMDetected(Display* display, XErrorEvent* event);
		static bool wm_detected_;
		Display* display_;
		const Window root_;
		std::unordered_map<Window, Window> clients_;
		Position<int> drag_start_pos_;
		Position<int> drag_start_frame_pos_;
		Size<int> drag_start_frame_size_;


		const Atom WM_PROTOCOLS;
		const Atom WM_DELETE_WINDOW;
};
#endif
		
