#pragma once

#include <ATen/native/vulkan/api/Common.h>
#include <ATen/native/vulkan/api/Allocator.h>

namespace at {
namespace native {
namespace vulkan {
namespace api {

struct C10_EXPORT Resource final {
  /*
    Memory
  */

  struct Memory final {
    VmaAllocator allocator;
    VmaAllocation allocation;
    VmaAllocationInfo allocation_info;

    struct Access final {
      typedef uint8_t Flags;

      enum Type : Flags {
        Read = 1u << 0u,
        Write = 1u << 1u,
      };

      template<typename Type, Flags access>
      using Pointer = std::add_pointer_t<
          std::conditional_t<
              0u != (access & Write),
              Type,
              std::add_const_t<Type>>>;
    };

    class Scope;
    template<typename Type>
    using Data = Handle<Type, Scope>;

    template<
        typename Type,
        typename Pointer = Access::Pointer<Type, Access::Read>>
    Data<Pointer> map() const &;

    template<
        typename Type,
        Access::Flags kAccess,
        typename Pointer = Access::Pointer<Type, kAccess>>
    Data<Pointer> map() &;

   private:
    // Intentionally disabed to ensure memory access is always properly
    // encapsualted in a scoped map-unmap region.  Allowing below overloads
    // to be invoked on a temporary would open the door to the possibility
    // of accessing the underlying memory out of the expected scope making
    // for seemingly ineffective memory writes and hard to hunt down bugs.

    template<typename Type, typename Pointer>
    Data<Pointer> map() const && = delete;

    template<typename Type, Access::Flags kAccess, typename Pointer>
    Data<Pointer> map() && = delete;
  };

  /*
    Buffer
  */

  struct Buffer final {
    /*
      Descriptor
    */

    struct Descriptor final {
      VkDeviceSize size;

      struct {
        VkBufferUsageFlags buffer;
        VmaMemoryUsage memory;
      } usage;
    };

    VkBuffer handle;
    Memory memory;

    operator bool() const;
  };

  /*
    Image
  */

  struct Image final {
    /*
      Descriptor
    */

    struct Descriptor final {
      VkImageType type;
      VkFormat format;
      VkExtent3D extent;

      struct {
        VkImageUsageFlags image;
        VmaMemoryUsage memory;
      } usage;

      struct {
        VkImageViewType type;
        VkFormat format;
      } view;
    };

    VkImage handle;
    VkImageView view;
    Memory memory;

    operator bool() const;
  };

  /*
    Pool
  */

  class Pool final {
   public:
    explicit Pool(const GPU& gpu);

    Buffer allocate(const Buffer::Descriptor& descriptor);
    Image allocate(const Image::Descriptor& descriptor);
    void purge();

   private:
    struct Configuration final {
      static constexpr uint32_t kReserve = 256u;
    };

    VkDevice device_;
    Handle<VmaAllocator, void(*)(VmaAllocator)> allocator_;
    std::vector<Handle<Buffer, void(*)(const Buffer&)>> buffers_;
    std::vector<Handle<Image, void(*)(const Image&)>> images_;
  } pool;

  explicit Resource(const GPU& gpu)
    : pool(gpu) {
  }
};

//
// Impl
//

class Resource::Memory::Scope final {
 public:
  Scope(
      VmaAllocator allocator,
      VmaAllocation allocation,
      Resource::Memory::Access::Flags access);

  void operator()(const void* data) const;

 private:
  VmaAllocator allocator_;
  VmaAllocation allocation_;
  Resource::Memory::Access::Flags access_;
};

template<typename, typename Pointer>
inline Resource::Memory::Data<Pointer> Resource::Memory::map() const & {
  void* map(const Memory& memory);

  return Data<Pointer>{
    reinterpret_cast<Pointer>(map(*this)),
    Scope(allocator, allocation, Access::Read),
  };
}

template<typename, Resource::Memory::Access::Flags kAccess, typename Pointer>
inline Resource::Memory::Data<Pointer> Resource::Memory::map() & {
  void* map(const Memory& memory);

  static_assert(
      (kAccess == Access::Read) ||
      (kAccess == Access::Write) ||
      (kAccess == (Access::Read | Access::Write)),
      "Invalid memory access!");

  return Data<Pointer>{
    reinterpret_cast<Pointer>(map(*this)),
    Scope(allocator, allocation, kAccess),
  };
}

inline Resource::Buffer::operator bool() const {
  return VK_NULL_HANDLE != handle;
}

inline Resource::Image::operator bool() const {
  return VK_NULL_HANDLE != handle;
}

} // namespace api
} // namespace vulkan
} // namespace native
} // namespace at
