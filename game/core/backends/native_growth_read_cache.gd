## 成长页原生行动的单代只读缓存；只持派生三标量/失败和一个Bridge，不保存旧世界三元组或回调。
extends RefCounted
const Codec=preload("res://core/backends/legacy_projection_codec.gd")
const BRIDGE_CLASS="HanmoWorldBridge"
const QUERY_ID="native_growth_action"
var _generation: int=0
var _attempted: bool=false
var _bridge: RefCounted=null
var _action: Dictionary={}
var _counts: Dictionary={"reads":0,"cache_hits":0,"attempts":0,"project":0,"encode":0,"bridge_creations":0,"install":0,"query":0,"invalidations":0}
var _total_usec: Dictionary={"project":0,"encode":0,"install":0,"query":0}
var _last_usec: Dictionary={"project":0,"encode":0,"install":0,"query":0}

## 来源或显式配置失效时立即释放旧Bridge与结果；统计保留，代次从唯一Backend接收。
func invalidate(generation: int) -> void:
	_generation=generation; _attempted=false; _bridge=null; _action={}
	_counts.invalidations+=1
	_last_usec={"project":0,"encode":0,"install":0,"query":0}

## 每代成功/失败均只尝试一次；临时源参数在返回后不被缓存，返回值也不别名内部结果。
func read(generation: int,state: Dictionary,catalog: Dictionary,registry: Dictionary,source_commit: String) -> Dictionary:
	if generation!=_generation: invalidate(generation)
	_counts.reads+=1
	if _attempted:
		_counts.cache_hits+=1
		return _action.duplicate(true)
	_attempted=true; _counts.attempts+=1
	# 同步尝试期间也不暴露旧值或假空闲；受信校验不得借重入触发第二次构建。
	_action=_failure("NativeGrowthReadFailure","/native_growth/attempt")
	var result: Dictionary=_attempt(generation,state,catalog,registry,source_commit)
	# 受信Schema回调可能同步发布新代；旧尝试只能丢弃，不能覆盖新代已完成的缓存。
	if generation!=_generation: return _failure("NativeGrowthReadSuperseded","/native_growth/generation")
	if result.ok: _action=result.action; _bridge=result.bridge
	else: _action=result; _bridge=null
	return _action.duplicate(true)

## 只使用已登记正式扩展；project包含原Schema一次，禁止为了计时再验或自动装载库。
func _attempt(generation: int,state: Dictionary,catalog: Dictionary,registry: Dictionary,source_commit: String) -> Dictionary:
	if not ClassDB.class_exists(BRIDGE_CLASS) or not ClassDB.can_instantiate(BRIDGE_CLASS): return _failure("NativeGrowthBridgeUnavailable","/native_growth/bridge")
	for method in ["install_legacy_artifact","query_actor_commitments"]:
		if not ClassDB.class_has_method(BRIDGE_CLASS,method): return _failure("NativeGrowthBridgeUnavailable","/native_growth/bridge/"+method)
	var started: int=Time.get_ticks_usec(); _counts.project+=1
	var projected: Variant=Codec.project_live(state,catalog,registry)
	_record("project",started,generation)
	if generation!=_generation: return _failure("NativeGrowthReadSuperseded","/native_growth/generation")
	var checked: Dictionary=_result_status(projected,"/native_growth/project")
	if not checked.ok: return checked
	if not projected.get("payload") is Dictionary or not projected.payload.get("provenance") is Dictionary: return _failure("InvalidNativeGrowthReadResult","/native_growth/project/payload")
	var provenance: Dictionary=projected.payload.provenance.duplicate(true); provenance["source_commit"]=source_commit
	started=Time.get_ticks_usec(); _counts.encode+=1
	var encoded: Variant=Codec.encode_artifact(projected,provenance)
	_record("encode",started,generation)
	if generation!=_generation: return _failure("NativeGrowthReadSuperseded","/native_growth/generation")
	checked=_result_status(encoded,"/native_growth/encode")
	if not checked.ok: return checked
	if not encoded.get("bytes") is PackedByteArray: return _failure("InvalidNativeGrowthReadResult","/native_growth/encode/bytes")
	var digest: String=_sha256(encoded.bytes)
	_counts.bridge_creations+=1
	var candidate: Variant=ClassDB.instantiate(BRIDGE_CLASS)
	if not candidate is RefCounted: return _failure("NativeGrowthBridgeUnavailable","/native_growth/bridge")
	started=Time.get_ticks_usec(); _counts.install+=1
	var installed: Variant=candidate.call("install_legacy_artifact",encoded.bytes)
	_record("install",started,generation)
	if generation!=_generation: return _failure("NativeGrowthReadSuperseded","/native_growth/generation")
	checked=_result_status(installed,"/native_growth/install")
	if not checked.ok: return checked
	if not installed.get("input_sha256") is String or installed.input_sha256!=digest: return _failure("InvalidNativeGrowthReadResult","/native_growth/install/input_sha256")
	# 当前玩家仅用于本次明确查询，既不保存完整DTO，也不把代次当持久world_id。
	var actor_id: String=state.player_actor_id
	var text: String=JSON.stringify({"format":"HM-NATIVE-ACTOR-COMMITMENTS-QUERIES/1","input_sha256":digest,"queries":[{"query_id":QUERY_ID,"actor_ids":[actor_id]}]},"",true,true)+"\n"
	var bytes: PackedByteArray=text.to_utf8_buffer()
	started=Time.get_ticks_usec(); _counts.query+=1
	var queried: Variant=candidate.call("query_actor_commitments",bytes)
	_record("query",started,generation)
	if generation!=_generation: return _failure("NativeGrowthReadSuperseded","/native_growth/generation")
	checked=_result_status(queried,"/native_growth/query")
	if not checked.ok: return checked
	var mapped: Dictionary=_map_action(queried,actor_id,digest,_sha256(bytes))
	if mapped.ok: mapped["bridge"]=candidate
	return mapped

## 核已消费的有限DTO路径与精确字节绑定，坏类型不转换成旧action或假空闲。
static func _map_action(result: Dictionary,actor_id: String,input_sha256: String,queries_sha256: String) -> Dictionary:
	for field in ["format","input_sha256","queries_sha256"]:
		if not result.get(field) is String: return _failure("InvalidNativeGrowthReadResult","/"+field)
	if result.format!="HM-GODOT-ACTOR-COMMITMENTS-RESULT/1" or result.input_sha256!=input_sha256 or result.queries_sha256!=queries_sha256: return _failure("InvalidNativeGrowthReadResult","/native_growth/query/binding")
	if not result.get("results") is Array or result.results.size()!=1: return _failure("InvalidNativeGrowthReadResult","/results")
	var row: Variant=result.results[0]
	if not row is Dictionary or not row.get("query_id") is String or row.query_id!=QUERY_ID or not row.get("actors") is Array or row.actors.size()!=1: return _failure("InvalidNativeGrowthReadResult","/results/0")
	var actor: Variant=row.actors[0]
	if not actor is Dictionary or not actor.get("actor_id") is String or actor.actor_id!=actor_id or not actor.has("current_activity"): return _failure("InvalidNativeGrowthReadResult","/results/0/actors/0")
	var activity: Variant=actor.current_activity
	if activity==null: return {"ok":true,"action":{}}
	var path: String="/results/0/actors/0/current_activity"
	if not activity is Dictionary: return _failure("InvalidNativeGrowthReadResult",path)
	if not activity.get("kind") is String or not activity.get("remaining_ticks") is int or not activity.get("params") is Dictionary: return _failure("InvalidNativeGrowthReadResult",path)
	var training: Variant=activity.params.get("training",{})
	if not training is Dictionary: return _failure("InvalidNativeGrowthReadResult",path+"/params/training")
	var skill_id: Variant=training.get("skill_id","")
	if not skill_id is String: return _failure("InvalidNativeGrowthReadResult",path+"/params/training/skill_id")
	return {"ok":true,"action":{"kind":activity.kind,"skill_id":skill_id,"remaining_ticks":activity.remaining_ticks}}

## 每次只复制开发计数和阶段时间；project总时长明确已经含原完整Schema。
func diagnostics() -> Dictionary:
	return {"generation":_generation,"attempted":_attempted,"has_bridge":_bridge!=null,"status":"unattempted" if not _attempted else ("failed" if _action.get("ok",true)==false else "ready"),"counts":_counts.duplicate(true),"total_usec":_total_usec.duplicate(true),"last_usec":_last_usec.duplicate(true),"project_includes_schema":true}

## 旧代已发生的成本只累计总时长，不能在同步失效后污染新代last计时。
func _record(stage: String,started: int,generation: int) -> void:
	var elapsed: int=Time.get_ticks_usec()-started
	_total_usec[stage]+=elapsed
	if generation==_generation: _last_usec[stage]=elapsed

## 验证阶段成功标记，原失败只提取code/path；任何异常形状都成为非空不可用结果。
static func _result_status(result: Variant,path: String) -> Dictionary:
	if not result is Dictionary or not result.get("ok") is bool: return _failure("InvalidNativeGrowthReadResult",path)
	if result.ok: return {"ok":true}
	var code: Variant=result.get("code","NativeGrowthReadFailure"); var location: Variant=result.get("path",path)
	return _failure(code if code is String and not code.is_empty() else "NativeGrowthReadFailure",location if location is String else path)

## 只哈希已经编码的临时字节用于Bridge绑定，不扫描或缓存完整World。
static func _sha256(bytes: PackedByteArray) -> String:
	var context=HashingContext.new(); context.start(HashingContext.HASH_SHA256); context.update(bytes)
	return context.finish().hex_encode()

## 不可用action永远非空且只含明确失败，不影响已成功的世界命令。
static func _failure(code: String,path: String) -> Dictionary: return {"ok":false,"code":code,"path":path}
