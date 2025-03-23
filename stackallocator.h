
#include "iterator"
#include "iostream"
#include "unordered_map"
#include "cstddef"
#include "memory"
#include "list"


template <size_t N>
struct StackStorage {
    std::byte storage[N];
    size_t free_mem = 0;

    StackStorage() = default;

    StackStorage(StackStorage& other) = delete;
};


template <typename T, size_t N>
struct StackAllocator {
    using propagate_on_container_copy_construction = std::false_type;
    using value_type                               = T;
    size_t align;
    StackStorage<N>* stackStorage;

    explicit StackAllocator(StackStorage<N>& st_stor) noexcept
            : align(alignof(T))
            , stackStorage(&st_stor)
    {}

    template <typename U>
    explicit StackAllocator(const StackAllocator<U, N>& other) noexcept
            : align(alignof(U))
            , stackStorage(other.stackStorage)
    {}

    ~StackAllocator() = default;

    template <typename U>
    StackAllocator& operator=(const StackAllocator<U, N>& other) noexcept {
      align = other.align;
      stackStorage = other.stackStorage;
      return *this;
    }

    T* allocate(size_t n) noexcept {
      size_t start_of_mem_area =
              stackStorage->free_mem + (align - (stackStorage->free_mem % align)) % align;
      stackStorage->free_mem = start_of_mem_area + n * sizeof(T);

      return reinterpret_cast<T*>(stackStorage->storage + start_of_mem_area);
    }

    void deallocate(T*, size_t) noexcept {
      /* do nothing */
    }

    template <typename U>
    struct rebind {
        using other = StackAllocator<U, N>;
    };

};


template<typename T, typename Allocator = std::allocator<T>>
class List {
    struct BaseNode {
        BaseNode* prev;
        BaseNode* next;

        BaseNode(BaseNode* p, BaseNode* n)
                : prev(p)
                , next(n)
        {}

        BaseNode()
                : prev(nullptr)
                , next(nullptr)
        {}

    };

    struct Node : BaseNode {
        T value;

        Node(BaseNode* p, BaseNode* n, const T& value)
                : BaseNode(p, n)
                , value(value)
        {}

        Node(BaseNode* p, BaseNode* n)
                : BaseNode(p, n)
        {}

    };

    using type_allocator= std::allocator_traits<Allocator>::template rebind_alloc<Node>;
    [[no_unique_address]] type_allocator alloc;
    size_t sz;
public:
    BaseNode endNode;
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
        base_node_type node;

        explicit base_iterator(base_node_type nd)
                : node(nd)
        {}

        base_iterator(const base_iterator<Const>& other)
                : node(other.node)
        {}

        base_iterator& operator++() {
          node = node->next;
          return *this;
        }

        base_iterator& operator++(int) {
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
          node = other.node;
          return *this;
        }

        operator base_iterator<true>() const {
          return base_iterator<true>(node);
        }

        bool operator==(const base_iterator<Const>& other) const {
          return node == other.node;
        }

        bool operator!=(const base_iterator<Const>& other) const {
          return node != other.node;
        }

        reference operator*() const {
          return reinterpret_cast<node_type>(node)->value;
        }

        pointer operator->() const {
          return &(reinterpret_cast<node_type>(node)->value);
        }


    };

    using iterator               = base_iterator<false>;
    using const_iterator         = base_iterator<true>;
    using reverse_iterator       = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;

    iterator begin() {
      return (iterator(endNode.next));
    }

    iterator end() {
      return (iterator(&endNode));
    }

    const_iterator begin() const {
      return (const_iterator(endNode.next));
    }

    const_iterator end() const {
      return (const_iterator(&endNode));
    }

    const_iterator cbegin() const {
      return begin();
    }

    const_iterator cend() const {
      return end();
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

//    iterator insert(const_iterator position, const T& value) {
//      Node* newNode = std::allocator_traits<type_allocator>::allocate(1);
//      try {
//        std::allocator_traits<type_allocator>::construct(alloc, &(newNode->value), value);
//        ++sz;
//      } catch (...) {
//        std::allocator_traits<type_allocator>::deallocate(alloc, newNode, 1);
//        throw;
//      }
//      position.node->prev->next = reinterpret_cast<BaseNode*>(newNode);
//      reinterpret_cast<BaseNode*>(newNode)->next = position.node;
//      reinterpret_cast<BaseNode*>(newNode)->prev = position.node->prev;
//      position.node->prev = reinterpret_cast<BaseNode*>(newNode);
//      return iterator(*reinterpret_cast<BaseNode*>(newNode));
//    }
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
      std::allocator_traits<type_allocator>
              ::destroy(alloc, reinterpret_cast<Node*>(position.node));
      iterator it(*(position.node->next));
      --sz;
      position.node->prev->next = position.node->next;
      position.node->next->prev = position.node->prev;
      std::allocator_traits<type_allocator>
              ::deallocate(alloc, reinterpret_cast<Node*>(position.node), 1);
      return it;
    }


    Allocator get_allocator() const noexcept {
      return alloc;
    }

//    List()
//    : sz(0)
//    , endNode{nullptr, nullptr}
//    {};

//    explicit List(size_t n) {
//      size_t count = 0;
//      for (; n > 0; --n) {
//        value_type t_obj;
//        try {
//          t_obj = value_type();
//        } catch (...) {
//          for (; count > 0; --count) {
//            pop_back();
//          }
//          throw;
//        }
//        push_back(t_obj);
//        ++count;
//      }
//    }
//
//    explicit List(size_t n, const T& x) {
//      for (; n > 0; --n)
//        push_back(x);
//    }

    explicit List(const Allocator& alloc = Allocator())
            : sz(0)
            , endNode()
            , alloc(alloc)
    {
      endNode.prev = &endNode;
      endNode.next = &endNode;
    }

    explicit List(size_t n, const Allocator& alloc = Allocator())
            : sz(0)
            , endNode()
            , alloc(alloc)
    {
      size_t count = 0;
      for (; n > 0; --n) {
        //value_type t_obj;
        Node* newNode = std::allocator_traits<type_allocator>::allocate(alloc, 1);
        try {
//          t_obj = value_type();
//          push_back(t_obj);
          std::allocator_traits<type_allocator>::construct(alloc, &(newNode->value));
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

    explicit List(size_t n, const T& x, const Allocator& alloc = Allocator())
            : sz(0)
            , endNode()
            , alloc(alloc)
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
      for (auto it = other.begin(); it != other.end(); ++it) {
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
      Node* newNode = alloc.allocate(1);
      try {
        std::allocator_traits<type_allocator>::construct(alloc, newNode, endNode.prev, &endNode, value);
      } catch (...) {
        alloc.deallocate(newNode, 1);
        throw;
      }
      if (endNode.prev != nullptr) {
        endNode.prev->next = (newNode);
      }
      endNode.prev = (newNode);
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
      if (endNode.prev != nullptr) {
        endNode.prev->next = reinterpret_cast<BaseNode*>(newNode);
      }
      endNode.prev = reinterpret_cast<BaseNode*>(newNode);
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


