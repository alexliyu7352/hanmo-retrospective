## 格式2物理边界的独立安全门；只用固定字节，不加载世界、Bridge或原生库。
extends SceneTree
const Codec=preload("res://storage/resident_slot_codec.gd")
const LEGACY: String='{"format_version":1,"generation":7,"magic":"HM-RESIDENT-SAVE","payload_base64":"AQID","payload_length":3,"payload_sha256":"039058c6f2c0cb492c533b0a4d14ef77cc0f78abccced5287d84a1a2011cfb81","save_id":"slot:test"}'
var checks: int=0
var failures: int=0

## 所有失败仍继续收集具体断言，最终用非零退出码报告。
func _check(condition: bool, label: String) -> void:
	checks+=1
	if not condition:
		failures+=1
		push_error("RESIDENT_SLOT_CODEC: "+label)

## 用规范JSON送入真正物理解码；不在测试中代替codec校验。
func _decode(header: Dictionary) -> Dictionary:
	return Codec.decode_payload(JSON.stringify(header,"",true,true))

## 检查实际错误码及不允许回退的未支持格式分类。
func _reject(header: Dictionary, code: String, label: String) -> void:
	var result: Dictionary=_decode(header)
	_check(not result.ok and result.code==code,label+": "+str(result))

## 原七字段字节是固定历史样本，新增默认参数不能改变任何一个字节。
func _legacy() -> void:
	var bytes: PackedByteArray=PackedByteArray([1,2,3])
	var encoded: Dictionary=Codec.encode(bytes,7,"slot:test")
	_check(encoded.ok and encoded.text==LEGACY,"format1 exact historical bytes")
	var explicit_empty: Dictionary=Codec.encode(bytes,7,"slot:test",PackedByteArray())
	_check(explicit_empty==encoded,"empty context preserves format1 exactly")
	var decoded: Dictionary=Codec.decode_payload(LEGACY)
	_check(decoded=={"ok":true,"payload":bytes},"format1 original decode shape")
	var inspected: Dictionary=Codec.inspect_metadata(LEGACY)
	_check(inspected.ok and inspected.generation==7 and inspected.metadata.size()==6 and inspected.metadata.format_version==1 and not inspected.metadata.has("payload_base64") and not inspected.metadata.has("continuation_base64"),"format1 original metadata shape")
	var extended: Dictionary=JSON.parse_string(LEGACY)
	extended.continuation_length=1
	_reject(extended,"InvalidSaveHeader","format1 refuses extra context field")

## 新封装只验证字节和绑定，尚未交给native解释的任意非空许可字节仍可往返。
func _round_trip() -> Dictionary:
	var world: PackedByteArray=PackedByteArray([1,2,3])
	var continuation: PackedByteArray=PackedByteArray([72,77,67,78,1,254,0])
	var encoded: Dictionary=Codec.encode(world,8,"slot:test",continuation)
	_check(encoded.ok,"format2 encodes")
	if not encoded.ok: return {}
	var decoded: Dictionary=Codec.decode_payload(encoded.text)
	_check(decoded=={"ok":true,"payload":world,"continuation_bytes":continuation},"format2 independent byte round trip")
	var header: Dictionary=JSON.parse_string(encoded.text)
	_check(header.size()==11 and header.format_version==2 and header.continuation_length==7 and header.continuation_payload_sha256==header.payload_sha256,"format2 exact fields and world binding")
	var inspected: Dictionary=Codec.inspect_metadata(encoded.text)
	_check(inspected.ok and inspected.metadata.size()==9 and not inspected.metadata.has("payload_base64") and not inspected.metadata.has("continuation_base64"),"metadata omits both encoded bodies")
	_check(inspected.metadata.continuation_length==7 and inspected.metadata.continuation_sha256==header.continuation_sha256 and inspected.metadata.continuation_payload_sha256==header.payload_sha256,"metadata retains independent digest and binding")
	return header

## 分别破坏世界和上下文，禁止只检查其中之一后采纳另一份字节。
func _tampering(header: Dictionary) -> void:
	var bad: Dictionary=header.duplicate(true)
	bad.continuation_base64=Marshalls.raw_to_base64(PackedByteArray([72,77,67,78,1,253,0]))
	_reject(bad,"ChecksumMismatch","tampered context bytes")
	bad=header.duplicate(true)
	bad.continuation_sha256="0".repeat(64)
	_reject(bad,"ChecksumMismatch","tampered context checksum")
	bad=header.duplicate(true)
	bad.continuation_payload_sha256="0".repeat(64)
	_reject(bad,"ChecksumMismatch","foreign world binding")
	bad=header.duplicate(true)
	bad.payload_base64="AQIE"
	_reject(bad,"ChecksumMismatch","tampered world bytes")
	bad=header.duplicate(true)
	bad.payload_sha256="0".repeat(64)
	_reject(bad,"ChecksumMismatch","tampered world checksum")
	bad=header.duplicate(true)
	bad.continuation_length=6
	_reject(bad,"ChecksumMismatch","context length mismatch")
	bad=header.duplicate(true)
	bad.continuation_sha256=bad.continuation_sha256.to_upper()
	_reject(bad,"InvalidSaveMetadata","noncanonical context digest")
	for malformed in ["", "SE1D", "SE1D\n", "SE1D----", "AA=A", "AR=="]:
		bad=header.duplicate(true)
		bad.continuation_base64=malformed
		if malformed=="SE1D":
			_reject(bad,"ChecksumMismatch","short canonical context")
		else:
			_reject(bad,"InvalidResidentBase64","noncanonical context base64 "+malformed)

## 严格版本、类型和字段集合阻止猜测读取；未知版本仍优先于已知字段缺失。
func _shape(header: Dictionary) -> void:
	var bad: Dictionary=header.duplicate(true)
	bad.format_version=3
	bad.erase("generation")
	var future: Dictionary=_decode(bad)
	_check(not future.ok and future.code=="UnsupportedSaveVersion" and not future.physical_corruption,"unknown version refuses fallback before field checks")
	bad.magic="OTHER-SAVE"
	var foreign: Dictionary=_decode(bad)
	_check(not foreign.ok and foreign.code=="UnsupportedSaveFormat" and not foreign.physical_corruption,"unknown magic retains first error")
	bad=header.duplicate(true)
	bad.extra=true
	_reject(bad,"InvalidSaveHeader","unknown format2 field")
	bad=header.duplicate(true)
	bad.erase("continuation_sha256")
	_reject(bad,"InvalidSaveHeader","missing context digest")
	for key in ["continuation_length","continuation_sha256","continuation_payload_sha256","continuation_base64"]:
		bad=header.duplicate(true)
		bad[key]=false
		_reject(bad,"InvalidSaveHeader","wrong context field type "+key)
	bad=header.duplicate(true)
	bad.continuation_length=0
	_reject(bad,"InvalidSaveMetadata","zero length format2 refused")
	bad=header.duplicate(true)
	bad.continuation_length=-1
	_reject(bad,"InvalidSaveMetadata","negative context length refused")
	var text: String=JSON.stringify(header,"",true,true)
	var trailing: Dictionary=Codec.decode_payload(text+" []")
	_check(not trailing.ok and trailing.code=="TrailingData","trailing JSON refused")
	var duplicate: Dictionary=Codec.decode_payload(text.substr(0,text.length()-1)+',"continuation_length":7}')
	_check(not duplicate.ok and duplicate.code=="DuplicateKey","duplicate context key refused")
	var truncated: Dictionary=Codec.decode_payload(text.substr(0,text.length()-1))
	_check(not truncated.ok,"truncated input refused")

## 限长在可能分配大副本前检查；最大合法上下文仍能独立往返。
func _bounds(header: Dictionary) -> void:
	var exact: PackedByteArray=PackedByteArray()
	exact.resize(2048)
	var encoded: Dictionary=Codec.encode(PackedByteArray([1,2,3]),1,"slot:test",exact)
	_check(encoded.ok and Codec.decode_payload(encoded.text).continuation_bytes==exact,"2048 byte context accepted")
	var oversized: PackedByteArray=exact.duplicate()
	oversized.append(1)
	var denied: Dictionary=Codec.encode(PackedByteArray([1,2,3]),1,"slot:test",oversized)
	_check(not denied.ok and denied.code=="SaveSizeLimit","2049 byte context encode refused")
	var bad: Dictionary=header.duplicate(true)
	bad.continuation_length=2049
	_reject(bad,"SaveSizeLimit","oversized declared context refused")
	bad=header.duplicate(true)
	bad.continuation_base64="A".repeat(2736)
	_reject(bad,"SaveSizeLimit","oversized encoded context refused")
	bad=header.duplicate(true)
	bad.continuation_length=2048
	bad.continuation_base64=Marshalls.raw_to_base64(oversized)
	_reject(bad,"SaveSizeLimit","padding-equivalent oversized raw context refused")
	var outer: Dictionary=Codec.decode_payload(" ".repeat(Codec.MAX_BYTES+1))
	_check(not outer.ok and outer.code=="SaveSizeLimit" and not outer.physical_corruption,"original outer text bound retained")

## 只运行本物理边界门，整个项目和世界不进入此独立测试。
func _initialize() -> void:
	_legacy()
	var header: Dictionary=_round_trip()
	if not header.is_empty():
		_tampering(header)
		_shape(header)
		_bounds(header)
	print("RESIDENT_SLOT_CODEC_CHECKS=",checks," FAILURES=",failures)
	quit(1 if failures else 0)
