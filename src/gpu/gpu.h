#pragma once
#include "vulkan/vulkan.h"

// Defines
#define MAX_GPU_QUEUE_TYPE_COUNT (3)
#define VKTYPE(s) (VK_STRUCTURE_TYPE_ ## s)
#define DESC_SET_COUNT (3)
#define DESC_POOL_MAX_SETS (4)
#define DESC_INDEX_SAMPLED_IMAGE (0)
#define DESC_INDEX_STORAGE_IMAGE (1)
#define DESC_INDEX_SAMPLER (2)
#define MAX_SAMPLER_COUNT (512)

namespace GPU {
  namespace Helper {
    // Structs
#define VULKAN_SWAP_CHAIN_SUPPORT_DETAILS_QUERY_COUNTS (16)
    struct Vulkan_swap_chain_support_details {
      VkSurfaceCapabilitiesKHR Capabilities;
      VkSurfaceFormatKHR Formats[VULKAN_SWAP_CHAIN_SUPPORT_DETAILS_QUERY_COUNTS];
      VkPresentModeKHR PresentModes[VULKAN_SWAP_CHAIN_SUPPORT_DETAILS_QUERY_COUNTS];
      u32 FormatCount;
      u32 PresentModeCount;
    };
  }

  enum Queue_type {
    GENERAL  = 0, // graphics, compute, transfer, present
    TRANSFER = 1, // transfer
    COMPUTE  = 2  // compute
  };
  enum Buffer_type {
    CPU_ACCESS = 0,
    GPU_PRIVATE = 1
  };
  struct Vulkan_queue_family_indices {
    i32			Indices[MAX_GPU_QUEUE_TYPE_COUNT];
  };

#define DEFAULT_SUBRESOURCE_RANGE { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
  // Exact same structure as VkImageCreateInfo (safe to cast) but with defaults
  struct Image_create_info {
    VkStructureType          SType = VKTYPE(IMAGE_CREATE_INFO);
    const void*              PNext = 0;
    VkImageCreateFlags       Flags = 0;
    VkImageType              ImageType = VK_IMAGE_TYPE_2D;
    VkFormat                 Format; // set this always
    VkExtent3D               Extent; // set this always
    uint32_t                 MipLevels = 1; // try to set this always
    uint32_t                 ArrayLayers = 1;
    VkSampleCountFlagBits    Samples = VK_SAMPLE_COUNT_1_BIT;
    VkImageTiling            Tiling = VK_IMAGE_TILING_OPTIMAL;
    VkImageUsageFlags        Usage; // set this always
    VkSharingMode            SharingMode = VK_SHARING_MODE_EXCLUSIVE;
    uint32_t                 QueueFamilyIndexCount = 0;
    const uint32_t*          PQueueFamilyIndices = 0;
    VkImageLayout            InitialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    // image view data
    VkImageSubresourceRange Range = DEFAULT_SUBRESOURCE_RANGE;
    VkImageViewType ViewType = VK_IMAGE_VIEW_TYPE_2D;

    // descriptor creation
    bool CreateAsSampled = false;
    bool CreateAsStorage = false;

    VkSampler Sampler = VK_NULL_HANDLE;
  };

  // Master struct init params
  struct Ctx_init_params {
    HWND WindowHandle;
    HINSTANCE WindowInstance;
    
    u32 SampleImageDescriptorCount;
    u32 StorageImageDescriptorCount;

    u32 VulkanVersion;

    u32 BufferMaxAllocationCount;
    u32 ImageMaxAllocationCount;

    u32 BumpAllocatorSizes[2] = { MEGA(118), MEGA(4) };

    // Set to a more realistic number in an application that
    // uses images
    u32 ImageMemoryBlockSize = MEGA(88);
  };

  typedef u64 Timeline;

  struct Pointer {
    // From MSB to LSB, data layout is 4: 30: 30,
    // where 4 is buffer index, 30 is node index, 30 is buffer offset
    u64 Data;

    inline u32
    GetBufferIndex() {
      return this->Data >> 60;
    }

    inline u32
    GetAllocationNodeIndex() {
      return (this->Data >> 30) & ((1 << 30) - 1);
    }

    inline u32
    GetBufferOffset() {
      return this->Data & ((1 << 30) - 1);
    }

    inline void
    SetBufferOffset(const u32 val) {
      this->Data &= ((1 << 30) - 1);
      this->Data |= (val);
    }

    inline OffsetAllocation
    AsOffsetAllocation() {
      return {
	.Offset = this->GetBufferOffset(),
	.Node = GetAllocationNodeIndex()
      };
    }

    static inline Pointer
    CreateFromOffsetAllocation(const u32 bufferIndex, const OffsetAllocation offsetAllocation) {
      ASSERT(offsetAllocation.Offset < (1 << 30) && offsetAllocation.Node < (1 << 30), "Buffer offset allocation out of compressable bound!");
      Pointer p;
      p.Data = 0;
      p.Data |= bufferIndex;
      p.Data = p.Data << 60;
      p.Data = p.Data | (offsetAllocation.Node << 30);
      p.Data = p.Data | (offsetAllocation.Offset);
      return p;
    }

    void*
    Cpu();
    u64
    Gpu();
    void
    Write(const void *pData, const u32 size) {
      ASSERT(this->Cpu() != NULL, "Cannot write directly to a gpu-private buffer!");
      memcpy(this->Cpu(), pData, size);
    }
  };

  Pointer&
  operator+=(Pointer &l, const u32 o);
  Pointer&
  operator-=(Pointer &l, const u32 o);
  Pointer
  operator+(const Pointer &l, const u32 o);
  Pointer
  operator-(const Pointer &l, const u32 o);

  struct Pointer_span {
    Pointer P;
    u32 Offset;
    u32 Size;
  };

  struct Kernel {
    VkPipeline Pipeline;
  };
  typedef Util::Handle<Kernel> KernelHandle;

  struct Image {
    VkImage Image;
    VkImageView View;
    VkFormat Format;
    VkImageLayout Layout;
    Util::Handle<Util::Any> DescriptorHandles[2];
    u16 Width, Height, Depth;
    u8 MipCount, LayerCount;

    inline u32
    GetSampledImageIndex() const {
      ASSERT(0 != this->DescriptorHandles[DESC_INDEX_SAMPLED_IMAGE].Generation, "Image has no sampled image descriptor!");
      return this->DescriptorHandles[DESC_INDEX_SAMPLED_IMAGE].Index;
    }
    inline u32
    GetStorageImageIndex() const {
      ASSERT(0 != this->DescriptorHandles[DESC_INDEX_STORAGE_IMAGE].Generation, "Image has no sampled image descriptor!");
      return this->DescriptorHandles[DESC_INDEX_STORAGE_IMAGE].Index;
    }

    inline uv2 GetDim() const {
      return {(u32)this->Width, (u32)this->Height};
    }
  };

  static_assert(sizeof (Image) == 40, "Unexpected size of GPU::Image");

  // Unused value in specs
#define IMAGE_BARRIER_DEFAULT_OLD_LAYOUT ((VkImageLayout)9)
  // Exact same structure as VkImageMemoryBarrier2 (safe to cast) but with defaults
  struct Image_barrier {
    VkStructureType            SType = VKTYPE(IMAGE_MEMORY_BARRIER_2);
    const void*                PNext;
    VkPipelineStageFlags2      SrcStageMask;
    VkAccessFlags2             SrcAccessMask;
    VkPipelineStageFlags2      DstStageMask;
    VkAccessFlags2             DstAccessMask;
    VkImageLayout              OldLayout = IMAGE_BARRIER_DEFAULT_OLD_LAYOUT;
    VkImageLayout              NewLayout;
    uint32_t                   SrcQueueFamilyIndex = 0;
    uint32_t                   DstQueueFamilyIndex = 0;
    // User writes to Img, app converts to VkImage and writes to _RawImage
    // Note that VkImage is defined to be 64 bits
    union {
      VkImage _RawImage;
      Image *Img;
    };
    VkImageSubresourceRange    SubresourceRange = DEFAULT_SUBRESOURCE_RANGE;
  };
  
  // Exact same structure as VkMemoryBarrier2 (safe to cast) but with defaults
  struct Stage_barrier {
    VkStructureType          SType = VKTYPE(MEMORY_BARRIER_2);
    const void*              PNext;
    VkPipelineStageFlags2    SrcStageMask;
    VkAccessFlags2           SrcAccessMask;
    VkPipelineStageFlags2    DstStageMask;
    VkAccessFlags2           DstAccessMask;
  };

  struct Image_meta {
    OffsetAllocation MemoryAllocation; // LSB indices memory chunk index
    Image_create_info CreateParam;
  };
  typedef Util::Handle<Image> ImageHandle;

  struct Timestamp {
    u64 Value;
    u64 *UserPointer; // destination to store value
  };

  // rendering
  struct Attachment_info {
    VkStructureType          SType = VKTYPE(RENDERING_ATTACHMENT_INFO);
    const void*              PNext;
    union {
      VkImageView              ImageView; // converted to this when render begins
      Image *Img; // user sets this
    };
    VkImageLayout            ImageLayout;
    VkResolveModeFlagBits    ResolveMode;
    VkImageView              ResolveImageView;
    VkImageLayout            ResolveImageLayout;
    VkAttachmentLoadOp       LoadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    VkAttachmentStoreOp      StoreOp = VK_ATTACHMENT_STORE_OP_STORE;
    VkClearValue             ClearValue;
  };

  struct Rendering_info {
    VkStructureType                     sType = VKTYPE(RENDERING_INFO);
    const void*                         PNext;
    VkRenderingFlags                    Flags;
    VkRect2D                            RenderArea;
    uint32_t                            LayerCount;
    uint32_t                            ViewMask;
    uint32_t                            ColorAttachmentCount;
    Attachment_info*    PColorAttachments;
    Attachment_info*    PDepthAttachment;
    Attachment_info*    PStencilAttachment;
  };

  struct Cmd_buf {
    VkCommandBuffer CommandBuffer;
    Timeline WaitTimeline;
    Queue_type QueueTypeEnum;
    u32 ForSwapchain; // if 1, then WaitTimeline waits for frameindex to be equal to it.

    // API
    void
    SetKernel(const KernelHandle handle) const;
    void
    BindDescriptors(VkPipelineBindPoint bindPoint) const;
    void
    ImageBarrier(Image_barrier *pBarriers, const u32 count) const;
    void
    StageBarrier(Stage_barrier *pBarriers, const u32 count) const;
    void
    BlitEntireColorImage(const Image *src, const Image *dst, const u32 srcMip, const u32 dstMip, VkFilter filter) const;
    void
    BlitEntireColorImage(const ImageHandle srcHandle, const ImageHandle dstHandle, const u32 srcMip, const u32 dstMip, VkFilter filter) const;
    void
    MemCpy(Pointer src, Pointer dst, const u32 siz) const;
    template<typename T> void
    PushConstants(T *pData) const;
    void
    Dispatch(uv3 workGroupCounts) const;
    void
    SetTimestamp(bool start, u64 *pDest) const;
    // call IF settimestamp is used
    void
    ResetTimestamps() const;
    void
    BeginRendering(const Rendering_info &pInfo, const iv2 areaTL, const uv2 areaWH) const;
    void
    EndRendering() const;
  };

#define BUMP_ALLOC_ALIGNMENT (16)
  struct Bump_alloc {
    Pointer Base;
    u64 Size;
    u64 BytesPushed;

    void
    Reset() {
      this->BytesPushed = 0;
    }

    template <typename T> inline Pointer
    Bump(const u64 count) {
      const u64 siz = ALIGN(sizeof (T) * count, BUMP_ALLOC_ALIGNMENT);
      u64 cur = this->BytesPushed;
      this->BytesPushed += siz;
      ASSERT(this->BytesPushed <= this->Size, "GPU Bump alloc overflow!");
      return this->Base + cur;
    }
  };

#define MAX_COMMAND_BUFFER_COUNT (12)
  // Master struct
  /*
    Contains all state needed to create/destroy resources.
    We keep ONE in RAM.
  */
  struct Ctx {
    
    /*
      Add one for each type of operation that has possible thread contention
    */
    enum MutexIndices {
      QUEUE_SUBMIT = 0,
      BEGIN_CMD_BUF = 1
    };
    alignas(64) Mutex Mutexes[16];
    Scratch_arena ScratchArena;
    VkInstance Instance;
    VkDevice Device;
    VkPhysicalDevice PhysicalDevice;
    VkPhysicalDeviceLimits PhysicalDeviceLimits;
    VkDebugUtilsMessengerEXT DebugMessenger;
    VkSurfaceKHR Surface;
    VkQueue Queues[MAX_GPU_QUEUE_TYPE_COUNT];
    Helper::Vulkan_swap_chain_support_details SwapchainSupportDetails;
    Vulkan_queue_family_indices QueueFamilyIndices;
    VkSemaphore RenderCompleteSemaphores[2];
    VkSemaphore PresentCompleteSemaphores[2];
    VkSemaphore TimelineSemaphore;
    Timeline GlobalTimeline = 0;
    Timeline LastConfirmedTimeline = 0;
    VkFence ImageWaitFences[2];
    VkCommandPool CommandPools[MAX_GPU_QUEUE_TYPE_COUNT];
    Bump_alloc BumpAllocs[2][2]; // x: type, y: index
    u32 DistinctQueueFamilyCount;
    float SamplerMaxAnisotropy;
    
    Util::Stack<VkCommandBuffer> CommandBufferStacks[MAX_GPU_QUEUE_TYPE_COUNT];
    Util::VolatileArray<Cmd_buf, MAX_GPU_QUEUE_TYPE_COUNT * MAX_COMMAND_BUFFER_COUNT> ExecutingCommandBuffers;

    Util::Pool2<Image, Image_meta> ImagePool;
    Util::Pool<Kernel> KernelPool;
    /*
      All pipelines use this layout for descriptor sets & push constant.
     */
    VkDescriptorPool GlobalDescriptorPool;
    Util::VirtualPool DescriptorArrays[DESC_SET_COUNT];
    VkDescriptorSet GlobalDescriptorSets[DESC_SET_COUNT];
    VkDescriptorSetLayout GlobalDescriptorSetLayouts[DESC_SET_COUNT];
    VkPipelineLayout GlobalPipelineLayout;
    VkSampler DefaultSampler;
    u32 VulkanVersion;
    u32 CurrentFrameIndex; // 0 or 1

    HWND WindowHandle;

    u32 FrameIndex;
    VkImageView SwapchainImageViews[2];
    VkImage SwapchainImages[2];
    VkSwapchainKHR Swapchain;
    VkExtent2D SwapchainExtent;
    VkFormat SwapchainFormat;
    u32 SwapchainImageCount;
    VkFormat SwapchainImageFormat;

    VkQueryPool TimestampQueryPools[2];
    u32 TimestampCounters[2];
    Timestamp *TimestampBuffers[2];
    u32 TimestampCapacity;

    struct Buffer_master {
      VkBuffer Buffer;
      VkDeviceAddress GpuAddress;
      void *CpuAddress; // probably null in case of private gpu mem
      Memory::Virtual_allocator *Allocator;
      u32 MemoryTypeIndex;
    };
    Buffer_master BufferMasters[2];

    struct Image_master {
      VkDeviceMemory Memory;
      Memory::Virtual_allocator *Allocator;
    };
    Image_master ImageMasters[1];

    // Helper methods
    VkResult
    InitSwapchain(bool isRestart = false);
    
    i32
    Init(const Ctx_init_params *pParams);

    void
    Free();
    
    void
    BufferOffsetFromPointer(Pointer p, VkBuffer *pBufferOut, u32 *pOffsetOut) const;

    void
    InitImgui();

  private:
    Vulkan_queue_family_indices
    FindQueueFamilies();
  };

  // API
  /*

    Reference vs pointer in parameters:
    Purely a stylistic issue, as I see it. I use refs for non-arrays big structs
    solely to allow call-site initialization like this;
    GPU::Init({.prop = a, .anotherProp = b});

  */
  i32
  Init(const Ctx_init_params &pParams);
  void
  Free();

  uv2
  GetSwapchainDim();
  
  Cmd_buf
  BeginCmdBuf(const Queue_type queueTypeEnum);

  // Check if any executing resources have finished, and recycle them
  void
  PollResources();
  
  // Increments global timeline. uses queue type in command buffer to decide submit target
  Timeline
  Submit(Cmd_buf *pCmdBufs, const u32 count, const Timeline waitTimeline = 0);
  
  // no reason to expose this -- we end internally right before submitting
  //   void EndCmdBuf(VkCommandBuffer cmdBuf);
  
  i32
  AcquireSwapchain();
  Image
  GetSwapchainImage(i32 imageIndex);
  VkImageView
  GetSwapchainImageView(i32 imageIndex);
  i32
  PresentSwapchain(i32 imageIndex, const Cmd_buf *pCmdBufs, const u32 cmdBufCount);
  void
  ReinitSwapchain();
  
  // Check that the timeline semaphore has reached the expected timeline value
  bool
  CheckTimeline(const Timeline timeline);

  Pointer
  Malloc(const u32 size, const Buffer_type type);
  void
  Free(Pointer p);

  ImageHandle
  CreateImage(const Image_create_info &createInfo);
  void
  CreateImageDirect(const Image_create_info &createInfo, Image *pImageOut, Image_meta *pMetaOut);
  Image*
  GetImage(const ImageHandle handle);
  void
  ResizeImage(const ImageHandle handle, const uv3 newSize);
  
  // (todo) destroy image

  // compute shader, called "kernel"
  KernelHandle
  CreateKernel(const void *pBinary, const u64 binarySize, const char *entryPoint);

  // (todo) destroy kernel

  Bump_alloc*
  GetBumpAlloc(Buffer_type type);

  double
  GetMicrosecondsElapsed(const u64 start, const u64 end);

  void
  WaitIdle();
}
