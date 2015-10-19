#include "window_manager.hpp"
extern "C" {
#include <X11/Xutil.h>
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

