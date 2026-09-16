#pragma once
#include <string>
#include <vector>


class JsonValue
{
public:
	enum class Type { Null, Bool, Number, String, Array, Object };

	JsonValue() = default;
	explicit JsonValue(bool b) : m_type(Type::Bool), m_bool(b) {}
	explicit JsonValue(double n) : m_type(Type::Number), m_num(n) {}
	explicit JsonValue(float n) : m_type(Type::Number), m_num(n) {}
	explicit JsonValue(int n) : m_type(Type::Number), m_num(n) {}
	explicit JsonValue(std::string s) : m_type(Type::String),
		m_str(std::move(s)) {}

	//空の配列
	static JsonValue MakeArray();
	static JsonValue MakeObject();

	Type GetType() const { return m_type; }
	bool IsNull() const { return m_type == Type::Null; }

	//値の取り出し
	bool	AsBool	(bool	def = false)const;
	double	AsNumber(double def = 0.0)	const;
	float	AsFloat(float	def = 0.0f)	const
	{
		return static_cast<float>(AsNumber(def));
	}
	std::string AsString(const std::string& def =std::string())const;

	//オブジェクト
	bool Has(const std::string& key)const;
	const JsonValue& operator[](const std::string& key)const;
	//なければNULLを返す
	void Set(const std::string& key, JsonValue value);
	//既存キーは上書き

	//配列
	// ※ 添字は At() を使う。operator[] を size_t でも多重定義すると
	//    v[0] が「整数」と「ヌルポインタ定数→std::string」の両方に取れて曖昧になるため。

	size_t Size()const;
	const JsonValue& At(size_t index) const;
	void Push(JsonValue value);

	//文字列化

	std::string Dump(int indent = 2)const;


	//解析

	static bool Parse(const std::string& text, JsonValue& out, std::string* error = nullptr);




private:
	void DumpTo(std::string& out, int indent, int depth) const;

	Type		m_type = Type::Null;
	bool		m_bool = false;
	double		m_num = 0.0;
	std::string	m_str{};

	std::vector<JsonValue> m_array{};

	// オブジェクトはキーと値を並行配列で持つ。
	// 挿入順が保たれるので、保存し直しても git の差分が散らからない。
	std::vector<std::string> m_keys{};
	std::vector<JsonValue>   m_values{};
};

