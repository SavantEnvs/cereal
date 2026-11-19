// Shared data model for the cereal fuzz harnesses.
//
// Include this AFTER the archive header (json.hpp / xml.hpp / portable_binary.hpp): cereal's
// polymorphic registration (CEREAL_REGISTER_TYPE) binds to the archives already included in the TU.
//
// The root type `Root` mixes named (NVP) and unnamed/positional fields, nested structs, the std
// containers, smart pointers (shared ids + a polymorphic pointer), optional/variant and a raw binary
// blob, so one `archive(root)` drives every loader under include/cereal/types and the archive's own
// node search / iteration (unnamed fields reach JSONInputArchive::Iterator::value(), the code fixed
// upstream in e735cc8bc1 "Catch short documents in JSON input").
//
// Containers (and, in the binary harness, strings) use `Bounded<T>`: a std::allocator that throws
// std::bad_alloc once the input's containers hold more than 8 MiB live (`g_live`). Binary
// archives read container lengths straight from the input (64-bit), so without a bound every
// length field would just be an out-of-memory report. A throwing allocator is ordinary behavior for
// the data model the harness defines; cereal's own loaders run unchanged.
#pragma once

#include <cereal/cereal.hpp>
#include <cereal/types/array.hpp>
#include <cereal/types/bitset.hpp>
#include <cereal/types/chrono.hpp>
#include <cereal/types/complex.hpp>
#include <cereal/types/deque.hpp>
#include <cereal/types/list.hpp>
#include <cereal/types/map.hpp>
#include <cereal/types/memory.hpp>
#include <cereal/types/optional.hpp>
#include <cereal/types/polymorphic.hpp>
#include <cereal/types/set.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/tuple.hpp>
#include <cereal/types/unordered_map.hpp>
#include <cereal/types/utility.hpp>
#include <cereal/types/variant.hpp>
#include <cereal/types/vector.hpp>

#include <cstddef>
#include <cstdint>
#include <new>
#include <string_view>
#include <type_traits>

namespace fz {

// Live bytes held by Bounded containers for the current input; the harness resets it per input.
constexpr std::size_t kMaxLive = std::size_t(8) << 20;
inline std::size_t g_live = 0;

template <class T>
struct Bounded : std::allocator<T> {
  using value_type = T;
  template <class U> struct rebind { using other = Bounded<U>; };
  Bounded() noexcept = default;
  template <class U> Bounded(const Bounded<U>&) noexcept {}
  T* allocate(std::size_t n) {
    if (n > (kMaxLive - g_live) / sizeof(T)) throw std::bad_alloc();
    T* p = std::allocator<T>::allocate(n);
    g_live += n * sizeof(T);
    return p;
  }
  void deallocate(T* p, std::size_t n) noexcept {
    g_live -= n * sizeof(T);
    std::allocator<T>::deallocate(p, n);
  }
};
template <class T, class U> bool operator==(const Bounded<T>&, const Bounded<U>&) { return true; }
template <class T, class U> bool operator!=(const Bounded<T>&, const Bounded<U>&) { return false; }

// Strings: the binary harness (length read from input) uses the bounded allocator. JSON/XML string
// lengths are bounded by the document itself, and their archives load strings through std::string.
#ifdef FZ_BOUNDED_STRING
using Str = std::basic_string<char, std::char_traits<char>, Bounded<char>>;
#else
using Str = std::string;
#endif
template <class T> using Vec = std::vector<T, Bounded<T>>;
template <class T> using Deq = std::deque<T, Bounded<T>>;
template <class T> using Lst = std::list<T, Bounded<T>>;
template <class K, class V> using Map = std::map<K, V, std::less<K>, Bounded<std::pair<const K, V>>>;
template <class K> using Set = std::set<K, std::less<K>, Bounded<K>>;
struct StrHash {
  std::size_t operator()(const Str& s) const { return std::hash<std::string_view>()(std::string_view(s.data(), s.size())); }
};
using UMapS = std::unordered_map<Str, int, StrHash, std::equal_to<Str>, Bounded<std::pair<const Str, int>>>;

// Polymorphic hierarchy, registered below.
struct Base {
  virtual ~Base() = default;
  int id = 0;
  template <class A> void serialize(A& ar) { ar(CEREAL_NVP(id)); }
};
struct Derived : Base {
  Str label;
  Vec<double> values;
  template <class A> void serialize(A& ar) {
    ar(cereal::base_class<Base>(this), CEREAL_NVP(label), CEREAL_NVP(values));
  }
};

// Unnamed, positional fields: JSON/XML read these by iterating the current node.
struct Point {
  int x = 0, y = 0, z = 0;
  template <class A> void serialize(A& ar) { ar(x, y, z); }
};

// Versioned class with a load/save pair: exercises class-version loading.
struct Versioned {
  std::uint32_t v = 0;
  Str name;
  template <class A> void save(A& ar, std::uint32_t const version) const {
    ar(CEREAL_NVP(name));
    if (version > 0) ar(CEREAL_NVP(v));
  }
  template <class A> void load(A& ar, std::uint32_t const version) {
    ar(CEREAL_NVP(name));
    if (version > 0) ar(CEREAL_NVP(v));
  }
};

// Raw bytes: text archives store them base64-encoded (loadBinaryValue), binary archives as-is.
struct Blob {
  unsigned char bytes[16] = {};
  template <class A> void save(A& ar) const {
    if constexpr (cereal::traits::is_text_archive<A>::value)
      ar.saveBinaryValue(bytes, sizeof(bytes), "bytes");
    else
      ar(cereal::binary_data(bytes, sizeof(bytes)));
  }
  template <class A> void load(A& ar) {
    if constexpr (cereal::traits::is_text_archive<A>::value)
      ar.loadBinaryValue(bytes, sizeof(bytes), "bytes");
    else
      ar(cereal::binary_data(bytes, sizeof(bytes)));
  }
};

// Minimal (single-value) serialization.
struct Minimal {
  long long n = 0;
  template <class A> long long save_minimal(A const&) const { return n; }
  template <class A> void load_minimal(A const&, long long const& in) { n = in; }
};

struct Inner {
  Str s;
  Vec<std::int32_t> ints;
  Point p;
  template <class A> void serialize(A& ar) { ar(s, ints, p); }  // unnamed
};

struct Root {
  bool flag = false;
  std::int8_t i8 = 0;
  std::uint16_t u16 = 0;
  std::int64_t i64 = 0;
  std::uint64_t u64 = 0;
  float f = 0;
  double d = 0;
  long double ld = 0;
  char c = 0;
  Str text;
  Vec<std::uint8_t> raw;
  Vec<Str> words;
  Vec<bool> bits;
  std::array<std::int16_t, 3> arr{};
  Deq<double> dq;
  Lst<Point> pts;
  Map<Str, Inner> table;
  Set<std::int32_t> ids;
  UMapS counts;
  std::pair<int, Str> pr;
  std::tuple<int, double, Str> tup;
  std::bitset<20> bs;
  std::complex<double> cx;
  std::chrono::milliseconds dur{0};
  std::optional<Str> opt;
  std::variant<int, Str, Point> var;
  std::shared_ptr<Inner> sp1, sp2;  // the second normally aliases the first (pointer ids)
  std::unique_ptr<Point> up;
  std::weak_ptr<Inner> wp;
  std::shared_ptr<Base> poly;
  Versioned ver;
  Blob blob;
  Minimal mini;
  Vec<Inner> nested;

  template <class A> void serialize(A& ar) {
    ar(CEREAL_NVP(flag), CEREAL_NVP(i8), CEREAL_NVP(u16), CEREAL_NVP(i64), CEREAL_NVP(u64),
       CEREAL_NVP(f), CEREAL_NVP(d), CEREAL_NVP(ld), CEREAL_NVP(c), CEREAL_NVP(text));
    ar(raw, words, bits, arr, dq);  // unnamed
    ar(CEREAL_NVP(pts), CEREAL_NVP(table), CEREAL_NVP(ids), CEREAL_NVP(counts), CEREAL_NVP(pr),
       CEREAL_NVP(tup), CEREAL_NVP(bs), CEREAL_NVP(cx), CEREAL_NVP(dur), CEREAL_NVP(opt),
       CEREAL_NVP(var), CEREAL_NVP(sp1), CEREAL_NVP(sp2), CEREAL_NVP(up), CEREAL_NVP(wp),
       CEREAL_NVP(poly));
    ar(ver, blob, mini, nested);  // unnamed
  }
};

// A sample value used to generate seeds and by the self-check.
inline Root sample() {
  Root r;
  r.flag = true; r.i8 = -7; r.u16 = 65000; r.i64 = -1234567890123LL; r.u64 = 18000000000000000000ULL;
  r.f = 1.5f; r.d = 3.25; r.ld = 2.5L; r.c = 'q'; r.text = "hello cereal";
  r.raw = {0, 1, 2, 255}; r.words = {"alpha", "beta"}; r.bits = {true, false, true};
  r.arr = {1, -2, 3}; r.dq = {0.5, -0.25}; r.pts = {{1, 2, 3}, {4, 5, 6}};
  Inner in; in.s = "inner"; in.ints = {7, 8, 9}; in.p = {10, 11, 12};
  r.table["k1"] = in; r.ids = {3, 1, 2}; r.counts["x"] = 1; r.counts["y"] = 2;
  r.pr = {42, "pair"}; r.tup = std::make_tuple(1, 2.0, Str("tuple"));
  r.bs = std::bitset<20>(0xABCDE); r.cx = {1.0, -1.0}; r.dur = std::chrono::milliseconds(1500);
  r.opt = Str("present"); r.var = Point{1, 1, 1};
  r.sp1 = std::make_shared<Inner>(in); r.sp2 = r.sp1; r.wp = r.sp1;
  r.up.reset(new Point{9, 8, 7});
  auto dv = std::make_shared<Derived>(); dv->id = 5; dv->label = "derived"; dv->values = {1.0, 2.0};
  r.poly = dv;
  r.ver.name = "ver"; r.ver.v = 3;
  for (int i = 0; i < 16; ++i) r.blob.bytes[i] = static_cast<unsigned char>(i * 17);
  r.mini.n = -99; r.nested = {in, in};
  return r;
}

}  // namespace fz

CEREAL_CLASS_VERSION(fz::Versioned, 1)
CEREAL_REGISTER_TYPE(fz::Derived)
CEREAL_REGISTER_POLYMORPHIC_RELATION(fz::Base, fz::Derived)
