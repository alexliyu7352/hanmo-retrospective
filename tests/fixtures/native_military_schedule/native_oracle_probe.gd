## 原oracle的可选native见证器；真实Bridge结果才计native通过，collector模式只验GD读集。
extends RefCounted
const Strict=preload("res://core/strict_json.gd")
var mode: String="legacy"
var expected_build: Dictionary={}
var evidence_path: String=""
var evidence: Array=[]
var Provider
var provider
var calls: int=0
var fault_checked: bool=false

## 故障代理先调用真实so，仅在返回后破坏一项；不制造任何有效native提案。
class FaultBridge extends RefCounted:
	var real: RefCounted
	var mode: String=""
	var calls: int=0
	var original: Dictionary={}
	var scopes: Array=[]
	## 保存真实原返回作见证；cpp_failure只破坏规范整数输入交由实际C++拒绝。
	func propose_military_schedule(bytes: PackedByteArray) -> Variant:
		calls+=1
		var request: Dictionary=JSON.parse_string(bytes.get_string_from_utf8())
		scopes.append(request.scope_token)
		if mode=="cpp_failure":
			request.actors[0].plan.money="01"
			return real.call("propose_military_schedule",JSON.stringify(request).to_utf8_buffer())
		var result: Dictionary=real.call("propose_military_schedule",bytes)
		original=result.duplicate(true)
		if not result.get("ok",false):return result
		match mode:
			"bad_late":result.proposals.append({"actor_id":"pure.invalid.late","kind":"travel","params":{"destination":"pure.invalid"}})
			"scope":result.scope_token="not-this-call"
			"build":result.build.source_sha256="0".repeat(64)
			"shape":return []
		return result

## 参数只选择明确模式与已冻结build；没有so不能伪装为native测试成功。
func initialize(args: PackedStringArray) -> Dictionary:
	var build_path: String="";var extension: String=""
	for arg in args:
		if arg=="--native":mode="native"
		if arg=="--collect-only":mode="collect"
		if arg=="--missing-library":mode="missing_library"
		if arg.begins_with("--build="):build_path=arg.trim_prefix("--build=")
		if arg.begins_with("--extension="):extension=arg.trim_prefix("--extension=")
		if arg.begins_with("--native-evidence="):evidence_path=arg.trim_prefix("--native-evidence=")
	if mode=="legacy":return {"ok":true}
	if "--record" in args:return {"ok":false,"code":"NativeCannotRecordOldOracle"}
	if not ResourceLoader.exists("res://core/backends/native_military_schedule_provider.gd"):return {"ok":false,"code":"ProviderSourceMissing"}
	Provider=load("res://core/backends/native_military_schedule_provider.gd")
	if mode=="collect":return {"ok":true}
	if build_path.is_empty() or not FileAccess.file_exists(build_path):return {"ok":false,"code":"FrozenBuildMissing"}
	var parsed: Dictionary=Strict.parse_text(FileAccess.get_file_as_string(build_path))
	if not parsed.get("ok",false) or not parsed.get("value") is Dictionary:return {"ok":false,"code":"FrozenBuildShape"}
	expected_build=parsed.value
	if mode=="native":
		if extension.is_empty() or GDExtensionManager.load_extension(extension)!=OK:return {"ok":false,"code":"RealExtensionLoadRequired"}
		if not ClassDB.class_has_method("HanmoWorldBridge","propose_military_schedule"):return {"ok":false,"code":"RealMilitaryBridgeMissing"}
	elif ClassDB.class_exists("HanmoWorldBridge"):return {"ok":false,"code":"MissingLibraryProcessContainsBridge"}
	provider=Provider.new();provider.configure(expected_build)
	return {"ok":true}

## 输入只读与完整旧数组一起比较；collector只报告采集，不拼造原生成功Envelope。
func compare(label: String,state: Dictionary,data: Dictionary,old: Array) -> Dictionary:
	if mode=="legacy":return {"ok":true}
	var before: Dictionary=state.duplicate(true);var source: Dictionary=data.duplicate(true)
	calls+=1
	var result: Dictionary=Provider.collect_request(state,data,str(calls)) if mode=="collect" else provider.propose(state,data)
	var ok: bool=result.get("ok",false)
	if mode=="native":ok=ok and result.get("proposals")==old
	if mode=="missing_library":ok=not ok and result.get("code")=="NativeMilitaryBridgeUnavailable" and not result.has("proposals")
	ok=ok and state==before and data==source
	evidence.append({"label":label,"ok":ok,"mode":mode,"result_code":result.get("code",""),"result_path":result.get("path",""),"proposals":result.get("proposals",[]),"input_unchanged":state==before and data==source})
	if not ok:return {"ok":false,"code":"OracleProviderMismatch","label":label,"detail":result}
	if mode=="native" and not fault_checked and not old.is_empty():
		fault_checked=true
		return batch_failures(state,data,old)
	return {"ok":true}

## 第一条真实有效返回之后添加坏后项，确认全批失败无partial，恢复后仍能真实重试。
func batch_failures(state: Dictionary,data: Dictionary,old: Array) -> Dictionary:
	for fault in ["bad_late","scope","build","shape","cpp_failure","wrong_api"]:
		var fresh=Provider.new();fresh.configure(expected_build)
		var proxy=FaultBridge.new();proxy.real=ClassDB.instantiate("HanmoWorldBridge");proxy.mode=fault
		fresh._bridge=RefCounted.new() if fault=="wrong_api" else proxy
		var before: Dictionary=state.duplicate(true);var source: Dictionary=data.duplicate(true)
		var result: Dictionary=fresh.propose(state,data)
		var ok: bool=not result.get("ok",true) and str(result.get("code","")).begins_with("NativeMilitary") and not result.has("proposals") and state==before and data==source
		if fault=="bad_late":ok=ok and proxy.original.get("ok",false) and proxy.original.get("proposals")==old
		proxy.mode="";fresh._bridge=proxy
		var recovered: Dictionary=fresh.propose(state,data)
		ok=ok and recovered.get("ok",false) and recovered.get("proposals")==old
		if proxy.scopes.size()>1:ok=ok and proxy.scopes[-1]!=proxy.scopes[0]
		evidence.append({"label":"batch_failure/"+fault,"ok":ok,"failure":result,"real_prefix_count":proxy.original.get("proposals",[]).size(),"recovered":recovered.get("ok",false),"proxy_calls":proxy.calls,"scopes":proxy.scopes.duplicate()})
		fresh._bridge=null;proxy.real=null
		if not ok:return {"ok":false,"code":"BatchFailureNotAtomic","fault":fault,"result":result,"recovered":recovered}
	return {"ok":true}

## 证据另存，绝不替换旧oracle；缺少执行次数不能被当作native通过。
func finish() -> Dictionary:
	if mode=="legacy":return {"ok":true}
	if calls==0:return {"ok":false,"code":"NoProviderCalls"}
	if not evidence_path.is_empty():
		if FileAccess.file_exists(evidence_path):return {"ok":false,"code":"EvidenceAlreadyExists"}
		var file=FileAccess.open(evidence_path,FileAccess.WRITE)
		if file==null:return {"ok":false,"code":"EvidenceNotWritable"}
		file.store_string(JSON.stringify({"format":"HM-MILITARY-PROVIDER-EVIDENCE/1","mode":mode,"build":expected_build,"calls":calls,"batch_faults_checked":fault_checked,"rows":evidence},"\t",true,true)+"\n");file.close()
	if provider!=null:provider._bridge=null
	return {"ok":true}
