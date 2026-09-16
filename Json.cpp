#include "Json.h"
#include <cstdio>
#include <cstdlib>
#include <cctype>
#include <cmath>


namespace
{

	const JsonValue g_null{};

	void EscapeTo(std::string& out, const std::string& s)
	{
		out += '"';
		for (unsigned char c : s)
		{
			switch (c)
			{
			case '"':  out += "\\\""; break;
			case '\\': out += "\\\\"; break;
			case '\b': out += "\\b";  break;
			case '\f': out += "\\f";  break;
			case '\n': out += "\\n";  break;
			case '\r': out += "\\r";  break;
			case '\t': out += "\\t";  break;
			default:
				if (c < 0x20)
				{
					char buf[8];
					std::snprintf(buf, sizeof(buf), "\\u%04X", c);
					out += buf;
				}
				else
				{
					out += static_cast<char>(c);
				}
				break;
			}

		}
		out += '"';
	}

	void NumberTo(std::string& out, double v)
	{
		char buf[40];
		if (std::isfinite(v) && v == std::floor(v) && std::fabs(v) < 1e15)
		{
			std::snprintf(buf, sizeof(buf), "%.0f", v);
		}
		else
		{
			std::snprintf(buf, sizeof(buf), "%.9g", v);//floatを往復できる桁数
		}

		out += buf;

	}

	//解析器
	class Parser
	{
	public:

		explicit Parser(const std::string& text) : m_s(text) {}

		bool Run(JsonValue& out)
		{
			SkipBom();
			SkipSpace();
			if (!ParseValue(out)) return false;
			SkipSpace();
			if (m_i != m_s.size()) return Fail("末尾に余分な文字があります");
			return true;
		}

		const std::string& Error() const { return m_error; }

	private:

		void SkipBom()
		{
			if (m_s.size() >= 3 &&
				static_cast<unsigned char>(m_s[0]) == 0xEF &&
				static_cast<unsigned char>(m_s[1]) == 0xBB &&
				static_cast<unsigned char>(m_s[2]) == 0xBF)
			{
				m_i = 3;
			}
		}

		void SkipSpace()
		{
			while (m_i < m_s.size())
			{
				const char c = m_s[m_i];
				if (c == ' ' || c == '\t' || c == '\r' || c == '\n')
				{
					++m_i;
				}
				else break;
			}
		}

		bool Fail(const char* why)
		{
			char buf[32];
			std::snprintf(buf, sizeof(buf), "%zu文字目:", m_i);
			m_error = buf;
			m_error += why;
			return false;
		}

		//先頭の1文字で値の種類を見分けて振り分ける
		bool ParseValue(JsonValue& out)
		{
			if (m_i >= m_s.size())return Fail("値がありません");

			switch (m_s[m_i])
			{
			case '{': return ParseObject(out);
			case '[': return ParseArray(out);

			case '"':
			{
				std::string s;
				if (!ParseString(s)) return false;
				out = JsonValue(std::move(s));
				return true;
			}

			case 't':
				if (m_s.compare(m_i, 4, "true") != 0) return Fail("true として読めません");
				m_i += 4;
				out = JsonValue(true);
				return true;

			case 'f':
				if (m_s.compare(m_i, 5, "false") != 0) return Fail("false として読めません");
				m_i += 5;
				out = JsonValue(false);
				return true;

			case 'n':
				if (m_s.compare(m_i, 4, "null") != 0) return Fail("null として読めません");
				m_i += 4;
				out = JsonValue();
				return true;

			default:
				return ParseNumber(out);
			}

		}

		bool ParseNumber(JsonValue& out)
		{
			const size_t start = m_i;
			bool any = false;

			//符号
			if (m_i < m_s.size() && (m_s[m_i] == '-' || m_s[m_i] == '+'))	++m_i;


			//整数部
			while (m_i < m_s.size() && std::isdigit(static_cast<unsigned char>(m_s[m_i])))
			{
				++m_i;
				any = true;

			}

			// 小数部
	// 指数部とは独立した if にする。入れ子にすると 3.5 のように e を持たない
	// 小数が読めず、m_i が '.' の手前で止まって呼び出し元まで失敗する。
			if (m_i < m_s.size() && m_s[m_i] == '.')
			{
				++m_i;
				while (m_i < m_s.size() && std::isdigit(static_cast<unsigned char>(m_s[m_i])))
				{
					++m_i;
					any = true;
				}
			}

			//指数部
			if (any && m_i < m_s.size() && (m_s[m_i] == 'e' || m_s[m_i] == 'E'))
			{
				++m_i;

				// 1e-5 のように指数へ符号が付く場合がある
				if (m_i < m_s.size() && (m_s[m_i] == '-' || m_s[m_i] == '+')) ++m_i;

				while (m_i < m_s.size() && std::isdigit(static_cast<unsigned char>(m_s[m_i])))++m_i;
			}

			if (!any)return Fail("数値として読めません");

			// strtod は小数点の文字がロケール依存。このプロジェクトは setlocale を
	// 呼んでいない（＝"C" ロケールのまま）ので '.' で正しく読める。
			out = JsonValue(std::strtod(m_s.substr(start, m_i - start).c_str(), nullptr));
			return true;



		}

		bool ParseString(std::string& out)
		{
			if (m_i >= m_s.size() || m_s[m_i] != '"')return Fail("文字列の開始が\"ではありません");
			
			++m_i;
			out.clear();
			while (true)
			{
				if (m_i >= m_s.size())return Fail("文字列が閉じられていません");

				const char c = m_s[m_i++];
				if (c == '"')return true;

				if (c != '\\')
				{
					out += c;			// UTF-8 のバイトはそのまま通す
					continue;
				}

				if (m_i >= m_s.size()) return Fail("エスケープが途中で終わっています");

				switch (m_s[m_i++])
				{
				case '"':  out += '"';  break;
				case '\\': out += '\\'; break;
				case '/':  out += '/';  break;
				case 'b':  out += '\b'; break;
				case 'f':  out += '\f'; break;
				case 'n':  out += '\n'; break;
				case 'r':  out += '\r'; break;
				case 't':  out += '\t'; break;
				case 'u':  return Fail("\\u エスケープには未対応です（UTF-8のまま保存してください）");
				default:   return Fail("不明なエスケープです");
				}
			}

		}
		bool ParseArray(JsonValue& out)
		{
			++m_i;						// '[' を読み飛ばす
			out = JsonValue::MakeArray();

			SkipSpace();
			if (m_i < m_s.size() && m_s[m_i] == ']')
			{
				++m_i;
				return true;			// 空配列
			}

			while (true)
			{
				JsonValue v;
				SkipSpace();
				if (!ParseValue(v)) return false;
				out.Push(std::move(v));

				SkipSpace();
				if (m_i >= m_s.size()) return Fail("配列が閉じられていません");
				if (m_s[m_i] == ',') { ++m_i; continue; }
				if (m_s[m_i] == ']') { ++m_i; return true; }
				return Fail("配列の区切りが , でも ] でもありません");
			}
		}

		bool ParseObject(JsonValue& out)
		{
			++m_i;						// '{' を読み飛ばす
			out = JsonValue::MakeObject();

			SkipSpace();
			if (m_i < m_s.size() && m_s[m_i] == '}')
			{
				++m_i;
				return true;			// 空オブジェクト
			}

			while (true)
			{
				SkipSpace();
				std::string key;
				if (!ParseString(key)) return false;

				SkipSpace();
				if (m_i >= m_s.size() || m_s[m_i] != ':') return Fail("キーの後に : がありません");
				++m_i;

				JsonValue v;
				SkipSpace();
				if (!ParseValue(v)) return false;
				out.Set(key, std::move(v));

				SkipSpace();
				if (m_i >= m_s.size()) return Fail("オブジェクトが閉じられていません");
				if (m_s[m_i] == ',') { ++m_i; continue; }
				if (m_s[m_i] == '}') { ++m_i; return true; }
				return Fail("オブジェクトの区切りが , でも } でもありません");
			}



		}

		const std::string& m_s;		// 解析対象（参照で持つのでコピーしない）
		size_t				m_i = 0;	// 今読んでいる位置
		std::string			m_error{};

	};


}

//	JsonValue

JsonValue JsonValue::MakeArray() { JsonValue v; v.m_type = Type::Array; return v; }
JsonValue JsonValue::MakeObject() { JsonValue v;v .m_type = Type::Object; return v; }

bool JsonValue::AsBool(bool def)const
{
	if (m_type == Type::Bool) return m_bool;
	if (m_type == Type::Number)return m_num != 0.0;
	return def;
	
}

double JsonValue::AsNumber(double def)const
{
	if (m_type == Type::Number)return m_num;
	if (m_type == Type::Bool) return m_bool ? 1.0 : 0.0;
	return def;
}

std::string JsonValue::AsString(const std::string& def)const
{
	return (m_type == Type::String) ? m_str : def;
}

bool JsonValue::Has(const std::string& key)const
{
	if (m_type != Type::Object)return false;

	for (const auto& k : m_keys)
	{
		if (k == key) return true;
	}
	return false;

}

const JsonValue& JsonValue::operator[](const std::string& key)const
{
	if (m_type == Type::Object)
	{
		for (size_t i = 0; i < m_keys.size(); ++i)
		{
			if (m_keys[i] == key)return m_values[i];
		}
	}
	return g_null;
}

void JsonValue::Set(const std::string& key, JsonValue value)
{
	m_type = Type::Object;

	for (size_t i = 0; i < m_keys.size(); i++)
	{
		if (m_keys[i] == key)
		{
			m_values[i] = std::move(value);
			return;
		}
	}

	m_keys.push_back(key);
	m_values.push_back(std::move(value));

	
}

size_t JsonValue::Size()const
{
	return (m_type == Type::Array) ? m_array.size() : 0;

}

const JsonValue& JsonValue::At(size_t index) const
{
	if (m_type == Type::Array && index < m_array.size()) return m_array[index];
	return g_null;

}

void JsonValue::Push(JsonValue value)
{
	m_type = Type::Array;//NullのままPushされた場合も

	m_array.push_back(std::move(value));
}

std::string JsonValue::Dump(int indent)const
{
	std::string out;
	DumpTo(out, indent, 0);
	return out;
}

void JsonValue::DumpTo(std::string& out, int indent, int depth)const
{
	const bool pretty = (indent >= 0);

	//改行
	auto newline = [&](int d)
		{
			if (!pretty)return;
			out += '\n';
			out.append(static_cast<size_t>(indent) * d, ' ');


		};

	switch (m_type)
	{


	case Type::Null:out += "null"; break;
	case Type::Bool:out += (m_bool ? "true" : "false"); break;
	case Type::Number:NumberTo(out, m_num); break;
	case Type::String:EscapeTo(out, m_str); break;
	case Type::Array:
	{
		if (m_array.empty())
		{
			out += "[]";
			break;

		}

		bool inlineArray = true;
		for (const auto& v : m_array)
		{
			if (v.m_type != Type::Number)
			{
				inlineArray = false;
				break;
			}
		}

		out += '[';
		for (size_t i = 0; i < m_array.size(); ++i)
		{
			if (i)
			{
				out += ',';
				if (pretty && inlineArray)out += ' ';

			}
			if (!inlineArray)newline(depth + 1);
			m_array[i].DumpTo(out, indent, depth + 1);

		}
		if (!inlineArray)newline(depth);
		out += ']';
		break;

	}

	case Type::Object:
		if (m_keys.empty())
		{
			out += "{}";
			break;
		}

		out += '{';
		for (size_t i = 0; i < m_keys.size(); ++i)
		{
			if (i)out += ',';
			newline(depth + 1);
			EscapeTo(out, m_keys[i]);
			out += ':';
			if (pretty)out += ' ';
			m_values[i].DumpTo(out, indent, depth + 1);


		}
		newline(depth);
		out += '}';
		break;

	
	}
}

bool JsonValue::Parse(const std::string& text, JsonValue& out, std::string* error)
{
	Parser p(text);
	if (p.Run(out))return true;

	if (error)*error = p.Error();
	out = JsonValue();//失敗時はNUllに
	return false;
}