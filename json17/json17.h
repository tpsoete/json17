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

	// convenience functions
	inline void write_c(const char* str) { write(str, strlen(str)); }

	template<size_t N>
	inline void write(const char(&arr)[N]) { static_assert(N > 2);  write(arr, N - 1); }

	template<class Target>
	static std::unique_ptr<writer> New(Target& target) {
		return std::make_unique<writer_interface<Target>>(target);
	}
};

// output iterators of char
template<class OutIt>
class writer_interface : public writer 
{
public:
	static_assert(std::is_same_v<std::iterator_traits<OutIt>::value_type, char>);

	OutIt it;
	writer_interface(OutIt it) : it(it) {}
	void write(char ch) override { *(it++) = ch; }
	void write(const char* str, size_t n) override {
		for (size_t i = 0; i < n; i++) *(it++) = str[i];
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

	variant_t&       get_variant()       { return m_var; }
	const variant_t& get_variant() const { return m_var; }

	json_type get_type() const { return (json_type)m_var.index(); }
	bool is_null()   const { return m_var.index() == 0; }
	bool is_bool()   const { return m_var.index() == 1; }
	bool is_number() const { return m_var.index() == 2; }
	bool is_string() const { return m_var.index() == 3; }
	bool is_array()  const { return m_var.index() == 4; }
	bool is_object() const { return m_var.index() == 5; }

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

	bool*   ptr_bool()   { return std::get_if<bool>(&m_var); }
	number* ptr_number() { return std::get_if<number>(&m_var); }
	string* ptr_string() { auto* ptr = std::get_if<sptr_string_t>(&m_var);  return ptr ? ptr->get() : nullptr; }
	array*  ptr_array()  { auto* ptr = std::get_if<sptr_array_t>(&m_var);  return ptr ? ptr->get() : nullptr; }
	object* ptr_object() { auto* ptr = std::get_if<sptr_object_t>(&m_var);  return ptr ? ptr->get() : nullptr; }

	const bool*   ptr_bool()   const { return std::get_if<bool>(&m_var); }
	const number* ptr_number() const { return std::get_if<number>(&m_var); }
	const string* ptr_string() const { auto* ptr = std::get_if<sptr_string_t>(&m_var);  return ptr ? ptr->get() : nullptr; }
	const array*  ptr_array()  const { auto* ptr = std::get_if<sptr_array_t>(&m_var);  return ptr ? ptr->get() : nullptr; }
	const object* ptr_object() const { auto* ptr = std::get_if<sptr_object_t>(&m_var);  return ptr ? ptr->get() : nullptr; }

	// do not set to nullptr, will lead to UB;
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
			sprintf(buf, "%.17g", num);
		}
		wr->write_c(buf);
	}

	static void _dump_string(writer* wr, const string& str, bool ensure_ascii) {
		wr->write('"');
		size_t n = str.length();
		for (size_t i = 0; i < n; i++) {
			char ch = str[i];
			switch (ch) {
			case '"': wr->write("\\\""); break;
			case '\\': wr->write("\\\\"); break;
			case '\b': wr->write("\\\b"); break;
			case '\f': wr->write("\\\f"); break;
			case '\n': wr->write("\\\n"); break;
			case '\r': wr->write("\\\r"); break;
			case '\t': wr->write("\\\t"); break;
			case '\x7f': wr->write("\\u007f"); break;
			default:
				if (ch < 0x20) {
					char buf[] = "\\u0000";
					buf[4] = i < 0x10 ? '0' : '1';
					buf[5] = '0' + (i & 0x0f);
					wr->write(buf, 6);
				}
				// TODO special chars, and ensure_ascii option
				else {
					wr->write(ch);
				}
			}
		}
		wr->write('"');
	}

	struct dump_context {
		writer* wr;
		const dump_options* opt;
		int indent = 0;
		static constexpr int SP_N = 32;
		char spaces[SP_N] = "";

		dump_context(writer* wr, const dump_options& options) : wr(wr), opt(&options) {
			if (opt->indent > 0) memset(spaces, opt->indent_char, SP_N);
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

	void dump(dump_context& ctx) const {
		// TODO use std::visit
		switch (m_var.index()) {
		case 0: return ctx.wr->write("null");
		case 1: return get_bool() ? ctx.wr->write("true") : ctx.wr->write("false");
		case 2: return _dump_number(ctx.wr, get_number());
		case 3: return _dump_string(ctx.wr, get_string(), ctx.opt->ensure_ascii);
		case 4: {	// array
			auto& arr = get_array();
			if (arr.empty()) return ctx.wr->write("[]");
			ctx.wr->write('[');
			ctx.indent += ctx.opt->indent;
			bool first = true;
			for (auto& j : arr) {
				if (first) first = false;
				else ctx.wr->write(',');
				ctx.newline();
				j.dump(ctx);
			}
			ctx.indent -= ctx.opt->indent;
			ctx.newline();
			return ctx.wr->write(']');
		}
		case 5: {	// object
			auto& obj = get_object();
			if (obj.empty()) return ctx.wr->write("{}");
			ctx.wr->write('{');
			ctx.indent += ctx.opt->indent;
			bool first = true;
			for (auto& p : obj) {
				if (first) first = false;
				else ctx.wr->write(',');
				ctx.newline();
				_dump_string(ctx.wr, p.first, ctx.opt->ensure_ascii);
				ctx.wr->write(": ");
				p.second.dump(ctx);
			}
			ctx.indent -= ctx.opt->indent;
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
		dump(ctx);
	}

	string dumps(const dump_options& options = {}) const {
		string str{};
		dump(str, options);
		return str;
	}

	template<class Iter>
	static basic_json load(Iter first, Iter last) {
		static_assert(std::is_same_v<std::iterator_traits<Iter>::value_type, char>);

		return basic_json();
	}
};

using json         = basic_json<json_traits>;
using json_shared  = basic_json<json_shared_traits>;
using json_inplace = basic_json<json_inplace_traits>;

}