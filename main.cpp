#include <cstdlib>
#include "window_manager.hpp"
#include <memory>
#include <iostream>
int main(int argc, char** argv) {
	std::unique_ptr<WindowManager> window_manager(WindowManager::Create());
	if (!window_manager) {
		cout << "ERROR" << "\n";
		return EXIT_FAILURE;
	}
	window_manager->Run();
	return EXIT_SUCCESS;
}
