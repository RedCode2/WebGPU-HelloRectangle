#define DEBUG_MODE

#include <iostream>
#include <stdexcept>
#include <cstdint>
#include <cassert>
#include <vector>

#include <glfw3webgpu.h>
#include <GLFW/glfw3.h>
#include <webgpu/webgpu.h>

#ifdef DEBUG_MODE

#define LOG_MSG_SUC(msg) std::cout << msg << '\n';
#define LOG_MSG_ERR(msg) std::cerr << "ERROR: " << msg << '\n';

#else

#define LOG_MSG_SUC(msg)
#define LOG_MSG_ERR(msg)

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
		initializeWGPU();
		createWindow();
		windowLoop();
		terminateApplication();
	}

private:
	GLFWwindow* window;

	std::vector<WGPUFeatureName> adapterFeatures;
	std::vector<WGPUFeatureName> deviceFeatures;
	WGPUAdapterProperties adapterProperties;
	WGPUSupportedLimits adapterSupportedLimits;
	WGPUSupportedLimits deviceSupportedLimits;

	WGPUInstance instance;
	WGPUAdapter adapter;
	WGPUDevice device;

private:
	void initializeGLFW()
	{
		if (!glfwInit()) throw std::runtime_error("Failed to initialize GLFW");

		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
	}

	void initializeWGPU()
	{
		createInstance();
		getAdapter();
		getDevice();
	}

	void createInstance()
	{
		WGPUInstanceDescriptor instanceDesc = {};
		instanceDesc.nextInChain = nullptr;
		instance = wgpuCreateInstance(&instanceDesc);

		if (!instance)
		{
			glfwTerminate();
			throw std::runtime_error("Could not initialize WebGPU");
		}

		LOG_MSG_SUC("WebGPU instance: " << instance);
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

	void getAdapter()
	{
		WGPURequestAdapterOptions adapterOpts{};
		adapterOpts.nextInChain = nullptr;
		adapter = requestAdapterSync(instance, adapterOpts);

		if (!adapter)
		{
			glfwTerminate();
			throw std::runtime_error("Couldn't get adapter");
		}

		LOG_MSG_SUC("\nGot adapter: " << adapter);

		adapterFeatures = {};
		size_t featureCount = wgpuAdapterEnumerateFeatures(adapter, nullptr);
		adapterFeatures.resize(featureCount);
		wgpuAdapterEnumerateFeatures(adapter, adapterFeatures.data());

		adapterProperties = {};
		adapterProperties.nextInChain = nullptr;
		wgpuAdapterGetProperties(adapter, &adapterProperties);

		adapterSupportedLimits = {};
		adapterSupportedLimits.nextInChain = nullptr;
		wgpuAdapterGetLimits(adapter, &adapterSupportedLimits);

#ifdef DEBUG_MODE
		logAdapter();
#endif
	}

	void getDevice()
	{
		WGPUDeviceDescriptor deviceDesc{};
		deviceDesc.nextInChain = nullptr;
		deviceDesc.label = "The device";
		deviceDesc.requiredFeatureCount = 0;
		deviceDesc.requiredLimits = nullptr;
		deviceDesc.defaultQueue.label = "Default queue";
		deviceDesc.defaultQueue.nextInChain = nullptr;
		deviceDesc.deviceLostCallback =
			[](WGPUDeviceLostReason arg_DeviceLostReason, char const* arg_Message, void*)
			{
				std::string err_msg = "Device lost: reason " + arg_DeviceLostReason;
				arg_Message ? err_msg += arg_Message : err_msg;
				LOG_MSG_SUC(err_msg);
			};

		device = requestDeviceSync(adapter, &deviceDesc);

		if (!device)
		{
			glfwTerminate();
			throw std::runtime_error("Could not get device");
		}

		LOG_MSG_SUC("Got device: " << device);
		
		deviceFeatures = {};
		size_t featureCount = wgpuDeviceEnumerateFeatures(device, nullptr);
		deviceFeatures.resize(featureCount);
		wgpuDeviceEnumerateFeatures(device, deviceFeatures.data());

		deviceSupportedLimits = {};
		deviceSupportedLimits.nextInChain = nullptr;
		wgpuDeviceGetLimits(device, &deviceSupportedLimits);

#ifdef DEBUG_MODE
		logDevice();
#endif
	}

	void logAdapter()
	{
		std::cout << "Adapter features:\n";
		std::cout << std::hex;
		for (const WGPUFeatureName feature : adapterFeatures)
			std::cout << " - 0x" << feature << '\n';

		std::cout << std::dec;

		std::cout << "\nAdapter properties:\n";

		std::cout << " - vendorID: "			<< adapterProperties.vendorID << '\n';
		std::cout << " - vendorName: "			<< adapterProperties.vendorName << '\n';
		std::cout << " - architecture: "		<< adapterProperties.architecture << '\n';
		std::cout << " - name: "				<< adapterProperties.name << '\n';
		std::cout << " - driverDescription: " 	<< adapterProperties.driverDescription << '\n';
		std::cout << " - backendType: " 		<< adapterProperties.backendType << '\n';

		std::cout << "\nAdapter limits:\n";
		
		std::cout << " - maxTextureDimension1D: " << adapterSupportedLimits.limits.maxTextureDimension1D << '\n';
		std::cout << " - maxTextureDimension2D: " << adapterSupportedLimits.limits.maxTextureDimension2D << '\n';
		std::cout << " - maxTextureDimension3D: " << adapterSupportedLimits.limits.maxTextureDimension3D << '\n';
		std::cout << " - maxTextureArrayLayers: " << adapterSupportedLimits.limits.maxTextureArrayLayers << '\n';
	}

	void logDevice()
	{
		std::cout << "Device features:\n";
		std::cout << std::hex;

		for (const WGPUFeatureName feature : deviceFeatures)
			std::cout << " - 0x" << feature << '\n';
		
		std::cout << std::dec;

		std::cout << "\nDevice limits:\n";

		std::cout << " - maxTextureDimension1D: " << deviceSupportedLimits.limits.maxTextureDimension1D << '\n';
		std::cout << " - maxTextureDimension2D: " << deviceSupportedLimits.limits.maxTextureDimension2D << '\n';
		std::cout << " - maxTextureDimension3D: " << deviceSupportedLimits.limits.maxTextureDimension3D << '\n';
		std::cout << " - maxTextureArrayLayers: " << deviceSupportedLimits.limits.maxTextureArrayLayers << '\n';
	}

	WGPUAdapter requestAdapterSync(WGPUInstance arg_Instance, WGPURequestAdapterOptions arg_RequestAdapterOpts)
	{
		struct UserData
		{
			WGPUAdapter adapter;
			bool requestEnded;
		};
		UserData userData{};
		userData.adapter = nullptr;
		userData.requestEnded = false;

		auto onAdapterRequestEnded =
			[](WGPURequestAdapterStatus arg_RequestAdapterStatus, WGPUAdapter arg_Adapter, char const* arg_Message, void* arg_UserData)
			{
				UserData& userData = *reinterpret_cast<UserData*>(arg_UserData);
				userData.adapter = nullptr;
				userData.requestEnded = false;

				if (arg_RequestAdapterStatus == WGPURequestAdapterStatus_Success)
				{
					userData.adapter = arg_Adapter;
					LOG_MSG_SUC("Got adapter successfully");
				}
				else
				{
					std::string err_msg = "WebGPU Adapter request denied: ";
					err_msg += arg_Message;
					throw std::runtime_error(err_msg.c_str());
				}

				userData.requestEnded = true;
			};

		wgpuInstanceRequestAdapter(
			arg_Instance,
			&arg_RequestAdapterOpts,
			onAdapterRequestEnded,
			(void*)&userData
		);

		assert(userData.requestEnded);

		return userData.adapter;
	};

	WGPUDevice requestDeviceSync(WGPUAdapter arg_Adapter, WGPUDeviceDescriptor const* arg_DeviceDescriptor)
	{
		struct UserData
		{
			WGPUDevice device;
			bool requestEnded;
		};
		UserData userData;
		userData.device = nullptr;
		userData.requestEnded = false;

		auto onDeviceRequestEnded =
			[](WGPURequestDeviceStatus arg_RequestDeviceStatus, WGPUDevice arg_Device, char const* arg_Message, void* arg_UserData)
			{
				UserData& userData = *reinterpret_cast<UserData*>(arg_UserData);

				if (arg_RequestDeviceStatus == WGPURequestDeviceStatus_Success)
				{
					userData.device = arg_Device;
					LOG_MSG_SUC("Got device successfully");
				}
				else
				{
					std::string err_msg = "WebGPU Device request denied: ";
					err_msg += arg_Message;
					throw std::runtime_error(err_msg);
				}

				userData.requestEnded = true;
			};

		wgpuAdapterRequestDevice(
			arg_Adapter,
			arg_DeviceDescriptor,
			onDeviceRequestEnded,
			(void*)&userData
		);

		assert(userData.requestEnded);

		return userData.device;
	}
};

int main() try
{
	Application app;
	app.run();

	LOG_MSG_SUC("\nApplication ran successfully");

	return EXIT_SUCCESS;
}
catch (const std::exception& err)
{
	LOG_MSG_ERR(err.what());
	return EXIT_FAILURE;
}