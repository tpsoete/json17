#pragma once

#include <cassert>	// assert
#include <map>
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

	dump_options(int indent = -1, char indent_char = ' ')
		: indent(indent), indent_char(indent_char) {}
};

class json
{
public:
	using number = double;
	using string = std::string;
	using array  = std::vector<json>;
	using object = std::map<string, json>;

	using variant = std::variant<std::nullptr_t, bool, number, string, array, object>;

private:
	variant m_var;

	class writer {
	public:
		string s;

		void write(char c) { s += c; }
		void write(const char* str, size_t n) { s.append(str, n); }
		void write_c(const char* str) { s.append(str); }

		template<size_t N>
		inline void write(const char(&arr)[N]) { static_assert(N > 2);  s.append(arr, N - 1); }
	};

	class reader {
	public:
		const char* ptr;

		reader(const char* ptr) : ptr(ptr) {}
		char read() { return *ptr == '\0' ? EOF : *ptr++; }

		char nonspace_read() {
			char ch;
			do ch = read(); while (isspace(ch));
			return ch;
		}
	};

public:
	json() = default;
	json(std::nullptr_t)  : m_var(nullptr) {}
	json(bool v)          : m_var(v) {}
	json(number v)        : m_var(v) {}
	json(int v)           : m_var(number(v)) {}
	json(const string& v) : m_var(v) {}
	json(string&& v)      : m_var(v) {}
	json(const char* v)   : m_var(string(v)) {}
	json(const array& v)  : m_var(v) {}
	json(array&& v)       : m_var(v) {}
	json(const object& v) : m_var(v) {}
	json(object&& v)      : m_var(v) {}

	json(json&&) = default;
	json& operator=(json&&) = default;

	json(const json&) = default;
	json& operator=(const json&) = default;

	variant&       get_variant()       noexcept { return m_var; }
	const variant& get_variant() const noexcept { return m_var; }

	json_type get_type() const noexcept { return (json_type)m_var.index(); }
	bool is_null()   const noexcept { return m_var.index() == 0; }
	bool is_bool()   const noexcept { return m_var.index() == 1; }
	bool is_number() const noexcept { return m_var.index() == 2; }
	bool is_string() const noexcept { return m_var.index() == 3; }
	bool is_array()  const noexcept { return m_var.index() == 4; }
	bool is_object() const noexcept { return m_var.index() == 5; }

	bool&   get_bool()   { return std::get<bool>(m_var); }
	number& get_number() { return std::get<number>(m_var); }
	string& get_string() { return std::get<string>(m_var); }
	array&  get_array()  { return std::get<array>(m_var); }
	object& get_object() { return std::get<object>(m_var); }

	bool          get_bool()   const { return std::get<bool>(m_var); }
	number        get_number() const { return std::get<number>(m_var); }
	int           get_int()    const { return static_cast<int>(get_number()); }
	const string& get_string() const { return std::get<string>(m_var); }
	const array&  get_array()  const { return std::get<array>(m_var); }
	const object& get_object() const { return std::get<object>(m_var); }

	string& set_string() { m_var = string();  return get_string(); }
	array&  set_array()  { m_var = array();  return get_array(); }
	object& set_object() { m_var = object();  return get_object(); }

	// auto expand if idx >= get_array().size(), or create one if is_null()
	// throws if *this is not null nor an array
	json& operator[](size_t idx) {
		if (is_null()) {
			m_var = array(idx + 1);
			return get_array()[idx];
		}
		auto& arr = get_array();
		if (arr.size() <= idx) arr.resize(idx + 1);
		return arr[idx];
	}

	// array is immutable, must be array and does range check
	const json& operator[](size_t idx) const { return get_array().at(idx); }

	// auto fill null if desired key does not exist, or create one if is_null()
	// throws if *this is not null nor an object
	json& operator[](const string& key) {
		if (is_null()) m_var = object();
		return get_object()[key];
	}

	// object is immutable, the key must exist
	const json& operator[](const string& key) const {
		auto& obj = get_object();
		auto it = obj.find(key);
		if (it == obj.end()) throw std::out_of_range("key does not exist");
		return it->second;
	}

	bool*   ptr_bool()   noexcept { return std::get_if<bool>(&m_var); }
	number* ptr_number() noexcept { return std::get_if<number>(&m_var); }
	string* ptr_string() noexcept { return std::get_if<string>(&m_var); }
	array*  ptr_array()  noexcept { return std::get_if<array>(&m_var); }
	object* ptr_object() noexcept { return std::get_if<object>(&m_var); }

	const bool*   ptr_bool()   const noexcept { return std::get_if<bool>(&m_var); }
	const number* ptr_number() const noexcept { return std::get_if<number>(&m_var); }
	const string* ptr_string() const noexcept { return std::get_if<string>(&m_var); }
	const array*  ptr_array()  const noexcept { return std::get_if<array>(&m_var); }
	const object* ptr_object() const noexcept { return std::get_if<object>(&m_var); }

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

	static void _dump_string(writer* wr, const string& str) {
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
		case 3: return _dump_string(ctx.wr, get_string());
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
				_dump_string(ctx.wr, p.first);
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
	string dumps(const dump_options& options = {}) const {
		writer wr;
		dump_context ctx(&wr, options);
		_dump(ctx);
		return wr.s;
	}

	string dumps(int indent, char indent_char) const {
		return dumps(dump_options(indent, indent_char));
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

	static void _store_utf8(int cp, string& out_str) {
		char out[5] = "";
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
		out_str += out;
	}

	static char _parse_string(reader* rd, string& out) {
		int last_cp = 0;	// used for surrogate pair
		for (char ch = rd->read(); ch != '"'; ch = rd->read()) {
			if (ch == EOF) return false;
			if (ch != '\\') out += ch;
			else switch (ch = rd->read())
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
				if (cp >= 0xD800 && cp < 0xDC00) {
					last_cp = cp;
					continue;
				}
				else if (last_cp) {
					if (cp >= 0xDC00 && cp < 0xE000) {
						cp = ((last_cp & 0x3ff) << 10 | cp & 0x3ff) + 0x10000;
					}
					else _store_utf8(last_cp, out);
					last_cp = 0;
				}
				_store_utf8(cp, out);
				continue;
			}
			default: (out += '\\') += ch; break;	// TODO return false?
			}

			if (last_cp) {
				_store_utf8(last_cp, out);
				last_cp = 0;
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
			json value;
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
	bool loads(const char* str, bool nothrow = false) {
		reader rd(str);
		return _load(&rd, nothrow); 
	}
	bool loads(const std::string& str, bool nothrow = false) { return loads(str.data(), nothrow); }

	static json parse(const char* str) {
		json j;
		j.loads(str);
		return j;
	}
	static json parse(const std::string& str) { return parse(str.data()); }
};

}