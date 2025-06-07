#define DEBUG_MODE

#include <iostream>
#include <stdexcept>
#include <cstdint>

#include <glfw3webgpu.h>
#include <GLFW/glfw3.h>
#include <webgpu/webgpu.h>

#ifdef DEBUG_MODE

#define LOG_MSG_SUC(msg) std::cout << msg << '\n';
#define LOG_MSG_ERR(msg) std::cerr << "ERROR: " << msg << '\n';

#else

#define LOG_MSG_SUC(msg)
#define LOG_MSG_ERR(err)

#endif

namespace WindowProperties
{
	const int WINDOW_WIDTH = 640;
	const int WINDOW_HEIGHT = 480;
	const char* WINDOW_TITLE = "WebGPU Hello Triangle";
	GLFWmonitor* monitor = nullptr;
	GLFWwindow* share = nullptr;
}

class Application
{
public:
	Application() = default;
	
	void run()
	{
		initializeGLFW();
		createWindow();
		windowLoop();
		terminateApplication();
	}

private:
	GLFWwindow* window;

private:
	void initializeGLFW()
	{
		if (!glfwInit()) throw std::runtime_error("Failed to initialize GLFW");

		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
	}

	void createWindow()
	{
		window = glfwCreateWindow(
			WindowProperties::WINDOW_WIDTH,
			WindowProperties::WINDOW_HEIGHT,
			WindowProperties::WINDOW_TITLE,
			WindowProperties::monitor,
			WindowProperties::share
		);

		if (!window)
		{
			glfwTerminate();
			throw std::runtime_error("Failed to create GLFW window");
		}
	}

	void windowLoop()
	{
		while (!glfwWindowShouldClose(window))
		{
			glfwPollEvents();
		}
	}

	void terminateApplication()
	{
		glfwDestroyWindow(window);
		glfwTerminate();
	}
};

int main() try
{
	Application app;
	app.run();

	LOG_MSG_SUC("Application ran successfully");

	return EXIT_SUCCESS;
}
catch (const std::exception& err)
{
	LOG_MSG_ERR(err.what());
	return EXIT_FAILURE;
}