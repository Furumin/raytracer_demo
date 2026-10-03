#pragma once

namespace GPU {

  // Globals
  static Ctx gCtx;
  i32
  Init(const Ctx_init_params &pParams) {
    return gCtx.Init(&pParams);
  }

  void
  Free() {
    gCtx.Free();
  }
  
  // Helpers
  namespace Helper {
    VkResult
    CreateShaderModule(const VkDevice device,
		       const void *pData,
		       const u64 dataSize,
		       VkShaderModule *pShaderModuleOut) {
      if (!pShaderModuleOut || dataSize < 1) {
	return VK_ERROR_UNKNOWN;
      }

      VkShaderModuleCreateInfo createInfo = {
	.sType = VKTYPE(SHADER_MODULE_CREATE_INFO),
	.codeSize = dataSize,
	.pCode = (const u32*)pData
      };
      return vkCreateShaderModule(device, &createInfo, NULL, pShaderModuleOut);
    }
    
    Vulkan_swap_chain_support_details
    QuerySwapchainSupport(const VkPhysicalDevice device,
				 const VkSurfaceKHR surface) {
      Vulkan_swap_chain_support_details details;
      vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device,
						surface,
						&(details.Capabilities));
      u32 formatCount;
      vkGetPhysicalDeviceSurfaceFormatsKHR(device,
					   surface,
					   &formatCount,
					   NULL);
      if (formatCount != 0) {
	details.FormatCount = formatCount;
	vkGetPhysicalDeviceSurfaceFormatsKHR(device,
					     surface,
					     &details.FormatCount,
					     details.Formats);
      }

      u32 presentModeCount;
      vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface,
						&presentModeCount, NULL);
      if (presentModeCount != 0) {
	details.PresentModeCount = presentModeCount;
	vkGetPhysicalDeviceSurfacePresentModesKHR(device,
						  surface,
						  &presentModeCount,
						  details.PresentModes);
      }

      return details;
    }
    u32
    AssignPhysicalDeviceScore(const VkPhysicalDevice device,
			      const VkSurfaceKHR surface,
			      const u32 apiVersion,
			      VkPhysicalDeviceFeatures *pFeaturesOut,
			      VkPhysicalDeviceVulkan11Features *pFeatures11Out,
			      VkPhysicalDeviceVulkan12Features *pFeatures12Out,
			      VkPhysicalDeviceVulkan13Features *pFeatures13Out,
			      Vulkan_swap_chain_support_details *pSwapChainSupportDetailsOut,
			      VkPhysicalDeviceProperties *pPhysicalDevicePropertiesOut) {
      VkPhysicalDeviceProperties deviceProperties;
      VkPhysicalDeviceFeatures deviceFeatures;
      VkPhysicalDeviceVulkan13Features deviceFeatures13 = {
	.sType = VKTYPE(PHYSICAL_DEVICE_VULKAN_1_3_FEATURES),
	.pNext = NULL
      };
      VkPhysicalDeviceVulkan12Features deviceFeatures12 = {
	.sType = VKTYPE(PHYSICAL_DEVICE_VULKAN_1_2_FEATURES),
	.pNext = &deviceFeatures13
      };
      VkPhysicalDeviceVulkan11Features deviceFeatures11 = {
	.sType = VKTYPE(PHYSICAL_DEVICE_VULKAN_1_1_FEATURES),
	.pNext = &deviceFeatures12
      };    
      VkPhysicalDeviceFeatures2 features2 = {
	.sType = VKTYPE(PHYSICAL_DEVICE_FEATURES_2),
	.pNext = &deviceFeatures11
      };
      vkGetPhysicalDeviceProperties(device, &deviceProperties);
      vkGetPhysicalDeviceFeatures(device, &deviceFeatures);
      vkGetPhysicalDeviceFeatures2(device, &features2);

      // positive score: device is eligible
      // 0 score: device cannot be used
      u32 score = 5;
      if (VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU == deviceProperties.deviceType) {
	score += 100;
      }
      if (VK_FALSE == deviceFeatures12.bufferDeviceAddress) {
	score = 0;
      }
      if (VK_FALSE == deviceFeatures12.descriptorIndexing) {
	score = 0;
      }
      if (VK_FALSE == deviceFeatures.samplerAnisotropy) {
	score = 0;
      }
      if (VK_FALSE == deviceFeatures12.descriptorBindingSampledImageUpdateAfterBind) {
	score = 0;
      }
      if (VK_FALSE == deviceFeatures13.dynamicRendering ||
	  VK_FALSE == deviceFeatures13.synchronization2) {
	score = 0;
      }
      if (VK_FALSE == deviceFeatures.shaderStorageImageExtendedFormats) {
	score = 0;
      }

      Vulkan_swap_chain_support_details swapChainDetails =
	QuerySwapchainSupport(device, surface);
      if (score && (swapChainDetails.FormatCount < 1 || swapChainDetails.PresentModeCount < 1)) {
	score = 0;
      }
  
      // check obligatory features (apiversion, extensions)
      u32 deviceMajorVersion = VK_API_VERSION_MAJOR(deviceProperties.apiVersion);
      u32 deviceMinorVersion = VK_API_VERSION_MINOR(deviceProperties.apiVersion);
      u32 majorVersion = VK_API_VERSION_MAJOR(apiVersion);
      u32 minorVersion = VK_API_VERSION_MINOR(apiVersion);
      if ( score && !(deviceMajorVersion > majorVersion || (deviceMajorVersion >= majorVersion &&
							    (deviceMinorVersion >= minorVersion)))) {
	score = 0;
      }
  
      if (pFeaturesOut) {
	*pFeaturesOut = deviceFeatures;
      }
      if (pFeatures11Out) {
	*pFeatures11Out = deviceFeatures11;
      }  
      if (pFeatures12Out) {
	*pFeatures12Out = deviceFeatures12;
      }
      if (pFeatures13Out) {
	*pFeatures13Out = deviceFeatures13;
      }  
      if (pSwapChainSupportDetailsOut) {
	*pSwapChainSupportDetailsOut = swapChainDetails;
      }
      if (pPhysicalDevicePropertiesOut) {
	*pPhysicalDevicePropertiesOut = deviceProperties;
      }

      return score;
    }
    VkPhysicalDevice
    SelectBestPhysicalDevice(const VkInstance instance,
			     const VkSurfaceKHR surface,
			     const u32 apiVersion,
			     const char **deviceExtensions,
			     const u32 extensionCount,
			     VkPhysicalDeviceFeatures *pFeaturesOut,
			     VkPhysicalDeviceVulkan11Features *pFeatures11Out,
			     VkPhysicalDeviceVulkan12Features *pFeatures12Out,
			     VkPhysicalDeviceVulkan13Features *pFeatures13Out,
			     Vulkan_swap_chain_support_details *pSwapChainSupportDetailsOut,
			     VkPhysicalDeviceProperties *pPhysicalDevicePropertiesOut) {
      // set physical device
      const u32 maxDeviceCount = 8;
      VkPhysicalDevice physicalDevices[maxDeviceCount];
      VkPhysicalDeviceFeatures deviceFeatures[maxDeviceCount];
      VkPhysicalDeviceVulkan11Features features11[maxDeviceCount];
      VkPhysicalDeviceVulkan12Features features12[maxDeviceCount];
      VkPhysicalDeviceVulkan13Features features13[maxDeviceCount];
      VkPhysicalDeviceProperties deviceProperties[maxDeviceCount];
      Vulkan_swap_chain_support_details swapChainSupportDetails[maxDeviceCount];
      u32 scores[maxDeviceCount];
  
      u32 deviceCount = 0;
      vkEnumeratePhysicalDevices(instance,
				 &deviceCount, NULL);
      deviceCount = CLAMP(deviceCount, 0, maxDeviceCount);
      vkEnumeratePhysicalDevices(instance,
				 &deviceCount,
				 physicalDevices);
      const u32 maxExtensions = 256;
      VkExtensionProperties extensionProperties[maxExtensions];
      for (u32 i = 0; i < deviceCount; i++) {
	// make sure it has swapchain support first
	u32 foundExtensionCount;
	vkEnumerateDeviceExtensionProperties(physicalDevices[i],
					     NULL, &foundExtensionCount, NULL);
	foundExtensionCount = CLAMP(foundExtensionCount, 0, maxExtensions);
	vkEnumerateDeviceExtensionProperties(physicalDevices[i],
					     NULL,
					     &foundExtensionCount,
					     extensionProperties);

	u32 found[maxExtensions];
	for (u32 j = 0; j < extensionCount; j++) {
	  found[j] = 0;
	  for (u32 k = 0; k < foundExtensionCount; k++) {
	    if (0 == strcmp(deviceExtensions[j], extensionProperties[k].extensionName)) {
	      found[j] = 1;
	    }
	  }
	}
	u32 checkSum = 0;
	for (u32 j = 0; j < extensionCount; j++) {
	  checkSum += found[j];
	}
	if (checkSum != extensionCount) {
	  scores[i] = 0;
	} else {
	  scores[i] = AssignPhysicalDeviceScore(physicalDevices[i],
						surface,
						apiVersion,
						deviceFeatures + i,
						features11 + i,
						features12 + i,
						features13 + i,
						swapChainSupportDetails + i,
						deviceProperties + i);	
	}
   
      }
    
      u32 indexWithHighestScore = 0;
      u32 highestScore = 0;
      for (u32 i = 0; i < deviceCount; i++) {
	if (highestScore < scores[i]) {
	  indexWithHighestScore = i;
	  highestScore = scores[i];
	}
      }

      if (highestScore == 0) {
	return VK_NULL_HANDLE;
      }

      if (pFeaturesOut) {
	*pFeaturesOut = deviceFeatures[indexWithHighestScore];
      }
      if (pFeatures11Out) {
	*pFeatures11Out = features11[indexWithHighestScore];
      }  
      if (pFeatures12Out) {
	*pFeatures12Out = features12[indexWithHighestScore];
      }
      if (pFeatures12Out) {
	*pFeatures13Out = features13[indexWithHighestScore];
      }  
      if (pSwapChainSupportDetailsOut) {
	*pSwapChainSupportDetailsOut = swapChainSupportDetails[indexWithHighestScore];
      }
      if (pPhysicalDevicePropertiesOut) {
	*pPhysicalDevicePropertiesOut = deviceProperties[indexWithHighestScore];
      }

      return physicalDevices[indexWithHighestScore];
    }
    VKAPI_ATTR VkBool32 VKAPI_CALL
    DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
		  VkDebugUtilsMessageTypeFlagsEXT messageType,
		  const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
		  void* pUserData) {
      bool error = false;
      if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
	error = true;
      } else if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
      } else {
      }
      fprintf(stderr, "VULKAN ::: %s\n", pCallbackData->pMessage);

      if (error) {
      } else {
      }

      return VK_FALSE;
    }
    
    VkDebugUtilsMessengerCreateInfoEXT
    MakeDebugUtilsMessengerCreateInfo() {
      VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo = {};
      debugCreateInfo.sType = VKTYPE(DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT);
      debugCreateInfo.messageSeverity =
	VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
	VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
      debugCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
	VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
	VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
      debugCreateInfo.pfnUserCallback =
	DebugCallback;
      return debugCreateInfo;
    }

    bool
    CheckValidationLayerSupport(const char **validationLayers, const u32 validationLayerCount) {
      const u32 maxLayerCount = 64;
      u32 layerCount = 0;
      VkLayerProperties availableLayers[maxLayerCount];

      vkEnumerateInstanceLayerProperties(&layerCount, NULL);
      layerCount = CLAMP(layerCount, 0, ARRAYCOUNT(availableLayers));
      vkEnumerateInstanceLayerProperties(&layerCount, availableLayers);

      // make sure all required validationlayers are supported
      for (u32 i = 0; i < validationLayerCount; i++) {
	bool found = false;
	for (u32 j = 0; j < layerCount; j++) {
	  if (!strcmp(validationLayers[i], availableLayers[j].layerName)) {
	    found = true;
	    break;
	  }
	}
	if (!found) {
	  return false;
	}
      }

      return true;
    }

    VkResult
      CreateDebugUtilsMessengerEXT(VkInstance instance,
				   const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
				   const VkAllocationCallbacks* pAllocator,
				   VkDebugUtilsMessengerEXT* pDebugMessenger) {
      PFN_vkCreateDebugUtilsMessengerEXT func = (PFN_vkCreateDebugUtilsMessengerEXT)
	vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
      if (NULL != func) {
	return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
      } else {
	return VK_ERROR_EXTENSION_NOT_PRESENT;
      }
    }

    void
    DestroyDebugUtilsMessengerEXT(VkInstance instance,
				    VkDebugUtilsMessengerEXT debugMessenger,
				    const VkAllocationCallbacks* pAllocator) {
      PFN_vkDestroyDebugUtilsMessengerEXT func =
	(PFN_vkDestroyDebugUtilsMessengerEXT)
	vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
      if (NULL != func) {
        func(instance, debugMessenger, pAllocator);
      }
    }
    VkSemaphore
    CreateSemaphore() {
      Ctx *ctx = &gCtx;
      VkSemaphore s;
      VkSemaphoreCreateInfo info = {.sType = VKTYPE(SEMAPHORE_CREATE_INFO)};
      if (VK_SUCCESS != vkCreateSemaphore(ctx->Device, &info, NULL, &s)) {
	return VK_NULL_HANDLE;
      }
      return s;
    }
  
    VkFence
    CreateFence(bool signaled) {
      Ctx *ctx = &gCtx;
      VkFence f;
      VkFenceCreateInfo createInfo = {
	.sType = VKTYPE(FENCE_CREATE_INFO),
	.flags = (signaled ? (u32)VK_FENCE_CREATE_SIGNALED_BIT : 0)
      };
      if (VK_SUCCESS != vkCreateFence(ctx->Device, &createInfo, NULL, &f)) {
	return VK_NULL_HANDLE;
      }
      return f;    
    }
  }
  // Helper namespace ends here

  Pointer&
  operator+=(Pointer &l, const u32 o) {
    Pointer p = l;
    p.SetBufferOffset(l.GetBufferOffset() + o);
    l = p;
    return l;
  }
  Pointer&
  operator-=(Pointer &l, const u32 o) {
    Pointer p = l;
    p.SetBufferOffset(l.GetBufferOffset() - o);
    l = p;
    return l;
  }
  Pointer
  operator+(const Pointer &l, const u32 o) {
    Pointer p = l;
    p += o;
    return p;
  }
    
  Pointer
  operator-(const Pointer &l, const u32 o) {
    Pointer p = l;
    p -= o;
    return p;
  }
  
  Vulkan_queue_family_indices
  Ctx::FindQueueFamilies() {
    // set index CONDITIONALLY (only if not already set)
#define SET_QUEUE_COND(var, target, ind) do {if (var.Indices[target] < 0) var.Indices[target] = ind;} while (0)
    // make container for indices, init to unvalid values
    Vulkan_queue_family_indices indices = {};
    for (u32 i = 0; i < ARRAYCOUNT(indices.Indices); i++) {
      indices.Indices[i] = -1;
    }
    const u32 maxQueueFamilyCount = 32;
    u32 queueFamilyCount = 0;
    VkQueueFamilyProperties queueFamilies[maxQueueFamilyCount];
    vkGetPhysicalDeviceQueueFamilyProperties(this->PhysicalDevice, &queueFamilyCount, NULL);
    queueFamilyCount = CLAMP(queueFamilyCount, 0, maxQueueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(this->PhysicalDevice, &queueFamilyCount, queueFamilies);
    for (u32 i = 0; i < queueFamilyCount; i++) {
      const VkQueueFamilyProperties *properties = queueFamilies + i;
      VkBool32 presentSupport = VK_FALSE;
      vkGetPhysicalDeviceSurfaceSupportKHR(this->PhysicalDevice, i, this->Surface, &presentSupport);
      if (properties->queueFlags & VK_QUEUE_GRAPHICS_BIT &&
	  properties->queueFlags & VK_QUEUE_COMPUTE_BIT &&
	  presentSupport) {
	SET_QUEUE_COND(indices, GENERAL, i);
      }
      if (properties->queueFlags & VK_QUEUE_COMPUTE_BIT &&
	  !(properties->queueFlags & VK_QUEUE_GRAPHICS_BIT)) {
	SET_QUEUE_COND(indices, COMPUTE, i);
      }
      if (properties->queueFlags & VK_QUEUE_TRANSFER_BIT) {
	SET_QUEUE_COND(indices, TRANSFER, i);
      }
      // prioritize this transfer queue, dont set conditionally
      if ((properties->queueFlags & VK_QUEUE_TRANSFER_BIT) &&
	  !(properties->queueFlags & VK_QUEUE_GRAPHICS_BIT ||
	    properties->queueFlags & VK_QUEUE_COMPUTE_BIT)) {
	indices.Indices[VK_QUEUE_TRANSFER_BIT] = i;
      }
    }

    // dedicated compute may not be set. in that case, set it to default
    if (-1 == indices.Indices[COMPUTE]) {
      indices.Indices[COMPUTE] =
	indices.Indices[GENERAL];
    }
#undef SET_QUEUE_COND
    return indices;    
  }

  void
  Ctx::BufferOffsetFromPointer(Pointer p, VkBuffer *pBufferOut, u32 *pOffsetOut) const {
    *pBufferOut = this->BufferMasters[p.GetBufferIndex()].Buffer;
    *pOffsetOut = p.GetBufferOffset();
  }

  void*
  Pointer::Cpu() {
    const Ctx *ctx = &gCtx;
    return (u8*)(ctx->BufferMasters[this->GetBufferIndex()].CpuAddress) + this->GetBufferOffset();
  }
  u64
  Pointer::Gpu() {
    const Ctx *ctx = &gCtx;
    return (u64)(ctx->BufferMasters[this->GetBufferIndex()].GpuAddress) + (u64)this->GetBufferOffset();
  }
  
  i32
  Ctx::Init(const Ctx_init_params *pParams) {
    // Settings
    const char* vulkanExtensions[] = {
      // win32
      "VK_KHR_surface"
#if VK_USE_PLATFORM_WIN32_KHR
      , "VK_KHR_win32_surface"
#endif
#if VULKAN_VALIDATE
      , VK_EXT_DEBUG_UTILS_EXTENSION_NAME
#endif
    };

    const char* vulkanDeviceExtensions[] = {
      VK_KHR_SWAPCHAIN_EXTENSION_NAME,
      "VK_KHR_dynamic_rendering",
      "VK_KHR_timeline_semaphore",
      "VK_EXT_descriptor_indexing",
      "VK_KHR_synchronization2",
      "VK_KHR_shader_draw_parameters",
      "VK_KHR_16bit_storage",
      "VK_KHR_16bit_storage"
    };
    const char* validationLayers[] = {
      "VK_LAYER_KHRONOS_validation"
    };
    bool enableValidationLayers =
#if VULKAN_VALIDATE
      true
#else
      false
#endif
      ;
    
    VkApplicationInfo appInfo  = {
      .sType		= VKTYPE(APPLICATION_INFO),
      .pApplicationName	= "AppName",
      .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
      .pEngineName	= "AppName",
      .engineVersion	= VK_MAKE_VERSION(1, 0, 0),
      .apiVersion	= pParams->VulkanVersion
    };

    u32	platformExtensionCount = (u32)ARRAYCOUNT(vulkanExtensions);
    VkInstanceCreateInfo createInfo = {
      .sType			  = VKTYPE(INSTANCE_CREATE_INFO),
      .pApplicationInfo		  = &appInfo,
      .enabledLayerCount	  = 0,
      .enabledExtensionCount	  = platformExtensionCount,
      .ppEnabledExtensionNames	  = vulkanExtensions,
    };

    // enumerate extensions
    const u32 maxExtensionCount = 256;
    VkExtensionProperties extensions[maxExtensionCount];
    u32 extensionCount = 0;
    vkEnumerateInstanceExtensionProperties(NULL, &extensionCount, NULL);
    extensionCount = CLAMP(extensionCount, 0, maxExtensionCount);
    vkEnumerateInstanceExtensionProperties(NULL, &extensionCount, extensions);

    // VALIDATION LAYER
    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo =
      Helper::MakeDebugUtilsMessengerCreateInfo();  
    if (enableValidationLayers && !Helper::CheckValidationLayerSupport(validationLayers, ARRAYCOUNT(validationLayers))) {
      enableValidationLayers = 0;
    }
    if (enableValidationLayers) {
      createInfo.ppEnabledLayerNames = validationLayers;
      createInfo.enabledLayerCount = ARRAYCOUNT(validationLayers);
      createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debugCreateInfo;
    }

    VkResult result = vkCreateInstance(&createInfo, NULL, &this->Instance);
    if (VK_SUCCESS != result) {
      // Display error message box
      Util::ErrMsgBox("Could not create vulkan instance!");
      return -1;
    }
    if (enableValidationLayers) {
      debugCreateInfo =
	Helper::MakeDebugUtilsMessengerCreateInfo();  
      if (VK_SUCCESS !=
	  Helper::CreateDebugUtilsMessengerEXT(this->Instance,
					       &debugCreateInfo,
					       NULL,
					       &this->DebugMessenger)) {
	// display error message box
	Util::ErrMsgBox("Could not initialize vulkan debug messenger!");
	return -1;
      }    
    }

    // Surface
    VkWin32SurfaceCreateInfoKHR surfaceCreateInfo  = {
      .sType     = VKTYPE(WIN32_SURFACE_CREATE_INFO_KHR),
      .hinstance = pParams->WindowInstance,
      .hwnd      = pParams->WindowHandle
    };
  if (VK_SUCCESS != vkCreateWin32SurfaceKHR(this->Instance,
					    &surfaceCreateInfo,
					    NULL,
					    &this->Surface)) {
    // display error message box;
    Util::ErrMsgBox("Could not create vulkan surface!");
    return -1;
  }

  // Device
  VkPhysicalDeviceFeatures deviceFeatures = {};
  VkPhysicalDeviceVulkan11Features features11 = {};
  VkPhysicalDeviceVulkan12Features features12 = {};
  VkPhysicalDeviceVulkan13Features features13 = {};
  Helper::Vulkan_swap_chain_support_details swapChainSupportDetails = {};
  VkPhysicalDeviceProperties deviceProperties;
  const u32 deviceExtensionCount = ARRAYCOUNT(vulkanDeviceExtensions);
  VkPhysicalDevice physicalDevice =
    Helper::SelectBestPhysicalDevice(this->Instance, this->Surface,
				     pParams->VulkanVersion,
				     vulkanDeviceExtensions, deviceExtensionCount,
				     &deviceFeatures, &features11, &features12, &features13,
				     &swapChainSupportDetails, &deviceProperties);
  if (VK_NULL_HANDLE == physicalDevice) {
    Util::ErrMsgBox("Could not find a suitable physical device!");
    return -1;
  }

  this->VulkanVersion = pParams->VulkanVersion;
  this->PhysicalDevice = physicalDevice;
  this->PhysicalDeviceLimits = deviceProperties.limits;
  this->SwapchainSupportDetails = swapChainSupportDetails;

  Vulkan_queue_family_indices indices = this->FindQueueFamilies();
  // require all indices to be supported
  for (u32 i = 0; i < ARRAYCOUNT(indices.Indices); i++) {
    if (indices.Indices[i] < 0) {
      Util::ErrMsgBox("Unsupported queue types found!");
      return -1;
    }
  }

  const i32 effectiveIndicesMaxCount = ARRAYCOUNT(indices.Indices);
  i32 effectiveIndices[effectiveIndicesMaxCount];
  i32 effectiveCount = 0;  
  for (i32 i = 0; i < effectiveIndicesMaxCount; i++) {
    effectiveIndices[i] = -1;
  }
  for (i32 i = 0; i < effectiveIndicesMaxCount; i++) {
    bool found = false;
    for (i32 j = 0; j < effectiveCount; j++) {
      if (indices.Indices[i] == effectiveIndices[j]) {
	found = true;
	break;
      }
    }
    if (!found) {
      effectiveIndices[effectiveCount] = indices.Indices[i];
      effectiveCount++;
    }
  }

  float queuePriority = 1.0f;
  VkDeviceQueueCreateInfo queueCreateInfo[ARRAYCOUNT(indices.Indices)];
  for (i32 i = 0; i < effectiveCount; i++) {
    queueCreateInfo[i]	= (VkDeviceQueueCreateInfo) {
      .sType		= VKTYPE(DEVICE_QUEUE_CREATE_INFO),
      .queueFamilyIndex = (u32)effectiveIndices[i],
      .queueCount	= 1,
      .pQueuePriorities = &queuePriority      
    };
  }

  deviceFeatures.samplerAnisotropy = VK_TRUE;
  features12.timelineSemaphore = VK_TRUE;
  features12.pNext = &features13;
  features13.pNext = &features11;
  features11.pNext = NULL;
  VkDeviceCreateInfo deviceCreateInfo = {
    .sType		     = VKTYPE(DEVICE_CREATE_INFO),
    .pNext		     = &features12,
    .queueCreateInfoCount    = (u32)effectiveCount,
    .pQueueCreateInfos	     = queueCreateInfo,
    .enabledExtensionCount   = ARRAYCOUNT(vulkanDeviceExtensions),
    .ppEnabledExtensionNames = vulkanDeviceExtensions,
    .pEnabledFeatures	     = &deviceFeatures,
  };

  if (VK_SUCCESS != vkCreateDevice(this->PhysicalDevice,
				   &deviceCreateInfo, NULL,
				   &this->Device)) {
    Util::ErrMsgBox("Could not create logical device!");
    return -1;
  }

  this->QueueFamilyIndices	    = indices;
  this->DistinctQueueFamilyCount = effectiveCount;
  this->WindowHandle = pParams->WindowHandle;
  for (u32 i = 0; i < ARRAYCOUNT(indices.Indices); i++) {
    vkGetDeviceQueue(this->Device, indices.Indices[i], 0,
		     &this->Queues[i]);    
  }
  
  if (VK_SUCCESS != this->InitSwapchain(false)){
    Util::ErrMsgBox("Could not initiate swapchain!");
    return -1;
  }

  // (todo) get memory info
  
  // Create global pipeline layout
  VkDescriptorSetLayout dsLayouts[DESC_SET_COUNT];
  VkDescriptorBindingFlagsEXT descBindingFlags =
    VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT_EXT |
    VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT_EXT |
    VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT_EXT |
    VK_DESCRIPTOR_BINDING_UPDATE_UNUSED_WHILE_PENDING_BIT_EXT;
    
  VkDescriptorSetLayoutBindingFlagsCreateInfoEXT bindingFlags = {
    .sType = VKTYPE(DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO_EXT),
    .bindingCount = 1,
    .pBindingFlags = &descBindingFlags
  };  
  VkDescriptorSetLayoutBinding sampledImageBindingLayout = {
    .binding = 0,
    .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
    .descriptorCount = pParams->SampleImageDescriptorCount, 
    .stageFlags = VK_SHADER_STAGE_ALL
  };
  VkDescriptorSetLayoutBinding storageImageBindingLayout = {
    .binding = 0,
    .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
    .descriptorCount = pParams->StorageImageDescriptorCount,
    .stageFlags = VK_SHADER_STAGE_ALL
  };
  VkDescriptorSetLayoutBinding samplerBindingLayout = {
    .binding = 0,
    .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
    .descriptorCount = MAX_SAMPLER_COUNT,
    .stageFlags = VK_SHADER_STAGE_ALL
  };  
  VkDescriptorSetLayoutCreateInfo setLayoutCi = {
    .sType = VKTYPE(DESCRIPTOR_SET_LAYOUT_CREATE_INFO),
    .pNext = &bindingFlags,
    .flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT,
    .bindingCount = 1,
    .pBindings = &sampledImageBindingLayout
  };

  vkCreateDescriptorSetLayout(this->Device,
			      &setLayoutCi,
			      0,
			      &dsLayouts[0]);
  setLayoutCi.pBindings = &storageImageBindingLayout;
  vkCreateDescriptorSetLayout(this->Device,
			      &setLayoutCi,
			      0,
			      &dsLayouts[1]);
  setLayoutCi.pBindings = &samplerBindingLayout;
  vkCreateDescriptorSetLayout(this->Device,
			      &setLayoutCi,
			      0,
			      &dsLayouts[2]);
  for (u32 i = 0; i < ARRAYCOUNT(dsLayouts); i++)
    this->GlobalDescriptorSetLayouts[i] = dsLayouts[i];
  
  VkPushConstantRange pcr = {
    VK_SHADER_STAGE_ALL,
    0, 128
  };

  VkPipelineLayoutCreateInfo plci = {
    .sType = VKTYPE(PIPELINE_LAYOUT_CREATE_INFO),
    .setLayoutCount = DESC_SET_COUNT,
    .pSetLayouts = dsLayouts,
    .pushConstantRangeCount = 1,
    .pPushConstantRanges = &pcr
  };

  if (VK_SUCCESS != vkCreatePipelineLayout(this->Device, &plci, 0, &this->GlobalPipelineLayout)) {
    Util::ErrMsgBox("Could not create global pipeline layout!");
    return -1;
  }

  u32 descriptorCounts[3] = {pParams->SampleImageDescriptorCount, pParams->StorageImageDescriptorCount, 256};
  // create descriptor pool
  VkDescriptorPoolSize poolSizes[] = {
    {
      .type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
      .descriptorCount = descriptorCounts[0]
    },
    {
      .type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
      .descriptorCount = descriptorCounts[1]
    },
    {
      .type = VK_DESCRIPTOR_TYPE_SAMPLER,
      .descriptorCount = descriptorCounts[2]
    }
  };
  VkDescriptorPoolCreateInfo poolInfo = {
    .sType = VKTYPE(DESCRIPTOR_POOL_CREATE_INFO),
    .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT | VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT,
    .maxSets = DESC_POOL_MAX_SETS,
    .poolSizeCount = DESC_SET_COUNT,
    .pPoolSizes = poolSizes
  };
  if (VK_SUCCESS != vkCreateDescriptorPool(this->Device, &poolInfo, 0, &this->GlobalDescriptorPool)) {
    Util::ErrMsgBox("Could not create global descriptor pool!");
    return -1;
  }

  // descriptor sets
  VkDescriptorSetAllocateInfo setAllocInfo = {
    .sType = VKTYPE(DESCRIPTOR_SET_ALLOCATE_INFO),
    .descriptorPool = this->GlobalDescriptorPool,
    .descriptorSetCount = DESC_SET_COUNT,
    .pSetLayouts = this->GlobalDescriptorSetLayouts
  };
  VkDescriptorSetVariableDescriptorCountAllocateInfo varInfo = {
    .sType = VKTYPE(DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO),
    .descriptorSetCount = DESC_SET_COUNT,
    .pDescriptorCounts = descriptorCounts
  };
  setAllocInfo.pNext = &varInfo;
  if (VK_SUCCESS != vkAllocateDescriptorSets(this->Device, &setAllocInfo, this->GlobalDescriptorSets)) {
    Util::ErrMsgBox("Could not create global descriptor sets!");
    return -1;
  }
  
  // semaphores, fences
  for (u32 i = 0; i < 2; i++) {
    this->RenderCompleteSemaphores[i] = Helper::CreateSemaphore();
    this->PresentCompleteSemaphores[i] = Helper::CreateSemaphore();
    this->ImageWaitFences[i] = Helper::CreateFence(true);
  }

  // command pool
  // allocate command buffers
  for (u32 i = 0; i < MAX_GPU_QUEUE_TYPE_COUNT; i++) {
    VkCommandPoolCreateInfo cpci = {
      .sType = VKTYPE(COMMAND_POOL_CREATE_INFO),
      .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
      .queueFamilyIndex = (u32)this->QueueFamilyIndices.Indices[i]
    };
    VkCommandBuffer cmdbuf[MAX_COMMAND_BUFFER_COUNT];
    vkCreateCommandPool(this->Device, &cpci, NULL, &this->CommandPools[i]);
    VkCommandBufferAllocateInfo ai = {
      .sType = VKTYPE(COMMAND_BUFFER_ALLOCATE_INFO),
      .commandPool = this->CommandPools[i],
      .commandBufferCount = ARRAYCOUNT(cmdbuf)
    };
    vkAllocateCommandBuffers(this->Device, &ai, cmdbuf);
    Util::Stack<VkCommandBuffer> *cmdBufStack = &this->CommandBufferStacks[i];
    Util::Platform_memory_alloc allocator;
    cmdBufStack->Init(&allocator, ARRAYCOUNT(cmdbuf));
    for (u32 j = 0; j < ARRAYCOUNT(cmdbuf); j++) {
      cmdBufStack->Push(&cmdbuf[j]);
    }
  }

  // timeline semaphore
  VkSemaphoreCreateInfo smci = {
    .sType = VKTYPE(SEMAPHORE_CREATE_INFO)
  };
  VkSemaphoreTypeCreateInfoKHR smtci = {
    VKTYPE(SEMAPHORE_TYPE_CREATE_INFO_KHR)
  };
  smtci.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE_KHR;
  smtci.initialValue = 0;
  smci.pNext = &smtci;
  vkCreateSemaphore(this->Device, &smci, NULL, &this->TimelineSemaphore);

  // memory arena
  this->ScratchArena.Init(MEGA(1));

  // buffers
  // for now, we create one host-accessible and one purely device-local
  {
    VkBufferCreateInfo bci = {
      .sType = VKTYPE(BUFFER_CREATE_INFO),
      .size = MEGA(64),
      .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
    };
    VkBuffer hostAccessibleBuffer, deviceBuffer;
    vkCreateBuffer(this->Device, &bci, 0, &hostAccessibleBuffer);
    bci.size = MEGA(512);
    vkCreateBuffer(this->Device, &bci, 0, &deviceBuffer);
    VkBuffer buffers[2] = {hostAccessibleBuffer, deviceBuffer};
    VkMemoryPropertyFlags necessaryFlags[2] = {
      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    };
    VkDeviceMemory memories[2];

    // bind memory
    VkPhysicalDeviceMemoryProperties physicalMemProperties;
    vkGetPhysicalDeviceMemoryProperties(this->PhysicalDevice, &physicalMemProperties);
    VkMemoryRequirements memRequirements[2];
    Util::Stdlib_memory_alloc alloc;
    for (u32 i = 0; i < ARRAYCOUNT(buffers); i++) {
      vkGetBufferMemoryRequirements(this->Device, buffers[i], &memRequirements[i]);
      i32 heapIndex = -1;
      i32 typeIndex = -1;
      // find appropriate memory type index
      for (u32 t = 0; t < physicalMemProperties.memoryTypeCount; t++) {
	if ((physicalMemProperties.memoryTypes[t].propertyFlags & necessaryFlags[i]) == necessaryFlags[i]) {
	  heapIndex = (i32)physicalMemProperties.memoryTypes[t].heapIndex;
	  typeIndex = (i32)t;
	  break;
	}
      }

      if (heapIndex < 0 || typeIndex < 0) {
	Util::ErrMsgBox("Could not find any appropriate memory type on GPU!");
	return -1;
      }

      VkMemoryAllocateInfo allocInfo {VKTYPE(MEMORY_ALLOCATE_INFO)};
      allocInfo.allocationSize = memRequirements[i].size;
      allocInfo.memoryTypeIndex = (u32)typeIndex;
      VkMemoryAllocateFlagsInfo allocFlagsInfo = {
	.sType = VKTYPE(MEMORY_ALLOCATE_FLAGS_INFO),
	.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT
      };
      allocInfo.pNext = &allocFlagsInfo;
      vkAllocateMemory(this->Device, &allocInfo, NULL, &memories[i]);
      vkBindBufferMemory(this->Device, buffers[i], memories[i], 0);
      VkBufferDeviceAddressInfo addressInfo = {
	.sType = VKTYPE(BUFFER_DEVICE_ADDRESS_INFO),
	.buffer = buffers[i]
      };
      this->BufferMasters[i] = {
	.Buffer = buffers[i],
	.GpuAddress = vkGetBufferDeviceAddress(this->Device, &addressInfo),
	.Allocator = Memory::Virtual_allocator::Create(&alloc, pParams->BufferMaxAllocationCount, allocInfo.allocationSize),
	.MemoryTypeIndex = (u32)typeIndex
      };
      // map first only
      if (0 == i)
	vkMapMemory(this->Device, memories[i], 0, allocInfo.allocationSize, 0, &this->BufferMasters[i].CpuAddress);
    }
    // make image memory
    for (u32 i = 0; i < ARRAYCOUNT(this->ImageMasters); i++) {
      VkMemoryAllocateInfo allocInfo {VKTYPE(MEMORY_ALLOCATE_INFO)};
      allocInfo.allocationSize = MEGA(512);
      allocInfo.memoryTypeIndex = (u32)this->BufferMasters[GPU_PRIVATE].MemoryTypeIndex;
      vkAllocateMemory(this->Device, &allocInfo, NULL, &this->ImageMasters[i].Memory);
      this->ImageMasters[i].Allocator = Memory::Virtual_allocator::Create(&alloc, pParams->ImageMaxAllocationCount, allocInfo.allocationSize);
    }

    // image pool
    this->ImagePool.Init(&alloc, pParams->ImageMaxAllocationCount);

    // kernel pool
    this->KernelPool.Init(&alloc, 256);

    // descriptor pools
    for (u32 i = 0; i < ARRAYCOUNT(this->DescriptorArrays); i++) {
      this->DescriptorArrays[i].Init(&alloc, descriptorCounts[i]);
    }
  }

  // timestamp query pool
  this->TimestampCapacity = 1024;
  for (u32 i = 0; i < 2; i++) {
    VkQueryPoolCreateInfo qpi{};
    qpi.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
    qpi.queryType = VK_QUERY_TYPE_TIMESTAMP;
    qpi.queryCount = this->TimestampCapacity;
    vkCreateQueryPool(this->Device, &qpi, NULL, &this->TimestampQueryPools[i]);
    this->TimestampBuffers[i] = (Timestamp*)(malloc(sizeof (Timestamp) * this->TimestampCapacity));
    this->TimestampCounters[i] = 0;
  }

  // default sampler
  VkSamplerCreateInfo samplerCi = {
    .sType = VKTYPE(SAMPLER_CREATE_INFO),
    .magFilter = VK_FILTER_LINEAR,
    .minFilter = VK_FILTER_LINEAR,
    .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
    .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
    .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
    .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
    .anisotropyEnable = VK_TRUE,
    .maxAnisotropy = this->PhysicalDeviceLimits.maxSamplerAnisotropy
  };
  vkCreateSampler(this->Device, &samplerCi, 0, &this->DefaultSampler);

  VkDescriptorImageInfo smplDsInfo = {
    .sampler = this->DefaultSampler
  };
  Util::Handle<Util::Any> defaultSamplerHandle = this->DescriptorArrays[DESC_INDEX_SAMPLER].Add();
  VkWriteDescriptorSet writeInfo = {
    .sType = VKTYPE(WRITE_DESCRIPTOR_SET),
    .dstSet = this->GlobalDescriptorSets[DESC_INDEX_SAMPLER],
    .dstBinding = 0,
    .dstArrayElement = defaultSamplerHandle.Index,
    .descriptorCount = 1,
    .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
    .pImageInfo = &smplDsInfo
  };
  vkUpdateDescriptorSets(this->Device, 1, &writeInfo, 0, NULL);

  // bump allocators
  for (u32 i = 0; i < 2; i++) { // type
    for (u32 j = 0; j < 2; j++) { // frame index
      u32 siz = ALIGN(pParams->BumpAllocatorSize, BUMP_ALLOC_ALIGNMENT);
      this->BumpAllocs[i][j].Base = Malloc(siz, (Buffer_type)i);
      this->BumpAllocs[i][j].Size = siz;
      this->BumpAllocs[i][j].BytesPushed = 0;      
    }
  }
  
  return 0;
  }
  // Init end

  
  void
  Ctx::Free() {
    this->ScratchArena.Free();
    // Not actually needed, but freeing just in case
    for (u32 i = 0; i < MAX_GPU_QUEUE_TYPE_COUNT; i++) {
      Util::Stack<VkCommandBuffer> *cmdBufStack = &this->CommandBufferStacks[i];
      Util::Platform_memory_alloc allocator;
      cmdBufStack->Free(&allocator);
    }
    this->ScratchArena.Free();
    {
      Util::Stdlib_memory_alloc alloc;
      // (todo) iterate over all images and free
      this->ImagePool.Free(&alloc);
      this->KernelPool.Free(&alloc);
      for (u32 i = 0; i < ARRAYCOUNT(this->DescriptorArrays); i++) {
	this->DescriptorArrays[i].Free(&alloc);
      }
    }
    
  }

  uv2
  GetSwapchainDim() {
    const Ctx *ctx = &gCtx;
    return { ctx->SwapchainExtent.width, ctx->SwapchainExtent.height };
  }

  Cmd_buf
  BeginCmdBuf(const Queue_type queueTypeEnum) {
    Ctx *ctx = &gCtx;
    Scoped_lock lock(&ctx->Mutexes[Ctx::MutexIndices::BEGIN_CMD_BUF]);
    Util::Stack<VkCommandBuffer> *s = &ctx->CommandBufferStacks[queueTypeEnum];
    VkCommandBuffer b = *s->Pop();
    VkCommandBufferBeginInfo bi = {
      .sType = VKTYPE(COMMAND_BUFFER_BEGIN_INFO),
      .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };
    VkResult r = vkBeginCommandBuffer(b, &bi);
    ASSERT(VK_SUCCESS == r, "Could not begin command buffer!");
    // pre-emptively bind all descriptor sets
    return {.CommandBuffer = b, .WaitTimeline = 0, .QueueTypeEnum = queueTypeEnum};
  }

  // Check all currently executing command buffers to see if any have finished.
  // If so, make them available to be used again.
  void
  PollResources() {
    Ctx *ctx = &gCtx;
    // We delete as we iterate, so we need to keep separate state of amount processed
    // and current element.
    u32 processed = 0;
    u32 i = 0;
    u32 count = ctx->ExecutingCommandBuffers.Count;
    while (processed++ < count) {
      Cmd_buf *c = &ctx->ExecutingCommandBuffers[i];
      Util::Stack<VkCommandBuffer> *s = &ctx->CommandBufferStacks[c->QueueTypeEnum];
      bool popped = false;
      // If the command buffer was for a swapchain, WaitTimeline is just the
      // frame index it was submitted.
      if (c->ForSwapchain && (u32)c->WaitTimeline == ctx->CurrentFrameIndex) {
	s->Push(&c->CommandBuffer);
	ctx->ExecutingCommandBuffers.Remove(i);
	popped = true;
      } else if (!c->ForSwapchain) {
	if (CheckTimeline(c->WaitTimeline)) {
	  s->Push(&c->CommandBuffer);
	  ctx->ExecutingCommandBuffers.Remove(i);
	  popped = true;
	}
      }
      // If we popped, the element at i will now be the last element of the volatile array
      // before popping, so we process it too.
      if (!popped)
	i++;
    }
  }

  VkResult
  Ctx::InitSwapchain(bool isRestart) {
    if (isRestart) {
      vkDeviceWaitIdle(this->Device);
      this->SwapchainSupportDetails = Helper::QuerySwapchainSupport(this->PhysicalDevice, this->Surface);
    }
    // Check if we can early out (if window is minimized)
    RECT rect;
    if (false == GetWindowRect(this->WindowHandle, &rect)) {
      return VK_ERROR_UNKNOWN;
    }
    u32 windowWidth  = rect.right - rect.left;
    u32 windowHeight = rect.bottom - rect.top;
    // window was minimized
    if (0 == windowWidth || 0 == windowHeight) {
      this->SwapchainExtent = {};
      return VK_SUCCESS;
    }
    // default format, present mode
    VkSurfaceFormatKHR surfaceFormat = this->SwapchainSupportDetails.Formats[0];
    VkPresentModeKHR swapchainPresentMode = this->SwapchainSupportDetails.PresentModes[0];
    VkExtent2D swapExtent;
    VkFormat swapchainFormat = VK_FORMAT_R8G8B8A8_SRGB;
    // find supported ones
    for (u32 i = 0; i < this->SwapchainSupportDetails.FormatCount; i++) {
      VkSurfaceFormatKHR *format = this->SwapchainSupportDetails.Formats + i;
      if (swapchainFormat == format->format &&
	  VK_COLOR_SPACE_SRGB_NONLINEAR_KHR == format->colorSpace) {
	surfaceFormat = *format;
	break;
      }
    }
    for (u32 i = 0; i < this->SwapchainSupportDetails.PresentModeCount; i++) {
      VkPresentModeKHR presentMode = this->SwapchainSupportDetails.PresentModes[i];
      if (VK_PRESENT_MODE_MAILBOX_KHR == presentMode) {
	swapchainPresentMode = presentMode;
	break;
      }
    }
    VkSurfaceCapabilitiesKHR surfaceCapabilities =
      this->SwapchainSupportDetails.Capabilities;
    if (surfaceCapabilities.currentExtent.width != UINT32_MAX) {
      swapExtent = surfaceCapabilities.currentExtent;
    } else {
      swapExtent.width = CLAMP(windowWidth, surfaceCapabilities.minImageExtent.width,
				surfaceCapabilities.maxImageExtent.width);
      swapExtent.height = CLAMP(windowHeight, surfaceCapabilities.minImageExtent.height,
				 surfaceCapabilities.maxImageExtent.height);    
    }
    if (0 == swapExtent.width || 0 == swapExtent.height) {
      this->SwapchainExtent = {};
      return VK_SUCCESS;
    }
    u32 imageCount = 2;//surfaceCapabilities.minImageCount + 1;
    if (surfaceCapabilities.maxImageCount > 0 &&
	imageCount > surfaceCapabilities.maxImageCount) {
      imageCount = surfaceCapabilities.maxImageCount;
    }

    VkSwapchainKHR swapChain;
    VkSwapchainCreateInfoKHR swapChainCreateInfo = {
      .sType = VKTYPE(SWAPCHAIN_CREATE_INFO_KHR),
      .surface = this->Surface,
      .minImageCount = imageCount,
      .imageFormat = surfaceFormat.format,
      .imageColorSpace = surfaceFormat.colorSpace,
      .imageExtent = swapExtent,
      .imageArrayLayers = 1,
      .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
      .preTransform = surfaceCapabilities.currentTransform,
      .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
      .presentMode = swapchainPresentMode,
      .clipped = VK_TRUE,
      .oldSwapchain = isRestart ? this->Swapchain : VK_NULL_HANDLE
    };

    u32 queueFamilyIndices[] = {
      (u32)this->QueueFamilyIndices.Indices[GENERAL],
      (u32)this->QueueFamilyIndices.Indices[TRANSFER],
      (u32)this->QueueFamilyIndices.Indices[COMPUTE]
    };
    // (todo) confirm that this works!
    if (this->QueueFamilyIndices.Indices[GENERAL] ==
	this->QueueFamilyIndices.Indices[COMPUTE]) {
      swapChainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    } else {
      swapChainCreateInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
      swapChainCreateInfo.queueFamilyIndexCount = 2;
      swapChainCreateInfo.pQueueFamilyIndices = queueFamilyIndices;
    }

    if (VK_SUCCESS != vkCreateSwapchainKHR(this->Device,
					   &swapChainCreateInfo,
					   NULL,
					   &swapChain)) {
      return VK_ERROR_UNKNOWN;
    }

    // Destroy currently existing resources if we're restarting
    if (isRestart) {
      for (u32 i = 0; i < 2; i++) {
	if (this->SwapchainImageViews[i] != VK_NULL_HANDLE) {
	  vkDestroyImageView(this->Device, this->SwapchainImageViews[i], NULL);
	}
      }
      if (this->Swapchain) {
	vkDestroySwapchainKHR(this->Device, this->Swapchain, NULL);
      }
    }
    
    this->Swapchain = swapChain;
    this->SwapchainExtent = swapExtent;
    this->SwapchainFormat = swapchainFormat;
    vkGetSwapchainImagesKHR(this->Device, this->Swapchain,
			    &imageCount, NULL);
    imageCount = CLAMP(imageCount, 0, 2);
    this->SwapchainImageCount = imageCount;
    vkGetSwapchainImagesKHR(this->Device, this->Swapchain,
			  &imageCount, this->SwapchainImages);
    this->SwapchainImageFormat = surfaceFormat.format;
    for (u32 i = 0; i < this->SwapchainImageCount; i++) {
      VkImageViewCreateInfo info = {
	.sType = VKTYPE(IMAGE_VIEW_CREATE_INFO),
	.image = this->SwapchainImages[i],
	.viewType = VK_IMAGE_VIEW_TYPE_2D,
	.format = this->SwapchainImageFormat,
	.subresourceRange = {
	  .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
	  .levelCount = 1,
	  .layerCount = 1
	}
      };
      VkResult r = vkCreateImageView(this->Device, &info, NULL, this->SwapchainImageViews + i);
      if (VK_SUCCESS != r) {
	return r;
      }
    }
    return VK_SUCCESS;
  }

  // Return -1 on a resize, otherwise returns image index
  i32
  AcquireSwapchain() {
    Ctx *ctx = &gCtx;
    if (ctx->SwapchainExtent.width == 0 || ctx->SwapchainExtent.height == 0)
      return -2;
    u32 currentFrame = ctx->CurrentFrameIndex;
    VkFence currentFence = ctx->ImageWaitFences[currentFrame];
    vkWaitForFences(ctx->Device, 1, &currentFence, VK_TRUE, UINT64_MAX);
    u32 imageIndex;
    VkResult res = vkAcquireNextImageKHR(ctx->Device, ctx->Swapchain, UINT64_MAX, ctx->PresentCompleteSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);
    // out of date, dont render
    if (VK_ERROR_OUT_OF_DATE_KHR == res) {
      return -1;
      // catastrophic failure
    } else if (res != VK_SUCCESS && res != VK_SUBOPTIMAL_KHR) {
      const char *msg = "GPU could not acquire swapchain image!";
      Util::ErrMsgBox(msg);
      ASSERT(0, msg);
      return -1;
    }

    vkResetFences(ctx->Device, 1, &currentFence);
    ctx->BumpAllocs[0][currentFrame].Reset();
    ctx->BumpAllocs[1][currentFrame].Reset();
    const u32 timestampCount = ctx->TimestampCounters[currentFrame];
    if (timestampCount) {
      vkGetQueryPoolResults(ctx->Device, ctx->TimestampQueryPools[currentFrame], 0, timestampCount, timestampCount * sizeof (Timestamp), ctx->TimestampBuffers[currentFrame], sizeof (Timestamp), VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);
    }
    for (u32 i = 0; i < timestampCount; i++) {
      Timestamp t = ctx->TimestampBuffers[currentFrame][i];
      *t.UserPointer = t.Value;
    }
    ctx->TimestampCounters[currentFrame] = 0;
    // assume swapchain will later be converted to clr attach optimal
    return (i32)imageIndex;
  }

  Image
  GetSwapchainImage(i32 imageIndex) {
    Ctx *ctx = &gCtx;
    return {
      .Image = ctx->SwapchainImages[imageIndex],
      .View = ctx->SwapchainImageViews[imageIndex],
      .Format = ctx->SwapchainFormat,
      .Layout = VK_IMAGE_LAYOUT_UNDEFINED,
      .Width = (u16)ctx->SwapchainExtent.width,
      .Height = (u16)ctx->SwapchainExtent.height,
      .Depth = 1,
      .MipCount = 1,
      .LayerCount = 1
    };
  }
  
  // Returns -1 on a resize
  i32
  PresentSwapchain(i32 imageIndex, Cmd_buf *pCmdBufs, const u32 cmdBufCount) {
    VkResult res;
    Ctx *ctx = &gCtx;
    Scratch scratch(&ctx->ScratchArena);
    VkCommandBuffer *buffers = scratch.Push<VkCommandBuffer>(cmdBufCount);
    for (u32 i = 0; i < cmdBufCount; i++) {
      buffers[i] = pCmdBufs[i].CommandBuffer;
      vkEndCommandBuffer(buffers[i]);
    }
    u32 imgIndex = (u32)imageIndex;
    u32 currentFrame = ctx->CurrentFrameIndex;
    VkFence currentFence = ctx->ImageWaitFences[currentFrame];
    VkSemaphore presentSemaphore = ctx->PresentCompleteSemaphores[currentFrame];
    VkSwapchainKHR swapchain = ctx->Swapchain;
    VkQueue generalQueue = ctx->Queues[GENERAL];
    // assume swapchain is in layout present
    VkPipelineStageFlags waitStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo submitInfo = {
      .sType = VKTYPE(SUBMIT_INFO),
      .waitSemaphoreCount = 1,
      .pWaitSemaphores = &presentSemaphore,
      .pWaitDstStageMask = &waitStageMask,
      .commandBufferCount = cmdBufCount,
      .pCommandBuffers = buffers,
      .signalSemaphoreCount = 1,
      .pSignalSemaphores = &ctx->RenderCompleteSemaphores[imgIndex],
    };
    {
      Scoped_lock lock(&ctx->Mutexes[Ctx::MutexIndices::QUEUE_SUBMIT]);
      res = vkQueueSubmit(generalQueue, 1, &submitInfo, currentFence);
      for (u32 i = 0; i < cmdBufCount; i++) {
	pCmdBufs[i].ForSwapchain = 1;
	pCmdBufs[i].WaitTimeline = ctx->CurrentFrameIndex;
	ctx->ExecutingCommandBuffers.Add(&pCmdBufs[i]);
      }
    }
    VkPresentInfoKHR presentInfo = {};
    presentInfo.sType = VKTYPE(PRESENT_INFO_KHR);
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &ctx->RenderCompleteSemaphores[imageIndex];
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &swapchain;
    presentInfo.pImageIndices = &imgIndex;
    {
      Scoped_lock lock(&ctx->Mutexes[Ctx::MutexIndices::QUEUE_SUBMIT]);
      res = vkQueuePresentKHR(generalQueue, &presentInfo);
    }
    if (VK_ERROR_OUT_OF_DATE_KHR == res || VK_SUBOPTIMAL_KHR == res) {
      return -1;
    } else if (res != VK_SUCCESS) {
      const char *msg = "GPU could not present swapchain image!";
      Util::ErrMsgBox(msg);
      ASSERT(0, msg);
      return -1;
    }
    ctx->CurrentFrameIndex++;
    ctx->CurrentFrameIndex %= 2;
    return 0;
  }

  void
  ReinitSwapchain() {
    Ctx *ctx = &gCtx;
    ctx->InitSwapchain(true);
  }

  Timeline
  Submit(Cmd_buf *pCmdBufs, const u32 count, const Timeline waitTimeline) {
    Ctx *ctx = &gCtx;
    if (count < 1) return ctx->GlobalTimeline;
    Scratch scratch(&ctx->ScratchArena);
    VkCommandBuffer *buffers = scratch.Push<VkCommandBuffer>(count);
    for (u32 i = 0; i < count; i++) {
      buffers[i] = pCmdBufs[i].CommandBuffer;
      vkEndCommandBuffer(buffers[i]);
    }
    VkTimelineSemaphoreSubmitInfoKHR timelineInfo = {
      .sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO_KHR
    };
    VkPipelineStageFlags waitStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    VkSubmitInfo submitInfo = {
      .sType = VKTYPE(SUBMIT_INFO),
      .pWaitDstStageMask = &waitStageMask,
      .commandBufferCount = count,
      .pCommandBuffers = buffers,
      .signalSemaphoreCount = 1,
      .pSignalSemaphores = &ctx->TimelineSemaphore
    };
    if (waitTimeline > 0) {
      timelineInfo.waitSemaphoreValueCount = 1;
      timelineInfo.pWaitSemaphoreValues = &ctx->GlobalTimeline;
      submitInfo.pWaitSemaphores = &ctx->TimelineSemaphore;
      submitInfo.waitSemaphoreCount = 1;
      submitInfo.pNext = &timelineInfo;
    }
    {
      Scoped_lock lock(&ctx->Mutexes[Ctx::MutexIndices::QUEUE_SUBMIT]);
      timelineInfo.signalSemaphoreValueCount = 1;
      timelineInfo.pSignalSemaphoreValues = &ctx->GlobalTimeline;
      submitInfo.pNext = &timelineInfo;
      // Set timeline to what will be expected from the GPU after execution completes.
      ctx->GlobalTimeline++;
      for (u32 i = 0; i < count; i++) {
	pCmdBufs[i].ForSwapchain = 0;
	pCmdBufs[i].WaitTimeline = ctx->GlobalTimeline;
	ctx->ExecutingCommandBuffers.Add(&pCmdBufs[i]);
      }
      u32 queueIndex = ctx->QueueFamilyIndices.Indices[pCmdBufs[0].QueueTypeEnum];
      vkQueueSubmit(ctx->Queues[queueIndex], 1, &submitInfo, NULL);
    }
    return ctx->GlobalTimeline;
  }

  bool
  CheckTimeline(const Timeline timeline) {
    Ctx *ctx = &gCtx;
    if (timeline <= ctx->LastConfirmedTimeline)
      return true;
    VkSemaphoreWaitInfo info = {
      .sType = VKTYPE(SEMAPHORE_WAIT_INFO),
      .semaphoreCount = 1,
      .pSemaphores = &ctx->TimelineSemaphore,
      .pValues = &timeline
    };
    VkResult res = vkWaitSemaphores(ctx->Device, &info, 0);
    if (VK_SUCCESS == res) {
      ctx->LastConfirmedTimeline = MAX(ctx->LastConfirmedTimeline, timeline);
      return true;
    } else {
      return false;
    }
  }

  Pointer
  Malloc(const u32 size, const Buffer_type type) {
    Ctx *ctx = &gCtx;
    Ctx::Buffer_master *m = &ctx->BufferMasters[type];
    OffsetAllocation a = m->Allocator->Alloc<u8>(ALIGN(size, 16));
    ASSERT(a.Offset != UINT32_MAX, "GPU::Malloc failed!");
    return Pointer::CreateFromOffsetAllocation(type, a);
  }
  
  void
  Free(Pointer p) {
    Ctx *ctx = &gCtx;
    Ctx::Buffer_master *m = &ctx->BufferMasters[p.GetBufferIndex()];
    m->Allocator->Free(p.AsOffsetAllocation());
  }

  void
  CreateImageDirect(const Image_create_info &createInfo, Image *pImageOut, Image_meta *pMetaOut) {
    Ctx *ctx = &gCtx;
    Image img;
    VkResult res = vkCreateImage(ctx->Device, (VkImageCreateInfo*)&createInfo, 0, &img.Image);
    ASSERT(res == VK_SUCCESS, "Could not create an image!");

    // (todo) dont limit to first imagemasters
    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(ctx->Device, img.Image, &memRequirements);
    const u32 totReqSize = memRequirements.size + memRequirements.alignment;
    OffsetAllocation a = ctx->ImageMasters[0].Allocator->Alloc<u8>(totReqSize);
    if (UINT32_MAX == a.Offset) {
      Util::ErrMsgBox("Not enough memory for image!");
      return;
    }
    const u32 alignedOffset = ((a.Offset + memRequirements.alignment - 1) / memRequirements.alignment) * memRequirements.alignment;
    VkBindImageMemoryInfo bindInfo = {
      .sType = VKTYPE(BIND_IMAGE_MEMORY_INFO),
      .image = img.Image,
      .memory = ctx->ImageMasters[0].Memory,
      .memoryOffset = alignedOffset
    };
    vkBindImageMemory2(ctx->Device, 1, &bindInfo);
    VkImageViewCreateInfo info = {
      .sType = VKTYPE(IMAGE_VIEW_CREATE_INFO),
      .image = img.Image,
      .viewType = createInfo.ViewType,
      .format = createInfo.Format,
      .subresourceRange = createInfo.Range
    };
    res = vkCreateImageView(ctx->Device, &info, 0, &img.View);
    img.Width = (u16)createInfo.Extent.width;
    img.Height = (u16)createInfo.Extent.height;
    img.Depth = (u16)createInfo.Extent.depth;
    img.Format = createInfo.Format;
    img.MipCount = (u8)createInfo.MipLevels;
    img.LayerCount = (u8)createInfo.ArrayLayers;
    img.Layout = VK_IMAGE_LAYOUT_UNDEFINED;
    Image_meta m = { .MemoryAllocation = a, .CreateParam = createInfo };

    // descriptor creation
    bool descCreateCond[] = { createInfo.CreateAsSampled, createInfo.CreateAsStorage };
    VkDescriptorType types[] = { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE };
    VkImageLayout layouts[] = { VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL };
    VkDescriptorImageInfo imgInfo[2];
    VkWriteDescriptorSet writes[2];
    u32 writeCount = 0;
    for (u32 i = 0; i < ARRAYCOUNT(descCreateCond); i++) {
      VkDescriptorSet set = ctx->GlobalDescriptorSets[i];
      imgInfo[i] = {
	.imageView = img.View,
	.imageLayout = layouts[i]
      };
      VkWriteDescriptorSet writeInfo = {
	.sType = VKTYPE(WRITE_DESCRIPTOR_SET),
	.dstSet = set,
	.dstBinding = 0,
	.descriptorCount = 1,
	.descriptorType = types[i],
	.pImageInfo = &imgInfo[i]
      };
      if (descCreateCond[i]) {
	// (TODO) HANDLE DESCRIPTOR INDEX ALLOC FAILURE
	Util::Handle<Util::Any> h = ctx->DescriptorArrays[i].Add();
	img.DescriptorHandles[i] = h;
	writeInfo.dstArrayElement = h.Index;
	writes[writeCount++] = writeInfo;
      } else {
	img.DescriptorHandles[i] = NULL_HANDLE;
      }
    }
    if (writeCount)
      vkUpdateDescriptorSets(ctx->Device, writeCount, writes, 0, NULL);
    *pImageOut = img;
    *pMetaOut = m;
  }

  Image*
  GetImage(const ImageHandle handle) {
    const Ctx *ctx = &gCtx;
    return ctx->ImagePool.Get(handle);
  }

  ImageHandle
  CreateImage(const Image_create_info &createInfo) {
    Ctx *ctx = &gCtx;
    Image img; Image_meta meta;
    CreateImageDirect(createInfo, &img, &meta);
    ImageHandle h = ctx->ImagePool.Add();
    *ctx->ImagePool.Get(h) = img;
    *ctx->ImagePool.GetMeta(h) = meta;
    return h;
  }

  void
  ResizeImage(const ImageHandle handle, const uv3 newSize) {
    Ctx *ctx = &gCtx;
    Image *img = GetImage(handle);
    Image_meta *meta = ctx->ImagePool.GetMeta(handle);
    meta->CreateParam.Extent.width = newSize.x;
    meta->CreateParam.Extent.height = newSize.y;
    meta->CreateParam.Extent.depth = newSize.z;
    // memory
    ctx->ImageMasters[0].Allocator->Free(meta->MemoryAllocation);
    // view
    vkDestroyImageView(ctx->Device, img->View, 0);
    // image
    vkDestroyImage(ctx->Device, img->Image, 0);
    // descriptors
    for (u32 i = 0; i < ARRAYCOUNT(img->DescriptorHandles); i++) {
      if (img->DescriptorHandles[i].IsValid()) {
	ctx->DescriptorArrays[i].Remove(img->DescriptorHandles[i]);
      }
    }

    Image newImg; Image_meta newMeta;
    CreateImageDirect(meta->CreateParam, &newImg, &newMeta);
    *img = newImg;
    *meta = newMeta;
  }

  KernelHandle
  CreateKernel(const void *pBinary, const u64 binarySize, const char *entryPoint) {
    Ctx *ctx = &gCtx;
    Kernel k;
    VkShaderModule m;
    VkComputePipelineCreateInfo cpci;
    VkResult r;
    r = Helper::CreateShaderModule(ctx->Device, pBinary, binarySize, &m);
    if (VK_SUCCESS != r) goto ON_EXIT;
    cpci = {
      .sType = VKTYPE(COMPUTE_PIPELINE_CREATE_INFO),
      .flags = 0,
      .stage = {
	.sType = VKTYPE(PIPELINE_SHADER_STAGE_CREATE_INFO),
	.stage = VK_SHADER_STAGE_COMPUTE_BIT,
	.module = m,
	.pName = entryPoint
      },
      .layout = ctx->GlobalPipelineLayout,
    };
    r = vkCreateComputePipelines(ctx->Device, 0, 1, &cpci, 0, &k.Pipeline);
  ON_EXIT:
    vkDestroyShaderModule(ctx->Device, m, 0);
    KernelHandle h = {};
    if (VK_SUCCESS == r) {
      h = ctx->KernelPool.Add();
      *ctx->KernelPool.Get(h) = k;
    }
    return h;
  }

  Bump_alloc*
  GetBumpAlloc(Buffer_type type) {
    Ctx *ctx = &gCtx;
    return &ctx->BumpAllocs[type][ctx->CurrentFrameIndex];
  }

  void
  Cmd_buf::SetKernel(const KernelHandle handle) const {
    const Ctx *ctx = &gCtx;
    vkCmdBindPipeline(this->CommandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, ctx->KernelPool.Get(handle)->Pipeline);
  }

  void
  Cmd_buf::BindDescriptors(VkPipelineBindPoint bindPoint) const {
    const Ctx *ctx = &gCtx;
    vkCmdBindDescriptorSets(this->CommandBuffer, bindPoint, ctx->GlobalPipelineLayout, 0, DESC_SET_COUNT, ctx->GlobalDescriptorSets, 0, NULL);
  }

  void
  Cmd_buf::ImageBarrier(Image_barrier *pBarriers, const u32 count) const {
    for (u32 i = 0; i < count; i++) {
      VkImage vkimg = pBarriers[i].Img->Image;
      // Load old layout only if it wasnt specified by user
      if (IMAGE_BARRIER_DEFAULT_OLD_LAYOUT == pBarriers[i].OldLayout) {
	pBarriers[i].OldLayout = pBarriers[i].Img->Layout;
      }
      Image *img = pBarriers[i].Img;
      img->Layout = pBarriers[i].NewLayout;
      pBarriers[i]._RawImage = vkimg;
    }
    VkDependencyInfo info = {
      .sType = VKTYPE(DEPENDENCY_INFO),
      .imageMemoryBarrierCount = 1,
      .pImageMemoryBarriers = (VkImageMemoryBarrier2*)pBarriers
    };
    vkCmdPipelineBarrier2(this->CommandBuffer, &info);
  }
  void
  Cmd_buf::StageBarrier(Stage_barrier *pBarriers, const u32 count) const {
    VkDependencyInfo info = {
      .sType = VKTYPE(DEPENDENCY_INFO),
      .memoryBarrierCount = 1,
      .pMemoryBarriers = (VkMemoryBarrier2*)pBarriers
    };
    vkCmdPipelineBarrier2(this->CommandBuffer, &info);
  }
  
  void
  Cmd_buf::BlitEntireColorImage(const ImageHandle srcHandle, const ImageHandle dstHandle, const u32 srcMip, const u32 dstMip, VkFilter filter) const {
    const Ctx *ctx = &gCtx;
    const Image *src = ctx->ImagePool.Get(srcHandle);
    const Image *dst = ctx->ImagePool.Get(dstHandle);
    this->BlitEntireColorImage(src, dst, srcMip, dstMip, filter);
  }
  void
  Cmd_buf::BlitEntireColorImage(const Image *src, const Image *dst, const u32 srcMip, const u32 dstMip, VkFilter filter) const {
    VkImageBlit region = {
      .srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, srcMip, 0, src->LayerCount},
      .srcOffsets = {{0, 0, 0}, {src->Width, src->Height, src->Depth}},
      .dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, dstMip, 0, dst->LayerCount},
      .dstOffsets = {{0, 0, 0}, {dst->Width, dst->Height, dst->Depth}}
    };
    vkCmdBlitImage(this->CommandBuffer, src->Image, src->Layout, dst->Image, dst->Layout, 1, &region, filter);
  }

  void
  Cmd_buf::MemCpy(Pointer src, Pointer dst, const u32 siz) const {
    const Ctx *ctx = &gCtx;
    VkBuffer s, d;
    u32 srcOff, dstOff;
    ctx->BufferOffsetFromPointer(src, &s, &srcOff);
    ctx->BufferOffsetFromPointer(dst, &d, &dstOff);
    VkBufferCopy2 c = {
      .sType = VKTYPE(BUFFER_COPY_2),
      .srcOffset = srcOff,
      .dstOffset = dstOff,
      .size = siz
    };
    VkCopyBufferInfo2 info = {
      .sType = VKTYPE(COPY_BUFFER_INFO_2),
      .srcBuffer = s,
      .dstBuffer = d,
      .regionCount = 1,
      .pRegions = &c
    };
    vkCmdCopyBuffer2(this->CommandBuffer, &info);
  }
  template<typename T> void
  Cmd_buf::PushConstants(T *pData) const {
    const Ctx *ctx = &gCtx;
    vkCmdPushConstants(this->CommandBuffer, ctx->GlobalPipelineLayout, VK_SHADER_STAGE_ALL, 0, sizeof (T), pData);
  }

  void
  Cmd_buf::Dispatch(uv3 workGroupCounts) const {
    vkCmdDispatch(this->CommandBuffer, workGroupCounts.x, workGroupCounts.y, workGroupCounts.z);
  }

  void
  Cmd_buf::SetTimestamp(bool start, u64 *pDest) const {
    Ctx *ctx = &gCtx;
    u64 c;
    if ((c = ctx->TimestampCounters[ctx->CurrentFrameIndex]++) <= ctx->TimestampCapacity) {
      vkCmdWriteTimestamp(this->CommandBuffer, start ? VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT : VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, ctx->TimestampQueryPools[ctx->CurrentFrameIndex], c);
      ctx->TimestampBuffers[ctx->CurrentFrameIndex][c].UserPointer = pDest;
    }
  }

  void
  Cmd_buf::ResetTimestamps() const {
    const Ctx *ctx = &gCtx;
    vkCmdResetQueryPool(this->CommandBuffer, ctx->TimestampQueryPools[ctx->CurrentFrameIndex], 0, ctx->TimestampCapacity);
  }

  double
  GetMicrosecondsElapsed(const u64 start, const u64 end) {
    const Ctx *ctx = &gCtx;
    double deltaMicro = ctx->PhysicalDeviceLimits.timestampPeriod / 1000.0; // microseconds per tick
    return (end - start) * deltaMicro;
  }

  void
  WaitIdle() {
    const Ctx *ctx = &gCtx;
    vkDeviceWaitIdle(ctx->Device);
  }
}
