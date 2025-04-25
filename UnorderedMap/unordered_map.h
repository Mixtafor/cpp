#include "iterator"
#include "iostream"
#include "cstddef"
#include "memory"
#include "vector"

#include <cmath>
#include <utility>
#include <cstddef>
#include <stdexcept>
#include <type_traits>
#include <iterator>
#include <utility>
#include <stdexcept>
#include <concepts>


template<typename Key,
        typename Value,
        typename Hash = std::hash<Key>,
        typename Equal = std::equal_to<Key>,
        typename Alloc = std::allocator<std::pair<const Key, Value>>>
class UnorderedMap {
public:
    using size_type = size_t;
    using NodeType = std::pair<const Key, Value>;

    struct BaseNode {
        BaseNode* next = this;
        BaseNode* prev = this;
    };

    struct Node : BaseNode {
        NodeType kv;
        size_t hash;
    };

    using AllocTraits = std::allocator_traits<Alloc>;
    using NodeAlloc = typename AllocTraits::template rebind_alloc<Node>;
    [[no_unique_address]] Hash hash_{};
    [[no_unique_address]] Alloc alloc_{};
    [[no_unique_address]] Equal eq_{};
    BaseNode node_list{};
    size_type bucket_count = 0;
    size_type elem_count = 0;
    float max_load_factor = 0.4f;
    std::vector<Node*, typename AllocTraits::template rebind_alloc<Node*>> buckets{alloc_};

    template<bool Const>
    struct base_iterator {
        using iterator_category = std::bidirectional_iterator_tag;
        using value_type        = NodeType;
        using reference         = std::conditional_t<Const, const NodeType&, NodeType&>;
        using pointer           = std::conditional_t<Const, const NodeType*, NodeType*>;
        using difference_type   = std::ptrdiff_t;
        using base_node_type    = std::conditional_t<Const, const Node*, Node*>;
        using node_type         = std::conditional_t<Const, const Node*, Node*>;
        BaseNode* node;

        base_iterator(BaseNode* nd) : node(nd) {}

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

        base_iterator operator--(int) {
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
          return static_cast<Node*>(node)->kv;
        }

        pointer operator->() const {
          return &(static_cast<Node*>(node)->kv);
        }
    };

    using iterator               = base_iterator<false>;
    using const_iterator         = base_iterator<true>;
    using reverse_iterator       = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;

    iterator begin() {
      return iterator(node_list.next);
    }

    iterator end() {
      return iterator(&node_list);
    }

    const_iterator begin() const {
      return cbegin();
    }

    const_iterator end() const {
      return cend();
    }

    const_iterator cbegin() const {
      return const_iterator(node_list.next);
    }

    const_iterator cend() const {
      return const_iterator(const_cast<BaseNode*>(&node_list));
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


    void insert_node(BaseNode* newNode) {
      size_t ind = static_cast<Node*>(newNode)->hash % bucket_count;
      if (buckets[ind] == nullptr) {
        BaseNode* next_node = node_list.next;
        node_list.next = newNode;
        newNode->prev = &node_list;
        newNode->next = next_node;
        next_node->prev = newNode;
        buckets[ind] = static_cast<Node*>(newNode);
      } else {
        BaseNode* head = buckets[ind];
        BaseNode* prev_node = head->prev;
        prev_node->next = newNode;
        newNode->prev = prev_node;
        newNode->next = head;
        head->prev = newNode;
        buckets[ind] = static_cast<Node*>(newNode);
      }
    }

    void reserve(size_t count) {
      size_t new_bucket_count = static_cast<size_t>(std::ceil(static_cast<long double>(count) / max_load_factor));
      rehash(new_bucket_count);
    }


    template <typename... Args>
    std::pair<iterator, bool> emplace(Args&&... args) {
      NodeAlloc node_alloc(alloc_);
      Node* newNode = std::allocator_traits<NodeAlloc>::allocate(node_alloc, 1);
      try {
        std::allocator_traits<Alloc>::construct(
                alloc_,
                &newNode->kv,
                std::forward<Args>(args)...
        );
      } catch (...) {
        std::allocator_traits<NodeAlloc>::deallocate(node_alloc, newNode, 1);
        throw;
      }
      try {
        newNode->hash = hash_(newNode->kv.first);
      } catch (...) {
        std::allocator_traits<Alloc>::destroy(alloc_, &newNode->kv);
        std::allocator_traits<NodeAlloc>::deallocate(node_alloc, newNode, 1);
      }
      if (bucket_count == 0) {
        reserve(10);
      }

      iterator it = nullptr;
      try {
        it = find(newNode->kv.first);
      } catch (...) {
        std::allocator_traits<Alloc>::destroy(alloc_, &newNode->kv);
        std::allocator_traits<NodeAlloc>::deallocate(node_alloc, newNode, 1);
      }
      if (it != end()) {
        std::allocator_traits<Alloc>::destroy(alloc_, &newNode->kv);
        std::allocator_traits<NodeAlloc>::deallocate(node_alloc, newNode, 1);
        return { it, false };
      }
      insert_node(newNode);
      ++elem_count;
      if (static_cast<float>(elem_count) / bucket_count > max_load_factor) {
        rehash(2 * bucket_count);
      }
      return { iterator(newNode), true };
    }

    void rehash(size_t n) {
      size_type new_bucket_count = n;
      if (elem_count > 0) {
        size_type required_buckets = static_cast<size_type>
                (std::ceil(static_cast<long double>(elem_count) / max_load_factor));
        new_bucket_count = std::max({n, required_buckets, static_cast<size_type>(8)});
      } else {
        new_bucket_count = std::max(n, static_cast<size_type>(8));
      }

      if (new_bucket_count == bucket_count) {
        return;
      }

      buckets.assign(new_bucket_count, nullptr);
      bucket_count = new_bucket_count;

      if (elem_count == 0) {
        return;
      }

      BaseNode* cur = node_list.next;
      while (cur != &node_list) {
        Node* node = static_cast<Node*>(cur);
        size_t ind = node->hash % bucket_count;

        if (buckets[ind] == nullptr) {
          buckets[ind] = node;
        }
        cur = cur->next;
      }
    }

    iterator find(const Key& key) {
      if (bucket_count == 0)
        return end();
      size_t h = hash_(key);
      size_t ind = h % bucket_count;
      BaseNode* curr = buckets[ind];
      while (curr != nullptr && curr != &node_list) {
        Node* node = static_cast<Node*>(curr);
        if (node->hash % bucket_count != ind)
          break;
        if (eq_(node->kv.first, key))
          return iterator(curr);
        curr = curr->next;
      }
      return end();
    }

    const_iterator find(const Key& key) const {
      if (bucket_count == 0)
        return cend();
      size_t h = hash_(key);
      size_t ind = h % bucket_count;
      BaseNode* curr = buckets[ind];
      while (curr != nullptr && curr != &node_list) {
        Node* node = static_cast<Node*>(curr);
        if (node->hash % bucket_count != ind)
          break;
        if (eq_(node->kv.first, key))
          return const_iterator(curr);
        curr = curr->next;
      }
      return cend();
    }

    Value& operator[](const Key& key) {
      auto res = emplace(key, Value());
      return res.first->second;
    }

    Value& operator[](Key&& key) {
      auto res = emplace(std::move(key), Value());
      return res.first->second;
    }

    Value& at(const Key& key) {
      auto it = find(key);
      if (it == end())
        throw std::out_of_range("Key not found");
      return it->second;
    }

    const Value& at(const Key& key) const {
      auto it = find(key);
      if (it == cend())
        throw std::out_of_range("Key not found");
      return it->second;
    }

    size_type size() const noexcept { return elem_count; }

    bool empty() const noexcept { return elem_count == 0; }

    std::pair<iterator, bool> insert(const NodeType& value) {
      return emplace(value.first, value.second);
    }

    std::pair<iterator, bool> insert(NodeType&& value) {
      return emplace(value.first, std::move(value.second));
    }

    template <typename P>
    std::pair<iterator, bool> insert(P&& value) {
      return emplace(std::forward<P>(value));
    }

    template <std::input_iterator InputIterator>
    void insert(InputIterator first, InputIterator last) {
      for (; first != last; ++first) {
        insert(*first);
      }
    }


    iterator erase(iterator pos) noexcept {
      NodeAlloc node_alloc(alloc_);
      Node* node_to_remove = static_cast<Node*>(pos.node);
      node_to_remove->prev->next = node_to_remove->next;
      node_to_remove->next->prev = node_to_remove->prev;
      size_t ind = node_to_remove->hash % bucket_count;
      if (buckets[ind] == node_to_remove) {
        BaseNode* candidate = node_to_remove->next;
        if (candidate == &node_list ||
            (static_cast<Node*>(candidate)->hash % bucket_count != ind)) {
          buckets[ind] = nullptr;
        }
        else {
          buckets[ind] = static_cast<Node*>(candidate);
        }
      }
      iterator next_it(node_to_remove->next);
      std::allocator_traits<Alloc>::destroy(alloc_, &node_to_remove->kv);
      std::allocator_traits<NodeAlloc>::deallocate(node_alloc, node_to_remove, 1);
      --elem_count;
      return next_it;
    }

    iterator erase(const_iterator pos) noexcept {
      NodeAlloc node_alloc(alloc_);
      Node* node_to_remove = static_cast<Node*>(pos.node);
      node_to_remove->prev->next = node_to_remove->next;
      node_to_remove->next->prev = node_to_remove->prev;
      size_t ind = node_to_remove->hash % bucket_count;
      if (buckets[ind] == node_to_remove) {
        BaseNode* candidate = node_to_remove->next;
        if (candidate == &node_list ||
            (static_cast<Node*>(candidate)->hash % bucket_count != ind)) {
          buckets[ind] = nullptr;
        }
        else {
          buckets[ind] = candidate;
        }
      }
      iterator next_it(node_to_remove->next);
      std::allocator_traits<Alloc>::destroy(alloc_, &node_to_remove->kv);
      std::allocator_traits<NodeAlloc>::deallocate(node_alloc, node_to_remove, 1);
      --elem_count;
      return next_it;
    }


    template <std::input_iterator InputIterator>
    iterator erase(InputIterator first, InputIterator last) {
      iterator current = iterator(const_cast<BaseNode*>(first.node));
      iterator end_it = iterator(const_cast<BaseNode*>(last.node));

      while (current != end_it) {
        current = erase(current);
      }
      return current;
    }

    float load_factor() const {
      return bucket_count ? static_cast<float>(elem_count) / bucket_count : 0.f;
    }
    float max_load_factor_value() const { return max_load_factor; }

    void max_load_factor_value(float ml) { max_load_factor = ml; }


    void swap(UnorderedMap& other) noexcept(
    AllocTraits::propagate_on_container_swap::value ||
    AllocTraits::is_always_equal::value)
    {
      using std::swap;
      swap(hash_, other.hash_);
      swap(eq_, other.eq_);
      swap(max_load_factor, other.max_load_factor);
      if constexpr (AllocTraits::propagate_on_container_swap::value) {
        swap(alloc_, other.alloc_);
      }
      swap(buckets, other.buckets);
      swap(bucket_count, other.bucket_count);
      swap(elem_count, other.elem_count);

      swap(node_list, other.node_list);

      if (node_list.next != &other.node_list) {
        node_list.next->prev = &node_list;
      } else {
        node_list.next = &node_list;
      }
      if (node_list.prev != &other.node_list) {
        node_list.prev->next = &node_list;
      } else {
        node_list.prev = &node_list;
      }

      if (other.node_list.next != &node_list) {
        other.node_list.next->prev = &other.node_list;
      } else {
        other.node_list.next = &other.node_list;
      }
      if (other.node_list.prev != &node_list) {
        other.node_list.prev->next = &other.node_list;
      } else {
        other.node_list.prev = &other.node_list;
      }
    }


    void clear() noexcept {
      if (elem_count == 0) {
        return;
      }
      BaseNode* current = node_list.next;
      while (current != &node_list) {
        BaseNode* next = current->next;
        Node* nodePtr = static_cast<Node*>(current);
        NodeAlloc node_alloc(alloc_);
        std::allocator_traits<Alloc>::destroy(alloc_, &nodePtr->kv);
        std::allocator_traits<NodeAlloc>::deallocate(node_alloc, nodePtr, 1);
        current = next;
      }
      node_list.next = &node_list;
      node_list.prev = &node_list;
      elem_count = 0;
      std::fill(buckets.begin(), buckets.end(), nullptr);
    }

    explicit UnorderedMap(size_type bucketCount = 8,
                          const Hash& hash = Hash(),
                          const Equal& equal = Equal(),
                          const Alloc& alloc = Alloc())
            : hash_(hash), alloc_(alloc), eq_(equal), elem_count(0)
    {
      rehash(bucketCount);
    }

    UnorderedMap(const UnorderedMap& other)
            : hash_(other.hash_),
              alloc_(AllocTraits::select_on_container_copy_construction(other.alloc_)),
              eq_(other.eq_),
              bucket_count(0),
              elem_count(0),
              max_load_factor(other.max_load_factor),
              buckets(alloc_)
    {
      reserve(other.elem_count);
      for (const auto& kv : other) {
        insert({ kv.first, kv.second });
      }
    }


    UnorderedMap& operator=(const UnorderedMap& other) {
      if (this == &other) {
        return *this;
      }
      UnorderedMap temp(other);
      swap(temp);
      if constexpr (AllocTraits::propagate_on_container_copy_assignment::value)
        alloc_ = other.alloc_;
      return *this;
    }


    UnorderedMap(UnorderedMap&& other) noexcept
            : hash_(std::move(other.hash_)),
              alloc_(std::move(other.alloc_)),
              eq_(std::move(other.eq_)),
              node_list{},
              bucket_count(other.bucket_count),
              elem_count(other.elem_count),
              max_load_factor(other.max_load_factor),
              buckets(std::move(other.buckets))
    {
      node_list = other.node_list;

      if (node_list.next != &other.node_list) {
        node_list.next->prev = &node_list;
      } else {
        node_list.next = &node_list;
      }
      if (node_list.prev != &other.node_list) {
        node_list.prev->next = &node_list;
      } else {
        node_list.prev = &node_list;
      }

      other.node_list.next = &other.node_list;
      other.node_list.prev = &other.node_list;
      other.bucket_count = 0;
      other.elem_count = 0;
    }

    UnorderedMap& operator=(UnorderedMap&& other) noexcept (
    AllocTraits::propagate_on_container_move_assignment::value ||
    AllocTraits::is_always_equal::value)
    {
      if (this == &other) {
        return *this;
      }
      clear();
      hash_ = std::move(other.hash_);
      alloc_ = std::move(other.alloc_);
      eq_ = std::move(other.eq_);
      max_load_factor = other.max_load_factor;

      if constexpr (AllocTraits::propagate_on_container_move_assignment::value) {
        buckets = std::move(other.buckets);
        node_list = other.node_list;
        bucket_count = other.bucket_count;
        elem_count = other.elem_count;

      } else {
        if (alloc_ == other.alloc_) {
          buckets = std::move(other.buckets);
          node_list = other.node_list;
          bucket_count = other.bucket_count;
          elem_count = other.elem_count;
        } else {
          bucket_count = 0;
          elem_count = 0;
          node_list.next = &node_list;
          node_list.prev = &node_list;
          buckets.clear();
          rehash(8);
          other.clear();
          return *this;
        }
      }

      if (node_list.next != &other.node_list) {
        node_list.next->prev = &node_list;
      } else {
        node_list.next = &node_list;
      }
      if (node_list.prev != &other.node_list) {
        node_list.prev->next = &node_list;
      } else {
        node_list.prev = &node_list;
      }

      other.node_list.next = &other.node_list;
      other.node_list.prev = &other.node_list;
      other.bucket_count = 0;
      other.elem_count = 0;

      return *this;
    }


    ~UnorderedMap() {
      clear();
    }

};

template<typename Key, typename Value, typename Hash, typename Equal, typename Alloc>
void swap(UnorderedMap<Key, Value, Hash, Equal, Alloc>& lhs,
          UnorderedMap<Key, Value, Hash, Equal, Alloc>& rhs) noexcept(
noexcept(lhs.swap(rhs))) {
  lhs.swap(rhs);
}
