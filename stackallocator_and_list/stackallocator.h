#include "iterator"
#include "iostream"
#include "unordered_map"
#include "cstddef"
#include "memory"
#include "list"


template <size_t N>
struct StackStorage {
    alignas(std::max_align_t) std::byte storage[N];
    void* free_mem = storage;

    StackStorage() = default;

    StackStorage(const StackStorage& other) = delete;
};


template <typename T, size_t N>
struct StackAllocator {
    using propagate_on_container_copy_construction = std::false_type;
    using propagate_on_container_copy_assignment   = std::false_type;
    using propagate_on_container_move_assignment   = std::false_type;
    using value_type      = T;
    using pointer         = T*;
    using const_pointer   = const T*;
    using size_type       = std::size_t;
    using difference_type = std::ptrdiff_t;
    size_t align;
    StackStorage<N>* stackStorage;

    StackAllocator(): stackStorage() {}

    StackAllocator(StackStorage<N>& st_stor) noexcept
            : align(alignof(T))
            , stackStorage(&st_stor)
    {}

    template <typename U>
    StackAllocator(const StackAllocator<U, N>& other) noexcept
            : align(alignof(U))
            , stackStorage(other.stackStorage)
    {}

    ~StackAllocator() = default;

    template <typename U>
    StackAllocator& operator=(const StackAllocator<U, N>& other) noexcept {
      if (this == &other) { return *this; }
      align = other.align;
      stackStorage = other.stackStorage;
      return *this;
    }

    StackAllocator& operator=(const StackAllocator& other) noexcept {
      if (this == &other) { return *this; }
      align = other.align;
      stackStorage = other.stackStorage;
      return *this;
    }


    T* allocate(size_t n) {
      std::byte* start_of_mem_area;
      size_t end = stackStorage->storage + N -
                   reinterpret_cast<std::byte*>(stackStorage->free_mem);
      if (std::align(
              alignof(T),
              sizeof(T) * n,
              stackStorage->free_mem,
              end)) {
        start_of_mem_area = reinterpret_cast<std::byte*>(stackStorage->free_mem);
        stackStorage->free_mem = reinterpret_cast<std::byte*>(stackStorage->free_mem)
                                 + sizeof(T) * n;
      } else {
        throw std::bad_alloc();
      }
      return reinterpret_cast<T*>(start_of_mem_area);
    }

    void deallocate(T*, size_t) noexcept {
      /* do nothing */
    }

    template <typename U>
    struct rebind {
        using other = StackAllocator<U, N>;
    };

    bool operator==(const StackAllocator& other) const noexcept {
      return stackStorage == other.stackStorage;
    }

    bool operator!=(const StackAllocator& other) const noexcept {
      return !(*this == other);
    }

};


template<typename T, typename Allocator = std::allocator<T>>
class List {
    struct BaseNode {
        BaseNode* prev = this;
        BaseNode* next = this;

        BaseNode(BaseNode* p, BaseNode* n)
                : prev(p)
                , next(n)
        {}

        BaseNode() = default;

    };

    struct Node : BaseNode {
        T value;

        Node(BaseNode* p, BaseNode* n, const T& value)
                : BaseNode(p, n)
                , value(value)
        {}

        Node(BaseNode* p, BaseNode* n)
                : BaseNode(p, n)
                , value()
        {}

    };

    using type_allocator= std::allocator_traits<Allocator>::template rebind_alloc<Node>;
    size_t sz;
public:
    BaseNode endNode;
    [[no_unique_address]] type_allocator alloc;
    using value_type = T;



public:

    template<bool Const>
    struct base_iterator {
        using iterator_category = std::bidirectional_iterator_tag;
        using value_type        = std::conditional<Const, const T, T>::type;
        using reference         = std::conditional<Const, const T&, T&>::type;
        using pointer           = std::conditional<Const, const T*, T*>::type;
        using difference_type   = std::ptrdiff_t;
        using base_node_type    = std::conditional<Const, const BaseNode*, BaseNode*>::type;
        using node_type         = std::conditional<Const, const Node*, Node*>::type;
        BaseNode* node;

        base_iterator(BaseNode* nd)
                : node(nd)
        {}

        base_iterator(const base_iterator<false>& other)
                : node(other.node)
        {}

        base_iterator& operator++() {
          node = node->next;
          return *this;
        }

        base_iterator operator++(int) {
          base_iterator tmp = *this;
          ++(*this);
          return tmp;
        }

        base_iterator& operator--() {
          node = node->prev;
          return *this;
        }

        base_iterator& operator--(int) {
          base_iterator tmp = *this;
          --(*this);
          return tmp;
        }

        base_iterator& operator=(const base_iterator<false>& other) {
          if (this == &other) { return *this; }
          node = other.node;
          return *this;
        }

        bool operator==(const base_iterator& other) const = default;

        reference operator*() const {
          return static_cast<Node*>(node)->value;
        }

        pointer operator->() const {
          return &(static_cast<Node*>(node)->value);
        }


    };

    using iterator               = base_iterator<false>;
    using const_iterator         = base_iterator<true>;
    using reverse_iterator       = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;

    iterator begin() {
      return iterator(endNode.next);
    }

    iterator end() {
      return iterator(&endNode);
    }

    const_iterator begin() const {
      return cbegin();
    }

    const_iterator end() const {
      return cend();
    }

    const_iterator cbegin() const {
      return const_iterator(endNode.next);
    }

    const_iterator cend() const {
      return const_iterator(const_cast<BaseNode*>(&endNode));
    }

    reverse_iterator rbegin() {
      return reverse_iterator(end());
    }

    reverse_iterator rend() {
      return reverse_iterator(begin());
    }

    const_reverse_iterator rbegin() const {
      return const_reverse_iterator(cend());
    }

    const_reverse_iterator rend() const {
      return const_reverse_iterator(cbegin());
    }

    const_reverse_iterator rcbegin() const {
      return const_reverse_iterator(cend());
    }

    const_reverse_iterator rcend() const {
      return const_reverse_iterator(cbegin());
    }

    iterator insert(const_iterator position, const T& value) {
      Node* newNode = std::allocator_traits<type_allocator>::allocate(alloc, 1);
      try {
        std::allocator_traits<type_allocator>::construct(
                alloc,
                newNode,
                position.node->prev,
                position.node,
                value
        );
        ++sz;
      } catch (...) {
        std::allocator_traits<type_allocator>::deallocate(alloc, newNode, 1);
        throw;
      }
      position.node->prev->next = newNode;
      newNode->next = position.node;
      newNode->prev = position.node->prev;
      position.node->prev = newNode;
      return iterator(newNode);
    }

    iterator erase(const_iterator position) {
      BaseNode* prev_node = position.node->prev;
      BaseNode* next_node = position.node->next;
      next_node->prev = prev_node;
      prev_node->next = next_node;
      --sz;
      std::allocator_traits<type_allocator>::destroy(alloc, static_cast<Node*>(position.node));
      std::allocator_traits<type_allocator>::deallocate(alloc, static_cast<Node*>(position.node), 1);
      return iterator(next_node);
    }


    Allocator get_allocator() const noexcept {
      return alloc;
    }

    List(const Allocator& _alloc = Allocator())
            : sz(0)
            , endNode()
            , alloc(_alloc)
    {
      endNode.prev = &endNode;
      endNode.next = &endNode;
    }

    List(size_t n, const Allocator& _alloc = Allocator())
            : sz(0)
            , endNode()
            , alloc(_alloc)
    {
      size_t count = 0;
      for (; n > 0; --n) {
        Node* newNode = std::allocator_traits<type_allocator>::allocate(alloc, 1);
        try {
          std::allocator_traits<type_allocator>::construct(alloc,
                                                           newNode,
                                                           endNode.prev,
                                                           &endNode);
          if (endNode.prev != nullptr) {
            endNode.prev->next = reinterpret_cast<BaseNode*>(newNode);
          }
          endNode.prev = reinterpret_cast<BaseNode*>(newNode);
          ++sz;
        } catch (...) {
          std::allocator_traits<type_allocator>::deallocate(alloc, newNode, 1);
          for (; count > 0; --count) {
            pop_back();
          }
          throw;
        }
        ++count;
      }
    }

    List(size_t n, const T& x, const Allocator& _alloc = Allocator())
            : sz(0)
            , endNode()
            , alloc(_alloc)
    {
      size_t count = 0;
      for (; n > 0; --n) {
        try {
          push_back(x);
        } catch (...) {
          for (; count > 0; --count) {
            pop_back();
          }
          throw;
        }
        ++count;
      }
    }

    List(const List& other)
            : sz(0)
            , endNode()
            , alloc(std::allocator_traits<Allocator>
                    ::select_on_container_copy_construction(other.alloc)) {
      size_t count = 0;
      try {
        for (auto it = other.begin(); it != other.end(); ++it) {
          push_back(*it);
          ++count;
        }
      } catch(...) {
        for (; count > 0; --count) {
          pop_back();
        }
        throw;
      }
    }

    List& operator=(const List& other) {
      if (this == &other) { return *this; }
      List tmp;
      if (std::allocator_traits<type_allocator>
      ::propagate_on_container_copy_assignment::value) {
        tmp.alloc = other.alloc;
      } else {
        tmp.alloc = alloc;
      }
      for (const_iterator it = other.begin(); it != other.end(); ++it) {
        tmp.push_back(*it);
      }
      if constexpr (std::allocator_traits<Allocator>
      ::propagate_on_container_copy_assignment::value) {
        std::swap(alloc, tmp.alloc);
      }
      std::swap((--tmp.end()).node->next, endNode.prev->next);
      std::swap((tmp.begin()).node->prev, endNode.next->prev);
      std::swap(endNode, tmp.endNode);
      std::swap(sz, tmp.sz);
      return *this;
    }

    ~List() {
      for (; sz > 0;)
        pop_back();
    }

    size_t size() const noexcept {
      return sz;
    }

    bool empty() const noexcept {
      return sz == 0;
    }

    void push_back(const T& value) {
      Node* newNode = std::allocator_traits<type_allocator>::allocate(alloc, 1);
      try {
        std::allocator_traits<type_allocator>::construct(alloc, newNode, endNode.prev, &endNode, value);
      } catch (...) {
        alloc.deallocate(newNode, 1);
        throw;
      }
      if (endNode.prev != nullptr) {
        endNode.prev->next = newNode;
      }
      endNode.prev = newNode;
      ++sz;
    }

    void push_front(const T& value) {
      Node* newNode = alloc.allocate(1);
      try {
        std::allocator_traits<type_allocator>::construct(alloc, newNode, &endNode, endNode.next, value);
      } catch (...) {
        alloc.deallocate(newNode, 1);
        throw;
      }
      if (endNode.next != nullptr) {
        endNode.next->prev = newNode;
      }
      endNode.next = newNode;
      ++sz;
    }

    void pop_back() noexcept {
      if (sz == 0)
        return;
      Node* toRemove = static_cast<Node*>(endNode.prev);
      endNode.prev = toRemove->prev;
      if (endNode.prev != nullptr) {
        endNode.prev->next = &endNode;
      }
      std::allocator_traits<type_allocator>::destroy(alloc, toRemove);
      alloc.deallocate(toRemove, 1);
      --sz;
    }

    void pop_front() noexcept {
      if (sz == 0)
        return;
      Node* toRemove = static_cast<Node*>(endNode.next);
      endNode.next = toRemove->next;
      if (endNode.next != nullptr) {
        endNode.next->prev = &endNode;
      }
      std::allocator_traits<type_allocator>::destroy(alloc, toRemove);
      alloc.deallocate(toRemove, 1);
      --sz;
    }
};
