#pragma once

#include <array>
#include <vector>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <vulkan/vulkan.h>

namespace vkp
{
    struct Vertex
    {
        glm::vec3 pos;
        glm::vec3 color;
    };

    inline constexpr VkApplicationInfo defaultVkappInfo{
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "VkProject",
        .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
        .pEngineName = "Vulkan",
        .engineVersion = VK_MAKE_VERSION(1, 0, 0),
        .apiVersion = VK_API_VERSION_1_3,
    };
    inline constexpr VkInstanceCreateInfo defaultVkInstanceCreateInfo{
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &defaultVkappInfo,
        .enabledLayerCount = 0,
        .ppEnabledLayerNames = nullptr,
        .enabledExtensionCount = 0,
        .ppEnabledExtensionNames = nullptr,
    };

    class VKEngine
    {
    public:
        explicit VKEngine(GLFWwindow* window, const VkApplicationInfo app_info = defaultVkappInfo,
                          const VkInstanceCreateInfo instance_create_info = defaultVkInstanceCreateInfo);
        ~VKEngine();

        VKEngine(VKEngine&& other) noexcept;
        VKEngine& operator=(VKEngine&& other) noexcept;

        VKEngine(const VKEngine&) = delete;
        VKEngine& operator=(const VKEngine&) = delete;

        void drawFrame();
        void waitIdle() const;

    private:
        void initVulkan();
        void createInstance();
        void createSurface();
        void pickPhysicalDevice();
        void createLogicalDevice();
        void createSwapChain();
        void createImageViews();
        void createRenderPass();
        void createDescriptorSetLayout();
        void createGraphicsPipeline();
        void createFramebuffers();
        void createCommandPool();
        void createVertexBuffer();
        void createIndexBuffer();
        void createUniformBuffers();
        void createDescriptorPool();
        void createDescriptorSets();
        void createCommandBuffers();
        void createSyncObjects();
        void cleanupSwapChain();
        void recreateSwapChain();

        uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const;
        VkShaderModule createShaderModule(const std::vector<char>& code) const;
        void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties,
                          VkBuffer& buffer, VkDeviceMemory& memory) const;
        void copyBuffer(VkBuffer src, VkBuffer dst, VkDeviceSize size) const;

        void updateUniformBuffer(uint32_t currentImage);
        void recordCommandBuffer(VkCommandBuffer cmd, VkFramebuffer framebuffer, VkDescriptorSet descSet);

    private:
        int m_graphicsFamily = -1;
        int m_presentFamily = -1;

        GLFWwindow* m_window;

        VkInstance m_instance{ VK_NULL_HANDLE };
        VkSurfaceKHR m_surface{ VK_NULL_HANDLE };
        VkPhysicalDevice m_physicalDevice{ VK_NULL_HANDLE };
        VkDevice m_device{ VK_NULL_HANDLE };
        VkQueue m_graphicsQueue{ VK_NULL_HANDLE };
        VkQueue m_presentQueue{ VK_NULL_HANDLE };

        VkSwapchainKHR m_swapChain{ VK_NULL_HANDLE };
        VkFormat m_swapChainImageFormat{};
        VkExtent2D m_swapChainExtent{};
        std::vector<VkImage> m_swapChainImages;
        std::vector<VkImageView> m_swapChainImageViews;

        VkRenderPass m_renderPass{ VK_NULL_HANDLE };
        VkDescriptorSetLayout m_descriptorSetLayout{ VK_NULL_HANDLE };
        VkPipelineLayout m_pipelineLayout{ VK_NULL_HANDLE };
        VkPipeline m_graphicsPipeline{ VK_NULL_HANDLE };

        std::vector<VkFramebuffer> m_swapChainFramebuffers;

        VkCommandPool m_commandPool{ VK_NULL_HANDLE };
        std::vector<VkCommandBuffer> m_commandBuffers;

        struct BufferObject
        {
            VkBuffer buffer{ VK_NULL_HANDLE };
            VkDeviceMemory memory{ VK_NULL_HANDLE };
        };
        BufferObject m_vertexBuffer;
        BufferObject m_indexBuffer;
        uint32_t m_indexCount{ 0 };

        static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;
        struct UniformBuffer
        {
            VkBuffer buffer{ VK_NULL_HANDLE };
            VkDeviceMemory memory{ VK_NULL_HANDLE };
            void* mapped{ nullptr };
        };
        std::array<UniformBuffer, MAX_FRAMES_IN_FLIGHT> m_uniformBuffers;

        VkDescriptorPool m_descriptorPool{ VK_NULL_HANDLE };
        std::vector<VkDescriptorSet> m_descriptorSets;

        std::array<VkSemaphore, MAX_FRAMES_IN_FLIGHT> m_imageAvailableSemaphores{};
        std::vector<VkSemaphore> m_renderFinishedSemaphores;
        std::array<VkFence, MAX_FRAMES_IN_FLIGHT> m_inFlightFences{};

        uint32_t m_currentFrame{ 0 };

        VkApplicationInfo m_appInfo;
        VkInstanceCreateInfo m_createInfo;

        const std::vector<const char*> m_deviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
#ifdef NDEBUG
        const bool m_enableValidationLayers = false;
#else
        const bool m_enableValidationLayers = true;
#endif
        const std::vector<const char*> m_validationLayers = { "VK_LAYER_KHRONOS_validation" };
    };
} // namespace vkp