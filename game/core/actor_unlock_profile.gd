## 人物解锁独立于发现记录和世界银行；仅保存已核本人的真实完成来源。
extends RefCounted
const VERSION=1
const MAX_ACTORS=4096
const MAX_BYTES=16777216
var _path: String
var _normal: Dictionary={}
var _free: Dictionary={}
var _loaded: bool=false
var _dirty: bool=false
var _present: bool=false
var _snapshot: String=""
var _version: int=VERSION
func _init(path: String="user://profiles/actor_unlock_v1.json") -> void: _path=path

## ID、整数与世界身份沿有限投影合同校验；JSON整数载入后的float表示须仍为精确整值。
static func _id(value: Variant) -> bool: return value is String and not value.is_empty() and value.length()<=256
static func _whole(value: Variant) -> bool: return (value is int or value is float) and is_finite(float(value)) and value>=0 and value<=2147483647 and value==floor(value)
static func _world(value: Variant) -> bool:
	if not value is String or value.length()!=32: return false
	for code in value.to_ascii_buffer():
		if not (code>=48 and code<=57 or code>=97 and code<=102): return false
	return true

## v2只收实际任务、呈递和返报的整份来源；不能把去程递交当成往返。
static func _envoy_receipt(value: Variant,acquired_tick: Variant) -> bool:
	if not value is Dictionary or value.size()!=12: return false
	for key in ["task_id","contract_id","acceptance_fact_id","mission_authority_fact_id","proposal_id","delivery_fact_id","presentation_fact_id","completion_fact_id","report_fact_id"]:
		if not _id(value.get(key)): return false
	for key in ["accepted_tick","delivered_tick","completed_tick"]:
		if not _whole(value.get(key)): return false
	var facts: Dictionary={}
	for key in ["acceptance_fact_id","mission_authority_fact_id","delivery_fact_id","presentation_fact_id","completion_fact_id","report_fact_id"]:
		if facts.has(value[key]): return false
		facts[value[key]]=true
	return value.accepted_tick<=value.delivered_tick and value.delivered_tick<=value.completed_tick and value.completed_tick==acquired_tick

## 原PC03来源字节和两份结算合同不变，v2才能接独立使者完成来源。
static func _receipt(value: Variant,version: int=1) -> bool:
	if not value is Dictionary or value.size()!=7: return false
	for key in ["card_id","target_actor_id","earner_actor_id","scenario_id"]:
		if not _id(value.get(key)): return false
	if not _world(value.get("world_id")) or not _whole(value.get("acquired_tick")): return false
	if value.has("envoy"): return version==2 and _envoy_receipt(value.envoy,value.acquired_tick)
	if not value.get("tasks") is Array or value.tasks.size()!=2: return false
	for source in value.tasks:
		if not source is Dictionary or source.size()!=4 or not _id(source.get("task_id")) or not _id(source.get("settlement_id")) or source.get("template_id")!="procurement" or not _whole(source.get("reported_tick")): return false
	var first: Dictionary=value.tasks[0]; var second: Dictionary=value.tasks[1]
	return first.task_id!=second.task_id and first.settlement_id!=second.settlement_id and (first.reported_tick<second.reported_tick or first.reported_tick==second.reported_tick and first.task_id<second.task_id) and value.acquired_tick==second.reported_tick

## 冗余ID清单必须与逐人来源完全一致，旧发现ID列表不能被读作人物资格。
static func _records(ids: Variant,sources: Variant,version: int=1) -> bool:
	if not ids is Array or ids.size()>MAX_ACTORS or not sources is Dictionary or sources.size()!=ids.size(): return false
	var seen: Dictionary={}
	for actor in ids:
		if not _id(actor) or seen.has(actor) or not _receipt(sources.get(actor),version) or sources[actor].target_actor_id!=actor: return false
		seen[actor]=true
	return true
static func _union(normal: Dictionary,free: Dictionary) -> Array[String]:
	var ids: Array[String]=[]
	for actor in normal: ids.append(actor)
	for actor in free:
		if not actor in ids: ids.append(actor)
	ids.sort(); return ids

## JSON解析把整数读作float；只在严格整值校验后恢复固定整数形状，冷读不会改来源值。
static func _typed_receipt(value: Dictionary) -> Dictionary:
	var result: Dictionary=value.duplicate(true); result.acquired_tick=int(result.acquired_tick)
	if result.has("envoy"):
		for key in ["accepted_tick","delivered_tick","completed_tick"]: result.envoy[key]=int(result.envoy[key])
	else:
		for task in result.tasks: task.reported_tick=int(task.reported_tick)
	return result
static func _typed_records(value: Dictionary) -> Dictionary:
	var result: Dictionary={}
	for actor in value: result[actor]=_typed_receipt(value[actor])
	return result

## 载入失败保持不可写，保留未知或损坏原件，不以空资格覆盖它。
func load_profile() -> Dictionary:
	_loaded=false; _dirty=false
	if not _path.begins_with("user://") or _path.contains("..") or not _path.ends_with(".json"): return {"ok":false,"code":"InvalidActorUnlockPath"}
	if not FileAccess.file_exists(_path):
		_normal={}; _free={}; _version=VERSION; _present=false; _snapshot=""; _loaded=true; return {"ok":true}
	var file=FileAccess.open(_path,FileAccess.READ)
	if file==null: return {"ok":false,"code":"ActorUnlockReadFailed"}
	if file.get_length()>MAX_BYTES: return {"ok":false,"code":"ActorUnlockTooLarge"}
	var text: String=file.get_as_text(); file.close()
	var parsed=JSON.new()
	if parsed.parse(text)!=OK or not parsed.data is Dictionary: return {"ok":false,"code":"InvalidActorUnlockProfile"}
	var value: Dictionary=parsed.data
	if value.size()!=5 or not _whole(value.get("version")) or not int(value.version) in [1,2] or not _records(value.get("unlocked_actor_ids"),value.get("sources"),int(value.version)) or not _records(value.get("free_unlocked_actor_ids"),value.get("free_sources"),int(value.version)): return {"ok":false,"code":"InvalidActorUnlockProfile"}
	if _union(value.sources,value.free_sources).size()>MAX_ACTORS: return {"ok":false,"code":"ActorUnlockLimit"}
	_version=int(value.version); _normal=_typed_records(value.sources); _free=_typed_records(value.free_sources); _present=true; _snapshot=text; _loaded=true
	return {"ok":true}

## 来自同一已验证世界的完整有限观察；全部校验成功才合入内存，不读发现标记或替世界授能力。
func observe(view: Dictionary,mode: String) -> Dictionary:
	if not _loaded: return {"ok":false,"code":"ActorUnlockNotLoaded"}
	var identity: Variant=view.get("source_identity")
	if mode not in ["standard","free"] or view.get("ok",true)!=true or not _id(view.get("actor_id")) or not identity is Dictionary or identity.size()!=4 or not _world(identity.get("world_id")) or not _whole(identity.get("execution_epoch")) or not _whole(identity.get("revision")) or not _whole(identity.get("tick")) or not view.get("cards") is Array or view.cards.size()>MAX_ACTORS: return {"ok":false,"code":"InvalidActorUnlockObservation"}
	var source_version: Variant=view.get("source_version",1)
	if not _whole(source_version) or not int(source_version) in [1,2]: return {"ok":false,"code":"UnsupportedActorUnlockSourceVersion"}
	var next_version: int=_version
	var normal: Dictionary=_normal.duplicate(true); var free: Dictionary=_free.duplicate(true); var changed: bool=false; var seen: Dictionary={}; var targets: Dictionary={}
	for row in view.cards:
		if not row is Dictionary or not _id(row.get("card_id")) or not _id(row.get("target_actor_id")) or not _whole(row.get("completed_count")) or row.completed_count>2 or not row.has("acquired") or seen.has(row.card_id) or targets.has(row.target_actor_id): return {"ok":false,"code":"InvalidActorUnlockObservation"}
		var condition: String="completed_procurement_twice_v1"
		var required: int=2
		if source_version==2:
			if not row.get("condition_kind") is String or not row.condition_kind in ["completed_procurement_twice_v1","completed_envoy_round_trip_v1"]: return {"ok":false,"code":"InvalidActorUnlockCondition"}
			condition=row.condition_kind; required=1 if condition=="completed_envoy_round_trip_v1" else 2
			if not _whole(row.get("required_count")) or row.required_count!=required: return {"ok":false,"code":"InvalidActorUnlockCondition"}
		elif row.has("condition_kind") or row.has("required_count"): return {"ok":false,"code":"ActorUnlockSourceVersionRequired"}
		if row.completed_count>required: return {"ok":false,"code":"InvalidActorUnlockObservation"}
		seen[row.card_id]=true; targets[row.target_actor_id]=true
		if row.acquired==null:
			if row.completed_count==required: return {"ok":false,"code":"MissingActorUnlockEvidence"}
			continue
		var source: Variant=row.acquired
		if row.completed_count!=required or not _receipt(source,int(source_version)) or source.card_id!=row.card_id or source.target_actor_id!=row.target_actor_id or source.earner_actor_id!=view.actor_id or source.world_id!=identity.world_id or source.acquired_tick>identity.tick: return {"ok":false,"code":"InvalidActorUnlockEvidence"}
		if (condition=="completed_envoy_round_trip_v1")!=source.has("envoy"): return {"ok":false,"code":"ActorUnlockEvidenceConditionMismatch"}
		var destination: Dictionary=free if mode=="free" else normal
		if not destination.has(source.target_actor_id):
			destination[source.target_actor_id]=_typed_receipt(source); changed=true
			if source.has("envoy"): next_version=2
	if _union(normal,free).size()>MAX_ACTORS: return {"ok":false,"code":"ActorUnlockLimit"}
	_normal=normal; _free=free; _version=next_version; _dirty=_dirty or changed
	return {"ok":true,"changed":changed}

## 对外只提供隔离副本；所有人物仍须通过新剧本的出生、存活与开局资格。
func unlocked_actor_ids() -> Array[String]:
	if _loaded: return _union(_normal,_free)
	var empty: Array[String]=[]
	return empty
func source_for(actor_id: String,free: bool=false) -> Dictionary:
	if not _loaded: return {}
	return ( _free if free else _normal ).get(actor_id,{}).duplicate(true)
func needs_save() -> bool: return _loaded and _dirty

## 写前重核载入的原字节，避免旧对象覆盖外部已改动的资格账。
func _unchanged() -> bool:
	if FileAccess.file_exists(_path)!=_present: return false
	if not _present: return true
	var file=FileAccess.open(_path,FileAccess.READ)
	if file==null or file.get_length()>MAX_BYTES: return false
	return file.get_as_text()==_snapshot

## 同目录临时文件替换；失败留下待保存记录，绝不回滚已成功的世界业务。
func save_profile() -> Dictionary:
	if not _loaded: return {"ok":false,"code":"ActorUnlockNotLoaded"}
	if not _unchanged(): return {"ok":false,"code":"ActorUnlockChanged"}
	var normal_ids: Array=_normal.keys(); normal_ids.sort(); var free_ids: Array=_free.keys(); free_ids.sort()
	var text: String=JSON.stringify({"version":_version,"unlocked_actor_ids":normal_ids,"free_unlocked_actor_ids":free_ids,"sources":_normal,"free_sources":_free})
	if text.to_utf8_buffer().size()>MAX_BYTES: return {"ok":false,"code":"ActorUnlockTooLarge"}
	var absolute: String=ProjectSettings.globalize_path(_path)
	if DirAccess.make_dir_recursive_absolute(absolute.get_base_dir())!=OK: return {"ok":false,"code":"ActorUnlockDirectoryFailed"}
	var temporary: String=absolute+".tmp-"+str(OS.get_process_id())+"-"+str(get_instance_id())+"-"+str(Time.get_ticks_usec())
	var file=FileAccess.open(temporary,FileAccess.WRITE)
	if file==null: return {"ok":false,"code":"ActorUnlockWriteFailed"}
	file.store_string(text); file.flush()
	if file.get_error()!=OK: file.close(); return {"ok":false,"code":"ActorUnlockWriteFailed"}
	file.close()
	if not _unchanged(): return {"ok":false,"code":"ActorUnlockChanged"}
	if DirAccess.rename_absolute(temporary,absolute)!=OK: return {"ok":false,"code":"ActorUnlockReplaceFailed"}
	_snapshot=text; _present=true; _dirty=false
	return {"ok":true}
