#include "VKEngine.hpp"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <iostream>
#include <set>
#include <stdexcept>
#include <utility>

#include <glm/gtc/matrix_transform.hpp>

namespace vkp
{
    VKEngine::VKEngine(GLFWwindow* window, const VkApplicationInfo app_info,
                       const VkInstanceCreateInfo instance_create_info)
        : m_window(window)
        , m_appInfo(app_info)
        , m_createInfo(instance_create_info)
    {
        initVulkan();
    }

    VKEngine::~VKEngine()
    {
        if (m_device != VK_NULL_HANDLE)
            vkDeviceWaitIdle(m_device);

        cleanupSwapChain();

        for (auto& ub : m_uniformBuffers)
        {
            if (ub.buffer)
                vkDestroyBuffer(m_device, ub.buffer, nullptr);
            if (ub.memory)
                vkFreeMemory(m_device, ub.memory, nullptr);
        }
        if (m_descriptorPool)
            vkDestroyDescriptorPool(m_device, m_descriptorPool, nullptr);
        if (m_graphicsPipeline)
            vkDestroyPipeline(m_device, m_graphicsPipeline, nullptr);
        if (m_pipelineLayout)
            vkDestroyPipelineLayout(m_device, m_pipelineLayout, nullptr);
        if (m_renderPass)
            vkDestroyRenderPass(m_device, m_renderPass, nullptr);
        if (m_descriptorSetLayout)
            vkDestroyDescriptorSetLayout(m_device, m_descriptorSetLayout, nullptr);
        if (m_commandPool)
            vkDestroyCommandPool(m_device, m_commandPool, nullptr);
        if (m_vertexBuffer.buffer)
            vkDestroyBuffer(m_device, m_vertexBuffer.buffer, nullptr);
        if (m_vertexBuffer.memory)
            vkFreeMemory(m_device, m_vertexBuffer.memory, nullptr);
        if (m_indexBuffer.buffer)
            vkDestroyBuffer(m_device, m_indexBuffer.buffer, nullptr);
        if (m_indexBuffer.memory)
            vkFreeMemory(m_device, m_indexBuffer.memory, nullptr);

        for (auto& sem : m_imageAvailableSemaphores)
            vkDestroySemaphore(m_device, sem, nullptr);
        for (auto& sem : m_renderFinishedSemaphores)
            vkDestroySemaphore(m_device, sem, nullptr);
        for (auto& fence : m_inFlightFences)
            vkDestroyFence(m_device, fence, nullptr);

        if (m_device)
            vkDestroyDevice(m_device, nullptr);
        if (m_surface)
            vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
        if (m_instance)
            vkDestroyInstance(m_instance, nullptr);
    }

    VKEngine::VKEngine(VKEngine&& other) noexcept
        : m_window(std::exchange(other.m_window, nullptr))
        , m_instance(std::exchange(other.m_instance, VK_NULL_HANDLE))
        , m_surface(std::exchange(other.m_surface, VK_NULL_HANDLE))
        , m_physicalDevice(std::exchange(other.m_physicalDevice, VK_NULL_HANDLE))
        , m_device(std::exchange(other.m_device, VK_NULL_HANDLE))
        , m_graphicsQueue(std::exchange(other.m_graphicsQueue, VK_NULL_HANDLE))
        , m_presentQueue(std::exchange(other.m_presentQueue, VK_NULL_HANDLE))
        , m_swapChain(std::exchange(other.m_swapChain, VK_NULL_HANDLE))
        , m_swapChainImageFormat(other.m_swapChainImageFormat)
        , m_swapChainExtent(other.m_swapChainExtent)
        , m_swapChainImages(std::move(other.m_swapChainImages))
        , m_swapChainImageViews(std::move(other.m_swapChainImageViews))
        , m_renderPass(std::exchange(other.m_renderPass, VK_NULL_HANDLE))
        , m_descriptorSetLayout(std::exchange(other.m_descriptorSetLayout, VK_NULL_HANDLE))
        , m_pipelineLayout(std::exchange(other.m_pipelineLayout, VK_NULL_HANDLE))
        , m_graphicsPipeline(std::exchange(other.m_graphicsPipeline, VK_NULL_HANDLE))
        , m_swapChainFramebuffers(std::move(other.m_swapChainFramebuffers))
        , m_commandPool(std::exchange(other.m_commandPool, VK_NULL_HANDLE))
        , m_commandBuffers(std::move(other.m_commandBuffers))
        , m_vertexBuffer(std::exchange(other.m_vertexBuffer, BufferObject{}))
        , m_indexBuffer(std::exchange(other.m_indexBuffer, BufferObject{}))
        , m_indexCount(other.m_indexCount)
        , m_descriptorPool(std::exchange(other.m_descriptorPool, VK_NULL_HANDLE))
        , m_descriptorSets(std::move(other.m_descriptorSets))
        , m_imageAvailableSemaphores(std::move(other.m_imageAvailableSemaphores))
        , m_renderFinishedSemaphores(std::move(other.m_renderFinishedSemaphores))
        , m_inFlightFences(std::move(other.m_inFlightFences))
        , m_currentFrame(other.m_currentFrame)
        , m_appInfo(other.m_appInfo)
        , m_createInfo(other.m_createInfo)
    {
        if (m_createInfo.pApplicationInfo == &other.m_appInfo)
        {
            m_createInfo.pApplicationInfo = &m_appInfo;
        }
        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
        {
            m_uniformBuffers[i] = other.m_uniformBuffers[i];
            other.m_uniformBuffers[i] = UniformBuffer{};
        }
    }

    VKEngine& VKEngine::operator=(VKEngine&& other) noexcept
    {
        if (this != &other)
        {
            std::swap(m_window, other.m_window);
            std::swap(m_instance, other.m_instance);
            std::swap(m_surface, other.m_surface);
            std::swap(m_physicalDevice, other.m_physicalDevice);
            std::swap(m_device, other.m_device);
            std::swap(m_graphicsQueue, other.m_graphicsQueue);
            std::swap(m_presentQueue, other.m_presentQueue);
            std::swap(m_swapChain, other.m_swapChain);
            std::swap(m_swapChainImageFormat, other.m_swapChainImageFormat);
            std::swap(m_swapChainExtent, other.m_swapChainExtent);
            m_swapChainImages.swap(other.m_swapChainImages);
            m_swapChainImageViews.swap(other.m_swapChainImageViews);
            std::swap(m_renderPass, other.m_renderPass);
            std::swap(m_descriptorSetLayout, other.m_descriptorSetLayout);
            std::swap(m_pipelineLayout, other.m_pipelineLayout);
            std::swap(m_graphicsPipeline, other.m_graphicsPipeline);
            m_swapChainFramebuffers.swap(other.m_swapChainFramebuffers);
            std::swap(m_commandPool, other.m_commandPool);
            m_commandBuffers.swap(other.m_commandBuffers);
            std::swap(m_vertexBuffer, other.m_vertexBuffer);
            std::swap(m_indexBuffer, other.m_indexBuffer);
            std::swap(m_indexCount, other.m_indexCount);
            m_uniformBuffers.swap(other.m_uniformBuffers);
            std::swap(m_descriptorPool, other.m_descriptorPool);
            m_descriptorSets.swap(other.m_descriptorSets);
            m_imageAvailableSemaphores.swap(other.m_imageAvailableSemaphores);
            m_renderFinishedSemaphores.swap(other.m_renderFinishedSemaphores);
            m_inFlightFences.swap(other.m_inFlightFences);
            std::swap(m_currentFrame, other.m_currentFrame);
            std::swap(m_appInfo, other.m_appInfo);
            std::swap(m_createInfo, other.m_createInfo);
            if (m_createInfo.pApplicationInfo == &other.m_appInfo)
            {
                m_createInfo.pApplicationInfo = &m_appInfo;
            }
        }
        return *this;
    }

    void VKEngine::drawFrame()
    {
        uint32_t frameIdx = m_currentFrame;

        vkWaitForFences(m_device, 1, &m_inFlightFences[frameIdx], VK_TRUE, UINT64_MAX);
        vkResetFences(m_device, 1, &m_inFlightFences[frameIdx]);

        uint32_t imageIndex;
        VkResult result = vkAcquireNextImageKHR(m_device, m_swapChain, UINT64_MAX, m_imageAvailableSemaphores[frameIdx],
                                                VK_NULL_HANDLE, &imageIndex);
        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
        {
            recreateSwapChain();
            return;
        }
        else if (result != VK_SUCCESS)
            throw std::runtime_error("Failed to acquire swap chain image!");

        updateUniformBuffer(frameIdx);

        VkCommandBuffer cmd = m_commandBuffers[imageIndex];
        vkResetCommandBuffer(cmd, 0);
        recordCommandBuffer(cmd, m_swapChainFramebuffers[imageIndex], m_descriptorSets[frameIdx]);

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        VkSemaphore waitSemaphores[] = { m_imageAvailableSemaphores[frameIdx] };
        VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = waitSemaphores;
        submitInfo.pWaitDstStageMask = waitStages;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &cmd;
        VkSemaphore signalSemaphores[] = { m_renderFinishedSemaphores[imageIndex] };
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = signalSemaphores;

        if (vkQueueSubmit(m_graphicsQueue, 1, &submitInfo, m_inFlightFences[frameIdx]) != VK_SUCCESS)
            throw std::runtime_error("Failed to submit draw command buffer!");

        VkPresentInfoKHR presentInfo{};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = signalSemaphores;
        VkSwapchainKHR swapChains[] = { m_swapChain };
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = swapChains;
        presentInfo.pImageIndices = &imageIndex;

        result = vkQueuePresentKHR(m_presentQueue, &presentInfo);
        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
            recreateSwapChain();
        else if (result != VK_SUCCESS)
            throw std::runtime_error("Failed to present swap chain image!");

        m_currentFrame = (m_currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
    }

    void VKEngine::waitIdle() const
    {
        if (m_device != VK_NULL_HANDLE)
            vkDeviceWaitIdle(m_device);
    }

    void VKEngine::initVulkan()
    {
        createInstance();
        createSurface();
        pickPhysicalDevice();
        createLogicalDevice();
        createSwapChain();
        createImageViews();
        createRenderPass();
        createDescriptorSetLayout();
        createGraphicsPipeline();
        createFramebuffers();
        createCommandPool();
        createVertexBuffer();
        createIndexBuffer();
        createUniformBuffers();
        createDescriptorPool();
        createDescriptorSets();
        createCommandBuffers();
        createSyncObjects();
    }

    void VKEngine::createInstance()
    {
        if (m_enableValidationLayers)
        {
            uint32_t layerCount;
            vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
            std::vector<VkLayerProperties> availableLayers(layerCount);
            vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

            for (const char* layerName : m_validationLayers)
            {
                bool found = false;
                for (const auto& layer : availableLayers)
                {
                    if (strcmp(layerName, layer.layerName) == 0)
                    {
                        found = true;
                        break;
                    }
                }
                if (!found)
                {
                    throw std::runtime_error("Validation layer not available!");
                }
            }
            m_createInfo.enabledLayerCount = static_cast<uint32_t>(m_validationLayers.size());
            m_createInfo.ppEnabledLayerNames = m_validationLayers.data();
        }
        else
        {
            m_createInfo.enabledLayerCount = 0;
            m_createInfo.ppEnabledLayerNames = nullptr;
        }

        uint32_t glfwExtensionCount = 0;
        const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
        std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);
        if (m_enableValidationLayers)
        {
            extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }

        m_createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
        m_createInfo.ppEnabledExtensionNames = extensions.data();

        if (vkCreateInstance(&m_createInfo, nullptr, &m_instance) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create Vulkan instance!");
        }
    }

    void VKEngine::createSurface()
    {
        if (glfwCreateWindowSurface(m_instance, m_window, nullptr, &m_surface) != VK_SUCCESS)
        {
            const char* desc;
            int code = glfwGetError(&desc);
            std::string msg = "Failed to create window surface!";
            if (code != 0 && desc)
            {
                msg += " GLFW error " + std::to_string(code) + ": " + desc;
            }
            throw std::runtime_error(msg);
        }
    }

    void VKEngine::pickPhysicalDevice()
    {
        uint32_t deviceCount = 0;
        vkEnumeratePhysicalDevices(m_instance, &deviceCount, nullptr);
        if (deviceCount == 0)
        {
            throw std::runtime_error("No Vulkan capable GPU found!");
        }
        std::vector<VkPhysicalDevice> devices(deviceCount);
        vkEnumeratePhysicalDevices(m_instance, &deviceCount, devices.data());

        for (const auto& device : devices)
        {
            uint32_t queueFamilyCount = 0;
            vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
            std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
            vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

            bool graphicsFound = false, presentFound = false;
            for (uint32_t i = 0; i < queueFamilyCount; ++i)
            {
                if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
                    graphicsFound = true;
                VkBool32 presentSupport = false;
                vkGetPhysicalDeviceSurfaceSupportKHR(device, i, m_surface, &presentSupport);
                if (presentSupport)
                    presentFound = true;
                if (graphicsFound && presentFound)
                    break;
            }
            if (!graphicsFound || !presentFound)
                continue;

            uint32_t extensionCount;
            vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);
            std::vector<VkExtensionProperties> availableExtensions(extensionCount);
            vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

            bool allExtensionsSupported = true;
            for (const char* required : m_deviceExtensions)
            {
                bool found = false;
                for (const auto& ext : availableExtensions)
                {
                    if (strcmp(required, ext.extensionName) == 0)
                    {
                        found = true;
                        break;
                    }
                }
                if (!found)
                {
                    allExtensionsSupported = false;
                    break;
                }
            }
            if (!allExtensionsSupported)
                continue;

            uint32_t formatCount;
            vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_surface, &formatCount, nullptr);
            uint32_t presentModeCount;
            vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_surface, &presentModeCount, nullptr);
            if (formatCount == 0 || presentModeCount == 0)
                continue;

            m_physicalDevice = device;
            break;
        }

        if (m_physicalDevice == VK_NULL_HANDLE)
        {
            throw std::runtime_error("Failed to find suitable GPU!");
        }
    }

    void VKEngine::createLogicalDevice()
    {
        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(m_physicalDevice, &queueFamilyCount, nullptr);
        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(m_physicalDevice, &queueFamilyCount, queueFamilies.data());

        int graphicsFamily = -1, presentFamily = -1;
        for (uint32_t i = 0; i < queueFamilyCount; ++i)
        {
            if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
            {
                graphicsFamily = i;
            }
            VkBool32 presentSupport = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(m_physicalDevice, i, m_surface, &presentSupport);
            if (presentSupport)
            {
                presentFamily = i;
            }
            if (graphicsFamily >= 0 && presentFamily >= 0)
                break;
        }
        if (graphicsFamily < 0 || presentFamily < 0)
        {
            throw std::runtime_error("Could not find required queue families!");
        }

        std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
        std::set<int> uniqueFamilies = { graphicsFamily, presentFamily };
        float queuePriority = 1.0f;
        for (int family : uniqueFamilies)
        {
            VkDeviceQueueCreateInfo qci{};
            qci.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            qci.queueFamilyIndex = family;
            qci.queueCount = 1;
            qci.pQueuePriorities = &queuePriority;
            queueCreateInfos.push_back(qci);
        }

        VkDeviceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
        createInfo.pQueueCreateInfos = queueCreateInfos.data();
        createInfo.enabledExtensionCount = static_cast<uint32_t>(m_deviceExtensions.size());
        createInfo.ppEnabledExtensionNames = m_deviceExtensions.data();
        createInfo.enabledLayerCount = 0;
        createInfo.ppEnabledLayerNames = nullptr;

        if (vkCreateDevice(m_physicalDevice, &createInfo, nullptr, &m_device) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create logical device!");
        }

        m_graphicsFamily = graphicsFamily;
        m_presentFamily = presentFamily;

        vkGetDeviceQueue(m_device, graphicsFamily, 0, &m_graphicsQueue);
        vkGetDeviceQueue(m_device, presentFamily, 0, &m_presentQueue);
    }

    void VKEngine::createSwapChain()
    {
        uint32_t formatCount;
        vkGetPhysicalDeviceSurfaceFormatsKHR(m_physicalDevice, m_surface, &formatCount, nullptr);
        std::vector<VkSurfaceFormatKHR> formats(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(m_physicalDevice, m_surface, &formatCount, formats.data());

        VkSurfaceFormatKHR chosenFormat = formats[0];
        for (const auto& f : formats)
        {
            if (f.format == VK_FORMAT_B8G8R8A8_SRGB && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            {
                chosenFormat = f;
                break;
            }
        }
        m_swapChainImageFormat = chosenFormat.format;

        uint32_t modeCount;
        vkGetPhysicalDeviceSurfacePresentModesKHR(m_physicalDevice, m_surface, &modeCount, nullptr);
        std::vector<VkPresentModeKHR> presentModes(modeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(m_physicalDevice, m_surface, &modeCount, presentModes.data());

        VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;
        for (const auto& pm : presentModes)
        {
            if (pm == VK_PRESENT_MODE_MAILBOX_KHR)
            {
                presentMode = pm;
                break;
            }
        }

        VkSurfaceCapabilitiesKHR capabilities;
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_physicalDevice, m_surface, &capabilities);
        if (capabilities.currentExtent.width != UINT32_MAX)
        {
            m_swapChainExtent = capabilities.currentExtent;
        }
        else
        {
            int w, h;
            glfwGetFramebufferSize(m_window, &w, &h);
            VkExtent2D actualExtent = { static_cast<uint32_t>(w), static_cast<uint32_t>(h) };
            actualExtent.width =
                std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
            actualExtent.height =
                std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
            m_swapChainExtent = actualExtent;
        }

        uint32_t imageCount = capabilities.minImageCount + 1;
        if (capabilities.maxImageCount > 0 && imageCount > capabilities.maxImageCount)
        {
            imageCount = capabilities.maxImageCount;
        }

        VkSwapchainCreateInfoKHR createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        createInfo.surface = m_surface;
        createInfo.minImageCount = imageCount;
        createInfo.imageFormat = chosenFormat.format;
        createInfo.imageColorSpace = chosenFormat.colorSpace;
        createInfo.imageExtent = m_swapChainExtent;
        createInfo.imageArrayLayers = 1;
        createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

        uint32_t queueFamilyIndices[] = { static_cast<uint32_t>(m_graphicsFamily),
                                          static_cast<uint32_t>(m_presentFamily) };
        if (m_graphicsFamily != m_presentFamily)
        {
            createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            createInfo.queueFamilyIndexCount = 2;
            createInfo.pQueueFamilyIndices = queueFamilyIndices;
        }
        else
        {
            createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        }

        createInfo.preTransform = capabilities.currentTransform;
        createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        createInfo.presentMode = presentMode;
        createInfo.clipped = VK_TRUE;
        createInfo.oldSwapchain = VK_NULL_HANDLE;

        if (vkCreateSwapchainKHR(m_device, &createInfo, nullptr, &m_swapChain) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create swap chain!");
        }

        vkGetSwapchainImagesKHR(m_device, m_swapChain, &imageCount, nullptr);
        m_swapChainImages.resize(imageCount);
        vkGetSwapchainImagesKHR(m_device, m_swapChain, &imageCount, m_swapChainImages.data());
    }

    void VKEngine::createImageViews()
    {
        m_swapChainImageViews.resize(m_swapChainImages.size());
        for (size_t i = 0; i < m_swapChainImages.size(); ++i)
        {
            VkImageViewCreateInfo viewInfo{};
            viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            viewInfo.image = m_swapChainImages[i];
            viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            viewInfo.format = m_swapChainImageFormat;
            viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
            viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
            viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
            viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
            viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            viewInfo.subresourceRange.baseMipLevel = 0;
            viewInfo.subresourceRange.levelCount = 1;
            viewInfo.subresourceRange.baseArrayLayer = 0;
            viewInfo.subresourceRange.layerCount = 1;

            if (vkCreateImageView(m_device, &viewInfo, nullptr, &m_swapChainImageViews[i]) != VK_SUCCESS)
            {
                throw std::runtime_error("Failed to create image views!");
            }
        }
    }

    void VKEngine::createRenderPass()
    {
        VkAttachmentDescription colorAttachment{};
        colorAttachment.format = m_swapChainImageFormat;
        colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        VkAttachmentReference colorAttachmentRef{};
        colorAttachmentRef.attachment = 0;
        colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &colorAttachmentRef;

        VkSubpassDependency dependency{};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.srcAccessMask = 0;
        dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        VkRenderPassCreateInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassInfo.attachmentCount = 1;
        renderPassInfo.pAttachments = &colorAttachment;
        renderPassInfo.subpassCount = 1;
        renderPassInfo.pSubpasses = &subpass;
        renderPassInfo.dependencyCount = 1;
        renderPassInfo.pDependencies = &dependency;

        if (vkCreateRenderPass(m_device, &renderPassInfo, nullptr, &m_renderPass) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create render pass!");
        }
    }

    void VKEngine::createDescriptorSetLayout()
    {
        VkDescriptorSetLayoutBinding uboLayoutBinding{};
        uboLayoutBinding.binding = 0;
        uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        uboLayoutBinding.descriptorCount = 1;
        uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = 1;
        layoutInfo.pBindings = &uboLayoutBinding;

        if (vkCreateDescriptorSetLayout(m_device, &layoutInfo, nullptr, &m_descriptorSetLayout) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create descriptor set layout!");
        }
    }

    void VKEngine::createGraphicsPipeline()
    {
        auto readFile = [](const std::string& filename) -> std::vector<char>
        {
            std::ifstream file(filename, std::ios::ate | std::ios::binary);
            if (!file.is_open())
            {
                throw std::runtime_error("Failed to open shader file: " + filename);
            }
            size_t fileSize = file.tellg();
            std::vector<char> buffer(fileSize);
            file.seekg(0);
            file.read(buffer.data(), fileSize);
            return buffer;
        };

#ifdef SHADER_DIR
        std::string vertPath = std::string(SHADER_DIR) + "/vert.spv";
        std::string fragPath = std::string(SHADER_DIR) + "/frag.spv";
#else
        std::string vertPath = "shaders/vert.spv";
        std::string fragPath = "shaders/frag.spv";
#endif

        VkShaderModule vertModule = createShaderModule(readFile(vertPath));
        VkShaderModule fragModule = createShaderModule(readFile(fragPath));

        VkPipelineShaderStageCreateInfo vertStageInfo{};
        vertStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        vertStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
        vertStageInfo.module = vertModule;
        vertStageInfo.pName = "main";

        VkPipelineShaderStageCreateInfo fragStageInfo{};
        fragStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        fragStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        fragStageInfo.module = fragModule;
        fragStageInfo.pName = "main";

        VkPipelineShaderStageCreateInfo shaderStages[] = { vertStageInfo, fragStageInfo };

        auto bindingDesc = VkVertexInputBindingDescription{ .binding = 0,
                                                            .stride = sizeof(Vertex),
                                                            .inputRate = VK_VERTEX_INPUT_RATE_VERTEX };
        std::array<VkVertexInputAttributeDescription, 2> attrDescs{};
        attrDescs[0] = {
            .location = 0, .binding = 0, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = offsetof(Vertex, pos)
        };
        attrDescs[1] = {
            .location = 1, .binding = 0, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = offsetof(Vertex, color)
        };

        VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
        vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInputInfo.vertexBindingDescriptionCount = 1;
        vertexInputInfo.pVertexBindingDescriptions = &bindingDesc;
        vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attrDescs.size());
        vertexInputInfo.pVertexAttributeDescriptions = attrDescs.data();

        VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
        inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        inputAssembly.primitiveRestartEnable = VK_FALSE;

        VkPipelineViewportStateCreateInfo viewportState{};
        viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.viewportCount = 1;
        viewportState.scissorCount = 1;

        VkPipelineRasterizationStateCreateInfo rasterizer{};
        rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizer.depthClampEnable = VK_FALSE;
        rasterizer.rasterizerDiscardEnable = VK_FALSE;
        rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizer.lineWidth = 1.0f;
        rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
        rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        rasterizer.depthBiasEnable = VK_FALSE;

        VkPipelineMultisampleStateCreateInfo multisampling{};
        multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.sampleShadingEnable = VK_FALSE;
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        VkPipelineColorBlendAttachmentState colorBlendAttachment{};
        colorBlendAttachment.colorWriteMask =
            VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        colorBlendAttachment.blendEnable = VK_FALSE;

        VkPipelineColorBlendStateCreateInfo colorBlending{};
        colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlending.logicOpEnable = VK_FALSE;
        colorBlending.attachmentCount = 1;
        colorBlending.pAttachments = &colorBlendAttachment;

        std::vector<VkDynamicState> dynamicStates = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
        VkPipelineDynamicStateCreateInfo dynamicState{};
        dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
        dynamicState.pDynamicStates = dynamicStates.data();

        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 1;
        pipelineLayoutInfo.pSetLayouts = &m_descriptorSetLayout;

        if (vkCreatePipelineLayout(m_device, &pipelineLayoutInfo, nullptr, &m_pipelineLayout) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create pipeline layout!");
        }

        VkGraphicsPipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineInfo.stageCount = 2;
        pipelineInfo.pStages = shaderStages;
        pipelineInfo.pVertexInputState = &vertexInputInfo;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState = &multisampling;
        pipelineInfo.pColorBlendState = &colorBlending;
        pipelineInfo.pDynamicState = &dynamicState;
        pipelineInfo.layout = m_pipelineLayout;
        pipelineInfo.renderPass = m_renderPass;
        pipelineInfo.subpass = 0;
        pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

        if (vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_graphicsPipeline) !=
            VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create graphics pipeline!");
        }

        vkDestroyShaderModule(m_device, vertModule, nullptr);
        vkDestroyShaderModule(m_device, fragModule, nullptr);
    }

    void VKEngine::createFramebuffers()
    {
        m_swapChainFramebuffers.resize(m_swapChainImageViews.size());
        for (size_t i = 0; i < m_swapChainImageViews.size(); ++i)
        {
            VkImageView attachments[] = { m_swapChainImageViews[i] };

            VkFramebufferCreateInfo framebufferInfo{};
            framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebufferInfo.renderPass = m_renderPass;
            framebufferInfo.attachmentCount = 1;
            framebufferInfo.pAttachments = attachments;
            framebufferInfo.width = m_swapChainExtent.width;
            framebufferInfo.height = m_swapChainExtent.height;
            framebufferInfo.layers = 1;

            if (vkCreateFramebuffer(m_device, &framebufferInfo, nullptr, &m_swapChainFramebuffers[i]) != VK_SUCCESS)
            {
                throw std::runtime_error("Failed to create framebuffer!");
            }
        }
    }

    void VKEngine::createCommandPool()
    {
        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        poolInfo.queueFamilyIndex = m_graphicsFamily;

        if (vkCreateCommandPool(m_device, &poolInfo, nullptr, &m_commandPool) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create command pool!");
        }
    }

    void VKEngine::createVertexBuffer()
    {
        std::vector<Vertex> vertices = {
            { { 0.0f, 0.5f, 0.0f }, { 1.0f, 0.0f, 0.0f } },   // 红色（顶部）
            { { -0.5f, -0.5f, 0.0f }, { 0.0f, 1.0f, 0.0f } }, // 绿色（左下）
            { { 0.5f, -0.5f, 0.0f }, { 0.0f, 0.0f, 1.0f } }   // 蓝色（右下）
        };

        VkDeviceSize bufferSize = sizeof(Vertex) * vertices.size();

        VkBuffer stagingBuffer;
        VkDeviceMemory stagingMemory;
        createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer,
                     stagingMemory);

        void* data;
        vkMapMemory(m_device, stagingMemory, 0, bufferSize, 0, &data);
        memcpy(data, vertices.data(), bufferSize);
        vkUnmapMemory(m_device, stagingMemory);

        createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_vertexBuffer.buffer, m_vertexBuffer.memory);

        copyBuffer(stagingBuffer, m_vertexBuffer.buffer, bufferSize);

        vkDestroyBuffer(m_device, stagingBuffer, nullptr);
        vkFreeMemory(m_device, stagingMemory, nullptr);
    }

    void VKEngine::createIndexBuffer()
    {
        std::vector<uint32_t> indices = { 0, 1, 2 };
        m_indexCount = static_cast<uint32_t>(indices.size());
        VkDeviceSize bufferSize = sizeof(uint32_t) * indices.size();

        VkBuffer stagingBuffer;
        VkDeviceMemory stagingMemory;
        createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer,
                     stagingMemory);

        void* data;
        vkMapMemory(m_device, stagingMemory, 0, bufferSize, 0, &data);
        memcpy(data, indices.data(), bufferSize);
        vkUnmapMemory(m_device, stagingMemory);

        createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_indexBuffer.buffer, m_indexBuffer.memory);

        copyBuffer(stagingBuffer, m_indexBuffer.buffer, bufferSize);

        vkDestroyBuffer(m_device, stagingBuffer, nullptr);
        vkFreeMemory(m_device, stagingMemory, nullptr);
    }

    void VKEngine::createUniformBuffers()
    {
        VkDeviceSize bufferSize = sizeof(glm::mat4) * 3;
        for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
        {
            createBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                         m_uniformBuffers[i].buffer, m_uniformBuffers[i].memory);
            vkMapMemory(m_device, m_uniformBuffers[i].memory, 0, bufferSize, 0, &m_uniformBuffers[i].mapped);
        }
    }

    void VKEngine::createDescriptorPool()
    {
        VkDescriptorPoolSize poolSize{};
        poolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        poolSize.descriptorCount = MAX_FRAMES_IN_FLIGHT;

        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = 1;
        poolInfo.pPoolSizes = &poolSize;
        poolInfo.maxSets = MAX_FRAMES_IN_FLIGHT;

        if (vkCreateDescriptorPool(m_device, &poolInfo, nullptr, &m_descriptorPool) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create descriptor pool!");
        }
    }

    void VKEngine::createDescriptorSets()
    {
        std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, m_descriptorSetLayout);
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = m_descriptorPool;
        allocInfo.descriptorSetCount = MAX_FRAMES_IN_FLIGHT;
        allocInfo.pSetLayouts = layouts.data();

        m_descriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
        if (vkAllocateDescriptorSets(m_device, &allocInfo, m_descriptorSets.data()) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to allocate descriptor sets!");
        }

        for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
        {
            VkDescriptorBufferInfo bufferInfo{};
            bufferInfo.buffer = m_uniformBuffers[i].buffer;
            bufferInfo.offset = 0;
            bufferInfo.range = sizeof(glm::mat4) * 3;

            VkWriteDescriptorSet descriptorWrite{};
            descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrite.dstSet = m_descriptorSets[i];
            descriptorWrite.dstBinding = 0;
            descriptorWrite.dstArrayElement = 0;
            descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            descriptorWrite.descriptorCount = 1;
            descriptorWrite.pBufferInfo = &bufferInfo;

            vkUpdateDescriptorSets(m_device, 1, &descriptorWrite, 0, nullptr);
        }
    }

    void VKEngine::createCommandBuffers()
    {
        uint32_t imageCount = static_cast<uint32_t>(m_swapChainImages.size());
        m_commandBuffers.resize(imageCount);

        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = m_commandPool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = imageCount;

        if (vkAllocateCommandBuffers(m_device, &allocInfo, m_commandBuffers.data()) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to allocate command buffers!");
        }
    }

    void VKEngine::createSyncObjects()
    {
        VkSemaphoreCreateInfo semaphoreInfo{};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
        {
            if (vkCreateSemaphore(m_device, &semaphoreInfo, nullptr, &m_imageAvailableSemaphores[i]) != VK_SUCCESS)
                throw std::runtime_error("Failed to create image available semaphore!");
        }

        m_renderFinishedSemaphores.resize(m_swapChainImages.size());
        for (uint32_t i = 0; i < m_swapChainImages.size(); ++i)
        {
            if (vkCreateSemaphore(m_device, &semaphoreInfo, nullptr, &m_renderFinishedSemaphores[i]) != VK_SUCCESS)
                throw std::runtime_error("Failed to create render finished semaphore!");
        }

        for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
        {
            if (vkCreateFence(m_device, &fenceInfo, nullptr, &m_inFlightFences[i]) != VK_SUCCESS)
                throw std::runtime_error("Failed to create fence!");
        }
    }

    void VKEngine::cleanupSwapChain()
    {
        for (auto& fb : m_swapChainFramebuffers)
            vkDestroyFramebuffer(m_device, fb, nullptr);
        m_swapChainFramebuffers.clear();

        if (!m_commandBuffers.empty())
        {
            vkFreeCommandBuffers(m_device, m_commandPool, static_cast<uint32_t>(m_commandBuffers.size()),
                                 m_commandBuffers.data());
            m_commandBuffers.clear();
        }

        for (auto& iv : m_swapChainImageViews)
            vkDestroyImageView(m_device, iv, nullptr);
        m_swapChainImageViews.clear();

        for (auto& sem : m_renderFinishedSemaphores)
            vkDestroySemaphore(m_device, sem, nullptr);
        m_renderFinishedSemaphores.clear();

        if (m_swapChain)
        {
            vkDestroySwapchainKHR(m_device, m_swapChain, nullptr);
            m_swapChain = VK_NULL_HANDLE;
        }
    }

    void VKEngine::recreateSwapChain()
    {
        vkDeviceWaitIdle(m_device);
        cleanupSwapChain();
        createSwapChain();
        createImageViews();
        createFramebuffers();
        createCommandBuffers();
        createSyncObjects();
        m_currentFrame = 0;
    }

    uint32_t VKEngine::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const
    {
        VkPhysicalDeviceMemoryProperties memProperties;
        vkGetPhysicalDeviceMemoryProperties(m_physicalDevice, &memProperties);
        for (uint32_t i = 0; i < memProperties.memoryTypeCount; ++i)
        {
            if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties)
            {
                return i;
            }
        }
        throw std::runtime_error("Failed to find suitable memory type!");
    }

    VkShaderModule VKEngine::createShaderModule(const std::vector<char>& code) const
    {
        VkShaderModuleCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        createInfo.codeSize = code.size();
        createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

        VkShaderModule module;
        if (vkCreateShaderModule(m_device, &createInfo, nullptr, &module) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create shader module!");
        }
        return module;
    }

    void VKEngine::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties,
                                VkBuffer& buffer, VkDeviceMemory& memory) const
    {
        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = size;
        bufferInfo.usage = usage;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        if (vkCreateBuffer(m_device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create buffer!");
        }

        VkMemoryRequirements memRequirements;
        vkGetBufferMemoryRequirements(m_device, buffer, &memRequirements);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

        if (vkAllocateMemory(m_device, &allocInfo, nullptr, &memory) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to allocate buffer memory!");
        }

        vkBindBufferMemory(m_device, buffer, memory, 0);
    }

    void VKEngine::copyBuffer(VkBuffer src, VkBuffer dst, VkDeviceSize size) const
    {
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandPool = m_commandPool;
        allocInfo.commandBufferCount = 1;

        VkCommandBuffer commandBuffer;
        vkAllocateCommandBuffers(m_device, &allocInfo, &commandBuffer);

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        vkBeginCommandBuffer(commandBuffer, &beginInfo);

        VkBufferCopy copyRegion{};
        copyRegion.size = size;
        vkCmdCopyBuffer(commandBuffer, src, dst, 1, &copyRegion);

        vkEndCommandBuffer(commandBuffer);

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer;

        vkQueueSubmit(m_graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
        vkQueueWaitIdle(m_graphicsQueue);

        vkFreeCommandBuffers(m_device, m_commandPool, 1, &commandBuffer);
    }

    void VKEngine::updateUniformBuffer(uint32_t currentImage)
    {
        struct UniformBufferObject
        {
            glm::mat4 model;
            glm::mat4 view;
            glm::mat4 proj;
        };

        UniformBufferObject ubo{};
        ubo.model = glm::mat4(1.0f);
        ubo.view = glm::lookAt(glm::vec3(0.0f, 0.0f, 2.0f),  // 相机在 Z 轴正方向
                               glm::vec3(0.0f, 0.0f, 0.0f),  // 看向原点
                               glm::vec3(0.0f, 1.0f, 0.0f)); // Y 轴向上
        ubo.proj = glm::perspective(glm::radians(45.0f), m_swapChainExtent.width / (float)m_swapChainExtent.height,
                                    0.1f, 10.0f);
        ubo.proj[1][1] *= -1;

        memcpy(m_uniformBuffers[currentImage].mapped, &ubo, sizeof(ubo));
    }

    void VKEngine::recordCommandBuffer(VkCommandBuffer cmd, VkFramebuffer framebuffer, VkDescriptorSet descSet)
    {
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        if (vkBeginCommandBuffer(cmd, &beginInfo) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to begin command buffer!");
        }

        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = m_renderPass;
        renderPassInfo.framebuffer = framebuffer;
        renderPassInfo.renderArea.offset = { 0, 0 };
        renderPassInfo.renderArea.extent = m_swapChainExtent;
        VkClearValue clearColor = { { { 0.0f, 0.0f, 0.0f, 1.0f } } };
        renderPassInfo.clearValueCount = 1;
        renderPassInfo.pClearValues = &clearColor;

        vkCmdBeginRenderPass(cmd, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphicsPipeline);

        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = static_cast<float>(m_swapChainExtent.width);
        viewport.height = static_cast<float>(m_swapChainExtent.height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(cmd, 0, 1, &viewport);

        VkRect2D scissor{};
        scissor.offset = { 0, 0 };
        scissor.extent = m_swapChainExtent;
        vkCmdSetScissor(cmd, 0, 1, &scissor);

        VkBuffer vertexBuffers[] = { m_vertexBuffer.buffer };
        VkDeviceSize offsets[] = { 0 };
        vkCmdBindVertexBuffers(cmd, 0, 1, vertexBuffers, offsets);
        vkCmdBindIndexBuffer(cmd, m_indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1, &descSet, 0, nullptr);
        vkCmdDrawIndexed(cmd, m_indexCount, 1, 0, 0, 0);

        vkCmdEndRenderPass(cmd);

        if (vkEndCommandBuffer(cmd) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to record command buffer!");
        }
    }

} // namespace vkp