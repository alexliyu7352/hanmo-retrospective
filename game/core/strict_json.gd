## 有界JSON读取器；在Godot覆盖同名键之前拒绝重复解码键，并保留数字、深度与字符串边界。
class_name HMStrictJSON
extends RefCounted

var source: String = ""
var cursor: int = 0
var error: String = ""
var string_limit: int = 16384
var _number_expression = RegEx.new()

## 每个读取器只编译一次数字语法；\G绑定当前搜索偏移，不复制余下全文，也不跳过错误字符。
func _init() -> void:
	_number_expression.compile("\\G-?(0|[1-9][0-9]*)(\\.[0-9]+)?([eE][+-]?[0-9]+)?")

## 读取一个完整JSON值；语法和边界通过后才规范化精确且有界的整数。
static func parse_text(text: String, max_string: int = 16384) -> Dictionary:
	var reader = HMStrictJSON.new()
	reader.source = text
	reader.string_limit = max_string
	reader._value(0)
	reader._space()
	if reader.error.is_empty() and reader.cursor != text.length():
		reader.error = "TrailingData"
	if not reader.error.is_empty():
		return {"ok":false, "code":reader.error}
	var parser = JSON.new()
	if parser.parse(text) != OK:
		return {"ok":false, "code":"InvalidJSON"}
	return {"ok":true, "value":_normalize(parser.data)}

## 保留小数供Schema拒绝，仅将精确且安全的整数浮点值转成GDScript整数。
static func _normalize(value: Variant) -> Variant:
	if value is Dictionary:
		var output: Dictionary = {}
		for key in value:
			output[key] = _normalize(value[key])
		return output
	if value is Array:
		var output: Array = []
		for entry in value:
			output.append(_normalize(entry))
		return output
	if typeof(value) == TYPE_FLOAT and is_finite(value) and value == floor(value) and abs(value) <= 2147483647:
		return int(value)
	return value

## 只跳过JSON定义的四种空白，不将任意Unicode空白当成合法分隔符。
func _space() -> void:
	while cursor < source.length() and source[cursor] in [" ", "\t", "\r", "\n"]:
		cursor += 1

## 消费预期标点；失败留给上层返回具体语法错误，不越过未知字符。
func _take(token: String) -> bool:
	_space()
	if source.substr(cursor, token.length()) == token:
		cursor += token.length()
		return true
	return false

## 先隔离并检查带引号字符串，再由Godot解码转义；超长或坏字符串立即拒绝。
func _string() -> String:
	_space()
	if cursor >= source.length() or source[cursor] != '"':
		error = "ExpectedString"
		return ""
	var start: int = cursor
	cursor += 1
	while cursor < source.length():
		var character: String = source[cursor]
		if character == '"':
			cursor += 1
			var parser = JSON.new()
			if parser.parse(source.substr(start, cursor - start)) != OK or not parser.data is String:
				error = "InvalidString"
				return ""
			var decoded: String = parser.data
			if decoded.length() > string_limit:
				error = "StringTooLong"
			return decoded
		if character.unicode_at(0) < 32:
			error = "InvalidString"
			return ""
		if character == "\\":
			cursor += 1
		cursor += 1
	error = "UnterminatedString"
	return ""

## 检查JSON语法、深度与重复键；数字继续匹配原有合法前缀，剩余坏后缀由原有边界规则拒绝。
func _value(depth: int) -> void:
	if not error.is_empty():
		return
	if depth > 32:
		error = "TooDeep"
		return
	_space()
	if cursor >= source.length():
		error = "UnexpectedEnd"
		return
	var character: String = source[cursor]
	if character == '"':
		_string()
	elif character == "{":
		cursor += 1
		var keys: Dictionary = {}
		if _take("}"):
			return
		while error.is_empty():
			var key: String = _string()
			if not error.is_empty():
				return
			if keys.has(key):
				error = "DuplicateKey"
				return
			keys[key] = true
			if not _take(":"):
				error = "ExpectedColon"
				return
			_value(depth + 1)
			# 深度限制不是损坏标点；保留首个限制原因，其他旧错误优先级保持原样。
			if error == "TooDeep": return
			if _take("}"):
				return
			if not _take(","):
				error = "ExpectedComma"
	elif character == "[":
		cursor += 1
		if _take("]"):
			return
		while error.is_empty():
			_value(depth + 1)
			# 深度限制不是损坏标点；保留首个限制原因，其他旧错误优先级保持原样。
			if error == "TooDeep": return
			if _take("]"):
				return
			if not _take(","):
				error = "ExpectedComma"
	elif character in ["t", "f", "n"]:
		if not (_take("true") or _take("false") or _take("null")):
			error = "InvalidLiteral"
	else:
		var found = _number_expression.search(source,cursor)
		if found == null or found.get_start()!=cursor:
			error = "InvalidNumber"
			return
		var token: String = found.get_string()
		if not is_finite(token.to_float()):
			error = "NonFiniteNumber"
		cursor += token.length()
