#pragma once
#include <string_view>
#include <list>
#include <string>
#include <sstream>


namespace pbcpp {
  using std::string_view;
  using std::vector;
  using std::string;
  using i32 = int32_t;
  using i64 = int64_t;
  using u8 = uint8_t;


  // ---- helper code
  namespace impl {
    template <class> struct is_vector : std::false_type {};
    template <class T, class A> struct is_vector<std::vector<T,A>> : std::true_type {};

    template <class> struct memptr_ret_type : std::type_identity<void> {};
    template <class C, class T> struct memptr_ret_type<T(C::*)> : std::type_identity<T> {};
  }


  // --- macros
  #define pb_assert(cond)   if (!(cond))  pb_throw("assert failed: " #cond)


  // --- constants
  enum pb_type {
    TYPE_DOUBLE, TYPE_FLOAT, TYPE_INT32, TYPE_INT64, TYPE_UINT32, TYPE_UINT64, TYPE_SINT32,
    TYPE_SINT64, TYPE_FIXED32, TYPE_FIXED64, TYPE_SFIXED32, TYPE_SFIXED64, TYPE_BOOL, TYPE_STRING,
    TYPE_ENUM, TYPE_MSG
  };
  enum pb_wire_type { WT_VARINT=0, WT_I64=1, WT_LEN=2, WT_I32=5 };

  template <class> struct reflect;

  template <class T> concept is_message = requires { reflect<std::decay_t<T>>::size; };

  template <size_t N> struct pb_name {
    char data[N];
    static constexpr size_t len = N-1;

    constexpr pb_name(const char (&str)[N]) {
      std::copy_n(str, N, data);
    }
  };

  void pb_throw(auto&&... args) {
    std::ostringstream oss;
    (oss << ... << args);
    throw std::runtime_error(oss.str());
  }

  template <pb_type type_, pb_name name_, i32 fnum_, auto memptr_>
  struct field {
    static constexpr i32 num = fnum_;
    static constexpr pb_type type = type_;
    static constexpr std::integral_constant<pb_type,type_> type_ic;
    static constexpr decltype(memptr_) mptr = memptr_;
    static constexpr string_view name{name_.data, name_.len};
    static constexpr bool can_pack = (type_ != TYPE_STRING) && (type_ != TYPE_MSG);
    using cpptype = impl::memptr_ret_type<decltype(memptr_)>::type;
    static constexpr bool is_repeated = impl::is_vector<cpptype>::value;
  };

  void each_field_r(auto&& fn, auto f, auto... fs) {
    if constexpr (sizeof...(fs) != 0)  each_field_r(fn, fs...);
    fn(f);
  }

  template <class... Fields> struct fields {
    static constexpr size_t size = sizeof...(Fields);

    static void each_field(auto&& fn) { (fn(Fields{}), ...); }
    static void each_field_r(auto&& fn) { each_field_r(fn, Fields{}...); }
    static void each_field_exitable(auto&& fn) { (fn(Fields{}) && ...); }
  };


  // ---- encoder
  struct rd_buf {
    rd_buf(rd_buf const&) = delete;
    rd_buf& operator=(rd_buf const&) = delete;

    char* const begin;
    char const* const end;
    char* curs;

    rd_buf(size_t size)
    : begin((char*)::malloc(size))
    , end(begin + size)
    , curs(begin + size)
    {}

    ~rd_buf() {
      ::free(begin);
    }
  };

  struct encoder {
    static constexpr size_t BUFSZ = (16*1024 - 128);

    std::list<rd_buf> bufs;
    size_t size = 0;
    size_t offset = 0;

    encoder() {
      bufs.emplace_back(BUFSZ);
    }

    i64 get_size() const {
      return offset + (bufs.back().end - bufs.back().curs);
    }

    void write(void const* p, size_t sz) {
      auto remaining = bufs.back().curs - bufs.back().begin;
      if (sz < remaining) {
        bufs.back().curs -= sz;
        ::memcpy(bufs.back().curs, p, sz);
      } else {
        auto leftover = sz - remaining;
        bufs.back().curs -= remaining;
        ::memcpy(bufs.back().curs, ((char*)p) + leftover, remaining);
        offset += BUFSZ;
        bufs.emplace_back(BUFSZ);
        write(p, leftover);
      }
    }

    void encode_varint(i64 val) {
      char buf[10];
      i32 pos = 0;
      while (val >= 0x80) {
        buf[pos++] = (val & 0x7f) | 0x80;
        val >>= 7;
      }
      buf[pos++] = (val & 0x7f);
      write(buf,pos);
    }

    void encode_tag(i32 field, i32 wire_type) {
      if (field <= 0)  return;
      encode_varint((field<<3) + wire_type);
    }

    void encode_varint(i64 val, i32 field, bool can_skip) {
      if (can_skip && (val==0))  return;
      encode_varint(val);
      encode_tag(field, WT_VARINT);
    }

    void encode_i32(i32 val) {
      write(&val, 4);
    }

    void encode_i32(i32 val, i32 field, bool can_skip) {
      if (can_skip && (val==0))  return;
      encode_i32(val);
      encode_tag(field, WT_I32);
    }

    void encode_i64(i64 val) {
      write(&val, 8);
    }

    void encode_i64(i64 val, i32 field, bool can_skip) {
      if (can_skip && (val==0))  return;
      encode_i64(val);
      encode_tag(field, WT_I64);
    }

    void encode_tag_len(i32 field, i32 len) {
      encode_varint(len);
      encode_tag(field, WT_LEN);
    }

    void encode_str(string_view sv, i32 field, bool can_skip) {
      if (can_skip && sv.empty())  return;
      write(sv.data(), sv.size());
      encode_tag_len(field, sv.size());
    }

    void encode_zigzag32(i32 val, i32 field, bool can_skip) {
      val = (val << 1) ^ (val >> 31);
      encode_varint(val, field, can_skip);
    }

    void encode_zigzag64(i64 val, i32 field, bool can_skip) {
      val = (val << 1) ^ (val >> 31);
      encode_varint(val, field, can_skip);
    }

    template <class T>
    void encode_field(vector<T> const& val, i32 field, auto fspec, bool can_skip) {
      if (can_skip && val.empty())  return;

      for (auto iter=val.rbegin(); iter != val.rend(); iter++) {
        if constexpr (fspec.can_pack) {
          encode_field(*iter, 0, fspec, false);
        } else {
          encode_field(*iter, field, fspec, false);
        }
      }

      if constexpr (fspec.can_pack) {
        encode_tag_len(field, val.size());
      }
    }

    void encode_field(auto const& val, i32 field, auto fspec, bool can_skip) {
      constexpr auto ftype = fspec.type;
      if constexpr (
        ftype == TYPE_INT32 || ftype == TYPE_INT64 || ftype == TYPE_UINT32 || ftype == TYPE_UINT64 ||
        ftype == TYPE_BOOL || ftype == TYPE_ENUM
      ) {
        encode_varint(val, field, can_skip);
      } else if constexpr (ftype == TYPE_SINT32) {
        encode_zigzag32(val, field, can_skip);
      } else if constexpr (ftype == TYPE_SINT64) {
        encode_zigzag64(val, field, can_skip);
      } else if constexpr (ftype == TYPE_SFIXED32 || ftype == TYPE_FIXED32 || ftype == TYPE_FLOAT) {
        encode_i32(*(i32*)&val, field, can_skip);
      } else if constexpr (ftype == TYPE_SFIXED64 || ftype == TYPE_FIXED64 || ftype == TYPE_DOUBLE) {
        encode_i64(*(i64*)&val, field, can_skip);
      } else if constexpr (ftype == TYPE_STRING) {
        encode_str(val, field, can_skip);

      // message
      } else {
        static_assert(ftype == TYPE_MSG);
        auto size = get_size();
        reflect<std::decay_t<decltype(val)>>::each_field_r([&](auto f) {
          encode_field(val.*f.mptr, f.num, f, true);
        });

        // write the size and tag
        if (field > 0) {
          size = get_size() - size;
          if (!can_skip || (size != 0)) {
            encode_tag_len(field, size);
          }
        }
      }
    }

    void encode_top(auto const& msg) {
      encode_field(msg, 0, field<TYPE_MSG,"",0,nullptr>{}, false);
    }

    void copy_into(char* p) {
      for (auto iter = bufs.rbegin(); iter != bufs.rend(); ++iter) {
        auto const bufsz = (iter->end - iter->curs);
        ::memcpy(p, iter->curs, bufsz);
        p += bufsz;
      }
    }

    string as_str() {
      string ret(get_size(), ' ');
      copy_into((char*)ret.data());
      return std::move(ret);
    }


    // --- static methods
    static string to_string(auto& msg) {
      encoder encoder;
      encoder.encode_top(msg);
      return encoder.as_str();
    }
  };


  struct decoder {
    char const* curs;
    char const* const end;

    decoder(char const* curs_, char const* end_) : curs(curs_), end(end_) {}
    decoder(string_view sv) : decoder(sv.data(), sv.data() + sv.size()) {}

    bool empty() const { return curs >= end; }

    void decode_msg(auto& msg) {
      while (!empty()) {
        auto tag = read_varint();
        auto field = (tag>>3);
        auto wire_type = (tag&7);

        reflect<std::decay_t<decltype(msg)>>::each_field([&](auto f) {
          if (f.num == field) {
            decode_field((msg.*f.mptr), wire_type, f);
          }
        });
      }
    }

    void assert_wire_type(i32 actual, i32 expected) {
      if (actual == -1)  return;
      if (actual != expected)  pb_throw("invalid wire_type");
    }

    template <class T>
    void decode_field(vector<T>& outv, i32 wire_type, auto fspec) {

      // handle non-packed
      if ((wire_type != WT_LEN) || !fspec.can_pack) {
        outv.emplace_back();
        decode_field(outv.back(), wire_type, fspec);

      // handle packed
      } else if constexpr (fspec.can_pack) {
        auto len = read_varint();
        auto prevsz = outv.size();
        outv.resize(len + prevsz);
        for (size_t i=0; i<len; i++) {
          decode_field(outv[i+prevsz], -1, fspec);
        }
      }
    }

    void decode_field(auto& outv, i32 wire_type, auto fspec) {
      constexpr auto ftype = fspec.type;
      if constexpr (
        ftype == TYPE_INT32 || ftype == TYPE_INT64 || ftype == TYPE_UINT32 || ftype == TYPE_UINT64 ||
        ftype == TYPE_BOOL || ftype == TYPE_ENUM
      ) {
        assert_wire_type(wire_type, WT_VARINT);
        outv = read_varint();

      } else if constexpr (ftype == TYPE_SINT32 || ftype == TYPE_SINT64) {
        assert_wire_type(wire_type, WT_VARINT);
        auto n = read_varint();
        outv = (n>>1) ^ (-(n&1));

      } else if constexpr (ftype == TYPE_SFIXED32 || ftype == TYPE_FIXED32 || ftype == TYPE_FLOAT) {
        assert_wire_type(wire_type, WT_I32);
        read_into(&outv, 4);

      } else if constexpr (ftype == TYPE_SFIXED64 || ftype == TYPE_FIXED64 || ftype == TYPE_DOUBLE) {
        assert_wire_type(wire_type, WT_I64);
        read_into(&outv, 8);

      } else if constexpr (ftype == TYPE_STRING) {
        assert_wire_type(wire_type, WT_LEN);
        auto decoder = read_buf(read_varint());
        outv.assign(decoder.curs, decoder.end);

      } else {
        static_assert(ftype == TYPE_MSG);
        assert_wire_type(wire_type, WT_LEN);
        auto decoder = read_buf(read_varint());
        decoder.decode_msg(outv);
      }
    }

    i64 read_varint() {
      i64 ret = 0;
      i32 shift = 0;
      while (!empty()) {
        auto ch = (u8)(*curs++);
        ret += i64(ch & 0x7f) << shift;
        if (!(ch & 0x80))  break;
        shift += 7;
      }
      return ret;
    }

    decoder read_buf(i32 len) {
      auto ret = decoder(curs, curs+len);
      curs += len;
      pb_assert(curs <= end);
      return ret;
    }

    void read_into(void* p, size_t len) {
      auto buf = read_buf(len);
      ::memcpy(p, buf.curs, len);
    }


    // ---- static methods
    static void from_string(string_view sv, auto& msg) {
      decoder decoder(sv);
      decoder.decode_msg(msg);
    }
  };


  auto get_reflect(is_message auto const& msg) {
    return reflect<std::decay_t<decltype(msg)>>{};
  }


  std::ostream& to_ostream(std::ostream& os, is_message auto const& msg) {
    os << "{";
    bool is_first = true;
    get_reflect(msg).each_field([&](auto f) {
      if (!is_first)  os << ",";
      is_first = false;
      os << f.name << ":";

      auto& val = msg.*f.mptr;
      if constexpr (f.is_repeated) {
        os << '[';
        int n = 0;
        for (auto&& el : val) {
          if (n++)  os << ',';
          os << el;
        }
        os << ']';
      } else {
        os << val;
      }
    });
    return os << "}";
  }

  string to_string(is_message auto const& msg) {
    std::ostringstream oss;
    oss << msg;
    return oss.str();
  }

  template <is_message T> std::strong_ordering compare(T const& a, T const& b) {
    std::strong_ordering ret = std::strong_ordering::equal;
    get_reflect(a).each_field_exitable([&](auto f) {
      auto const& aval = (a.*f.mptr);
      auto const& bval = (b.*f.mptr);
      if constexpr (f.is_repeated) {
        ret = aval.size() <=> bval.size();
        for (int i=0; i<aval.size() && (ret==0); i++) {
          ret = aval[i] <=> bval[i];
        }
      } else {
        ret = aval <=> bval;
      }
      return (ret == 0);
    });
    return ret;
  }
}


namespace std {
  ostream& operator<<(ostream& os, pbcpp::is_message auto const& msg) {
    return pbcpp::to_ostream(os,msg);
  }
  string to_string(pbcpp::is_message auto const& msg) {
    return pbcpp::to_string(msg);
  }
}


template <pbcpp::is_message T>
std::strong_ordering operator<=>(T const& a, T const& b) { return pbcpp::compare(a,b); }

template <pbcpp::is_message T> bool operator==(T const& a, T const& b) { return (a <=> b) == 0; }
template <pbcpp::is_message T> bool operator!=(T const& a, T const& b) { return (a <=> b) != 0; }
