#ifndef CIRCULAR_BUFFER_H
#define CIRCULAR_BUFFER_H
#include <array>
#include <iterator>
#include <limits>
#include <new>
#include <stdexcept>
#include <cstddef>
#include <stdexcept>
#include <iostream>
#include <type_traits>


// Default capacity for the dynamic buffer.
constexpr size_t DYNAMIC_CAPACITY = std::numeric_limits<size_t>::max();


template <typename T, size_t Capacity>
class Container {
public:

    // fields
    std::array<char, Capacity * sizeof(T)> mem_{};
    T* arr_ = nullptr;
    static constexpr size_t capacity_value = Capacity;

    // constructors

    Container() {
      arr_ = reinterpret_cast<T*>(mem_.data());
    }

    Container(const size_t cap) : Container() {
      if (cap != Capacity) {
        throw std::invalid_argument("Capacity mismatch");
      }
    }

    // methods

    size_t capacity() const noexcept {
      return capacity_value;
    }

    T& operator[](size_t idx) noexcept {
      return arr_[idx];
    }

    const T& operator[](size_t idx) const noexcept {
      return arr_[idx];
    }

    Container(const Container& other) noexcept : mem_(other.mem_) {
      arr_ = reinterpret_cast<T*>(mem_.data());
    }

    Container& operator=(const Container& other) noexcept {
      if (this != &other) {
        arr_ = reinterpret_cast<T*>(mem_.data());
      }
      return *this;
    }

};


// Specialization for dynamic capacity 
template <typename T>
class Container<T, DYNAMIC_CAPACITY> {
public:

    // fields
    size_t capacity_;
    T* arr_;

    // constructor
    Container(const size_t cap) : capacity_(cap) {
      char* mem = new char[cap * sizeof(T)];
      arr_ = reinterpret_cast<T*>(mem);
    }

    // methods
    size_t capacity() const {
      return capacity_;
    }

    T& operator[](size_t idx) {
      return arr_[idx];
    }

    const T& operator[](size_t idx) const {
      return arr_[idx];
    }

    ~Container() {
      char* mem = reinterpret_cast<char*>(arr_);
      delete[] mem;
    }

    Container(const Container& other) : capacity_(other.capacity_) {
      char* mem = new char[capacity_ * sizeof(T)];
      arr_ = reinterpret_cast<T*>(mem);
    }

    Container& operator=(const Container& other) {
      if (this != &other) {
        char* old_mem = reinterpret_cast<char*>(arr_);
        delete[] old_mem;
        capacity_ = other.capacity_;
        char* mem = new char[capacity_ * sizeof(T)];
        arr_ = reinterpret_cast<T*>(mem);
      }
      return *this;
    }
};

// Base class for iterators with fixed capacity
template <size_t Capacity, bool Const>
struct IteratorBase {
    static constexpr size_t capacity_ = Capacity;
};

// Base class for iterators with dynamic capacity
template <bool Const>
struct IteratorBase<DYNAMIC_CAPACITY, Const> {
    size_t capacity_;
    IteratorBase(size_t cap) noexcept : capacity_(cap) {}
    IteratorBase() noexcept = default;
};



template <typename T, size_t Capacity = DYNAMIC_CAPACITY>
class CircularBuffer {
public:
    
    // Iterator class for CircularBuffer
    template<bool Const>
    class Iterator : public IteratorBase<Capacity, Const>{
        private:
        using buffer_ptr = std::conditional_t<Const, const T*, T*>;
        buffer_ptr base_;
        size_t front_index_;
        size_t offset_;
        template<bool C2> friend class Iterator;

        public:
        using iterator_category = std::random_access_iterator_tag;
        using value_type        = T;
        using difference_type   = std::ptrdiff_t;
        using reference         = std::conditional_t<Const, const T&, T&>;
        using pointer           = std::conditional_t<Const, const T*, T*>;


        // Constructor for dynamic capacity
        Iterator(buffer_ptr base, size_t cap, size_t front_index, size_t offset)
        noexcept requires(Capacity == DYNAMIC_CAPACITY)
                : IteratorBase<Capacity, Const>(cap),
                  base_(base),
                  front_index_(front_index),
                  offset_(offset) {}

        // Copy constructor from non-const to const iterator for dynamic capacity
        template <bool C = Const, typename = std::enable_if_t<C>>
        Iterator(const Iterator<false>& other) noexcept
        requires(Capacity == DYNAMIC_CAPACITY)
                : IteratorBase<Capacity, Const>(other.capacity_),
                  base_(other.base_),
                  front_index_(other.front_index_),
                  offset_(other.offset_) {}

        // Constructor for fixed capacity
        Iterator(buffer_ptr base, size_t cap, size_t front_index, size_t offset)
        requires(Capacity != DYNAMIC_CAPACITY)
                : IteratorBase<Capacity, Const>(),
                  base_(base),
                  front_index_(front_index),
                  offset_(offset) {
                    if (cap != Capacity) {
                        throw std::runtime_error("Capacity mismatch");
                    }
                  }

        // Copy constructor from non-const to const iterator for fixed capacity
        template <bool C = Const, typename = std::enable_if_t<C>>
        Iterator(const Iterator<false>& other)
        noexcept requires(Capacity != DYNAMIC_CAPACITY)
                : IteratorBase<Capacity, Const>(),
                  base_(other.base_),
                  front_index_(other.front_index_),
                  offset_(other.offset_) {}


        Iterator() noexcept = default;

        reference operator*() const noexcept {
          size_t idx = front_index_ + offset_;
          if (idx >= this->capacity_) { idx -= this->capacity_; }
          return base_[idx];
        }
        pointer operator->() const noexcept {
          return &(**this);
        }

        Iterator& operator++()    noexcept { ++offset_; return *this; }
        Iterator  operator++(int) noexcept { Iterator tmp(*this); ++offset_; return tmp; }
        Iterator& operator--()    noexcept { --offset_; return *this; }
        Iterator  operator--(int) noexcept { Iterator tmp(*this); --offset_; return tmp; }

        Iterator& operator+=(difference_type n) noexcept { offset_ += n; return *this; }
        Iterator& operator-=(difference_type n) noexcept { offset_ -= n; return *this; }
        Iterator  operator+(difference_type n)  const noexcept { Iterator it(*this); it += n; return it; }
        Iterator  operator-(difference_type n)  const noexcept { Iterator it(*this); it -= n; return it; }

        difference_type operator-(const Iterator& other) const noexcept {
          return difference_type(offset_) - difference_type(other.offset_);
        }

        bool operator==(const Iterator& other) const noexcept {
          return offset_ == other.offset_ && base_ == other.base_ && front_index_ == other.front_index_;
        }
        bool operator!=(const Iterator& other) const noexcept { return !(*this == other); }
        bool operator< (const Iterator& other) const noexcept { return offset_ < other.offset_ && base_ == other.base_ && front_index_ == other.front_index_; }
        bool operator> (const Iterator& other) const noexcept { return other < *this; }
        bool operator<=(const Iterator& other) const noexcept { return !(other < *this); }
        bool operator>=(const Iterator& other) const noexcept { return !(*this < other); }
    };


    // fields
    Container<T, Capacity> cont_;
    using iterator                 = Iterator<false>;
    using const_iterator           = Iterator<true>;
    using reverse_iterator         = std::reverse_iterator<iterator>;
    using const_reverse_iterator   = std::reverse_iterator<const_iterator>;
    size_t size_;
    size_t front_index_;


    // constructors
    CircularBuffer() noexcept requires (Capacity != DYNAMIC_CAPACITY) : cont_(),
                                                               size_(0),
                                                               front_index_(0) {}

    CircularBuffer(size_t cap) requires (Capacity != DYNAMIC_CAPACITY) : CircularBuffer() {
      if (cap != Capacity) {
        throw std::invalid_argument("Capacity mismatch");
      }
    }

    CircularBuffer(size_t cap) noexcept requires (Capacity == DYNAMIC_CAPACITY)
            : cont_(cap),
              size_(0),
              front_index_(0) {}


    CircularBuffer(const CircularBuffer<T, Capacity>& other)
                      : cont_(other.cont_), size_(other.size_), front_index_(other.front_index_) {
                size_t constructed = 0;
                try {
                  for (size_t i = 0; i < size_; ++i) {
                    new(cont_.arr_ + (front_index_ + i) % cont_.capacity()) T(other[i]);
                    ++constructed;
                  }
                } catch (...) {
                  for (size_t j = 0; j < constructed; ++j) {
                    (cont_.arr_ + (front_index_ + j) % cont_.capacity())->~T();
                  }
                  throw;
                }
              }


    CircularBuffer& operator=(const CircularBuffer<T, Capacity>& other)
    noexcept(std::is_nothrow_copy_assignable_v<T>) {
      if (this != &other) {
        cont_        = other.cont_;
        front_index_ = other.front_index_;
        size_        = other.size_;
        for (size_t i = 0; i < size_; ++i) {
          new(cont_.arr_ + (front_index_ + i) % cont_.capacity()) T(other[i]);
        }
      }
      return *this;
    }

    // methods
      void push_front(const T& value) {
        T temp(value);
        if (size_ < capacity()) {
          size_t new_front = (front_index_ + capacity() - 1) % capacity();
          new (cont_.arr_ + new_front) T(std::move(temp));
          front_index_ = new_front;
          ++size_;
        } else {
          size_t back_index = (front_index_ + size_ - 1) % capacity();
          T old_value(std::move((*this)[size_ - 1]));

          (cont_.arr_ + back_index)->~T();
          try {
            new (cont_.arr_ + back_index) T(std::move(temp));
          } catch (...) {
            try {
              new (cont_.arr_ + back_index) T(std::move(old_value));
            } catch (...) {
            }
            throw;
          }
          front_index_ = back_index;
        }
      }


    void push_back(const T& value) {
      T temp(value);

      if (size_ < capacity()) {
        size_t idx = front_index_ + size_;
        if (idx >= capacity()) idx -= capacity();
        new (cont_.arr_ + idx) T(std::move(temp));
        ++size_;
      } else {
        size_t old_front = front_index_;
        T old_val(std::move((*this)[0]));

        (cont_.arr_ + old_front)->~T();

        try {
          new (cont_.arr_ + old_front) T(std::move(temp));
        } catch (...) {
          try {
            new (cont_.arr_ + old_front) T(std::move(old_val));
          } catch (...) {
          }
          throw;
        }
        front_index_ = (front_index_ + 1) % capacity();
      }
    }

    void pop_back() noexcept {
      if (size_ > 0) {
        (cont_.arr_ + (front_index_ + size_ - 1) % cont_.capacity()) -> ~T();
        --size_;
      }
    }

    void pop_front() noexcept {
      if (size_ > 0) {
        (cont_.arr_ + front_index_) -> ~T();
        front_index_ = (front_index_ + 1) % cont_.capacity();
        --size_;
      }
    }

    size_t size()     const noexcept { return size_; }
    size_t capacity() const noexcept { return cont_.capacity(); }
    bool empty()      const noexcept { return size_ == 0; }
    bool full()       const noexcept { return size_ == cont_.capacity(); }

    T& operator[](size_t index) noexcept {
      size_t idx = front_index_ + index;
      if (idx >= cont_.capacity()) {
        idx -= cont_.capacity();
      }
      return cont_[idx];
    }

    const T& operator[](size_t index) const noexcept {
      size_t idx = front_index_ + index;
      if (idx >= cont_.capacity()) {
        idx -= cont_.capacity();
      }
      return cont_[idx];
    }

    T& at(size_t index) {
      if (index >= size_) {
        throw std::out_of_range("Index out of range");
      }
      return (*this)[index];
    }


    const T& at(size_t index) const {
      if (index >= size_) {
        throw std::out_of_range("Index out of range");
      }
      return (*this)[index];
    }

    // Iterators methods
    iterator begin() noexcept {
      return iterator(cont_.arr_, cont_.capacity(), front_index_, 0);
    }
    const_iterator begin() const noexcept {
      return const_iterator(cont_.arr_, cont_.capacity(), front_index_, 0);
    }
    const_iterator cbegin() const noexcept {
      return begin();
    }
    iterator end() noexcept {
      return iterator(cont_.arr_, cont_.capacity(), front_index_, size_);
    }
    Iterator<true> end() const noexcept {
      return const_iterator(cont_.arr_, cont_.capacity(), front_index_, size_);
    }
    Iterator<true> cend() const noexcept {
      return end();
    }
    std::reverse_iterator<Iterator<false>> rbegin() noexcept {
      return reverse_iterator(end());
    }
    std::reverse_iterator<Iterator<true>> rbegin() const noexcept {
      return const_reverse_iterator(end());
    }
    std::reverse_iterator<Iterator<true>> crbegin() const noexcept {
      return rbegin();
    }
    std::reverse_iterator<Iterator<false>> rend() noexcept {
      return reverse_iterator(begin());
    }
    std::reverse_iterator<Iterator<true>> rend() const noexcept {
      return const_reverse_iterator(begin());
    }
    std::reverse_iterator<Iterator<true>> crend() const noexcept {
      return rend();
    }

    iterator insert(const const_iterator pos, const T& value) noexcept {
      size_t offset = pos - cbegin();
      if (offset > size_) offset = size_;

      if (full()) {
        if (offset == 0) {
          return begin();
        } else {
          front_index_ = (front_index_ + 1) % cont_.capacity();
          --size_;
          offset = offset - 1;
        }
      }

      if (offset == 0) {
        push_front(value);
        return begin();
      }
      if (offset == size_) {
        push_back(value);
        auto it = end();
        --it;
        return it;
      }

      for (size_t i = size_; i > offset; --i) {
        size_t src = front_index_ + i - 1;
        if (src >= cont_.capacity()) src -= cont_.capacity();
        size_t dst = front_index_ + i;
        if (dst >= cont_.capacity()) dst -= cont_.capacity();
        new(cont_.arr_ + dst) T(cont_.arr_[src]);
      }

      size_t insert_idx = front_index_ + offset;
      if (insert_idx >= cont_.capacity()) insert_idx -= cont_.capacity();
      cont_[insert_idx] = value;
      ++size_;
      return iterator(cont_.arr_, cont_.capacity(), front_index_, offset);
    }


    iterator erase(const const_iterator pos) noexcept {
      size_t offset = pos - cbegin();
      if(offset >= size_) {
        return end();
      }
      if(offset == 0) {
        pop_front();
        return begin();
      }
      if(offset == size_ - 1) {
        pop_back();
        return end();
      }
      for (size_t i = offset; i < size_ - 1; ++i) {
        size_t src = front_index_ + i + 1;
        if(src >= cont_.capacity()) src -= cont_.capacity();
        size_t dst = front_index_ + i;
        if(dst >= cont_.capacity()) dst -= cont_.capacity();
        cont_[dst] = cont_[src];
      }
      --size_;
      return iterator(cont_.arr_, cont_.capacity(), front_index_, offset);
    }
    // friend operators

    friend Iterator<false> operator+(size_t n, Iterator<false>& it) { return it + n; }
    friend Iterator<false> operator-(size_t n, Iterator<false>& it) { return it + n; }
};

#endif //CIRCULAR_BUFFER_H
