## 真实历史v1字节上的Codec物理/语义边界探针；未接inspect_slot，不能宣称M3完整恢复通过。
extends SceneTree
const Codec=preload("res://core/save_codec.gd")
const Strict=preload("res://core/strict_json.gd")
const ROOT="res://../tests/fixtures/saves/a2/"
var checks: int=0
var failed: int=0

## 在引擎初始化后读保留原字节的fixture，不创建世界或实际玩家槽位。
func _init() -> void:
	call_deferred("_run")

## 独立断言与失败计数；探针成功不隐藏尚未接线的M3门。
func check(value: bool, label: String) -> void:
	checks+=1
	if not value:
		failed+=1
		printerr("FAIL: ",label)

## 测试才构造明确合成的负例；从不覆盖、归一化或重新保存历史输入。
func mutated(text: String, mutation: String) -> String:
	if mutation=="none": return text
	if mutation=="truncated": return text.substr(0,text.length()-10)
	if mutation=="duplicate": return '{"magic":"HM-SAVE","magic":"HM-SAVE"}'
	var header: Dictionary=Strict.parse_text(text,Codec.MAX_BYTES).value
	match mutation:
		"checksum": header.payload_sha256="0".repeat(64)
		"length": header.payload_length+=1
		"header_type": header.generation="not-an-integer"
		"generation": header.generation=-1
		"future_format": header.format_version=999
		"future_rules": header.ruleset_id="unknown-future-rules"
		"identity": header.content_digest="0".repeat(64)
		"engine":
			var body: Dictionary=Strict.parse_text(header.payload_json).value
			body.engine_build="unknown-future-engine"; body.rng_backend="godot:unknown-future-engine"
			header.engine_build=body.engine_build; header.rng_backend=body.rng_backend
			header.payload_json=JSON.stringify(body,"",true,true)
			header.payload_length=header.payload_json.to_utf8_buffer().size()
			header.payload_sha256=header.payload_json.sha256_text()
		"opaque":
			header.payload_json="opaque future body, deliberately not JSON"
			header.payload_length=header.payload_json.to_utf8_buffer().size()
			header.payload_sha256=header.payload_json.sha256_text()
		_:
			check(false,"未知探针变体不能静默使用原输入："+mutation)
	return JSON.stringify(header,"",true,true)

## 封装检查必须接受不兼容规则/引擎及opaque正文，只有明确物理损坏可授权后续选代回退。
func check_metadata(text: String, cases: Dictionary) -> void:
	for entry in cases.cases:
		var inspected: Dictionary=Codec.inspect_metadata(mutated(text,entry.mutation))
		if entry.future_inspect_should_accept_physical_envelope:
			check(inspected.get("ok",false),"inspect不解正文或解释兼容性："+entry.name+" "+str(inspected))
			if inspected.get("ok",false):
				check(inspected.has("generation") and inspected.has("save_id") and inspected.get("metadata") is Dictionary and not inspected.metadata.has("payload_json") and not inspected.has("state"),"inspect只返回元信息和选代身份："+entry.name)
		else:
			check(not inspected.get("ok",false) and inspected.get("code")==entry.decode_code,"inspect明确物理/未知版本错误："+entry.name+" "+str(inspected))
			check(inspected.get("physical_corruption")== (entry.classification=="physical_corruption"),"只有物理白名单允许降代："+entry.name)
	var future: String=FileAccess.get_file_as_string(ROOT+"synthetic-unknown-future-envelope.hmsave")
	for result in [Codec.inspect_metadata(future),Codec.decode(future,{"ok":false,"code":"MissingContent"})]:
		check(not result.get("ok",false) and result.get("code")=="UnsupportedSaveVersion" and not result.get("physical_corruption",false),"未知future字段不能先套v1结构或猜generation")
	var limited: Dictionary=Codec.inspect_metadata("[".repeat(34)+"0"+"]".repeat(34))
	check(limited.get("code")=="TooDeep" and limited.get("physical_corruption")==false,"解析资源限制不是已证明的字节损坏")
	for depth in [33,34,64]:
		for sample in ["[".repeat(depth)+"0"+"]".repeat(depth),'{"层":'.repeat(depth)+"0"+"}".repeat(depth)]:
			var too_deep: Dictionary=Codec.inspect_metadata(sample)
			check(too_deep.get("code")=="TooDeep" and too_deep.get("physical_corruption")==false,"数组/对象递归必须保留首次深度限制："+str(depth))
	check(Strict.parse_text("[".repeat(32)+"0"+"]".repeat(32)).ok,"原32层合法深度边界不被缩紧")
	for sample in ["[1e+]","[1e309x]","[01]"]:
		check(Strict.parse_text(sample).get("code")=="ExpectedComma","非资源错误保留原ExpectedComma优先级："+sample)
	var priority: Dictionary=Strict.parse_text(text,Codec.MAX_BYTES).value
	priority.ruleset_id="unknown-future-rules"; priority.payload_sha256="0".repeat(64)
	check(Codec.decode(JSON.stringify(priority),{"ok":false,"code":"MissingContent"})=={"ok":false,"code":"UnsupportedSaveVersion"},"旧decode仍先拒绝未知规则且不附选代flag")
	var physical_first: Dictionary=Codec.inspect_metadata(JSON.stringify(priority))
	check(physical_first.get("code")=="ChecksumMismatch" and physical_first.get("physical_corruption")==true,"inspect不解释规则，所以仍核实际坏checksum")
	priority.format_version=999; priority.generation="opaque future generation"
	var unknown_first: Dictionary=Codec.inspect_metadata(JSON.stringify(priority))
	check(unknown_first.get("code")=="UnsupportedSaveVersion" and unknown_first.get("physical_corruption")==false,"未知格式优先于旧字段类型和旧checksum算法")
	var numeric_limit: Dictionary=Codec.inspect_metadata("1e309")
	check(numeric_limit.get("code")=="NonFiniteNumber" and numeric_limit.get("physical_corruption")==false,"数值表示限制不授权回退另一代")
	var empty: Dictionary=Codec.inspect_metadata("")
	check(empty.get("code")=="InvalidSaveHeader" and empty.get("physical_corruption")==true,"零字节封装是明确损坏")
	var wrong_magic: Dictionary=Strict.parse_text(text,Codec.MAX_BYTES).value
	wrong_magic.magic="OTHER-FORMAT"
	check(Codec.inspect_metadata(JSON.stringify(wrong_magic)).get("code")=="InvalidSaveHeader","未知magic不当作合法HM-SAVE目录")
	check(Codec.decode(JSON.stringify(wrong_magic),{"ok":false,"code":"MissingContent"}).get("code")=="UnsupportedSaveVersion","旧v1 decode的错误优先级保持原行为")

## 明确合成v2封装只测试元信息合同；它不是历史v1改版本，也不证明v2解码或恢复已接通。
func check_v2_metadata() -> void:
	var fixture: Dictionary=Strict.parse_text(FileAccess.get_file_as_string(ROOT+"resolver-contract.json")).value.catalog
	var payload: String="未来正文，故意不是 JSON"
	var header: Dictionary={"magic":"HM-SAVE","format_version":2,"ruleset_id":"unknown-future-rules","content_version":fixture.content_version,"content_id":fixture.content_id,"content_digest":fixture.content_digest,"engine_build":"unknown-future-engine","rng_backend":"unknown-future-rng","save_id":"manual-1","generation":9,"payload_length":payload.to_utf8_buffer().size(),"payload_sha256":payload.sha256_text(),"payload_json":payload,"scenario_id":"scenario::start","player_actor_id":"people::player","pack_set":fixture.pack_set.duplicate(true),"pack_set_digest":fixture.pack_set_digest,"rule_modules":[{"id":"future.module","version":"999.0.0"}]}
	var encoded: String=JSON.stringify(header,"",true,true)
	var inspected: Dictionary=Codec.inspect_metadata(encoded)
	check(inspected.get("ok",false) and inspected.get("generation")==9 and inspected.get("save_id")=="manual-1","v2封装正确时无需当前规则/引擎或正文解析")
	if inspected.get("ok",false):
		var expected: Dictionary=header.duplicate(true); expected.erase("payload_json")
		check(inspected.metadata==expected,"v2返回全部封装身份，唯一排除opaque正文")
		inspected.metadata.pack_set.packs.clear()
		check(Codec.inspect_metadata(encoded).get("metadata",{}).get("pack_set")==header.pack_set,"返回元信息深复制，重复inspect不漂移")
	check(Codec.decode(encoded,{"ok":false,"code":"MissingContent"}).get("code")=="UnsupportedSaveVersion","v2正文decode仍明确拒绝未知规则版本")
	for field in ["scenario_id","player_actor_id","pack_set","pack_set_digest","rule_modules"]:
		var missing: Dictionary=header.duplicate(true); missing.erase(field)
		var result: Dictionary=Codec.inspect_metadata(JSON.stringify(missing))
		check(result.get("code")=="InvalidSaveHeader" and result.get("physical_corruption")==true,"v2封装必须给齐新增字段类型："+field)
	var semantic: Dictionary=header.duplicate(true)
	semantic.pack_set={"format":"HM-PACK-SET","format_version":999}
	semantic.rule_modules=[{"unknown":"opaque future descriptor"}]
	check(Codec.inspect_metadata(JSON.stringify(semantic)).get("ok",false),"锁和模块内部语义交resolver，inspect只核封装容器类型")
	var unicode_wrong: Dictionary=header.duplicate(true); unicode_wrong.payload_length=payload.length()
	check(Codec.inspect_metadata(JSON.stringify(unicode_wrong)).get("code")=="ChecksumMismatch","payload_length严格用UTF-8原字节而非字符数")

## 正式Codec必须在对应真实边界拒绝；没有第二Codec、默认世界或假造恢复成功。
func _run() -> void:
	var provenance: Dictionary=Strict.parse_text(FileAccess.get_file_as_string(ROOT+"v1-provenance.json")).get("value",{})
	var cases: Dictionary=Strict.parse_text(FileAccess.get_file_as_string(ROOT+"codec-probe-cases.json")).get("value",{})
	check(not provenance.is_empty() and not cases.is_empty(),"来源及探针分类manifest可读")
	if provenance.is_empty() or cases.is_empty():
		quit(1); return
	var path: String=ROOT+provenance.fixture_file
	var bytes: PackedByteArray=FileAccess.get_file_as_bytes(path)
	var text: String=bytes.get_string_from_utf8()
	check(bytes.size()==provenance.fixture_bytes and text.sha256_text()==provenance.fixture_sha256,"真实v1字节与来源manifest SHA及长度完全一致")
	check(provenance.fixture_sha256==provenance.source_manifest_entry.sha256 and provenance.fixture_sha256==provenance.source_trace_checkpoint.save_sha256,"原manifest和原trace记录指向同一真实持久化字节")
	check(provenance.source_commit=="4f16935ca7e838480839143e2a92f9e0992c0683" and provenance.original_receipt_status=="INCOMPLETE" and not provenance.original_receipt_ok and provenance.historical_saves.status=="GAP","保留来源与F0.5历史原档缺口，不升级原始验收状态")
	var original: Dictionary=Strict.parse_text(text,Codec.MAX_BYTES).value
	check(original.format_version==1 and original.ruleset_id=="f09-military-1","历史封装版本和ruleset原字节没有重写")
	var body: Dictionary=Strict.parse_text(original.payload_json).value
	check(body.family.children.size()==1 and body.tick==2,"保留真实完成收养后的非空家庭代表状态")
	var before: String=text.sha256_text()
	for entry in cases.cases:
		var sample: String=mutated(text,entry.mutation)
		# 故意传入不可用目录，让真实Codec在目录边界停止；这不是decode/restore成功。
		var result: Dictionary=Codec.decode(sample,{"ok":false,"code":"MissingContent"})
		check(not result.get("ok",false) and result.get("code")==entry.decode_code,"真实Codec边界："+entry.name+" -> "+str(result))
		if entry.classification=="semantic_stop":
			check(not result.get("code") in ["ChecksumMismatch","InvalidSaveHeader","CorruptSlot"],"语义/未来版本错误不能伪装成物理损坏："+entry.name)
	check_metadata(text,cases)
	check_v2_metadata()
	check(FileAccess.get_file_as_string(path).sha256_text()==before and text.sha256_text()==before,"所有负例只在内存生成，原存档字节保持不变")
	print("A2_CODEC_BOUNDARY_CHECKS=",checks," FAILURES=",failed)
	print("SAVE_METADATA_INSPECT=EXERCISED; SLOT_SELECTION=NOT_RUN; M3_FULL_RESTORE=NOT_RUN")
	quit(0 if failed==0 else 1)
