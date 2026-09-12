# RHI 当前设计

核对日期：2026-09-12。源码目录：[src/KuEngine/RHI](../../src/KuEngine/RHI)。

## 对象边界

| 类 | 持有与职责 |
|---|---|
| RHIInstance | Instance 创建/销毁、GLFW Surface 创建入口 |
| RHIDevice | 物理设备选择、逻辑设备、Graphics/Present Queue、VMA allocator |
| SwapChain | 交换链和交换链 ImageView；Image 本身由交换链提供 |
| SyncManager | 每帧的 imageAvailable、renderFinished Semaphore 与 inFlight Fence |
| CommandList | 从调用方 CommandPool 分配的命令缓冲、绘制统计、Timestamp QueryPool |
| RHIBuffer | VkBuffer 与 VMA allocation，map/unmap/flush/invalidate |
| RHITexture | 2D VkImage、ImageView 与 VMA allocation，不包含 Sampler |
| RHIShader | 从磁盘 SPIR-V 创建 ShaderModule |
| RHIPipeline | Graphics Pipeline 与 PipelineLayout |
| ResourceUploader | 临时命令池、Staging、拷贝和同步上传 |

没有独立 RHIImage、RHICommandPool 或 RHIDescriptor 类。Engine 持有原始 VkCommandPool；Descriptor/Sampler 目前由 UI 或示例资源容器创建。

## 设备与交换链

[RHIDevice](../../src/KuEngine/RHI/RHIDevice.cpp) 检查 Vulkan 1.3、SwapChain 扩展、dynamicRendering/synchronization2、Graphics/Present 队列和可用 Surface 格式/呈现模式/颜色附件用法。合格设备按类型和能力评分；优先寻找可同时绘制和呈现的队列族。

设备暴露实际 Properties、Features、Features13 和图形队列 timestampValidBits。时间戳能力是可选项，0 位时不创建计时 QueryPool。

[SwapChain](../../src/KuEngine/RHI/SwapChain.cpp) 根据 Surface 能力选择格式、呈现模式、extent、图像数量和 Composite Alpha，处理格式未指定、能力不满足及构造失败清理。实际格式通过 RenderContext 传递给 Pipeline。重建采用设备空闲等待，没有旧 SwapChain 复用优化。

## 命令与帧同步

CommandList::begin 重置并开始命令缓冲；end 结束记录。SyncManager 等待 Fence，submit 前重置 Fence，提交时等待 imageAvailable、发出 renderFinished，present 等待 renderFinished。

imageBarrier 使用传统 vkCmdPipelineBarrier，按 Layout 映射 Access Mask，调用方传入 Stage 和 Aspect。覆盖 mip 0、layer 0；不是子资源状态追踪器，也没有 Queue Ownership Transfer 或 Graph Buffer Barrier。

直接 Vulkan 句柄仍可用。业务 draw 应通过 draw()/drawIndexed()，保证计数覆盖；新增原始 vkCmdDraw* 调用会绕过统计。

## 绘制数量

CommandListStatistics 持有：
- drawCalls：通过封装记录的 draw 命令数量；
- submittedVertices：普通绘制 vertexCount × instanceCount，索引绘制 indexCount × instanceCount；
- gpuTimeMilliseconds / gpuTimeValid：最近一次读回的 GPU 时间。

begin 只清零 draw 与顶点数量，保留读回的 GPU 时间。索引计数不是去重顶点数，也不是 Vertex Shader 实际调用数。ImGui 直接通过 Vulkan Backend 绘制，因此不计入业务 draw 数。

## GPU Timestamp

```text
Fence 完成 → collectGpuTime()
begin → reset 两个 Query → TOP_OF_PIPE 时间戳
业务绘制、UI、布局转换
end → BOTTOM_OF_PIPE 时间戳 → 结束命令缓冲
submit → 下一次 Fence 完成后读回
```

collectGpuTime 用 64 位结果和 availability，不使用 WAIT_BIT 额外阻塞。时间换算为 ticks × timestampPeriod / 1,000,000 毫秒；根据 timestampValidBits 掩码处理计数器回绕。尚无结果或队列不支持时 gpuTimeValid 为 false。

QueryPool 在 CommandList 析构时销毁，命令缓冲随外部命令池释放；调用方负责 GPU 完成后再读取、重用或销毁。end 将时间戳标记待收集，当前 Runtime 保证其后正常 submit；该接口本身不追踪任意调用方的提交状态。

GPU 数值是两时间戳间的命令执行跨度，可能受调度/队列等待影响；它不是 GPU 利用率或纯 Shader 耗时。当前没有每 Pass 计时、曲线或多帧 Query 管理。

时间戳逻辑当前作用于所有 CommandList，因此上传器创建的临时命令列表也会分配并记录 Query；这些上传 Query 没有汇入 Engine 统计。

## 资源、上传与 Pipeline

RHIBuffer 的 CreateInfo 显式提供大小、Usage、VMA 用法和分配标记。CPU 写入非 coherent 内存时通过 flush 保证可见；上层仍负责 GPU/CPU 使用时序。

RHITexture 持有一个 2D 图像及视图，可用于颜色纹理或 Runtime 深度。Sampler 由上层装配。

ResourceUploader 接收字节数据并创建 Staging，统一拷贝 Buffer/2D Texture，纹理走 UNDEFINED → TRANSFER_DST → SHADER_READ_ONLY。上传后 queueWaitIdle；当前是初始化期同步设施。

GraphicsPipelineDesc 提供顶点布局、DescriptorSetLayout、PushConstantRange、颜色/深度格式、拓扑、混合、剔除和深度状态。RHIPipeline 采用 Dynamic Rendering，前两个 Shader 按 Vertex/Fragment 处理；没有 Compute Pipeline 或 Pipeline Cache。附件格式必须与 Runtime 实际选择一致。
