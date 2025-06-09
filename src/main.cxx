#ifdef DEBUG_MODE
	#include <iostream>
#endif
#include <stdexcept>
#include <cstdint>
#include <cassert>
#include <vector>
#include <chrono>
#include <thread>

#include <glfw3webgpu.h>
#include <GLFW/glfw3.h>
#include <webgpu/webgpu.h>
#include <webgpu/wgpu.h>

#ifdef DEBUG_MODE

#define LOG_MSG_SUC(msg) std::cout << msg << '\n';
#define LOG_MSG_ERR(msg) std::cerr << "ERROR: " << msg << '\n';

#else

#define LOG_MSG_SUC(msg)
#define LOG_MSG_ERR(msg)

#endif

const char* shaderSource = R"(
@vertex
fn vs_main(@location(0) in_vertex_position: vec2f) -> @builtin(position) vec4f {
	return vec4f(in_vertex_position, 0.0, 1.0);
}

@fragment
fn fs_main() -> @location(0) vec4f {
	return vec4f(1.0, 0.0, 0.0, 1.0);
}
)";

void wgpuPollEvents([[maybe_unused]] WGPUDevice device, [[maybe_unused]] bool yieldToWebBrowser) {
#if defined(WEBGPU_BACKEND_DAWN)
	wgpuDeviceTick(device);
#elif defined(WEBGPU_BACKEND_WGPU)
	wgpuDevicePoll(device, false, nullptr);
#elif defined(WEBGPU_BACKEND_EMSCRIPTEN)
	if (yieldToWebBrowser) {
		emscripten_sleep(100);
	}
#endif
}

namespace WindowProperties
{
	const int WINDOW_WIDTH = 640;
	const int WINDOW_HEIGHT = 480;
	const char* WINDOW_TITLE = "WebGPU C++ Hello Triangle";
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
		initializeWGPU();
		windowLoop();
		terminateApplication();
	}

private:
	GLFWwindow* window;

	uint32_t vertexCount;

	std::vector<WGPUFeatureName> adapterFeatures;
	std::vector<WGPUFeatureName> deviceFeatures;
	WGPUAdapterProperties adapterProperties;
	WGPUSupportedLimits adapterSupportedLimits;
	WGPUSupportedLimits deviceSupportedLimits;
	WGPUTextureFormat surfaceFormat;
	WGPUBuffer vertexBuffer;

	WGPUInstance instance;
	WGPUAdapter adapter;
	WGPUDevice device;
	WGPUQueue queue;
	WGPUSurface surface;
	WGPURenderPipeline pipeline;

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
		initializeRenderPipeline();
		getQueue();
		initializeBuffers();
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

	void createShaderModule(WGPUShaderModule& arg_ShaderModule) const
	{
		WGPUShaderModuleWGSLDescriptor shaderWGSLDesc{};
		shaderWGSLDesc.chain.next = nullptr;
		shaderWGSLDesc.chain.sType = WGPUSType_ShaderModuleWGSLDescriptor;
		shaderWGSLDesc.code = shaderSource;

		WGPUShaderModuleDescriptor shaderDesc{};
		shaderDesc.nextInChain = &shaderWGSLDesc.chain;

		arg_ShaderModule = wgpuDeviceCreateShaderModule(device, &shaderDesc);
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
			renderFrame();

			glfwPollEvents();
		}
	}

	void terminateApplication()
	{
		wgpuRenderPipelineRelease(pipeline);

		glfwDestroyWindow(window);
		glfwTerminate();

		wgpuQueueRelease(queue);
		wgpuSurfaceUnconfigure(surface);
		wgpuSurfaceRelease(surface);
		wgpuBufferRelease(vertexBuffer);
	}

	void renderFrame()
	{
		std::pair<WGPUSurfaceTexture, WGPUTextureView> surfaceData = getNextSurfaceViewData();
		WGPUSurfaceTexture surfaceTexture = surfaceData.first;
		WGPUTextureView targetView = surfaceData.second;

		if (!targetView) return;

		WGPUCommandEncoderDescriptor encoderDesc = {};
		encoderDesc.label = "Command Encoder";
		WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(device, &encoderDesc);

		WGPURenderPassColorAttachment renderPassColorAttachment = {};
		renderPassColorAttachment.view = targetView;
		renderPassColorAttachment.loadOp = WGPULoadOp_Clear;
		renderPassColorAttachment.storeOp = WGPUStoreOp_Store;
		renderPassColorAttachment.clearValue = WGPUColor{ 0.0, 0.0, 0.0, 1.0 };

		WGPURenderPassDescriptor renderPassDesc = {};
		renderPassDesc.colorAttachmentCount = 1;
		renderPassDesc.colorAttachments = &renderPassColorAttachment;
		WGPURenderPassEncoder renderPass = wgpuCommandEncoderBeginRenderPass(encoder, &renderPassDesc);

		wgpuRenderPassEncoderSetPipeline(renderPass, pipeline);
		wgpuRenderPassEncoderSetVertexBuffer(renderPass, 0, vertexBuffer, 0, wgpuBufferGetSize(vertexBuffer));
		wgpuRenderPassEncoderDraw(renderPass, vertexCount, 1, 0, 0);

		wgpuRenderPassEncoderEnd(renderPass);

		WGPUCommandBufferDescriptor commandBufferDesc = {};
		commandBufferDesc.label = "Command Buffer";
		WGPUCommandBuffer commandBuffer = wgpuCommandEncoderFinish(encoder, &commandBufferDesc);
		wgpuQueueSubmit(queue, 1, &commandBuffer);

		wgpuSurfacePresent(surface);

		wgpuCommandBufferRelease(commandBuffer);
		wgpuTextureViewRelease(targetView);
		
		wgpuTextureRelease(surfaceTexture.texture);
		wgpuCommandEncoderRelease(encoder);
	}

	void initializeBuffers()
	{
		std::vector<float> vertexData = {
			
			-0.5, -0.5,
			+0.5, -0.5,
			+0.5, +0.5,

			+0.5, +0.5,
			-0.5, +0.5,
			-0.5, -0.5
		};

		vertexCount = static_cast<uint32_t>(vertexData.size() / 2);

		WGPUBufferDescriptor vertexBufferDesc{};
		vertexBufferDesc.nextInChain = nullptr;
		vertexBufferDesc.label = "Vertex buffer";
		vertexBufferDesc.size = vertexData.size() * sizeof(float);
		vertexBufferDesc.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Vertex;
		vertexBufferDesc.mappedAtCreation = false;
		vertexBuffer = wgpuDeviceCreateBuffer(device, &vertexBufferDesc);

		wgpuQueueWriteBuffer(queue, vertexBuffer, 0, vertexData.data(), vertexBufferDesc.size);

		WGPUCommandEncoderDescriptor encoderDesc{};
		encoderDesc.nextInChain = nullptr;
		encoderDesc.label = "Command encoder";
		WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(device, &encoderDesc);

		WGPUCommandBuffer command = wgpuCommandEncoderFinish(encoder, nullptr);
		wgpuCommandEncoderRelease(encoder);
		wgpuQueueSubmit(queue, 1, &command);
		wgpuCommandBufferRelease(command);
	}

	void getAdapter()
	{
		surface = glfwGetWGPUSurface(instance, window);

		WGPURequestAdapterOptions adapterOpts{};
		adapterOpts.nextInChain = nullptr;
		adapterOpts.compatibleSurface = surface;
		adapter = requestAdapterSync(instance, adapterOpts);
		wgpuInstanceRelease(instance);

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
		WGPURequiredLimits requiredLimits = getRequiredLimits(adapter);
		WGPUDeviceDescriptor deviceDesc{};
		deviceDesc.nextInChain = nullptr;
		deviceDesc.label = "The device";
		deviceDesc.requiredFeatureCount = 0;
		deviceDesc.requiredLimits = &requiredLimits;
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
		configSurface();

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

	void getQueue()
	{
		queue = wgpuDeviceGetQueue(device);

		auto onQueueWorkDone =
			[](WGPUQueueWorkDoneStatus arg_WorkDoneStatus, void*)
			{
#ifdef DEBUG_MODE
				std::cout << "Queue work finished with status: " << arg_WorkDoneStatus;
#else
				(void)arg_WorkDoneStatus;
#endif
			};

		wgpuQueueOnSubmittedWorkDone(queue, onQueueWorkDone, nullptr);
	}

	void configSurface() const
	{
		WGPUSurfaceConfiguration surfaceConfig = {};
		WGPUTextureFormat textureFormat = wgpuSurfaceGetPreferredFormat(surface, adapter);
		surfaceConfig.nextInChain = nullptr;
		surfaceConfig.width = WindowProperties::WINDOW_WIDTH;
		surfaceConfig.height = WindowProperties::WINDOW_WIDTH;
		surfaceConfig.format = textureFormat;
		surfaceConfig.viewFormatCount = 0;
		surfaceConfig.viewFormats = nullptr;
		surfaceConfig.usage = WGPUTextureUsage_RenderAttachment;
		surfaceConfig.device = device;
		surfaceConfig.presentMode = WGPUPresentMode_Fifo;
		surfaceConfig.alphaMode = WGPUCompositeAlphaMode_Auto;
		
		wgpuSurfaceConfigure(surface, &surfaceConfig);
	}

	void initializeRenderPipeline()
	{
		WGPUShaderModule shaderModule;
		createShaderModule(shaderModule);

		WGPUVertexAttribute positionAttrib;
		positionAttrib.shaderLocation = 0;
		positionAttrib.format = WGPUVertexFormat_Float32x2;
		positionAttrib.offset = 0;

		WGPUVertexBufferLayout vertexBufferLayout{};
		vertexBufferLayout.attributeCount = 1;
		vertexBufferLayout.attributes = &positionAttrib;
		vertexBufferLayout.arrayStride = 2 * sizeof(float);
		vertexBufferLayout.stepMode = WGPUVertexStepMode_Vertex;

		surfaceFormat = wgpuSurfaceGetPreferredFormat(surface, adapter);
		wgpuAdapterRelease(adapter);

		WGPUBlendState blendState{};
		blendState.color.srcFactor = WGPUBlendFactor_SrcAlpha;
		blendState.color.dstFactor = WGPUBlendFactor_OneMinusSrcAlpha;
		blendState.color.operation = WGPUBlendOperation_Add;

		blendState.alpha.srcFactor = WGPUBlendFactor_Zero;
		blendState.alpha.dstFactor = WGPUBlendFactor_One;
		blendState.alpha.operation = WGPUBlendOperation_Add;

		WGPUColorTargetState colorTarget{};
		colorTarget.format = surfaceFormat;
		colorTarget.blend = &blendState;
		colorTarget.writeMask = WGPUColorWriteMask_All;

		WGPURenderPipelineDescriptor pipelineDesc{};
		pipelineDesc.nextInChain = nullptr;
		pipelineDesc.vertex.bufferCount = 1;
		pipelineDesc.vertex.buffers = &vertexBufferLayout;
		pipelineDesc.vertex.module = shaderModule;
		pipelineDesc.vertex.entryPoint = "vs_main";
		pipelineDesc.vertex.constantCount = 0;
		pipelineDesc.vertex.constants = nullptr;

		pipelineDesc.primitive.topology = WGPUPrimitiveTopology_TriangleList;
		pipelineDesc.primitive.stripIndexFormat = WGPUIndexFormat_Undefined;
		pipelineDesc.primitive.frontFace = WGPUFrontFace_CCW;
		pipelineDesc.primitive.cullMode = WGPUCullMode_None;

		WGPUFragmentState fragmentState{};
		fragmentState.module = shaderModule;
		fragmentState.entryPoint = "fs_main";
		fragmentState.constantCount = 0;
		fragmentState.constants = nullptr;
		fragmentState.targetCount = 1;
		fragmentState.targets = &colorTarget;

		pipelineDesc.fragment = &fragmentState;
		pipelineDesc.depthStencil = nullptr;

		pipelineDesc.multisample.count = 1;
		pipelineDesc.multisample.mask = ~0u;
		pipelineDesc.multisample.alphaToCoverageEnabled = false;
		
		pipeline = wgpuDeviceCreateRenderPipeline(device, &pipelineDesc);
	}

	void limitsSetDefault(WGPULimits& limits)
	{
		limits.maxBindGroups = WGPU_LIMIT_U32_UNDEFINED;
		limits.maxBindGroupsPlusVertexBuffers = WGPU_LIMIT_U32_UNDEFINED;
		limits.maxBindingsPerBindGroup = WGPU_LIMIT_U32_UNDEFINED;
		limits.maxBufferSize = WGPU_LIMIT_U32_UNDEFINED;
		limits.maxColorAttachmentBytesPerSample = WGPU_LIMIT_U32_UNDEFINED;
		limits.maxColorAttachments = WGPU_LIMIT_U32_UNDEFINED;
		limits.maxComputeInvocationsPerWorkgroup = WGPU_LIMIT_U32_UNDEFINED;
		limits.maxComputeWorkgroupSizeX = WGPU_LIMIT_U32_UNDEFINED;
		limits.maxComputeWorkgroupSizeY = WGPU_LIMIT_U32_UNDEFINED;
		limits.maxComputeWorkgroupSizeZ = WGPU_LIMIT_U32_UNDEFINED;
		limits.maxComputeWorkgroupsPerDimension = WGPU_LIMIT_U32_UNDEFINED;
		limits.maxComputeWorkgroupStorageSize = WGPU_LIMIT_U32_UNDEFINED;
		limits.maxDynamicStorageBuffersPerPipelineLayout = WGPU_LIMIT_U32_UNDEFINED;
		limits.maxDynamicUniformBuffersPerPipelineLayout = WGPU_LIMIT_U32_UNDEFINED;
		limits.maxInterStageShaderComponents = WGPU_LIMIT_U32_UNDEFINED;
		limits.maxInterStageShaderVariables = WGPU_LIMIT_U32_UNDEFINED;
		limits.maxSampledTexturesPerShaderStage = WGPU_LIMIT_U32_UNDEFINED;
		limits.maxSamplersPerShaderStage = WGPU_LIMIT_U32_UNDEFINED;
		// no maxStorageBufferBindingSize
		limits.maxStorageBuffersPerShaderStage = WGPU_LIMIT_U32_UNDEFINED;
		limits.maxStorageTexturesPerShaderStage = WGPU_LIMIT_U32_UNDEFINED;
		limits.maxTextureArrayLayers = WGPU_LIMIT_U32_UNDEFINED;
		limits.maxTextureDimension1D = WGPU_LIMIT_U32_UNDEFINED;
		limits.maxTextureDimension2D = WGPU_LIMIT_U32_UNDEFINED;
		limits.maxTextureDimension3D = WGPU_LIMIT_U32_UNDEFINED;
		// no maxUniformBufferBindingSize
		limits.maxUniformBuffersPerShaderStage = WGPU_LIMIT_U32_UNDEFINED;
		limits.maxVertexAttributes = WGPU_LIMIT_U32_UNDEFINED;
		limits.maxVertexBufferArrayStride = WGPU_LIMIT_U32_UNDEFINED;
		limits.maxVertexBuffers = WGPU_LIMIT_U32_UNDEFINED;
		limits.minStorageBufferOffsetAlignment = WGPU_LIMIT_U32_UNDEFINED;
		limits.minUniformBufferOffsetAlignment = WGPU_LIMIT_U32_UNDEFINED;
	}

	void logAdapter()
	{
#ifdef DEBUG_MODE
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
#endif
	}

	void logDevice()
	{
#ifdef DEBUG_MODE
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
#endif
	}

	std::pair<WGPUSurfaceTexture, WGPUTextureView> getNextSurfaceViewData()
	{
		WGPUSurfaceTexture surfaceTexture;
		wgpuSurfaceGetCurrentTexture(surface, &surfaceTexture);

		if (surfaceTexture.status != WGPUSurfaceGetCurrentTextureStatus_Success)
		{
			return { surfaceTexture, nullptr };
		}

		WGPUTextureViewDescriptor viewDescriptor;
		viewDescriptor.nextInChain = nullptr;
		viewDescriptor.label = "Surface texture view";
		viewDescriptor.format = wgpuTextureGetFormat(surfaceTexture.texture);
		viewDescriptor.dimension = WGPUTextureViewDimension_2D;
		viewDescriptor.baseMipLevel = 0;
		viewDescriptor.mipLevelCount = 1;
		viewDescriptor.baseArrayLayer = 0;
		viewDescriptor.arrayLayerCount = 1;
		viewDescriptor.aspect = WGPUTextureAspect_All;
		WGPUTextureView targetView = wgpuTextureCreateView(surfaceTexture.texture, &viewDescriptor);

		return { surfaceTexture, targetView };
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
					LOG_MSG_SUC("\nGot device successfully");
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

	WGPURequiredLimits getRequiredLimits(WGPUAdapter arg_Adapter)
	{
		(void)arg_Adapter;

		WGPURequiredLimits requiredLimits{};
		limitsSetDefault(requiredLimits.limits);

		requiredLimits.limits.maxVertexAttributes = 1;
		requiredLimits.limits.maxVertexBufferArrayStride = 1;
		requiredLimits.limits.maxBufferSize = 2 * 6 * sizeof(float);
		requiredLimits.limits.maxVertexBufferArrayStride = 2 * sizeof(float);
		requiredLimits.limits.minStorageBufferOffsetAlignment = adapterSupportedLimits.limits.minStorageBufferOffsetAlignment;
		requiredLimits.limits.minUniformBufferOffsetAlignment = adapterSupportedLimits.limits.minUniformBufferOffsetAlignment;

		return requiredLimits;
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
#ifdef DEBUG_MODE
	std::cerr << err.what();
#else
	(void)err;
#endif
	return EXIT_FAILURE;
}