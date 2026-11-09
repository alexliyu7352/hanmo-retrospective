// 显式冷job的实际容器分配器；不拦截全进程new，也不让未接入的旧对象假称已计费。
#pragma once
#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <memory>
#include <new>
#include <type_traits>
#include <unordered_map>
#include <vector>
namespace hanmo::resident {
class CheckpointRetentionCensus;
// 每个资源拥有原job状态；容器可长于调用栈scope，释放必须noexcept且不重新分配。
class CheckpointMemoryResource {
public:
  virtual ~CheckpointMemoryResource() = default;
  virtual void *allocate_bytes(std::size_t, std::size_t alignment) = 0;
  virtual void deallocate_bytes(void *) noexcept = 0;
  virtual bool
  shares_job(const CheckpointMemoryResource &other) const noexcept {
    return this == &other;
  }
  // 已活resource的真实control与原job首错窗口，同一census去重后才跨job保留。
  virtual void retain_control(CheckpointRetentionCensus &) const;
  // 资源必须保留真实拥有关系；默认生产资源不启用严格内存累计。
  virtual bool memory_accounting_enabled() const noexcept { return false; }
  virtual void reserve_external(std::size_t) = 0;
  virtual void release_external(std::size_t) noexcept = 0;
};
// 只作用于明确使用本分配器的新容器/BigNat；线程之间须显式传同一resource并建立scope。
std::shared_ptr<CheckpointMemoryResource> current_checkpoint_memory() noexcept;
class ScopedCheckpointMemory {
public:
  explicit ScopedCheckpointMemory(
      std::shared_ptr<CheckpointMemoryResource>) noexcept;
  ~ScopedCheckpointMemory() noexcept;
  ScopedCheckpointMemory(const ScopedCheckpointMemory &) = delete;
  ScopedCheckpointMemory &operator=(const ScopedCheckpointMemory &) = delete;

private:
  std::shared_ptr<CheckpointMemoryResource> previous_;
};
// 普通 C++ 深字段的原 job 生命周期句柄；不记录、累计或退还字段字节。
// 真 JobBytes 的容量/数量仍由独立 CheckpointJobReservation 严格维护。
class CheckpointMemoryCharge {
public:
  CheckpointMemoryCharge() noexcept = default;
  explicit CheckpointMemoryCharge(std::size_t)
      : resource_(current_checkpoint_memory()) {
    if (resource_) resource_->reserve_external(0);
  }
  CheckpointMemoryCharge(const CheckpointMemoryCharge &)
      : CheckpointMemoryCharge(0) {}
  CheckpointMemoryCharge(CheckpointMemoryCharge &&) noexcept = default;
  CheckpointMemoryCharge &operator=(CheckpointMemoryCharge other) noexcept {
    swap(other);
    return *this;
  }
  void grow(std::size_t) {
    if (!resource_) {
      auto current = current_checkpoint_memory();
      if (current) {
        current->reserve_external(0);
        resource_ = std::move(current);
        return;
      }
    }
    if (resource_) resource_->reserve_external(0);
  }
  const std::shared_ptr<CheckpointMemoryResource> &resource() const noexcept {
    return resource_;
  }
  void swap(CheckpointMemoryCharge &other) noexcept {
    resource_.swap(other.resource_);
  }
private:
  std::shared_ptr<CheckpointMemoryResource> resource_;
};
// 仅标识真实 allocator 家族及其 job 资源；不记录 block 用量或估算容器布局。
class CheckpointAllocationOwner {
public:
  explicit CheckpointAllocationOwner(
      std::shared_ptr<CheckpointMemoryResource> resource) noexcept
      : resource_(std::move(resource)) {}
  const std::shared_ptr<CheckpointMemoryResource> &resource() const noexcept {
    return resource_;
  }

private:
  std::shared_ptr<CheckpointMemoryResource> resource_;
};
std::shared_ptr<CheckpointAllocationOwner>
    checkpoint_allocation_owner(std::shared_ptr<CheckpointMemoryResource>);
// allocator 保留真实 allocate/deallocate 和数量乘法溢出门，不维护用量账。
template <class T> class CheckpointAllocator {
public:
  using value_type = T;
  using propagate_on_container_move_assignment = std::true_type;
  using propagate_on_container_swap = std::true_type;
  using is_always_equal = std::false_type;
  CheckpointAllocator() : CheckpointAllocator(current_checkpoint_memory()) {}
  explicit CheckpointAllocator(
      std::shared_ptr<CheckpointMemoryResource> resource)
      : resource_(std::move(resource)),
        owner_(checkpoint_allocation_owner(resource_)) {}
  template <class U>
  CheckpointAllocator(const CheckpointAllocator<U> &other) noexcept
      : resource_(other.resource()), owner_(other.allocation_owner()) {}
  T *allocate(std::size_t n) {
    if (!owner_)
      owner_ = checkpoint_allocation_owner(resource_);
    if (n > std::numeric_limits<std::size_t>::max() / sizeof(T))
      throw std::bad_alloc();
    const auto bytes = n * sizeof(T);
    return resource_ ? static_cast<T *>(resource_->allocate_bytes(bytes, alignof(T)))
                     : std::allocator<T>{}.allocate(n);
  }
  void deallocate(T *p, std::size_t n) noexcept {
    if (resource_)
      resource_->deallocate_bytes(p);
    else
      std::allocator<T>{}.deallocate(p, n);
  }
  // 复制产生新的实际分配并绑定当前 job；没有 scope 时保持普通运行容器语义。
  CheckpointAllocator select_on_container_copy_construction() const {
    return CheckpointAllocator{};
  }
  const std::shared_ptr<CheckpointMemoryResource> &resource() const noexcept {
    return resource_;
  }
  const std::shared_ptr<CheckpointAllocationOwner> &
  allocation_owner() const noexcept {
    return owner_;
  }
  template <class U>
  bool operator==(const CheckpointAllocator<U> &other) const noexcept {
    return resource_ == other.resource() && owner_ == other.allocation_owner();
  }
  template <class U>
  bool operator!=(const CheckpointAllocator<U> &other) const noexcept {
    return !(*this == other);
  }

private:
  std::shared_ptr<CheckpointMemoryResource> resource_;
  std::shared_ptr<CheckpointAllocationOwner> owner_;
};
// 原目录工厂的具名shared拥有者：对象与control
// block均由同一实际allocator家族记录。
// get_deleter仅取原factory凭据，不从shared_ptr<const>猜唯一性或估计STL控制块大小。
template <class T> struct CheckpointSharedDelete {
  CheckpointAllocator<T> allocator;
  void operator()(T *value) noexcept {
    if (value) {
      value->~T();
      allocator.deallocate(value, 1);
    }
  }
};
template <class T, class... A>
std::shared_ptr<T> checkpoint_make_shared(A &&...args) {
  using Value = std::remove_const_t<T>;
  CheckpointAllocator<Value> allocator;
  Value *raw = allocator.allocate(1);
  try {
    ::new (static_cast<void *>(raw)) Value(std::forward<A>(args)...);
  } catch (...) {
    allocator.deallocate(raw, 1);
    throw;
  }
  // shared_ptr构造失败必须调用已提供的deleter；这里不重复销毁raw。
  return std::shared_ptr<T>(raw, CheckpointSharedDelete<Value>{allocator},
                            allocator);
}
template <class T>
std::shared_ptr<CheckpointAllocationOwner>
checkpoint_shared_allocation_owner(const std::shared_ptr<T> &value) noexcept {
  using Value = std::remove_const_t<T>;
  const auto *deleter = std::get_deleter<CheckpointSharedDelete<Value>>(value);
  return deleter ? deleter->allocator.allocation_owner()
                 : std::shared_ptr<CheckpointAllocationOwner>{};
}
// 保留标准unique_ptr/default-delete接口的实际对象分配；头持resource直到真正operator
// delete。
// 仅显式继承的对象采用，不拦截进程new；构造抛出也由配对delete归还同一block。
void *checkpoint_allocate_object(std::size_t, std::size_t);
void checkpoint_deallocate_object(void *) noexcept;
// 只供已知 CheckpointAllocatedObject 原对象读取其真实 resource；不记录分配大小。
struct CheckpointObjectAllocation {
  std::shared_ptr<CheckpointMemoryResource> resource;
};
CheckpointObjectAllocation checkpoint_object_allocation(const void *) noexcept;
struct CheckpointAllocatedObject {
  static void *operator new(std::size_t n) {
    return checkpoint_allocate_object(n, alignof(std::max_align_t));
  }
  static void *operator new(std::size_t n, std::align_val_t a) {
    return checkpoint_allocate_object(n, std::size_t(a));
  }
  static void operator delete(void *p) noexcept {
    checkpoint_deallocate_object(p);
  }
  static void operator delete(void *p, std::align_val_t) noexcept {
    checkpoint_deallocate_object(p);
  }
  static void *operator new(std::size_t, void *p) noexcept { return p; }
  static void operator delete(void *, void *) noexcept {}
};
// 固定 typed 对象仍在构造前真实分配；不估算或计量对象内部动态字段。
template <class T> struct CheckpointDelete {
  std::shared_ptr<CheckpointMemoryResource> resource;
  void operator()(T *value) const noexcept {
    if (!value)
      return;
    if (resource) {
      value->~T();
      resource->deallocate_bytes(value);
    } else
      delete value;
  }
};
template <class T>
using CheckpointUniquePtr = std::unique_ptr<T, CheckpointDelete<T>>;
template <class T, class... A>
CheckpointUniquePtr<T> checkpoint_make_unique(A &&...args) {
  auto resource = current_checkpoint_memory();
  if (!resource)
    return CheckpointUniquePtr<T>(new T(std::forward<A>(args)...), {});
  void *block = resource->allocate_bytes(sizeof(T), alignof(T));
  try {
    return CheckpointUniquePtr<T>(new (block) T(std::forward<A>(args)...),
                                  CheckpointDelete<T>{resource});
  } catch (...) {
    resource->deallocate_bytes(block);
    throw;
  }
}
// shared
// owner控制块也由原job实际allocator申请；异常由shared_ptr构造负责销毁已释放裸指针。
template <class T>
std::shared_ptr<T> checkpoint_share(CheckpointUniquePtr<T> &&value) {
  auto deleter = value.get_deleter();
  auto resource = deleter.resource;
  // allocator自身owner可能分配，必须仍由unique_ptr拥有对象时完成。
  CheckpointAllocator<T> allocator{std::move(resource)};
  T *raw = value.release();
  return std::shared_ptr<T>(raw, std::move(deleter), std::move(allocator));
}
// 已有default-delete拥有者只移动指针；控制块的真实rebind分配仍走当前同job。
// 对象自身须由其原真实allocator或CheckpointAllocatedObject负责，不在此估算深值。
template <class T>
std::shared_ptr<T> checkpoint_share(std::unique_ptr<T> &&value) {
  CheckpointAllocator<T> allocator;
  T *raw = value.release();
  return std::shared_ptr<T>(raw, std::default_delete<T>{},
                            std::move(allocator));
}
template <class T>
using CheckpointVector = std::vector<T, CheckpointAllocator<T>>;
// 派生索引节点和bucket也用真实rebind分配量；值内普通STL字段由具名copy/retention另接。
template <class K, class V, class Compare = std::less<K>>
using CheckpointMap =
    std::map<K, V, Compare, CheckpointAllocator<std::pair<const K, V>>>;
template <class K, class V, class Hash = std::hash<K>,
          class Equal = std::equal_to<K>>
using CheckpointHashMap =
    std::unordered_map<K, V, Hash, Equal,
                       CheckpointAllocator<std::pair<const K, V>>>;
} // namespace hanmo::resident
