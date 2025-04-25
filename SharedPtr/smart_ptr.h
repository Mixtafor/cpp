#include <iostream>
#include <memory>
template <typename T>
class WeakPtr;

template <typename T>
class SharedPtr;

template <typename T>
struct EnableSharedFromThis {
    WeakPtr<T> weak_this_;
    SharedPtr<T> shared_from_this() {
      return SharedPtr<T>(weak_this_);
    }
};

struct BaseControlBlock {
protected:
    size_t shared_count;
    size_t weak_count;

    BaseControlBlock(size_t sh_c, size_t wk_c)
            : shared_count(sh_c), weak_count(wk_c)
    {}

    virtual ~BaseControlBlock() = default;
    virtual void destruct() noexcept = 0;
    virtual void deallocate() noexcept = 0;
    //virtual bool operator==(BaseControlBlock* other); //todo
    template <typename T>
    friend class SharedPtr;

    template <typename T>
    friend class WeakPtr;
};

template <typename U, typename Alloc = std::allocator<U>>
struct ControlBlockMakeShared : BaseControlBlock {
    union { U value; };
    [[no_unique_address]] Alloc alloc;

    template <typename... Args>
    ControlBlockMakeShared(size_t sh_c, size_t wk_c, const Alloc& a, Args&&... args)
            : BaseControlBlock(sh_c, wk_c)
            , alloc(a)
    {
      std::allocator_traits<Alloc>::construct(alloc, &value, std::forward<Args>(args)...);
    }

    void destruct() noexcept override {
      std::allocator_traits<Alloc>::destroy(alloc, &value);
    }

    void deallocate() noexcept override {
      this->~ControlBlockMakeShared();
      using CBAlloc = typename std::allocator_traits<Alloc>::template rebind_alloc<ControlBlockMakeShared>;
      CBAlloc cbAlloc(alloc);
      std::allocator_traits<CBAlloc>::deallocate(cbAlloc, this, 1);
    }

    ~ControlBlockMakeShared() noexcept override {}
};

template <typename U, typename Deleter = std::default_delete<U>, typename Alloc = std::allocator<U>>
struct ControlBlockRegular : BaseControlBlock {
    U* ptr_to_delete;
    [[no_unique_address]] Deleter del;
    [[no_unique_address]] Alloc alloc;

    ControlBlockRegular(size_t sh_c, size_t wk_c, U* p, Deleter d, const Alloc& a)
            : BaseControlBlock(sh_c, wk_c)
            , ptr_to_delete(p)
            , del(std::move(d))
            , alloc(a)
    {}

    void destruct() noexcept override {
      if (ptr_to_delete) {
        del(ptr_to_delete);
        ptr_to_delete = nullptr;
      }
    }

    void deallocate() noexcept override {
      this->~ControlBlockRegular();
      using CBAlloc = typename std::allocator_traits<Alloc>::template rebind_alloc<ControlBlockRegular>;
      CBAlloc cbAlloc(alloc);
      std::allocator_traits<CBAlloc>::deallocate(cbAlloc, this, 1);
    }

    ~ControlBlockRegular() noexcept override = default;
};

template <typename T>
class SharedPtr {

  template <typename U>
  friend class WeakPtr;

  template <typename U>
  friend class SharedPtr;

private:
    T* ptr = nullptr;
    BaseControlBlock* control_block = nullptr;

    explicit SharedPtr(BaseControlBlock* cb, T* p) noexcept
            : ptr(p), control_block(cb) {}

    void decrement() noexcept {
      if (!control_block) return;
      if (--control_block->shared_count == 0) {
        control_block->destruct();
        if (control_block->weak_count == 0) {
          control_block->deallocate();
        }
      }
    }

    template <typename U, typename Alloc, typename... Args>
    friend SharedPtr<U> allocateShared(const Alloc& alloc, Args&&... args);

public:
    SharedPtr() noexcept = default;

    template <typename U, typename Deleter = std::default_delete<U>, typename Alloc = std::allocator<U>>
    explicit SharedPtr(U* pointer, Deleter d = Deleter(), Alloc a = Alloc())
            : ptr(pointer)
    {
      using CB = ControlBlockRegular<U, Deleter, Alloc>;
      using CBAlloc = typename std::allocator_traits<Alloc>::template rebind_alloc<CB>;
      CBAlloc cbAlloc(a);
      auto* mem = std::allocator_traits<CBAlloc>::allocate(cbAlloc, 1);
      new (mem) CB(1, 0, pointer, std::move(d), a);
      control_block = mem;
      
      if constexpr (std::is_base_of_v<EnableSharedFromThis<U>, U>) { //todo MAY BE T
        ptr->sptr = *this;
      }
    }

    template<class U>
    explicit SharedPtr(const WeakPtr<U>& other)
      : ptr(other.ptr), control_block(other.control_block)
      {
        if (other.expired()) { throw std::bad_weak_ptr(); }
        if (control_block) {
          ++control_block->shared_count;
        }
      }

    SharedPtr(const SharedPtr& other) noexcept
            : ptr(other.ptr), control_block(other.control_block)
    {
      if (control_block) {
        ++control_block->shared_count;
      }
    }

    template <typename U>
    SharedPtr(const SharedPtr<U>& other) noexcept
            : ptr(other.ptr), control_block(other.control_block)
    {
      if (control_block) {
        ++control_block->shared_count;
      }
    }

    SharedPtr(SharedPtr&& other) noexcept
            : ptr(other.ptr), control_block(other.control_block)
    {
      other.ptr = nullptr;
      other.control_block = nullptr;
    }

    template <typename U>
    SharedPtr(SharedPtr<U>&& other) noexcept
            : ptr(other.ptr), control_block(other.control_block)
    {
      other.ptr = nullptr;
      other.control_block = nullptr;
    }

    SharedPtr& operator=(const SharedPtr& other) noexcept {
      if (this != &other) {
        decrement();
        ptr = other.ptr;
        control_block = other.control_block;
        if (control_block) {
          ++control_block->shared_count;
        }
      }
      return *this;
    }

    template <typename U>
    SharedPtr(const SharedPtr<U>& other, T* alias_ptr) noexcept
            : ptr(alias_ptr)
            , control_block(other.control_block)
    {
      if (control_block) {
        ++control_block->shared_count;
      }
    }

    SharedPtr& operator=(SharedPtr&& other) noexcept {
      if (this != &other) {
        decrement();
        ptr = other.ptr;
        control_block = other.control_block;
        other.ptr = nullptr;
        other.control_block = nullptr;
      }
      return *this;
    }

    ~SharedPtr() { decrement(); }

    void reset() noexcept {
      decrement();
      ptr = nullptr;
      control_block = nullptr;
    }

    template <typename U, typename Deleter = std::default_delete<U>, typename Alloc = std::allocator<U>>
    void reset(U* pointer, Deleter d = Deleter(), Alloc a = Alloc()) {
      decrement();
      *this = SharedPtr(pointer, d, a);
    }

    void swap(SharedPtr& other) noexcept {
      std::swap(ptr, other.ptr);
      std::swap(control_block, other.control_block);
    }

    T* get() const noexcept { return ptr; }

    T& operator*() const noexcept { return *ptr; }

    T* operator->() const noexcept { return ptr; }

    size_t use_count() const noexcept {
      return control_block ? control_block->shared_count : 0;
    }

    explicit operator bool() const noexcept { return ptr != nullptr; }
};


template <typename T>
class WeakPtr {
    template <typename U>
    friend class WeakPtr;

    template <typename U>
    friend class SharedPtr;

public:
    WeakPtr() noexcept = default;

    template <typename U>
    WeakPtr(const SharedPtr<U>& sp) noexcept
            : ptr(sp.ptr), control_block(sp.control_block)
    {
      if (control_block != nullptr) {
        ++control_block->weak_count;
      }
    }

    WeakPtr(const WeakPtr& other) noexcept
            : ptr(other.ptr), control_block(other.control_block)
    {
      if (control_block != nullptr) {
        ++control_block->weak_count;
      }
    }

    template <typename U>
    WeakPtr(const WeakPtr<U>& other) noexcept
            : ptr(other.ptr), control_block(other.control_block)
    {
      if (control_block != nullptr) {
        ++control_block->weak_count;
      }
    }

    WeakPtr(WeakPtr&& other) noexcept
            : ptr(other.ptr), control_block(other.control_block)
    {
      other.ptr = nullptr;
      other.control_block = nullptr;
    }

    template <typename U>
    WeakPtr(WeakPtr<U>&& other) noexcept
            : ptr(other.ptr), control_block(other.control_block)
    {
      other.ptr = nullptr;
      other.control_block = nullptr;
    }

    ~WeakPtr() {
      if (control_block == nullptr) { return; }
      if (--control_block->weak_count == 0 && control_block->shared_count == 0) {
        control_block->deallocate();
      }
    }

    WeakPtr& operator=(const WeakPtr& other) noexcept {
      if (this != &other) {
        this->~WeakPtr();
        ptr = other.ptr;
        control_block = other.control_block;
        if (control_block != nullptr) {
          ++control_block->weak_count;
        }
      }
      return *this;
    }
    WeakPtr& operator=(WeakPtr&& other) noexcept {
      if (this != &other) {
        this->~WeakPtr();
        ptr = other.ptr;
        control_block = other.control_block;
        other.ptr = nullptr;
        other.control_block = nullptr;
      }
      return *this;
    }

    bool expired() const noexcept {
      return control_block == nullptr || control_block->shared_count == 0;
    }

    SharedPtr<T> lock() const noexcept {
      if (expired()) {
        return SharedPtr<T>();
      }
      ++control_block->shared_count;
      return SharedPtr<T>(control_block, ptr);
    }

    size_t use_count() const noexcept {
      return control_block ? control_block->shared_count : 0;
    }

private:
    T* ptr = nullptr;
    BaseControlBlock* control_block = nullptr;
};


template <typename T, typename Alloc, typename... Args>
SharedPtr<T> allocateShared(const Alloc& a, Args&&... args) {
  using CB = ControlBlockMakeShared<T, Alloc>;
  using CBAlloc = typename std::allocator_traits<Alloc>::template rebind_alloc<CB>;
  CBAlloc cbAlloc(a);
  auto* mem = std::allocator_traits<CBAlloc>::allocate(cbAlloc, 1);
  new (mem) CB(1, 0, a, std::forward<Args>(args)...);
  SharedPtr<T> sp;
  sp.control_block = mem;
  sp.ptr = &static_cast<CB*>(mem)->value;
  return sp;
}

template <typename T, typename... Args>
SharedPtr<T> makeShared(Args&&... args) {
  return allocateShared<T>(std::allocator<T>{}, std::forward<Args>(args)...);
}


