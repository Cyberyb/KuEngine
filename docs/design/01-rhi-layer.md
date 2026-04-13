# RHI 层详细设计

## 1. 概述

RHI (Rendering Hardware Interface) 层是 Vulkan 的薄封装，遵循以下原则：
- **不隐藏 Vulkan 语义**：Pipeline、DescriptorSet、RenderPass 等概念保持原样
- **消除样板代码**：`vkCreate*` / `vkDestroy*` 流程统一封装
- **RAII 生命周期**：所有 GPU 资源使用 RAII 封装，析构时自动释放
- **Vulkan 1.3 优先**：使用 Dynamic Rendering，不创建传统 VkRenderPass 对象

---

## 2. 错误处理

```cpp
#define VK_CHECK(expr)                                                           \
    do {                                                                         \
        VkResult _result = (expr);                                                \
        if (_result != VK_SUCCESS) {                                             \
            throw std::runtime_error("Vulkan error at " __FILE__ ":" +            \
                std::to_string(__LINE__) + ": " + to_string(_result));          \
        }                                                                        \
    } while (false)
```

所有 Vulkan 调用必须通过 `VK_CHECK` 宏封装。

---

## 3. RHIInstance

### 职责
- 创建 / 销毁 VkInstance
- 配置 Validation Layer
- 设置 Debug Messenger

### 关键接口

```cpp
class RHIInstance {
public:
    RHIInstance(const char* appName, uint32_t appVersion);
    ~RHIInstance();

    [[nodiscard]] VkInstance instance() const { return m_instance; }
    [[nodiscard]] bool validationEnabled() const { return m_validationEnabled; }
    [[nodiscard]] VkSurfaceKHR createSurface(GLFWwindow* window) const;

private:
    VkInstance m_instance = VK_NULL_HANDLE;
    bool       m_validationEnabled = false;
};
```

### Validation Layer 开关
- Debug 模式（`NDEBUG` 未定义）：启用 Validation Layer
- Release 模式：禁用 Validation Layer

---

## 4. RHIDevice

### 职责
- 选择物理设备（离散 GPU 优先）
- 创建逻辑设备 + 队列
- 初始化 VMA 内存分配器
- 创建 Command Pool

### 关键接口

```cpp
class RHIDevice {
public:
    RHIDevice(VkInstance instance, VkSurfaceKHR surface);
    ~RHIDevice();

    [[nodiscard]] VkInstance instance() const { return m_instance; }
    [[nodiscard]] VkPhysicalDevice physicalDevice() const { return m_physicalDevice; }
    [[nodiscard]] VkDevice device() const { return m_device; }
    [[nodiscard]] VkQueue graphicsQueue() const { return m_graphicsQueue; }
    [[nodiscard]] VkQueue presentQueue() const { return m_presentQueue; }
    [[nodiscard]] VmaAllocator allocator() const { return m_allocator; }
    [[nodiscard]] uint32_t graphicsQueueFamily() const { return m_graphicsQueueFamily; }
    [[nodiscard]] uint32_t presentQueueFamily() const { return m_presentQueueFamily; }

    // 内存分配辅助
    [[nodiscard]] uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags flags) const;
    void waitIdle() const;
    [[nodiscard]] VkCommandPool createCommandPool() const;

private:
    void pickPhysicalDevice(VkSurfaceKHR surface);
    void createLogicalDevice();
    void initVMA();

    VkInstance               m_instance = VK_NULL_HANDLE;
    VkPhysicalDevice         m_physicalDevice = VK_NULL_HANDLE;
    VkDevice                 m_device = VK_NULL_HANDLE;
    VmaAllocator             m_allocator = VK_NULL_HANDLE;
    VkQueue                  m_graphicsQueue = VK_NULL_HANDLE;
    VkQueue                  m_presentQueue = VK_NULL_HANDLE;
    uint32_t                 m_graphicsQueueFamily = UINT32_MAX;
    uint32_t                 m_presentQueueFamily = UINT32_MAX;

    VkPhysicalDeviceProperties    m_properties;
    VkPhysicalDeviceFeatures      m_features;
    VkPhysicalDeviceMemoryProperties m_memoryProperties;
};
```

---

## 5. SwapChain

### 职责
- 创建 / 重建交换链
- 管理 SwapChain Images 和 ImageViews
- 处理 Resize 事件

### 关键接口

```cpp
class SwapChain {
public:
    SwapChain(const RHIDevice& device, GLFWwindow* window, VkSurfaceKHR surface);
    ~SwapChain();

    void recreate(GLFWwindow* window, VkSurfaceKHR surface);
    [[nodiscard]] uint32_t acquireNextImage(VkSemaphore semaphore);

    [[nodiscard]] VkSwapchainKHR swapChain() const { return m_swapChain; }
    [[nodiscard]] VkFormat imageFormat() const { return m_imageFormat; }
    [[nodiscard]] const std::vector<VkImage>& images() const { return m_images; }
    [[nodiscard]] VkExtent2D extent() const { return m_extent; }
    [[nodiscard]] const std::vector<VkImageView>& imageViews() const { return m_imageViews; }
    [[nodiscard]] size_t imageCount() const { return m_images.size(); }

    [[nodiscard]] uint32_t width() const { return m_extent.width; }
    [[nodiscard]] uint32_t height() const { return m_extent.height; }

private:
    void create(GLFWwindow* window, VkSurfaceKHR surface);
    void destroy();
    void createImageViews();

    const RHIDevice*         m_device = nullptr;
    VkSwapchainKHR          m_swapChain = VK_NULL_HANDLE;
    std::vector<VkImage>     m_images;
    std::vector<VkImageView> m_imageViews;
    VkFormat                 m_imageFormat = VK_FORMAT_UNDEFINED;
    VkExtent2D              m_extent = {0, 0};
    bool                     m_needsRebuild = false;
};
```

### 偏好设置
- Surface Format: `VK_FORMAT_B8G8R8A8_SRGB` + `VK_COLOR_SPACE_SRGB_NONLINEAR_KHR`
- Present Mode: `VK_PRESENT_MODE_MAILBOX_KHR`（三重缓冲）
- 图像数量: `minImageCount + 1`，不超过 `maxImageCount`

---

## 6. RHIBuffer

### RAII 封装 GPU 缓冲区

```cpp
class RHIBuffer {
public:
    RHIBuffer() = default;
    RHIBuffer(const RHIDevice& device, VkDeviceSize size, 
              VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage);
    ~RHIBuffer();

    // 禁止拷贝
    RHIBuffer(const RHIBuffer&) = delete;
    RHIBuffer& operator=(const RHIBuffer&) = delete;

    // 支持移动
    RHIBuffer(RHIBuffer&& other) noexcept;
    RHIBuffer& operator=(RHIBuffer&& other) noexcept;

    void uploadData(const void* data, VkDeviceSize size, VkDeviceSize offset = 0);
    void* mapMemory();
    void unmapMemory();

    [[nodiscard]] VkBuffer buffer() const { return m_buffer; }
    [[nodiscard]] VkDeviceSize size() const { return m_size; }

private:
    VkBuffer     m_buffer = VK_NULL_HANDLE;
    VmaAllocation m_allocation = VK_NULL_HANDLE;
    VkDeviceSize m_size = 0;
    VmaAllocator m_allocator = VK_NULL_HANDLE;
};
```

### 缓冲区类型
| 类型 | Usage | Memory |
|------|-------|--------|
| Vertex | `VERTEX_BUFFER` | `VMA_MEMORY_USAGE_GPU_ONLY` |
| Index | `INDEX_BUFFER` | `VMA_MEMORY_USAGE_GPU_ONLY` |
| Uniform | `UNIFORM_BUFFER` | `HOST_VISIBLE + HOST_COHERENT` |
| Staging | `TRANSFER_SRC` | `HOST_VISIBLE + HOST_COHERENT` |

---

## 7. RHITexture

```cpp
class RHITexture {
public:
    RHITexture() = default;
    RHITexture(const RHIDevice& device, uint32_t width, uint32_t height,
               VkFormat format, VkImageUsageFlags usage, VkImageAspectFlags aspect);
    ~RHITexture();

    [[nodiscard]] VkImage image() const { return m_image; }
    [[nodiscard]] VkImageView imageView() const { return m_imageView; }
    [[nodiscard]] VkFormat format() const { return m_format; }
    [[nodiscard]] uint32_t width() const { return m_width; }
    [[nodiscard]] uint32_t height() const { return m_height; }

private:
    VkImageView createImageView(VkImage image, VkFormat format, 
                                 VkImageAspectFlags aspect) const;

    VkImage       m_image = VK_NULL_HANDLE;
    VkImageView   m_imageView = VK_NULL_HANDLE;
    VmaAllocation m_allocation = VK_NULL_HANDLE;
    VkFormat      m_format = VK_FORMAT_UNDEFINED;
    uint32_t      m_width = 0;
    uint32_t      m_height = 0;
};
```

---

## 8. RHIShader

### 职责
- 从文件加载 SPIR-V
- 创建 VkShaderModule

```cpp
class RHIShader {
public:
    RHIShader() = default;
    explicit RHIShader(const RHIDevice& device, const std::filesystem::path& path);
    ~RHIShader();

    [[nodiscard]] VkShaderModule module() const { return m_module; }
    [[nodiscard]] bool isValid() const { return m_module != VK_NULL_HANDLE; }

private:
    static std::vector<char> readFile(const std::filesystem::path& path);

    VkShaderModule m_module = VK_NULL_HANDLE;
    VkDevice       m_device = VK_NULL_HANDLE;
};
```

---

## 9. RHIPipeline

### Graphics Pipeline（Vulkan 1.3 Dynamic Rendering）

```cpp
struct GraphicsPipelineDesc {
    std::vector<std::reference_wrapper<RHIShader>> shaders;
    std::vector<VkFormat>       colorFormats;
    std::vector<VkPushConstantRange> pushConstantRanges;
    VkFormat                    depthFormat = VK_FORMAT_UNDEFINED;
    VkSampleCountFlagBits       samples = VK_SAMPLE_COUNT_1_BIT;
    VkPrimitiveTopology         topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    VkCullModeFlags             cullMode = VK_CULL_MODE_BACK_BIT;
    VkFrontFace                 frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    bool                        depthTest = true;
    bool                        depthWrite = true;
    bool                        blendEnable = false;
};

class RHIPipeline {
public:
    RHIPipeline() = default;
    RHIPipeline(const RHIDevice& device, const GraphicsPipelineDesc& desc);
    ~RHIPipeline();

    [[nodiscard]] VkPipeline pipeline() const { return m_pipeline; }
    [[nodiscard]] VkPipelineLayout layout() const { return m_layout; }

    void bind(VkCommandBuffer cmd) const;

private:
    VkPipeline      m_pipeline = VK_NULL_HANDLE;
    VkPipelineLayout m_layout = VK_NULL_HANDLE;
    VkDevice        m_device = VK_NULL_HANDLE;
};
```

### Dynamic Rendering 使用方式
```cpp
VkRenderingInfo renderingInfo{};
renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
renderingInfo.renderArea = {{0, 0}, swapChain.extent()};
renderingInfo.layerCount = 1;
renderingInfo.colorAttachmentCount = 1;
renderingInfo.pColorAttachments = &colorAttachment;
if (hasDepth) {
    renderingInfo.pDepthAttachment = &depthAttachment;
}

vkCmdBeginRendering(cmd, &renderingInfo);
// ... 绘制命令 ...
vkCmdEndRendering(cmd);
```

---

## 10. CommandList

```cpp
class CommandList {
public:
    CommandList(const RHIDevice& device, VkCommandPool pool);
    ~CommandList();

    void begin();
    void end();

    // 屏障
    void imageBarrier(VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout,
                      VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage,
                      VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT);

    // 复制操作
    void copyBuffer(VkBuffer src, VkBuffer dst, VkDeviceSize size);

    [[nodiscard]] VkCommandBuffer cmd() const { return m_cmd; }
    [[nodiscard]] operator VkCommandBuffer() const { return m_cmd; }

private:
    VkCommandBuffer m_cmd = VK_NULL_HANDLE;
    VkDevice        m_device = VK_NULL_HANDLE;
    bool            m_recording = false;
};
```

---

## 11. DescriptorSet 封装（规划中）

> 当前仓库尚未提供 `DescriptorSet` 封装实现。
> v0.2 后将按 RenderGraph 与材质系统接入节奏落地该能力。

### DescriptorPool 和 DescriptorSetLayoutBuilder

```cpp
class DescriptorSet {
public:
    DescriptorSet() = default;
    DescriptorSet(const RHIDevice& device, VkDescriptorPool pool, 
                  const std::vector<VkDescriptorSetLayoutBinding>& bindings);
    
    void writeBuffer(uint32_t binding, VkDescriptorType type, 
                     VkBuffer buffer, VkDeviceSize range, uint32_t arrayIndex = 0);
    void writeImage(uint32_t binding, VkDescriptorType type,
                    VkImageView view, VkSampler sampler, VkImageLayout layout);
    void update();

    [[nodiscard]] VkDescriptorSet set() const { return m_set; }

private:
    VkDescriptorSet   m_set = VK_NULL_HANDLE;
    VkDevice          m_device = VK_NULL_HANDLE;
    std::vector<VkWriteDescriptorSet> m_writes;
};
```

---

## 12. 同步机制

### MVP 使用 Fence + Semaphore

```cpp
constexpr uint32_t FRAMES_IN_FLIGHT = 2;

struct FrameSync {
    VkSemaphore imageAvailable;
    VkSemaphore renderFinished;
    VkFence     inFlight;
};

class SyncManager {
public:
    SyncManager(const RHIDevice& device, uint32_t framesInFlight = 2);
    ~SyncManager();

    void waitForFrame(uint32_t frame);
    void submit(uint32_t frame, VkQueue queue,
                 std::span<VkCommandBuffer> cmds);
    bool present(uint32_t frame, VkQueue queue, VkSwapchainKHR swapChain, uint32_t imageIndex);

    [[nodiscard]] uint32_t currentFrame() const { return m_currentFrame; }
    [[nodiscard]] uint32_t currentImage() const { return m_currentImage; }
    [[nodiscard]] FrameSync& frameSync(uint32_t frame) { return m_frames[frame]; }
    void setCurrentImage(uint32_t image) { m_currentImage = image; }
    void incrementFrame() { m_currentFrame = (m_currentFrame + 1) % m_frames.size(); }

private:
    std::vector<FrameSync> m_frames;
    uint32_t m_currentFrame = 0;
    uint32_t m_currentImage = 0;
    const RHIDevice* m_device = nullptr;
};
```
