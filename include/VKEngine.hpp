#pragma once

#include <array>
#include <vector>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <vulkan/vulkan.h>

namespace vkp
{
    // 顶点结构：位置 + 颜色
    struct Vertex
    {
        glm::vec3 pos;
        glm::vec3 color;
    };

    // 默认Vulkan应用信息
    inline constexpr VkApplicationInfo defaultVkappInfo{
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "VkProject",
        .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
        .pEngineName = "Vulkan",
        .engineVersion = VK_MAKE_VERSION(1, 0, 0),
        .apiVersion = VK_API_VERSION_1_3,
    };
    // 默认实例创建信息
    inline constexpr VkInstanceCreateInfo defaultVkInstanceCreateInfo{
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &defaultVkappInfo,
        .enabledLayerCount = 0,
        .ppEnabledLayerNames = nullptr,
        .enabledExtensionCount = 0,
        .ppEnabledExtensionNames = nullptr,
    };

    // Vulkan渲染引擎类
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

        void drawFrame();      // 渲染一帧
        void waitIdle() const; // 等待设备空闲

    private:
        // 初始化Vulkan所有组件
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
        void cleanupSwapChain();  // 清理交换链相关资源
        void recreateSwapChain(); // 重新创建交换链（窗口尺寸变化时）

        // 辅助函数
        uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const;
        VkShaderModule createShaderModule(const std::vector<char>& code) const;
        void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties,
                          VkBuffer& buffer, VkDeviceMemory& memory) const;
        void copyBuffer(VkBuffer src, VkBuffer dst, VkDeviceSize size) const;

        void updateUniformBuffer(uint32_t currentImage);
        void recordCommandBuffer(VkCommandBuffer cmd, VkFramebuffer framebuffer, VkDescriptorSet descSet);

    private:
        // 队列族索引
        int m_graphicsFamily = -1;
        int m_presentFamily = -1;

        GLFWwindow* m_window; // 关联的窗口

        // Vulkan核心对象
        VkInstance m_instance{ VK_NULL_HANDLE };
        VkSurfaceKHR m_surface{ VK_NULL_HANDLE };
        VkPhysicalDevice m_physicalDevice{ VK_NULL_HANDLE };
        VkDevice m_device{ VK_NULL_HANDLE };
        VkQueue m_graphicsQueue{ VK_NULL_HANDLE };
        VkQueue m_presentQueue{ VK_NULL_HANDLE };

        // 交换链
        VkSwapchainKHR m_swapChain{ VK_NULL_HANDLE };
        VkFormat m_swapChainImageFormat{};
        VkExtent2D m_swapChainExtent{};
        std::vector<VkImage> m_swapChainImages;
        std::vector<VkImageView> m_swapChainImageViews;

        // 渲染管线
        VkRenderPass m_renderPass{ VK_NULL_HANDLE };
        VkDescriptorSetLayout m_descriptorSetLayout{ VK_NULL_HANDLE };
        VkPipelineLayout m_pipelineLayout{ VK_NULL_HANDLE };
        VkPipeline m_graphicsPipeline{ VK_NULL_HANDLE };

        std::vector<VkFramebuffer> m_swapChainFramebuffers;

        // 命令池与命令缓冲
        VkCommandPool m_commandPool{ VK_NULL_HANDLE };
        std::vector<VkCommandBuffer> m_commandBuffers;

        // 顶点与索引缓冲
        struct BufferObject
        {
            VkBuffer buffer{ VK_NULL_HANDLE };
            VkDeviceMemory memory{ VK_NULL_HANDLE };
        };
        BufferObject m_vertexBuffer;
        BufferObject m_indexBuffer;
        uint32_t m_indexCount{ 0 };

        // 双缓冲 Uniform
        static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;
        struct UniformBuffer
        {
            VkBuffer buffer{ VK_NULL_HANDLE };
            VkDeviceMemory memory{ VK_NULL_HANDLE };
            void* mapped{ nullptr };
        };
        std::array<UniformBuffer, MAX_FRAMES_IN_FLIGHT> m_uniformBuffers;

        // 描述符
        VkDescriptorPool m_descriptorPool{ VK_NULL_HANDLE };
        std::vector<VkDescriptorSet> m_descriptorSets;

        // 同步对象
        std::array<VkSemaphore, MAX_FRAMES_IN_FLIGHT> m_imageAvailableSemaphores{};
        std::vector<VkSemaphore> m_renderFinishedSemaphores;
        std::array<VkFence, MAX_FRAMES_IN_FLIGHT> m_inFlightFences{};

        uint32_t m_currentFrame{ 0 }; // 当前帧索引

        // 保存的应用信息和实例创建信息（用于移动语义）
        VkApplicationInfo m_appInfo;
        VkInstanceCreateInfo m_createInfo;

        // 设备扩展
        const std::vector<const char*> m_deviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
#ifdef NDEBUG
        const bool m_enableValidationLayers = false;
#else
        const bool m_enableValidationLayers = true;
#endif
        const std::vector<const char*> m_validationLayers = { "VK_LAYER_KHRONOS_validation" };
    };
} // namespace vkp