## 真v1世界与原件派生sidecar的v2写入合同；边界侧锁不是生产安装验证，完整restore仍未接。
extends SceneTree
const Codec=preload("res://core/save_codec.gd")
const Catalog=preload("res://core/content_catalog.gd")
const Strict=preload("res://core/strict_json.gd")
const Registry=preload("res://core/runtime/module_registry.gd")
const Builtin=preload("res://core/runtime/builtin_modules.gd")
const ROOT="res://../tests/fixtures/saves/a2/"
var checks: int=0
var failed: int=0
var registry: Dictionary={}
var wrapped_validator: Callable
var validation_calls: int=0
var reject_injected: bool=false

## 引擎准备后读真实捕获与可信原件，不先创建默认世界。
func _init() -> void:
	call_deferred("_run")

## 全部断言独立记录，输出错误即非零退出。
func check(value: bool, label: String) -> void:
	checks+=1
	if not value:
		failed+=1
		printerr("FAIL: ",label)

## 负例可重算声明摘要，但不能因此改变原始fixture或已有世界。
func relock(catalog: Dictionary) -> void:
	catalog.pack_set_digest=(JSON.stringify(catalog.pack_set,"",true,true)+"\n").sha256_text()

## 模块清单只从真实受信Registry投影，不能来自测试侧锁或存档自报。
func current_modules() -> Array:
	var registered: Dictionary=registry
	check(registered.ok,"受信Registry可构建")
	var result: Array=[]
	var ids: Array=registered.get("modules",{}).keys(); ids.sort()
	for id in ids: result.append({"id":id,"version":registered.modules[id].version})
	return result

## 包装实际领域校验以核对同一registry贯穿Schema；正常分支仍执行原规则，不伪造合法世界。
func validate_injected(state: Dictionary, data: Dictionary) -> Dictionary:
	validation_calls+=1
	if reject_injected: return {"ok":false,"code":"InjectedValidationRejected"}
	return wrapped_validator.call(state,data)

## 只有真正通过Schema的v1世界参与编码；元信息来自该状态，正文不增加第二份锁。
func _run() -> void:
	var fixture: Dictionary=Strict.parse_text(FileAccess.get_file_as_string(ROOT+"v2-boundary-sidecar.json")).get("value",{})
	check(fixture.get("claims",{}).get("real_catalog_original_bytes",false) and not fixture.get("claims",{}).get("public_compose_pack_set",true) and not fixture.get("claims",{}).get("installed_resource_verification",true),"侧锁明确标注真实原件派生的边界fixture，不冒充生产compose/安装验证")
	var raw_catalog: String=FileAccess.get_file_as_string(ROOT+fixture.catalog_file)
	check(raw_catalog.sha256_text()==fixture.catalog_file_sha256,"真实旧catalog原字节SHA匹配来源")
	var plain: Dictionary=Catalog.load_checked(raw_catalog)
	check(plain.ok,"真实原件通过正式ContentCatalog校验")
	if not plain.ok:
		quit(1); return
	var raw: String=FileAccess.get_file_as_string(ROOT+"v1-4f16935-family-after-adoption.hmsave")
	var source_sha: String=raw.sha256_text()
	var original: Dictionary=Strict.parse_text(raw,Codec.MAX_BYTES).value
	check(original.format_version==1 and original.ruleset_id=="f09-military-1","原v1捕获未被改格式或冒充v2")
	var decoded: Dictionary=Codec.decode(raw,plain)
	check(decoded.ok and decoded.get("state",{}).get("family",{}).get("children",{}).size()==1,"真实v1经过现有合法迁移/校验，非伪造的当前世界")
	if not decoded.ok:
		quit(1); return
	var state: Dictionary=decoded.state
	registry=Registry.build(Builtin.descriptors())
	var locked: Dictionary=plain.duplicate(true)
	locked.pack_set=fixture.pack_set.duplicate(true); locked.pack_set_digest=fixture.pack_set_digest
	var before_state: Dictionary=state.duplicate(true); var before_catalog: Dictionary=locked.duplicate(true)
	var v1: String=Codec.encode(state,4,"manual-1",plain)
	check(not v1.is_empty() and Codec.inspect_metadata(v1).get("metadata",{}).get("format_version")==1 and Codec.decode(v1,plain).ok,"完全无锁的旧encode公共签名继续生成可decode的v1")
	check(Codec.encode(state,4,"manual-1",locked).is_empty(),"v2缺失显式受信registry必须拒写，不能内部重建")
	var registry_before: Dictionary=registry.duplicate(true)
	for invalid_registry in [null,{},[],"bad",3,true,{"ok":true,"modules":{}}]:
		check(Codec.encode(state,4,"manual-1",locked,invalid_registry).is_empty(),"缺失或坏registry必须拒写，不重建或降v1")
	for field in ["id","version"]:
		for invalid_value in [null,3,"",[],{}]:
			var malformed: Dictionary=registry.duplicate(true); malformed.modules.economy[field]=invalid_value
			check(Codec.encode(state,4,"manual-1",locked,malformed).is_empty(),"坏模块身份拒写而非脚本错误："+field)
	var injected: Dictionary=registry.duplicate(true)
	injected.modules.economy.version="injected-registry-version"
	var entry: Dictionary=injected.validators["economy.state"].duplicate(true)
	wrapped_validator=entry.callback; entry.callback=Callable(self,"validate_injected")
	injected.validators["economy.state"]=entry
	for index in range(injected.validation_plan.before_actions.size()):
		if injected.validation_plan.before_actions[index].id==entry.id: injected.validation_plan.before_actions[index]=entry
	var injected_before: Dictionary=injected.duplicate(true)
	var injected_text: String=Codec.encode(state,4,"manual-1",locked,injected)
	check(not injected_text.is_empty() and validation_calls==1,"同一注入registry执行一次实际Schema校验并通过Resolver，不暗中重建")
	var injected_modules: Array=Codec.inspect_metadata(injected_text).get("metadata",{}).get("rule_modules",[])
	check({"id":"economy","version":"injected-registry-version"} in injected_modules,"v2保存实际注入模块版本，不信目录或Builtin默认版本")
	reject_injected=true; validation_calls=0
	check(Codec.encode(state,4,"manual-1",locked,injected).is_empty() and validation_calls==1,"注入registry的校验拒绝必须传回编码失败")
	reject_injected=false
	check(injected==injected_before and registry==registry_before,"编码不改变调用者registry或回调计划")
	check(Codec.encode(state,4,"manual-1",plain,{})=="","无锁v1显式坏registry沿Schema拒绝，不能重建")
	var text: String=Codec.encode(state,4,"manual-1",locked,registry)
	check(not text.is_empty(),"真实已验证世界和匹配边界侧锁可写v2")
	if text.is_empty():
		print("SAVE_V2_ENCODING_CHECKS=",checks," FAILURES=",failed)
		quit(1); return
	var envelope: Dictionary=Strict.parse_text(text,Codec.MAX_BYTES).value
	check(envelope.get("format_version")==2,"带锁目录必须选择v2封装，不能静默输出v1")
	if envelope.get("format_version")!=2:
		print("SAVE_V2_ENCODING_CHECKS=",checks," FAILURES=",failed)
		quit(1); return
	check(envelope.format_version==2 and envelope.scenario_id==state.scenario_id and envelope.player_actor_id==state.player_actor_id,"v2封装使用当前保存世界的scenario/player")
	check(envelope.pack_set==locked.pack_set and envelope.pack_set_digest==locked.pack_set_digest,"写入完整精确锁及复算身份")
	check(envelope.rule_modules==current_modules(),"模块版本来自受信Registry且按id排序")
	var payload: Dictionary=Strict.parse_text(envelope.payload_json).value
	check(payload==state and not payload.has("pack_set") and not payload.has("pack_set_digest") and not payload.has("rule_modules"),"payload保留权威状态原结构，不新增可漂移的第二份锁或模块身份")
	check(envelope.payload_json==JSON.stringify(state,"",true,true) and envelope.payload_sha256==envelope.payload_json.sha256_text() and envelope.payload_length==envelope.payload_json.to_utf8_buffer().size(),"payload原字节、UTF-8长度与SHA一致")
	check(Codec.encode(state,4,"manual-1",locked,registry)==text,"重复写入相同输入逐字节一致，不消费RNG")
	var inspected: Dictionary=Codec.inspect_metadata(text)
	check(inspected.ok and inspected.metadata.pack_set==locked.pack_set and inspected.metadata.rule_modules==envelope.rule_modules,"新v2写入可由真正inspect读取完整元信息")
	check(Codec.decode(text,locked).get("code")=="IncompatibleRuleModules","v2正文恢复必须显式提供受信registry")
	var path: String=OS.get_environment("TMPDIR").path_join("a2-v2-write-"+str(OS.get_process_id())+"-"+str(Time.get_ticks_usec())+".hmsave")
	var file=FileAccess.open(path,FileAccess.WRITE)
	check(file!=null,"独立临时文件可写")
	if file!=null:
		file.store_string(text); file.close()
		check(FileAccess.get_file_as_string(path)==text and FileAccess.get_sha256(path)==text.sha256_text(),"实际文件回读与encoder输出原字节一致")
		check(Codec.inspect_metadata(FileAccess.get_file_as_string(path)).get("metadata")==inspected.metadata,"真实文件回读元信息一致")
		DirAccess.remove_absolute(path)
	for field in ["pack_set","pack_set_digest"]:
		var bad: Dictionary=locked.duplicate(true); bad.erase(field)
		check(Codec.encode(state,4,"manual-1",bad,registry).is_empty(),"只有部分锁字段时拒绝，绝不降回v1："+field)
	for value in [null,{},[],"not-a-lock"]:
		var bad: Dictionary=locked.duplicate(true); bad.pack_set=value
		check(Codec.encode(state,4,"manual-1",bad,registry).is_empty(),"无效锁容器明确拒写")
	for value in [null,3,"","0".repeat(64)]:
		var bad: Dictionary=locked.duplicate(true); bad.pack_set_digest=value
		check(Codec.encode(state,4,"manual-1",bad,registry).is_empty(),"缺失/坏类型/错误自报锁摘要拒写")
	for field in ["manifest_sha256","content_sha256","resources_sha256"]:
		var bad: Dictionary=locked.duplicate(true); bad.pack_set.packs[0][field]="0".repeat(64)
		check(Codec.encode(state,4,"manual-1",bad,registry).is_empty(),"包字节摘要变动且旧lockdigest未更新不能保存："+field)
	var bad: Dictionary=locked.duplicate(true); bad.pack_set.catalog_sha256="0".repeat(64); relock(bad)
	check(Codec.encode(state,4,"manual-1",bad,registry).is_empty(),"即使重算lockdigest也不能把别的catalog绑定当前世界")
	bad=locked.duplicate(true); bad.pack_set.format_version=999; relock(bad)
	check(Codec.encode(state,4,"manual-1",bad,registry).is_empty(),"未知锁格式停止写入，不猜当前格式")
	bad=locked.duplicate(true); bad.pack_set.catalog_path="res://untrusted"; relock(bad)
	check(Codec.encode(state,4,"manual-1",bad,registry).is_empty(),"额外路径不是合法锁身份")
	for field in ["content_id","content_version","content_digest"]:
		bad=locked.duplicate(true); bad[field]="different"
		check(Codec.encode(state,4,"manual-1",bad,registry).is_empty(),"错catalog旧三元组拒写："+field)
	bad=locked.duplicate(true); bad.ok=false
	check(Codec.encode(state,4,"manual-1",bad,registry).is_empty(),"未成功验证的catalog不能给锁签入v2")
	for change in [{"scenario_id":"unknown.scenario"},{"player_actor_id":"unknown.actor"},{"player_actor_id":"actor.chen"},{"engine_build":"future-engine"},{"rng_backend":"future-rng"}]:
		var invalid: Dictionary=state.duplicate(true); invalid.merge(change,true)
		check(Codec.encode(invalid,4,"manual-1",locked,registry).is_empty(),"错scenario/player或引擎身份拒写："+str(change.keys()))
	for field in ["pack_set","pack_set_digest","rule_modules"]:
		var invalid: Dictionary=state.duplicate(true); invalid[field]=locked.get(field,{})
		check(Codec.encode(invalid,4,"manual-1",locked,registry).is_empty(),"正文并行身份副本明确拒绝而非静默裁掉："+field)
	bad=locked.duplicate(true); bad.rule_modules=[{"id":"forged","version":"999"}]
	var trusted_only: String=Codec.encode(state,4,"manual-1",bad,registry)
	check(not trusted_only.is_empty() and Codec.inspect_metadata(trusted_only).get("metadata",{}).get("rule_modules")==envelope.rule_modules,"目录自报模块版本不能取代受信Registry")
	for identity in [[-1,"manual-1"],[4,""],[4,"x".repeat(129)]]:
		check(Codec.encode(state,identity[0],identity[1],locked,registry).is_empty(),"旧generation/save_id合法性未放宽")
	check(state==before_state and locked==before_catalog,"成功和全部失败均未改世界、RNG或catalog")
	check(FileAccess.get_file_as_string(ROOT+"v1-4f16935-family-after-adoption.hmsave").sha256_text()==source_sha,"真实历史v1原字节未改写")
	print("SAVE_V2_ENCODING_CHECKS=",checks," FAILURES=",failed)
	print("BOUNDARY_SIDECAR_ONLY=true; V2_DECODE_AND_RESTORE=NOT_RUN; NATIVE_V2_PUT=NOT_RUN")
	quit(0 if failed==0 else 1)
