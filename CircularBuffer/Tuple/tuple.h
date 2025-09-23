#pragma once
#include <concepts>
#include <utility>
#include <type_traits>
#include <compare>
#include <cstddef>

// === Концепты-хелперы ===
template<class T>
concept brace_initializable = requires { T{}; };

template<class... Us>
concept all_default_constructible = (std::default_initializable<Us> && ...);
template<class... Us>
concept all_copy_constructible    = (std::copy_constructible<Us> && ...);
template<class... Us>
concept all_move_constructible    = (std::move_constructible<Us> && ...);
template<class... Us>
concept all_copy_assignable       = (std::assignable_from<Us&, Us const&> && ...);
template<class... Us>
concept all_move_assignable       = (std::assignable_from<Us&, Us&&> && ...);
template<class... Us>
concept all_convertible_to        = (std::convertible_to<const Us&, Us> && ...);

// === Объявление Tuple ===
template<class... Ts>
struct Tuple;

// === Пустой кортеж ===
template<>
struct Tuple<> {
    constexpr Tuple() noexcept = default;
    constexpr auto operator<=>(Tuple const&) const noexcept = default;
};

// ===  get_impl  ===
namespace _detail {

    template<std::size_t I, class Head, class... Tail>
    constexpr decltype(auto) get_impl(Tuple<Head, Tail...>& tup) {
      if constexpr (I == 0) {
        return (tup.head);
      } else {
        return get_impl<I - 1>(tup.tail);
      }
    }

    template<std::size_t I, class Head, class... Tail>
    constexpr decltype(auto) get_impl(Tuple<Head, Tail...>&& tup) {
      if constexpr (I == 0) {
        return std::move(tup.head);
      } else {
        return get_impl<I - 1>(std::move(tup.tail)); }
    }

    template<std::size_t I, class Head, class... Tail>
    constexpr decltype(auto) get_impl(Tuple<Head, Tail...> const& tup) {
      if constexpr (I == 0) {
        return (tup.head);
      } else {
        return get_impl<I - 1>(tup.tail);
      }
    }

    template<std::size_t I, class Head, class... Tail>
    constexpr decltype(auto) get_impl(Tuple<Head, Tail...> const&& tup) {
      if constexpr (I == 0) {
        return std::move(tup.head);
      } else {
        return get_impl<I - 1>(std::move(tup.tail));
      }
    }

} // namespace _detail

// === get по индексу ===
template<std::size_t I, class... Ts>
constexpr decltype(auto) get(Tuple<Ts...>& t) {
  return _detail::get_impl<I>(t);
}

template<std::size_t I, class... Ts>
constexpr decltype(auto) get(Tuple<Ts...>&& t) {
  return _detail::get_impl<I>(std::move(t));
}

template<std::size_t I, class... Ts>
constexpr decltype(auto) get(Tuple<Ts...> const& t) {
  return _detail::get_impl<I>(t);
}

template<std::size_t I, class... Ts>
constexpr decltype(auto) get(Tuple<Ts...> const&& t) {
  return _detail::get_impl<I>(std::move(t));
}

// === get по типу ===
template<class T, class Head, class... Tail>
constexpr T& get(Tuple<Head, Tail...>& tup) {
  if constexpr (std::same_as<T, Head>) {
    static_assert((!std::same_as<T, Tail> && ...),
                  "Tuple::get<T>: T appears more than once");
    return tup.head;
  } else {
    return get<T>(tup.tail);
  }
}

template<class T, class Head, class... Tail>
constexpr T&& get(Tuple<Head, Tail...>&& tup) {
  if constexpr (std::same_as<T, Head>) {
    static_assert((!std::same_as<T, Tail> && ...),
                  "Tuple::get<T>: T appears more than once");
    return std::move(tup.head);
  } else {
    return get<T>(std::move(tup.tail));
  }
}

template<class T, class Head, class... Tail>
constexpr T const& get(Tuple<Head, Tail...> const& tup) {
  if constexpr (std::same_as<T, Head>) {
    static_assert((!std::same_as<T, Tail> && ...),
                  "Tuple::get<T>: T appears more than once");
    return tup.head;
  } else {
    return get<T>(tup.tail);
  }
}

template<class T, class Head, class... Tail>
constexpr T const&& get(Tuple<Head, Tail...> const&& tup) {
  if constexpr (std::same_as<T, Head>) {
    static_assert((!std::same_as<T, Tail> && ...),
                  "Tuple::get<T>: T appears more than once");
    return std::move(tup.head);
  } else {
    return get<T>(std::move(tup.tail));
  }
}

// === Tuple ===
template<class Head, class... Tail>
struct Tuple<Head, Tail...> {
    Head head;
    Tuple<Tail...> tail;

    // --- Конструкторы ---
    constexpr Tuple() requires all_default_constructible<Head, Tail...> : head(), tail() {}

    constexpr explicit(!(all_convertible_to<Head, Tail...>))
    Tuple(Head const& h, Tail const&... ts)
    requires all_copy_constructible<Head, Tail...>
            : head(h), tail(ts...) {}

    template<class UH, class... UT>
    requires (sizeof...(UT) == sizeof...(Tail)) &&
    (std::constructible_from<Head, UH> &&
    (std::constructible_from<Tail, UT> && ...))
    explicit(!(std::convertible_to<UH, Head> &&
               (std::convertible_to<UT, Tail> && ...)))
    constexpr Tuple(UH&& h, UT&&... ut)
            : head(std::forward<UH>(h))
            , tail(std::forward<UT>(ut)...) {}

    template<class OHead, class... OTail>
    requires (sizeof...(OTail) == sizeof...(Tail)) &&
    std::constructible_from<Head,  OHead const&> &&
    (std::constructible_from<Tail, OTail const&> && ...) &&
    (!(sizeof...(Tail) == 0 &&
    ( std::is_convertible_v<Tuple<OHead, OTail...> const&, Head>   ||
    std::is_constructible_v<Head, Tuple<OHead, OTail...> const&> ||
    std::is_same_v<Head, OHead>)))
    explicit(!(std::is_convertible_v<OHead const&, Head> &&
               (std::is_convertible_v<OTail const&, Tail> && ...)))
    constexpr Tuple(Tuple<OHead, OTail...> const& other)
            : head(other.head), tail(other.tail) {}

    template<class OHead, class... OTail>
    requires (sizeof...(OTail) == sizeof...(Tail)) &&
    std::constructible_from<Head,  OHead&&>  &&
    (std::constructible_from<Tail, OTail&&> && ...) &&
    (!(sizeof...(Tail) == 0 &&
    ( std::is_convertible_v<Tuple<OHead, OTail...>, Head>   ||
    std::is_constructible_v<Head, Tuple<OHead, OTail...>> ||
    std::is_same_v<Head, OHead>)))
    explicit(!(std::is_convertible_v<OHead&&, Head> &&
               (std::is_convertible_v<OTail&&, Tail> && ...)))
    constexpr Tuple(Tuple<OHead, OTail...>&& other)
            : head(std::forward<OHead>(other.head))
            , tail(std::forward<Tuple<OTail...>>(other.tail)) {}

    template<class U1, class U2>
    constexpr Tuple(std::pair<U1, U2> const& p)
            : head(p.first), tail(p.second) {}

    template<class U1, class U2>
    constexpr Tuple(std::pair<U1, U2>&& p)
            : head(std::forward<U1>(p.first))
            , tail(std::forward<U2>(p.second)) {}


    constexpr Tuple(const Tuple&)
    requires all_copy_constructible<Head, Tail...> = default;
    constexpr Tuple(Tuple&&) noexcept
    requires all_move_constructible<Head, Tail...> = default;

    // --- Присваивания ---
    Tuple& operator=(Tuple const& other)
    requires all_copy_assignable<Head, Tail...> {
      head = other.head;
      tail = other.tail;
      return *this;
    }

    Tuple& operator=(Tuple&& other) noexcept
    requires all_move_assignable<Head, Tail...> {
      head = std::move(other.head);
      tail = std::move(other.tail);
      return *this;
    }

    template<typename UHead, typename... UTail>
    requires (sizeof...(UTail) == sizeof...(Tail)) &&
    std::assignable_from<Head&, UHead const&> &&
    (std::assignable_from<Tail&, UTail const&> && ...)
    Tuple& operator=(Tuple<UHead, UTail...> const& other) {
      head = other.head;
      tail = other.tail;
      return *this;
    }

    template<typename UHead, typename... UTail>
    requires (sizeof...(UTail) == sizeof...(Tail)) &&
    std::assignable_from<Head&, UHead&&> &&
    (std::assignable_from<Tail&, UTail&&> && ...)
    Tuple& operator=(Tuple<UHead, UTail...>&& other) {
      head = std::forward<UHead>(other.head);
      tail = std::forward<decltype(other.tail)>(other.tail);
      return *this;
    }

    template<typename P1, typename P2>
    requires (sizeof...(Tail) == 1) &&
    std::assignable_from<Head&, P1 const&> &&
    (std::assignable_from<Tail&, P2 const&> && ...)
    Tuple& operator=(std::pair<P1, P2> const& p) {
      head = p.first;
      tail = p.second;
      return *this;
    }

    template<typename P1, typename P2>
    requires (sizeof...(Tail) == 1) &&
    std::assignable_from<Head&, P1> &&
    (std::assignable_from<Tail&, P2> && ...)
    Tuple& operator=(std::pair<P1, P2>&& p) {
      head = std::forward<P1>(p.first);
      tail = std::forward<P2>(p.second);
      return *this;
    }

    // --- Сравнения ---
    bool operator==(Tuple const&) const = default;

    constexpr auto operator<=>(Tuple const& rhs) const {
      if (auto cmp = head <=> rhs.head; cmp != 0) return cmp;
      return tail <=> rhs.tail;
    }
};

// === CTAD для std::pair ===
template<class U1, class U2>
Tuple(std::pair<U1, U2> const&) -> Tuple<U1, U2>;
template<class U1, class U2>
Tuple(std::pair<U1, U2>&&)      -> Tuple<U1, U2>;

// === tupleSize ===
template<class> struct tupleSize;

template<class... Ts>
struct tupleSize<Tuple<Ts...>> : std::integral_constant<std::size_t, sizeof...(Ts)> { };

template<class T>
inline constexpr std::size_t tuple_size_v = tupleSize<std::remove_reference_t<T>>::value;

// === makeTuple / tie ===
template<class... Args>
constexpr auto makeTuple(Args&&... args) {
  return Tuple<std::unwrap_ref_decay_t<Args>...>(std::forward<Args>(args)...);
}

template<class... Args>
constexpr Tuple<Args&...> tie(Args&... args) noexcept { return {args...}; }

// === tupleCat: объединение кортежей ===
namespace _detail {

    template<class... Tuples>
    struct cat_type;
    template<>
    struct cat_type<> { using type = Tuple<>; };
    template<class... Ts>
    struct cat_type<Tuple<Ts...>> { using type = Tuple<Ts...>; };
    template<class... Ts1, class... Ts2, class... Rest>
    struct cat_type<Tuple<Ts1...>, Tuple<Ts2...>, Rest...> {
        using type = typename cat_type<Tuple<Ts1..., Ts2...>, Rest...>::type;
    };

    template<class Result, class... Tuples>
    struct cat_collect;

    template<class Result>
    struct cat_collect<Result> {
        template<class... Elems>
        static constexpr Result make(Elems&&... elems) {
          return Result(std::forward<Elems>(elems)...);
        }
    };

    template<class Result, class First, class... Rest>
    struct cat_collect<Result, First, Rest...> {
    private:
        template<std::size_t... Ix, class... Collected>
        static constexpr Result step(std::index_sequence<Ix...>,
                                     First&& first, Rest&&... rest,
                                     Collected&&... collected) {
          return cat_collect<Result, Rest...>::make(
                  std::forward<Rest>(rest)...,
                  std::forward<Collected>(collected)...,
                  get<Ix>(std::forward<First>(first))...);
        }
    public:
        template<class... Collected>
        static constexpr Result make(First&& first, Rest&&... rest,
                                     Collected&&... collected) {
          return step(
                  std::make_index_sequence<
                          tuple_size_v<std::remove_reference_t<First>>>{},
                  std::forward<First>(first),
                  std::forward<Rest>(rest)...,
                  std::forward<Collected>(collected)...);
        }
    };

} // namespace _detail

template<class... Tuples>
constexpr auto tupleCat(Tuples&&... tup) {
  using Result = typename _detail::cat_type<
          std::remove_reference_t<Tuples>...
  >::type;

  return _detail::cat_collect<Result, Tuples...>::make(
          std::forward<Tuples>(tup)...);
}

