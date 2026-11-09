## 常驻checkpoint的有界物理封装；不解释世界、不安装候选、不把摘要当作语义验证。
extends RefCounted
const Strict=preload("res://core/strict_json.gd")
const Values=preload("res://core/base/value_checks.gd")
const MAX_BYTES: int=64*1024*1024
const MAX_CONTINUATION_BYTES: int=2048
const MAGIC: String="HM-RESIDENT-SAVE"
const FIELDS: Dictionary={"magic":TYPE_STRING,"format_version":TYPE_INT,"save_id":TYPE_STRING,"generation":TYPE_INT,"payload_length":TYPE_INT,"payload_sha256":TYPE_STRING,"payload_base64":TYPE_STRING}
const CONTINUATION_FIELDS: Dictionary={"continuation_length":TYPE_INT,"continuation_sha256":TYPE_STRING,"continuation_payload_sha256":TYPE_STRING,"continuation_base64":TYPE_STRING}
const PHYSICAL_ERRORS: Array[String]=["InvalidSaveHeader","InvalidSaveMetadata","ChecksumMismatch","InvalidResidentBase64","InvalidJSON","TrailingData","ExpectedString","InvalidString","UnterminatedString","UnexpectedEnd","DuplicateKey","ExpectedColon","ExpectedComma","InvalidLiteral","InvalidNumber"]

## 槽路径许可仍由NativeStorage检查；这里只保原封装非空、最多128码点的身份规则。
static func _valid_save_id(value: String) -> bool:
	return not value.is_empty() and value.length()<=128

## base64字符全是ASCII；先计算扩张长度，避免已知超限时分配巨大编码副本。
static func _base64_length(byte_count: int) -> int:
	@warning_ignore("integer_division")
	return ((byte_count+2)/3)*4

## 对解码原字节求摘要；引擎哈希失败是资源/内部错误，不准许可旧银行回退。
static func _sha256(bytes: PackedByteArray) -> Dictionary:
	var context=HashingContext.new()
	if context.start(HashingContext.HASH_SHA256)!=OK or context.update(bytes)!=OK:
		return {"ok":false,"code":"ResidentHashFailed"}
	return {"ok":true,"digest":context.finish().hex_encode()}

## 无许可仍逐字产生原七字段格式；有许可采用显式格式2，语义验证仍归唯一worker。
static func encode(bytes: PackedByteArray, generation: int, slot: String, continuation_bytes: PackedByteArray=PackedByteArray()) -> Dictionary:
	if not Values.whole(generation) or not _valid_save_id(slot): return {"ok":false,"code":"InvalidSaveMetadata"}
	if bytes.is_empty(): return {"ok":false,"code":"InvalidSaveMetadata"}
	var encoded_length: int=_base64_length(bytes.size())
	if bytes.size()>MAX_BYTES or encoded_length>=MAX_BYTES or continuation_bytes.size()>MAX_CONTINUATION_BYTES: return {"ok":false,"code":"SaveSizeLimit"}
	var hashed: Dictionary=_sha256(bytes)
	if not hashed.ok: return hashed
	var header: Dictionary={"magic":MAGIC,"format_version":1,"save_id":slot,"generation":generation,"payload_length":bytes.size(),"payload_sha256":hashed.digest,"payload_base64":""}
	var continuation_encoded_length: int=0
	if not continuation_bytes.is_empty():
		var continuation_hash: Dictionary=_sha256(continuation_bytes)
		if not continuation_hash.ok: return continuation_hash
		header.format_version=2
		header.continuation_length=continuation_bytes.size()
		header.continuation_sha256=continuation_hash.digest
		header.continuation_payload_sha256=hashed.digest
		header.continuation_base64=""
		continuation_encoded_length=_base64_length(continuation_bytes.size())
	var overhead: int=JSON.stringify(header,"",true,true).to_utf8_buffer().size()
	if encoded_length+continuation_encoded_length>MAX_BYTES-overhead: return {"ok":false,"code":"SaveSizeLimit"}
	header.payload_base64=Marshalls.raw_to_base64(bytes)
	if not continuation_bytes.is_empty(): header.continuation_base64=Marshalls.raw_to_base64(continuation_bytes)
	var text: String=JSON.stringify(header,"",true,true)
	if text.to_utf8_buffer().size()>MAX_BYTES: return {"ok":false,"code":"SaveSizeLimit"}
	return {"ok":true,"text":text}

## 摘要规范只允许64位小写十六进制，不让大小写或部分摘要形成第二编码。
static func _valid_digest(value: String) -> bool:
	if value.length()!=64: return false
	for index in range(value.length()):
		var scalar: int=value.unicode_at(index)
		if not (scalar>=48 and scalar<=57 or scalar>=97 and scalar<=102): return false
	return true

## 在调用引擎解码前检查字母表、完整分组和尾填充；不接受空白、URL变体或中间等号。
static func _base64_shape(value: String) -> bool:
	if value.is_empty() or value.length()%4!=0: return false
	var padding: int=2 if value.ends_with("==") else 1 if value.ends_with("=") else 0
	for index in range(value.length()-padding):
		var scalar: int=value.unicode_at(index)
		if not (scalar>=65 and scalar<=90 or scalar>=97 and scalar<=122 or scalar>=48 and scalar<=57 or scalar==43 or scalar==47): return false
	return true

## 错误分类保守封闭：未来/外来格式与资源限制都不是已证明的物理损坏。
static func _failure(code: String) -> Dictionary:
	return {"ok":false,"code":code,"physical_corruption":code in PHYSICAL_ERRORS}

## 严格解析一次并核验原字节；未知版本优先，不套用已知字段或猜测generation。
static func _physical(text: String) -> Dictionary:
	if text.is_empty(): return _failure("InvalidSaveHeader")
	if text.length()>MAX_BYTES or text.to_utf8_buffer().size()>MAX_BYTES: return _failure("SaveSizeLimit")
	var parsed: Dictionary=Strict.parse_text(text,MAX_BYTES)
	if not parsed.ok: return _failure(parsed.code)
	var header: Variant=parsed.value
	if header is Dictionary and typeof(header.get("magic"))==TYPE_STRING:
		if header.magic!=MAGIC: return _failure("UnsupportedSaveFormat")
		if typeof(header.get("format_version"))==TYPE_INT and header.format_version not in [1,2]: return _failure("UnsupportedSaveVersion")
	if not Values.fields(header,FIELDS): return _failure("InvalidSaveHeader")
	var with_continuation: bool=header.format_version==2
	if with_continuation and not Values.fields(header,CONTINUATION_FIELDS): return _failure("InvalidSaveHeader")
	var field_count: int=FIELDS.size()+CONTINUATION_FIELDS.size() if with_continuation else FIELDS.size()
	if header.size()!=field_count: return _failure("InvalidSaveHeader")
	if not Values.whole(header.generation) or not _valid_save_id(header.save_id) or not Values.whole(header.payload_length,1,MAX_BYTES) or not _valid_digest(header.payload_sha256): return _failure("InvalidSaveMetadata")
	var encoded: String=header.payload_base64
	if not _base64_shape(encoded): return _failure("InvalidResidentBase64")
	var payload: PackedByteArray=Marshalls.base64_to_raw(encoded)
	# 重编码还拒绝非零unused pad bits；引擎宽松解码成功不等于规范base64。
	if payload.is_empty() or Marshalls.raw_to_base64(payload)!=encoded: return _failure("InvalidResidentBase64")
	if payload.size()!=header.payload_length: return _failure("ChecksumMismatch")
	var hashed: Dictionary=_sha256(payload)
	if not hashed.ok: return _failure(hashed.code)
	if hashed.digest!=header.payload_sha256: return _failure("ChecksumMismatch")
	var result: Dictionary={"ok":true,"header":header,"payload":payload}
	if with_continuation:
		if header.continuation_length>MAX_CONTINUATION_BYTES: return _failure("SaveSizeLimit")
		if not Values.whole(header.continuation_length,1,MAX_CONTINUATION_BYTES) or not _valid_digest(header.continuation_sha256) or not _valid_digest(header.continuation_payload_sha256): return _failure("InvalidSaveMetadata")
		if header.continuation_payload_sha256!=header.payload_sha256: return _failure("ChecksumMismatch")
		var continuation_encoded: String=header.continuation_base64
		if continuation_encoded.length()>_base64_length(MAX_CONTINUATION_BYTES): return _failure("SaveSizeLimit")
		if not _base64_shape(continuation_encoded): return _failure("InvalidResidentBase64")
		var continuation: PackedByteArray=Marshalls.base64_to_raw(continuation_encoded)
		if continuation.is_empty() or Marshalls.raw_to_base64(continuation)!=continuation_encoded: return _failure("InvalidResidentBase64")
		if continuation.size()>MAX_CONTINUATION_BYTES: return _failure("SaveSizeLimit")
		if continuation.size()!=header.continuation_length: return _failure("ChecksumMismatch")
		var continuation_hash: Dictionary=_sha256(continuation)
		if not continuation_hash.ok: return _failure(continuation_hash.code)
		if continuation_hash.digest!=header.continuation_sha256: return _failure("ChecksumMismatch")
		result.continuation_bytes=continuation
	return result

## 元信息仅供原物理选代形状；没有world身份副本，也不返回payload或语义有效标志。
static func inspect_metadata(text: String) -> Dictionary:
	var inspected: Dictionary=_physical(text)
	if not inspected.ok: return inspected
	var metadata: Dictionary=inspected.header.duplicate(true)
	metadata.erase("payload_base64")
	metadata.erase("continuation_base64")
	return {"ok":true,"save_id":metadata.save_id,"generation":metadata.generation,"metadata":metadata}

## 只解出独立二进制payload；真正的checkpoint目录/领域验证仍在同一worker上完成。
static func decode_payload(text: String) -> Dictionary:
	var inspected: Dictionary=_physical(text)
	if not inspected.ok: return inspected
	var result: Dictionary={"ok":true,"payload":inspected.payload}
	if inspected.has("continuation_bytes"): result.continuation_bytes=inspected.continuation_bytes
	return result
