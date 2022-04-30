#pragma once

#include <cassert>	// assert
#include <iostream>	// ostream
#include <map>
#include <memory>	// unique_ptr
#include <stdexcept>	// out_of_range
#include <string>
#include <variant>
#include <vector>

#ifdef _MSC_VER
#pragma warning(disable: 4996)
#endif


namespace json17 {
	
enum class json_type {
	null,
	boolean,
	number,
	string,
	array,
	object
};

struct dump_options {
	int indent;
	char indent_char;
	bool ensure_ascii;

	dump_options(int indent = -1, char indent_char = ' ', bool ensure_ascii = false)
		: indent(indent), indent_char(indent_char), ensure_ascii(ensure_ascii) {}
};
	
struct json_traits {
	using number_type = double;

	using string_type = std::string;
	
	template<class T>
	using array_type = std::vector<T>;

	template<class K, class V>
	using map_type = std::map<K, V>;

	template<class T>
	using smart_pointer_type = std::unique_ptr<T>;

	template<class T, class... Args>
	static std::unique_ptr<T> make_smart(Args&&... args) {
		return std::make_unique<T>(std::forward<Args>(args)...);
	}
};

struct json_shared_traits : json_traits {
	template<class T>
	using smart_pointer_type = std::shared_ptr<T>;

	template<class T, class... Args>
	static std::shared_ptr<T> make_smart(Args&&... args) {
		return std::make_shared<T>(std::forward<Args>(args)...);
	}
};

// do not use pointers, the "smart pointer" stores data itself
struct json_inplace_traits : json_traits {
	template<class T>
	struct smart_pointer_type {
		// modifying data is always available through smart pointers, so m_value is mutable
		mutable T m_value;

		operator T& () const { return m_value; }
		T& operator*() const { return const_cast<T&>(m_value); }
		T* operator->() const { return &m_value; }
		T* get() const { return const_cast<T*>(&m_value); }
		void reset(T* p /*not-null*/) { if (p) m_value = *p; }
	};

	template<class T, class... Args>
	static smart_pointer_type<T> make_smart(Args&&... args) {
		return { T(std::forward<Args>(args)...) };
	}
};

template<class OutIt>
class writer_interface;

// a writer interface which supports writting multiple chars, need writer_interface<> class to link with underlying type
// the general writer_interface<> template supports output iterator by default
// inspired by golang's Writer interface
class writer {
public:
	virtual void write(char ch) = 0;
	virtual void write(const char* str, size_t n) = 0;
	virtual ~writer() = default;

	// convenience functions
	inline void write_c(const char* str) { write(str, strlen(str)); }

	template<size_t N>
	inline void write(const char(&arr)[N]) { static_assert(N > 2);  write(arr, N - 1); }

	template<class Target>
	static std::unique_ptr<writer> New(Target& target) {
		if constexpr (std::is_base_of_v<std::ostream, Target>) {
			return std::make_unique<writer_interface<std::ostream>>(target);
		}
		else {
			static_assert(std::is_base_of_v<writer, writer_interface<Target>>);
			return std::make_unique<writer_interface<Target>>(target);
		}
	}
};

// output iterators of char
template<class OutIt>
class writer_interface : public writer
{
public:
	// make sure OutIt is an output iterator of char
	static_assert(std::is_void<std::void_t<decltype(*++std::declval<OutIt>() = ' ')>>::value);

	OutIt it;
	writer_interface(OutIt it) : it(it) {}
	void write(char ch) override { *it = ch;  ++it; }
	void write(const char* str, size_t n) override {
		for (size_t i = 0; i < n; i++) *it = str[i], ++it;
	}
};

// specialize for std::string, using .append()
template<>
class writer_interface<std::string> : public writer
{
public:
	std::string* ptr;
	writer_interface(std::string& str) : ptr(&str) {}
	void write(char ch) override { ptr->push_back(ch); }
	void write(const char* str, size_t n = 0) override { ptr->append(str, n); }
};

// specialize for std::basic_ostream<>, using write() for unformatted output
// this is because std::ostream_iterator<> and std::ostreambuf_iterator<> do not meet the static_assert in main template
template<>
class writer_interface<std::ostream> : public writer
{
public:
	std::ostream* ptr;
	writer_interface(std::ostream& os) : ptr(&os) {}
	void write(char ch) override { ptr->put(ch); }
	void write(const char* str, size_t n = 0) override { ptr->write(str, n); }
};

template<class Iter>
class reader_interface;

// a reader interface which supports reading chars, need reader_interface<> class to link with underlying type
// the general reader_interface<> template supports an input iterator pair
// inspired by golang's Reader interface
class reader
{
public:
	// return EOF if nothing to read
	virtual char read() = 0;
	virtual ~reader() = default;

	char nonspace_read() {
		char ch;
		do ch = read(); while (isspace(ch));
		return ch;
	}

	template<class Target>
	static std::unique_ptr<reader> New(Target& target) {
		if constexpr (std::is_base_of_v<std::istream, Target>) {
			return std::make_unique<reader_interface<std::istream>>(target);
		}
		else {
			static_assert(std::is_base_of_v<reader, reader_interface<Target>>);
			return std::make_unique<reader_interface<Target>>(target);
		}
	}

	template<class Iter>
	static std::unique_ptr<reader> New(Iter first, Iter last) {
		static_assert(std::is_base_of_v<reader, reader_interface<Iter>>);
		return std::make_unique<reader_interface<Iter>>(first, last);
	}
};

template<class Iter>
class reader_interface : public reader
{
public:
	static_assert(std::is_same_v<std::iterator_traits<Iter>::value_type, char>);

	Iter first, last;
	reader_interface(Iter first, Iter last) : first(first), last(last) {}
	char read() override { return first == last ? EOF : *first++; }
};

template<>
class reader_interface<std::istream> : public reader
{
public:
	std::istream* ptr;
	reader_interface(std::istream& is) : ptr(&is) {}
	char read() override { return ptr->get(); }
};

// null-terminated c-style string, use simplified implementation, turn \0 into EOF and stop iterating
template<>
class reader_interface<const char*> : public reader
{
public:
	const char* it;
	reader_interface(const char* it) : it(it) {}
	char read() override { return *it == '\0' ? EOF : *it++; }
};


template<class Traits = json_traits>
class basic_json
{
public:
	using number = typename Traits::number_type;
	using string = typename Traits::string_type;
	using array  = typename Traits::template array_type<basic_json>;
	using object = typename Traits::template map_type<string, basic_json>;

	template<class T>
	using smart_ptr = typename Traits::template smart_pointer_type<T>;

	// make sure make_smart<> is consistent with smart_pointer_type<>
	static_assert(std::is_same_v<smart_ptr<int>, decltype(Traits::template make_smart<int>())>);

	using sptr_string_t = typename smart_ptr<string>; // should be not-null
	using sptr_array_t  = typename smart_ptr<array>;  // should be not-null
	using sptr_object_t = typename smart_ptr<object>; // should be not-null

	using variant_t = std::variant<std::nullptr_t, bool, number, sptr_string_t, sptr_array_t, sptr_object_t>;

private:
	variant_t m_var;

	template<class T, class... Args>
	static smart_ptr<T> _make_smart(Args&&... args) {
		return Traits::template make_smart<T>(std::forward<Args>(args)...);
	}

public:
	basic_json() = default;
	basic_json(std::nullptr_t)  : m_var(nullptr) {}
	basic_json(bool v)          : m_var(v) {}
	basic_json(number v)        : m_var(v) {}
	basic_json(int v)           : m_var(number(v)) {}
	basic_json(const string& v) : m_var(_make_smart<string>(v)) {}
	basic_json(string&& v)      : m_var(_make_smart<string>(v)) {}
	basic_json(const char* v)   : m_var(_make_smart<string>(v)) {}
	basic_json(const array& v)  : m_var(_make_smart<array>(v)) {}
	basic_json(array&& v)       : m_var(_make_smart<array>(v)) {}
	basic_json(const object& v) : m_var(_make_smart<object>(v)) {}
	basic_json(object&& v)      : m_var(_make_smart<object>(v)) {}

	basic_json(basic_json&&) = default;
	basic_json& operator=(basic_json&&) = default;

	// make a deep copy even if using shared pointer
	basic_json& operator=(const basic_json& other) {
		// TODO use std::visit
		switch (other.m_var.index()) {
		case 0: m_var = nullptr; break;
		case 1: m_var = other.get_bool(); break;
		case 2: m_var = other.get_number(); break;
		case 3: m_var = _make_smart<string>(other.get_string()); break;
		case 4: m_var = _make_smart<array>(other.get_array()); break;
		case 5: m_var = _make_smart<object>(other.get_object()); break;
		}
		return *this;
	}
	basic_json(const basic_json& other) { operator=(other); }

	variant_t&       get_variant()       noexcept { return m_var; }
	const variant_t& get_variant() const noexcept { return m_var; }

	json_type get_type() const noexcept { return (json_type)m_var.index(); }
	bool is_null()   const noexcept { return m_var.index() == 0; }
	bool is_bool()   const noexcept { return m_var.index() == 1; }
	bool is_number() const noexcept { return m_var.index() == 2; }
	bool is_string() const noexcept { return m_var.index() == 3; }
	bool is_array()  const noexcept { return m_var.index() == 4; }
	bool is_object() const noexcept { return m_var.index() == 5; }

	bool&   get_bool()   { return std::get<bool>(m_var); }
	number& get_number() { return std::get<number>(m_var); }
	string& get_string() { return *std::get<sptr_string_t>(m_var); }
	array&  get_array()  { return *std::get<sptr_array_t>(m_var); }
	object& get_object() { return *std::get<sptr_object_t>(m_var); }

	bool          get_bool()   const { return std::get<bool>(m_var); }
	number        get_number() const { return std::get<number>(m_var); }
	int           get_int()    const { return static_cast<int>(get_number()); }
	const string& get_string() const { return *std::get<sptr_string_t>(m_var); }
	const array&  get_array()  const { return *std::get<sptr_array_t>(m_var); }
	const object& get_object() const { return *std::get<sptr_object_t>(m_var); }

	string& set_string() { m_var = _make_smart<string>();  return get_string(); }
	array&  set_array()  { m_var = _make_smart<array>();  return get_array(); }
	object& set_object() { m_var = _make_smart<object>();  return get_object(); }

	// auto expand if idx >= get_array().size(), or create one if is_null()
	// throws if *this is not null nor an array
	basic_json& operator[](size_t idx) {
		if (is_null()) {
			m_var = _make_smart<array>(idx + 1);
			return get_array()[idx];
		}
		auto& arr = get_array();
		if (arr.size() <= idx) arr.resize(idx + 1);
		return arr[idx];
	}

	// array is immutable, must be array and does range check
	const basic_json& operator[](size_t idx) const { return get_array().at(idx); }

	// auto fill null if desired key does not exist, or create one if is_null()
	// throws if *this is not null nor an object
	basic_json& operator[](const string& key) {
		if (is_null()) m_var = _make_smart<object>();
		return get_object()[key];
	}

	// object is immutable, the key must exist
	const basic_json& operator[](const string& key) const {
		auto& obj = get_object();
		auto it = obj.find(key);
		if (it == obj.end()) throw std::out_of_range("key does not exist");
		return it->second;
	}

	bool*   ptr_bool()   noexcept { return std::get_if<bool>(&m_var); }
	number* ptr_number() noexcept { return std::get_if<number>(&m_var); }
	string* ptr_string() noexcept { auto* ptr = std::get_if<sptr_string_t>(&m_var);  return ptr ? ptr->get() : nullptr; }
	array*  ptr_array()  noexcept { auto* ptr = std::get_if<sptr_array_t>(&m_var);  return ptr ? ptr->get() : nullptr; }
	object* ptr_object() noexcept { auto* ptr = std::get_if<sptr_object_t>(&m_var);  return ptr ? ptr->get() : nullptr; }

	const bool*   ptr_bool()   const noexcept { return std::get_if<bool>(&m_var); }
	const number* ptr_number() const noexcept { return std::get_if<number>(&m_var); }
	const string* ptr_string() const noexcept { auto* ptr = std::get_if<sptr_string_t>(&m_var);  return ptr ? ptr->get() : nullptr; }
	const array*  ptr_array()  const noexcept { auto* ptr = std::get_if<sptr_array_t>(&m_var);  return ptr ? ptr->get() : nullptr; }
	const object* ptr_object() const noexcept { auto* ptr = std::get_if<sptr_object_t>(&m_var);  return ptr ? ptr->get() : nullptr; }

	// return the underlying smart pointer
	// do not set to nullptr, will lead to nullptr dereference
	sptr_string_t& sptr_string() { return std::get<sptr_string_t>(m_var); }
	sptr_array_t&  sptr_array()  { return std::get<sptr_array_t>(m_var); }
	sptr_object_t& sptr_object() { return std::get<sptr_object_t>(m_var); }

private:
	template<class T>
	smart_ptr<T> _get_moved() {
		smart_ptr<T>* ptr = std::get_if<smart_ptr<T>>(&m_var);
		if (!ptr) return nullptr;
		smart_ptr<T> sptr(std::move(*ptr));
		m_var = nullptr;
		return sptr;
	}

public:
	sptr_string_t get_moved_string() { return _get_moved<string>(); }
	sptr_array_t  get_moved_array()  { return _get_moved<array>(); }
	sptr_object_t get_moved_object() { return _get_moved<object>(); }

	// if sptr_string_t is not copyable (i.e. std::unique_ptr), disable get_shared_*
	template<class P = sptr_string_t>
	std::enable_if_t<std::is_copy_assignable_v<P>, P> get_shared_string() const { 
		auto* ptr = std::get_if<sptr_string_t>(&m_var);  
		return ptr ? *ptr : nullptr; 
	}

	template<class P = sptr_array_t>
	std::enable_if_t<std::is_copy_assignable_v<P>, P> get_shared_array() const { 
		auto* ptr = std::get_if<sptr_array_t>(&m_var);  
		return ptr ? *ptr : nullptr;
	}

	template<class P = sptr_object_t>
	std::enable_if_t<std::is_copy_assignable_v<P>, P> get_shared_object() const { 
		auto* ptr = std::get_if<sptr_object_t>(&m_var);  
		return ptr ? *ptr : nullptr; 
	}

private:
	static void _dump_number(writer* wr, number num) {
		if (!isfinite(num)) {
			wr->write("null");
			return;
		}
		char buf[32];
		if (fabs(num) <= INT_MAX && int(num) == num) {
			sprintf(buf, "%d", int(num));
		}
		else {
			sprintf(buf, "%.17g", num);	 // 17 == std::numeric_limits<double>::max_digits10
		}
		wr->write_c(buf);
	}

	static void _dump_string(writer* wr, const string& str, bool ensure_ascii) {
		static constexpr char HEX[] = "0123456789abcdef";
		wr->write('"');
		size_t n = str.length();
		for (size_t i = 0; i < n; i++) {
			char ch = str[i];
			switch (ch) {
			case '"': wr->write("\\\""); break;
			case '\\': wr->write("\\\\"); break;
			case '\b': wr->write("\\b"); break;
			case '\f': wr->write("\\f"); break;
			case '\n': wr->write("\\n"); break;
			case '\r': wr->write("\\r"); break;
			case '\t': wr->write("\\t"); break;
			case '\x7f': wr->write("\\u007f"); break;
			default:
				uint8_t uch = ch;
				if (uch < 0x20) {
					char buf[] = "\\u0000";
					buf[4] = ch < 0x10 ? '0' : '1';
					buf[5] = HEX[ch & 0x0f];
					wr->write(buf, 6);
				}
				// TODO convert utf8
				else if (ensure_ascii && uch >= 0x80) {
					char buf[] = "\\u0000";
					buf[4] = HEX[uch >> 4];
					buf[5] = HEX[uch & 0x0f];
					wr->write(buf, 6);
				}
				else {
					wr->write(ch);
				}
			}
		}
		wr->write('"');
	}

	struct dump_context {
		writer* wr;
		const dump_options opt;
		int indent = 0;
		static constexpr int SP_N = 64;
		char spaces[SP_N] = "";	// fill consecutive indent_char, may be redundant

		dump_context(writer* wr, const dump_options& options) : opt(options), wr(wr) {
			if (opt.indent > 0) memset(spaces, opt.indent_char, SP_N);
			else indent = -1;
		}

		void newline() {
			if (indent < 0) return;
			wr->write('\n');
			if (indent == 0) return;
			int n = indent;
			while (n >= SP_N) wr->write(spaces), n -= SP_N;
			wr->write(spaces, n);
		}
	};

	void _dump(dump_context& ctx) const {
		// TODO use std::visit
		switch (m_var.index()) {
		case 0: return ctx.wr->write("null");
		case 1: return get_bool() ? ctx.wr->write("true") : ctx.wr->write("false");
		case 2: return _dump_number(ctx.wr, get_number());
		case 3: return _dump_string(ctx.wr, get_string(), ctx.opt.ensure_ascii);
		case 4: {	// array
			auto& arr = get_array();
			if (arr.empty()) return ctx.wr->write("[]");
			ctx.wr->write('[');
			ctx.indent += ctx.opt.indent;
			bool first = true;
			for (auto& j : arr) {
				if (first) first = false;
				else ctx.wr->write(',');
				ctx.newline();
				j._dump(ctx);
			}
			ctx.indent -= ctx.opt.indent;
			ctx.newline();
			return ctx.wr->write(']');
		}
		case 5: {	// object
			auto& obj = get_object();
			if (obj.empty()) return ctx.wr->write("{}");
			ctx.wr->write('{');
			ctx.indent += ctx.opt.indent;
			bool first = true;
			for (auto& p : obj) {
				if (first) first = false;
				else ctx.wr->write(',');
				ctx.newline();
				_dump_string(ctx.wr, p.first, ctx.opt.ensure_ascii);
				ctx.wr->write(": ");
				p.second._dump(ctx);
			}
			ctx.indent -= ctx.opt.indent;
			ctx.newline();
			return ctx.wr->write('}');
		}
		}
	}

public:
	template<class Target>
	void dump(Target& target, const dump_options& options = {}) const {
		auto wr = writer::New(target);
		dump_context ctx(wr.get(), options);
		_dump(ctx);
		if (options.indent >= 0) wr->write('\n');
	}

	template<class OutIt>
	void dump(OutIt&& iter, const dump_options& options = {}) const {
		std::void_t<decltype(*iter++ = ' '), decltype(OutIt(iter))>(0);	// check if iter is an output iterator
		dump(iter, options);
	}

	string dumps(const dump_options& options = {}) const {
		string str{};
		dump(str, options);
		return str;
	}

private:
	// all _parse* return EOF for nothing to read, '\0'(false) for parse failed

	// parse number and store to *this, ch is the read char and must be - or 0-9
	// since number do not have a terminator, return the non-number char, returning '\0' means parse failed
	char _parse_number(reader* rd, char ch) {
		bool neg = ch == '-';
		if (neg) {
			ch = rd->read();
			if (!isdigit(ch)) return false;
		}
		number num = 0;
		if (ch != '0') {
			do {
				num = num * 10 + (ch - '0');
				ch = rd->read();
			} while (isdigit(ch));
		}
		else ch = rd->read();

		if (ch == '.') {
			number base = 1;
			while (isdigit(ch = rd->read())) {
				base /= 10;
				num += base * (ch - '0');
			}
		}
		if (ch == 'E' || ch == 'e') {
			ch = rd->read();
			bool eneg = ch == '-';
			if (ch == '+' || ch == '-') ch = rd->read();
			if (!isdigit(ch)) return false;
			int expo = ch - '0';
			while (isdigit(ch = rd->read())) {
				expo = expo * 10 + (ch - '0');
			}
			num *= pow(10, eneg ? -expo : expo);
		}
		m_var = neg ? -num : num;
		return isspace(ch) ? rd->nonspace_read() : ch;
	}

	static int _read_hex4(reader* rd) {
		char h[5]{ rd->read(), rd->read(), rd->read(), rd->read(), '\0' };
		int ret = 0;
		for (int i = 0; i < 4; i++) {
			int bits = (3 - i) * 4;
			if (isdigit(h[i])) ret |= (h[i] - '0') << bits;
			else if (unsigned(h[i] - 'a') < 6u) ret |= (h[i] - 'a' + 10) << bits;
			else if (unsigned(h[i] - 'A') < 6u) ret |= (h[i] - 'A' + 10) << bits;
			else return false;
		}
		return ret;
	}

	static void _store_utf8(int cp, char* out) {
		if (cp < 0x80) out[0] = cp, out[1] = 0;
		else if (cp <= 0x07ff) {
			out[0] = 0xc0 | cp >> 6;
			out[1] = 0x80 | cp & 0x3f;
			out[2] = 0;
		}
		else if (cp <= 0xffff) {
			out[0] = 0xe0 | cp >> 12;
			out[1] = 0x80 | cp >> 6 & 0x3f;
			out[2] = 0x80 | cp & 0x3f;
			out[3] = 0;
		}
		else {
			out[0] = 0xf0 | cp >> 18;
			out[1] = 0x80 | cp >> 12 & 0x3f;
			out[2] = 0x80 | cp >> 6 & 0x3f;
			out[3] = 0x80 | cp & 0x3f;
			out[4] = 0;
		}
	}

	static char _parse_string(reader* rd, string& out) {
		for (char ch = rd->read(); ch != '"'; ch = rd->read()) {
			if (ch == EOF) return false;
			if (ch != '\\') {
				out += ch;
				continue;
			}
			switch (ch = rd->read())
			{
			case '"': 
			case '\\':
			case '/': out += ch; break;
			case 'b': out += '\b'; break;
			case 'f': out += '\f'; break;
			case 'n': out += '\n'; break;
			case 'r': out += '\r'; break;
			case 't': out += '\t'; break;
			case 'u': {
				int cp = _read_hex4(rd);
				if (!cp) return false;
				// TODO UTF-16 surrogate pair
				char u8str[8];
				_store_utf8(cp, u8str);
				out += u8str;
				break;
			}
			default: (out += '\\') += ch; break;	// TODO return false?
			}
		}
		return rd->nonspace_read();
	}

	static char _parse_array(reader* rd, array& out) {
		char ch = rd->nonspace_read();
		if (ch == ']') return rd->nonspace_read();
		for (;;) {
			ch = out.emplace_back()._parse(rd, ch);
			if (!ch) return false;
			if (ch == ']') return rd->nonspace_read();
			if (ch != ',') return false;
			ch = rd->nonspace_read();
		}
	}

	static char _parse_object(reader* rd, object& out) {
		char ch = rd->nonspace_read();
		if (ch == '}') return rd->nonspace_read();
		for (; ch == '"'; ch = rd->nonspace_read()) {
			string key;
			basic_json value;
			if (!(ch = _parse_string(rd, key))) return false;
			if (ch != ':') return false;
			if (!(ch = value._parse(rd, rd->nonspace_read()))) return false;
			out.emplace(std::move(key), std::move(value));
			if (ch == '}') return rd->nonspace_read();
			if (ch != ',') return false;
		}
		return false;
	}

	char _parse(reader* rd, char ch) {
		if (isdigit(ch)) return _parse_number(rd, ch);
		else switch (ch) {
		case '"': return _parse_string(rd, set_string());
		case '{': return _parse_object(rd, set_object());
		case '[': return _parse_array(rd, set_array());
		case '-': return _parse_number(rd, ch);
		case 't': 
			if (rd->read() != 'r' || rd->read() != 'u' || rd->read() != 'e') return false;
			m_var = true;
			return rd->nonspace_read();
		case 'f':
			if (rd->read() != 'a' || rd->read() != 'l' || rd->read() != 's' || rd->read() != 'e') return false;
			m_var = false;
			return rd->nonspace_read();
		case 'n':
			if (rd->read() != 'u' || rd->read() != 'l' || rd->read() != 'l') return false;
			m_var = nullptr;
			return rd->nonspace_read();
		default: return false;
		}
	}

	bool _load(reader* rd, bool nothrow) {
		char ch = rd->nonspace_read();
		bool res = _parse(rd, ch);
		if (!res && !nothrow) throw std::invalid_argument("not a valid json");
		return res;
	}

public:
	template<class Target>
	bool load(Target& target, bool nothrow = false) {
		auto rd = reader::New(target);
		return _load(rd.get(), nothrow);
	}

	template<class Iter>
	bool load(Iter first, Iter last, bool nothrow = false) {
		static_assert(std::is_same_v<std::iterator_traits<Iter>::value_type, char>);
		auto rd = reader::New(first, last);
		return _load(rd.get(), nothrow);
	}

	bool loads(const char* str, bool nothrow = false) { return load(str, nothrow); }
	bool loads(const std::string& str, bool nothrow = false) { return loads(str.data(), nothrow); }

	template<class Target, class = typename std::iterator_traits<Target>::value_type>
	static basic_json parse(Target& target) { 
		basic_json j;
		j.load(target);
		return j;
	}

	template<class Iter>
	static basic_json parse(Iter first, Iter last) {
		basic_json j;
		j.load(first, last);
		return j;
	}

	static basic_json parse(const char* str) { return parse<const char*>(str); }
	static basic_json parse(const std::string& str) { return parse(str.data()); }
};

using json         = basic_json<json_traits>;
using json_shared  = basic_json<json_shared_traits>;
using json_inplace = basic_json<json_inplace_traits>;

}