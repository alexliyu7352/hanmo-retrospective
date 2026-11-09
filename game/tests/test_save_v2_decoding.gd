## 真实v1来源派生的v2正文恢复门；边界侧锁不冒充生产安装或Kernel原子恢复。
extends SceneTree
const Codec=preload("res://core/save_codec.gd")
const Catalog=preload("res://core/content_catalog.gd")
const Strict=preload("res://core/strict_json.gd")
const Registry=preload("res://core/runtime/module_registry.gd")
const Builtin=preload("res://core/runtime/builtin_modules.gd")
const ROOT="res://../tests/fixtures/saves/a2/"
var checks: int=0
var failed: int=0
var validation_calls: int=0

## 推迟到引擎准备后读取已捕获世界，不创建新世界。
func _init() -> void:
	call_deferred("_run")

## 所有失败计数并以非零状态退出。
func check(value: bool, label: String) -> void:
	checks+=1
	if not value:
		failed+=1
		printerr("FAIL: ",label)

## 内存负例重新核payload长度与摘要，不改原档或把派生封装称为历史捕获。
func with_payload(header: Dictionary, payload: String) -> String:
	var changed: Dictionary=header.duplicate(true)
	changed.payload_json=payload; changed.payload_length=payload.to_utf8_buffer().size(); changed.payload_sha256=payload.sha256_text()
	return JSON.stringify(changed,"",true,true)

## 正文变体只供边界测试；默认沿给定头部以暴露镜像错配。
func with_state(header: Dictionary, state: Dictionary) -> String:
	return with_payload(header,JSON.stringify(state,"",true,true))

## 语义拒绝不返回半成品state或物理回退许可。
func expect_error(text: String, catalog: Dictionary, registry: Variant, code: String, label: String) -> void:
	var result: Dictionary=Codec.decode(text,catalog,registry)
	check(not result.get("ok",false) and result.get("code")==code and not result.has("state") and not result.get("physical_corruption",false),label+" code="+str(result.get("code")))

## 显式受信校验回调的拒绝证明decode未替换成另一注册组合。
func reject_injected(_state: Dictionary, _data: Dictionary) -> Dictionary:
	validation_calls+=1
	return {"ok":false,"code":"InjectedDecodeRejected"}

## 先读真正的v1捕获，再调用当前writer生成v2，不能改历史版本冒充原档。
func _run() -> void:
	var fixture: Dictionary=Strict.parse_text(FileAccess.get_file_as_string(ROOT+"v2-boundary-sidecar.json")).value
	var catalog: Dictionary=Catalog.load_checked(FileAccess.get_file_as_string(ROOT+fixture.catalog_file))
	var raw: String=FileAccess.get_file_as_string(ROOT+"v1-4f16935-family-after-adoption.hmsave")
	var legacy: Dictionary=Codec.decode(raw,catalog)
	check(legacy.get("ok",false),"真实v1捕获可走原两参decode")
	if not legacy.get("ok",false):
		quit(1); return
	catalog.pack_set=fixture.pack_set; catalog.pack_set_digest=fixture.pack_set_digest
	var registry: Dictionary=Registry.build(Builtin.descriptors())
	var text: String=Codec.encode(legacy.state,4,"manual-1",catalog,registry)
	check(not text.is_empty(),"实际writer生成带锁v2封装")
	var result: Dictionary=Codec.decode(text,catalog)
	check(result.get("code")=="IncompatibleRuleModules","已知v2格式进入正文恢复入口，缺registry独立报错")
	result=Codec.decode(text,catalog,registry)
	check(result.get("ok",false) and result.get("state")==legacy.state,"真实v2写入后完整decode与原合法状态严格一致")
	var selection: Dictionary=Codec.inspect_restore_selection(text)
	check(selection=={"ok":true,"scenario_id":legacy.state.scenario_id,"founder_actor_id":legacy.state.family.founder_actor_id},"selection只返回未获语义信任的scenario/founder，不公开state")
	var state: Dictionary=legacy.state
	var before: Dictionary=state.duplicate(true); var catalog_before: Dictionary=catalog.duplicate(true); var registry_before: Dictionary=registry.duplicate(true)
	var envelope: Dictionary=Strict.parse_text(text,Codec.MAX_BYTES).value
	check(result.get("generation")==4 and result.get("save_id")=="manual-1" and result.state.rng==state.rng and result.state.next_instance_id==state.next_instance_id and result.state.family==state.family,"完整v2恢复保留generation/save_id/RNG/实例计数/家庭事实")
	check(Codec.encode(result.state,4,"manual-1",catalog,registry)==text,"v2往返规范字节完全一致")
	# 真实v1正文只在内存包入新v2封装，验证现有迁移；不是历史v2原件。
	var historical: Dictionary=Strict.parse_text(raw,Codec.MAX_BYTES).value
	var migration_header: Dictionary=envelope.duplicate(true); migration_header.ruleset_id=historical.ruleset_id
	var migrated: Dictionary=Codec.decode(with_payload(migration_header,historical.payload_json),catalog,registry)
	check(migrated.get("ok",false) and migrated.get("state")==state,"v2使用现有Rules迁移真实f09正文，完整状态与原v1迁移结果相等")
	check(raw.sha256_text()=="153c7b14d6024ad7759ee8e7610b5b5d0d40568f9bfcffd83b4413d6c9325690","原捕获SHA保持已核真实来源")

	result.state.rng.clear()
	check(state==before and Codec.decode(text,catalog,registry).get("state")==before,"返回候选可变引用不共享源状态，重复恢复不消费RNG")
	for invalid_registry in [null,{},[],"bad",{"ok":true}]:
		expect_error(text,catalog,invalid_registry,"IncompatibleRuleModules","v2缺/坏registry不能内部重建")
	expect_error(text,{},registry,"InvalidInstalledCatalog","v2必须显式给已选择目录，不能读取默认目录")
	var wrong: Dictionary=catalog.duplicate(true); wrong.erase("pack_set_digest")
	expect_error(text,wrong,registry,"InvalidInstalledCatalog","有效正文不能替代目录精确锁")
	for field in ["scenario_id","player_actor_id","content_id","content_version","content_digest","engine_build","rng_backend","ruleset_id"]:
		var bad_body: Dictionary=state.duplicate(true); bad_body[field]="body-mismatch"
		var mismatch: String=with_state(envelope,bad_body)
		expect_error(mismatch,catalog,registry,"HeaderPayloadMismatch","头体镜像必须相等："+field)
		check(Codec.inspect_restore_selection(mismatch).get("code")=="HeaderPayloadMismatch","selection也拒绝头体错配："+field)
	for field in ["scenario_id","player_actor_id"]:
		var bad_header: Dictionary=envelope.duplicate(true); bad_header[field]="header-mismatch"
		expect_error(JSON.stringify(bad_header),catalog,registry,"HeaderPayloadMismatch","头部单独篡改身份拒绝："+field)
	var changed: Dictionary=envelope.duplicate(true); changed.pack_set_digest="0".repeat(64)
	expect_error(JSON.stringify(changed),catalog,registry,"PackDigestMismatch","自报lockdigest不能替代canonical摘要")
	changed=envelope.duplicate(true); changed.pack_set.packs[0].content_sha256="0".repeat(64)
	changed.pack_set_digest=(JSON.stringify(changed.pack_set,"",true,true)+"\n").sha256_text()
	expect_error(JSON.stringify(changed),catalog,registry,"PackDigestMismatch","不同精确锁即使自摘要一致也不能使用已选目录")
	changed=envelope.duplicate(true); changed.pack_set.format_version=999
	expect_error(JSON.stringify(changed),catalog,registry,"UnsupportedSaveVersion","未知锁格式不可猜当前版本")
	changed=envelope.duplicate(true); changed.rule_modules[0].version="future-module"
	expect_error(JSON.stringify(changed),catalog,registry,"IncompatibleRuleModules","精确模块列表不匹配拒绝")
	for field in ["scenario_id","player_actor_id","pack_set","pack_set_digest","rule_modules"]:
		changed=envelope.duplicate(true); changed.erase(field)
		expect_error(JSON.stringify(changed),catalog,registry,"InvalidSaveHeader","v2封装必要字段不能省略："+field)
	for payload in ["opaque future payload","{\"duplicate\":1,\"duplicate\":2}","[1,2]", "[".repeat(70)+"0"+"]".repeat(70),JSON.stringify({"too_long":"x".repeat(16385)})]:
		var malformed: String=with_payload(envelope,payload)
		check(Codec.inspect_metadata(malformed).get("ok",false),"opaque/非法正文的物理长度和checksum仍合法")
		expect_error(malformed,catalog,registry,"InvalidSavePayload","正文严格JSON/深度/字符串限制失败不授权物理回退")
		check(Codec.inspect_restore_selection(malformed).get("code")=="InvalidSavePayload","selection使用相同Strict正文限制")
	for field in ["pack_set","pack_set_digest","rule_modules"]:
		var duplicate: Dictionary=state.duplicate(true); duplicate[field]=envelope[field]
		expect_error(with_state(envelope,duplicate),catalog,registry,"InvalidSavePayload","正文并行身份副本拒绝："+field)
	for family in [null,[],"bad"]:
		var bad_body: Dictionary=state.duplicate(true); bad_body.family=family
		var malformed: String=with_state(envelope,bad_body)
		expect_error(malformed,catalog,registry,"InvalidFamilyState","v2缺坏family不能用迁移补造")
		check(Codec.inspect_restore_selection(malformed).get("code")=="InvalidFamilyState","selection拒绝坏family容器")
	for founder in [null,3,"",[],{}]:
		var bad_body: Dictionary=state.duplicate(true); bad_body.family.founder_actor_id=founder
		var malformed: String=with_state(envelope,bad_body)
		expect_error(malformed,catalog,registry,"InvalidFamilyFounder","v2缺坏founder必须拒绝")
		check(Codec.inspect_restore_selection(malformed).get("code")=="InvalidFamilyFounder","selection不默认actor.player或当前player")
	var bad_body: Dictionary=state.duplicate(true); bad_body.family.founder_actor_id="actor.chen"
	var wrong_founder: String=with_state(envelope,bad_body)
	check(Codec.inspect_restore_selection(wrong_founder).get("founder_actor_id")=="actor.chen","selection只提取未信任字符串，不把它宣称通过Family语义")
	expect_error(wrong_founder,catalog,registry,"InvalidFamilyFounder","完整Schema/Family拒绝与effective目录不符的founder")
	bad_body=state.duplicate(true); bad_body.family.successions=["family:1"]
	expect_error(with_state(envelope,bad_body),catalog,registry,"InvalidSuccessionChain","完整Family校验不能省略继任事实链")
	bad_body=state.duplicate(true); bad_body.player_actor_id="unknown.actor"
	changed=envelope.duplicate(true); changed.player_actor_id=bad_body.player_actor_id
	expect_error(with_state(changed,bad_body),catalog,registry,"InvalidPlayerIdentity","头体一致也不能恢复不存在的当前玩家")
	bad_body=state.duplicate(true); bad_body.scenario_id="unknown.scenario"
	changed=envelope.duplicate(true); changed.scenario_id=bad_body.scenario_id
	expect_error(with_state(changed,bad_body),catalog,registry,"InvalidScenario","头体一致仍须实际目录中的剧本")
	var injected: Dictionary=registry.duplicate(true)
	var entry: Dictionary=injected.validators["economy.state"].duplicate(true); entry.callback=Callable(self,"reject_injected")
	injected.validators["economy.state"]=entry
	for index in range(injected.validation_plan.before_actions.size()):
		if injected.validation_plan.before_actions[index].id==entry.id: injected.validation_plan.before_actions[index]=entry
	expect_error(text,catalog,injected,"InjectedDecodeRejected","完整状态校验必须使用同一注入registry")
	check(validation_calls==1,"decode只运行一次该状态校验，不重新构建世界")
	changed=envelope.duplicate(true); changed.ruleset_id="future-rules"; changed.payload_sha256="0".repeat(64)
	expect_error(JSON.stringify(changed),catalog,registry,"UnsupportedSaveVersion","旧规则版本首错仍优先于checksum")
	changed=envelope.duplicate(true); changed.payload_sha256="0".repeat(64)
	expect_error(JSON.stringify(changed),catalog,registry,"ChecksumMismatch","已知格式仍核原字节checksum")
	var future: String="{\"magic\":\"HM-SAVE\",\"format_version\":999,\"opaque\":true}"
	expect_error(future,catalog,registry,"UnsupportedSaveVersion","未知future封装立即停止")
	check(Codec.inspect_restore_selection(future).get("code")=="UnsupportedSaveVersion","selection不猜未来正文")
	var missing_family: Dictionary=state.duplicate(true); missing_family.erase("family")
	check(Codec.inspect_restore_selection(with_state(envelope,missing_family)).get("code")=="InvalidFamilyState","v2缺family明确拒绝")
	var v1_header: Dictionary=Strict.parse_text(Codec.encode(state,4,"manual-1",Catalog.load_checked(FileAccess.get_file_as_string(ROOT+fixture.catalog_file))),Codec.MAX_BYTES).value
	check(Codec.inspect_restore_selection(with_state(v1_header,missing_family)).get("code")=="InvalidFamilyState","CURRENT v1不从当前player猜founder")
	# 明确合成的最小迁移边界，不是历史原档或已合法世界；只观察现有pre-family迁移取候选。
	var synthetic_old: Dictionary={"ruleset_id":"f07-world-1","content_id":state.content_id,"content_version":state.content_version,"content_digest":state.content_digest,"engine_build":state.engine_build,"rng_backend":state.rng_backend,"scenario_id":state.scenario_id,"player_actor_id":"synthetic.founder","actions":{},"facts":{},"actors":{}}
	var synthetic_header: Dictionary=v1_header.duplicate(true); synthetic_header.ruleset_id=synthetic_old.ruleset_id
	var migrated_selection: Dictionary=Codec.inspect_restore_selection(with_state(synthetic_header,synthetic_old))
	check(migrated_selection=={"ok":true,"scenario_id":state.scenario_id,"founder_actor_id":"synthetic.founder"},"v1只有既有pre-family Rules迁移可补founder，仍是未验证候选")
	synthetic_header=envelope.duplicate(true); synthetic_header.ruleset_id=synthetic_old.ruleset_id; synthetic_header.player_actor_id=synthetic_old.player_actor_id
	check(Codec.inspect_restore_selection(with_state(synthetic_header,synthetic_old)).get("code")=="InvalidFamilyState","v2不能套旧规则迁移补造缺失家族起点")
	check(state==before and catalog==catalog_before and registry==registry_before,"成功/失败不改原state、目录或registry")
	check(FileAccess.get_file_as_string(ROOT+"v1-4f16935-family-after-adoption.hmsave")==raw,"真实历史原字节未改")
	print("SAVE_V2_DECODING_CHECKS=",checks," FAILURES=",failed)
	print("KERNEL_ATOMIC_RESTORE=NOT_RUN; PRODUCTION_SIDECAR=NOT_RUN")
	quit(0 if failed==0 else 1)
