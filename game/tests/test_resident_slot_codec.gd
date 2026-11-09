## 仅检验常驻检查点的物理文本封装；不创建世界、不写银行、不把校验和当语义成功。
extends SceneTree
const Codec=preload("res://storage/resident_slot_codec.gd")
var checks: int=0
var failures: int=0

## 延迟至真实引擎初始化完成，失败必须返回非零状态。
func _init() -> void:
	call_deferred("_run")

## 断言记录实际值，不以提前返回或脚本未运行冒充通过。
func check(condition: bool, label: String) -> void:
	checks+=1
	if not condition:
		failures+=1
		printerr("FAIL: ",label)

## 单字段畸变只用于边界负例，不重算摘要掩盖应拒绝的输入。
func changed(text: String, key: String, value: Variant) -> String:
	var header: Dictionary=JSON.parse_string(text)
	header[key]=value
	return JSON.stringify(header,"",true,true)

## 两个只读入口必须给出一致错误分类，失败不携带部分payload或元信息。
func rejected(text: String, code: String, corrupt: bool) -> void:
	for result in [Codec.inspect_metadata(text),Codec.decode_payload(text)]:
		check(result=={"ok":false,"code":code,"physical_corruption":corrupt},"精确错误 "+code+"："+str(result))

## 小字节向量验证规范base64；大小门只分配原始零字节，不创建超限编码副本。
func _run() -> void:
	var binary=PackedByteArray()
	for n in range(256): binary.append(n)
	var encoded: Dictionary=Codec.encode(binary,1,"manual-1")
	check(encoded.get("ok",false) and encoded.has("text"),"任意二进制完整编码")
	if not encoded.get("ok",false):
		print("RESIDENT_SLOT_CODEC_CHECKS=",checks," FAILURES=",failures); quit(1); return
	var text: String=encoded.text
	var header: Dictionary=JSON.parse_string(text)
	check(header.size()==7 and header.magic=="HM-RESIDENT-SAVE" and header.format_version==1,"包装只有冻结七字段")
	check(header.payload_length==256 and header.payload_base64==Marshalls.raw_to_base64(binary),"base64长度与原二进制一致")
	var decoded: Dictionary=Codec.decode_payload(text)
	check(decoded.get("ok",false) and decoded.get("payload")==binary,"全部0至255字节无损往返")
	check(decoded.size()==2 and typeof(decoded.payload)==TYPE_PACKED_BYTE_ARRAY,"返回payload是独立PackedByteArray")
	decoded.payload[0]=99
	check(Codec.decode_payload(text).payload==binary and binary[0]==0,"改变返回缓冲不改变源或再次解码")
	var inspected: Dictionary=Codec.inspect_metadata(text)
	check(inspected.get("ok",false) and inspected.save_id=="manual-1" and inspected.generation==1,"物理列表所需slot/generation形状")
	check(inspected.metadata.size()==6 and not inspected.metadata.has("payload_base64") and not inspected.has("payload") and not inspected.has("state"),"inspect不泄漏正文或伪造世界")
	inspected.metadata.generation=99
	check(Codec.inspect_metadata(text).metadata.generation==1,"元信息每次独立拥有")
	for sample in [["f","Zg=="],["fo","Zm8="],["foo","Zm9v"],["foob","Zm9vYg=="]]:
		var row: Dictionary=Codec.encode(sample[0].to_utf8_buffer(),2,"manual-2")
		check(row.ok and JSON.parse_string(row.text).payload_base64==sample[1],"规范填充向量："+sample[1])
		check(Codec.decode_payload(row.text).payload==sample[0].to_utf8_buffer(),"填充向量原字节："+sample[1])
	var one: String=Codec.encode("f".to_utf8_buffer(),1,"manual-1").text
	for invalid in ["Zh==","Zg","Zg=","Zg===","Zg==\n","Zg =","Zg-_","Zg==AAAA","Ｚg=="]:
		rejected(changed(one,"payload_base64",invalid),"InvalidResidentBase64",true)
	rejected(changed(one,"payload_sha256","0".repeat(64)),"ChecksumMismatch",true)
	rejected(changed(one,"payload_sha256","A".repeat(64)),"InvalidSaveMetadata",true)
	rejected(changed(one,"payload_length",2),"ChecksumMismatch",true)
	for sample in [[-1,"InvalidSaveMetadata"],[0,"InvalidSaveMetadata"],[2147483648,"InvalidSaveHeader"],[1.5,"InvalidSaveHeader"],["1","InvalidSaveHeader"]]:
		rejected(changed(one,"payload_length",sample[0]),sample[1],true)
	for invalid in [-1,2147483648]: rejected(changed(one,"generation",invalid),"InvalidSaveMetadata" if invalid==-1 else "InvalidSaveHeader",true)
	rejected(changed(one,"generation",1.5),"InvalidSaveHeader",true)
	rejected(changed(one,"save_id",""),"InvalidSaveMetadata",true)
	rejected(changed(one,"save_id","汉".repeat(129)),"InvalidSaveMetadata",true)
	var missing: Dictionary=JSON.parse_string(one); missing.erase("payload_length")
	rejected(JSON.stringify(missing),"InvalidSaveHeader",true)
	rejected(changed(one,"world_id","untrusted duplicate"),"InvalidSaveHeader",true)
	rejected(one.replace('"generation":1','"generation":1,"generation":1'),"DuplicateKey",true)
	rejected("{", "ExpectedString",true)
	rejected("", "InvalidSaveHeader",true)
	rejected(changed(one,"magic","HM-SAVE"),"UnsupportedSaveFormat",false)
	rejected(changed(one,"magic","FUTURE-SAVE"),"UnsupportedSaveFormat",false)
	var future: Dictionary={"magic":"HM-RESIDENT-SAVE","format_version":99,"generation":"future opaque"}
	rejected(JSON.stringify(future),"UnsupportedSaveVersion",false)
	check(Codec.encode(binary,0,"manual-1").ok and Codec.encode(binary,2147483647,"manual-1").ok,"generation沿原whole上下界")
	check(not Codec.encode(binary,-1,"manual-1").ok and not Codec.encode(binary,2147483648,"manual-1").ok,"generation不溢出或窄化")
	check(not Codec.encode(PackedByteArray(),1,"manual-1").ok,"空checkpoint字节明确拒绝")
	check(not Codec.encode(binary,1,"").ok and not Codec.encode(binary,1,"汉".repeat(129)).ok,"空或超长slot身份拒绝")
	var unicode_slot: Dictionary=Codec.encode(binary,1,"汉".repeat(128))
	check(unicode_slot.ok and Codec.inspect_metadata(unicode_slot.text).ok,"slot按原码点上界检查；路径许可由NativeStorage负责")
	var large=PackedByteArray(); large.resize(Codec.MAX_BYTES)
	check(Codec.encode(large,1,"manual-1")=={"ok":false,"code":"SaveSizeLimit"},"base64扩张在编码分配前明确拒绝")
	large=PackedByteArray()
	var opaque: Dictionary=Codec.encode("not a semantic checkpoint".to_utf8_buffer(),1,"manual-1")
	check(opaque.ok and Codec.inspect_metadata(opaque.text).ok and Codec.decode_payload(opaque.text).ok,"物理成功不冒充checkpoint语义成功")
	check(Codec.encode(binary,1,"manual-1").text==text,"相同输入确定性输出且不修改源")
	print("RESIDENT_SLOT_CODEC_CHECKS=",checks," FAILURES=",failures," SCOPE=physical codec only")
	quit(0 if failures==0 else 1)
