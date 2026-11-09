## 跨局发现只存稳定卡ID；普通与自由创建记录分开，绝不写入局内本领或人物解锁。
extends RefCounted
const VERSION=1
const MAX_CARDS=4096
const MAX_BYTES=1048576
var _path: String="user://profiles/card_discovery_v1.json"
var _normal: Array[String]=[]
var _free: Array[String]=[]
var _loaded: bool=false
var _dirty: bool=false
var _file_present: bool=false
var _file_snapshot: String=""

## 可注入独立测试路径；生产默认使用专属图鉴文件，不接触世界银行或旧档。
func _init(path: String="user://profiles/card_discovery_v1.json") -> void:
	_path=path

## 读取失败保持未就绪，调用者不能用空列表覆盖未知或损坏的原图鉴。
func load_profile() -> Dictionary:
	_loaded=false; _dirty=false
	if not _path.begins_with("user://") or _path.contains("..") or not _path.ends_with(".json"): return {"ok":false,"code":"InvalidDiscoveryPath"}
	if not FileAccess.file_exists(_path): _normal=[]; _free=[]; _file_present=false; _file_snapshot=""; _loaded=true; return {"ok":true}
	var file=FileAccess.open(_path,FileAccess.READ)
	if file==null: return {"ok":false,"code":"DiscoveryReadFailed"}
	if file.get_length()>MAX_BYTES: return {"ok":false,"code":"DiscoveryTooLarge"}
	var contents: String=file.get_as_text(); file.close()
	var parsed=JSON.new()
	if parsed.parse(contents)!=OK or not parsed.data is Dictionary: return {"ok":false,"code":"InvalidDiscoveryProfile"}
	var value: Dictionary=parsed.data
	if value.keys().size()!=3 or value.get("version")!=VERSION or not _valid_ids(value.get("discovered_card_ids")) or not _valid_ids(value.get("free_discovered_card_ids")): return {"ok":false,"code":"InvalidDiscoveryProfile"}
	var normal: Array[String]=[]; var free: Array[String]=[]
	for id in value.discovered_card_ids: normal.append(id)
	for id in value.free_discovered_card_ids: free.append(id)
	if _union(normal,free).size()>MAX_CARDS: return {"ok":false,"code":"DiscoveryLimit"}
	_normal=normal; _free=free; _file_present=true; _file_snapshot=contents; _loaded=true; return {"ok":true}

## 验证闭合、有界、唯一ID列表，数字或重复项不被归一化成合法发现。
static func _valid_ids(value: Variant) -> bool:
	if not value is Array or value.size()>MAX_CARDS: return false
	var seen: Dictionary={}
	for id in value:
		if not id is String or id.is_empty() or id.length()>256 or seen.has(id): return false
		seen[id]=true
	return true

## 合集只供图鉴展示，不是本局能力或新局人物资格列表。
static func _union(normal: Array[String], free: Array[String]) -> Array[String]:
	var ids: Array[String]=normal.duplicate()
	for id in free:
		if not id in ids: ids.append(id)
	ids.sort(); return ids
func discovered_ids() -> Array[String]: return _union(_normal,_free) if _loaded else []

## 写失败的内存发现仍需保存，下次没有新卡时也不能忘掉未落盘记录。
func needs_save() -> bool: return _loaded and _dirty

## 见闻取得只接正式本人经历投影；不借掌握/军职字段，也不保存临时执行代。
static func _valid_insight(view: Dictionary,row: Dictionary) -> bool:
	if row.get("experienced_this_world")!=true or not row.get("experience") is Dictionary or not view.get("source_identity") is Dictionary: return false
	for ability in ["learned","mastery","personal_skill_id","configured"]:
		if row.has(ability): return false
	var source: Dictionary=view.source_identity;var receipt: Dictionary=row.experience
	if receipt.size()!=10 or receipt.get("kind")!="completed_camp_training_v1" or receipt.get("actor_id")!=view.actor_id or receipt.get("world_id")!=source.get("world_id"): return false
	for key in ["scenario_id","actor_id","fact_id","garrison_id","place_id","unit_id"]:
		if not receipt.get(key) is String or receipt[key].is_empty() or receipt[key].to_utf8_buffer().size()>256: return false
	if not source.get("world_id") is String or source.world_id.length()!=32 or not source.world_id.is_valid_hex_number(): return false
	for key in ["execution_epoch","revision","tick"]:
		if not source.get(key) is int or source[key]<0: return false
	if source.execution_epoch<1 or not receipt.get("started_tick") is int or not receipt.get("completed_tick") is int or not row.get("acquired_tick") is int: return false
	return receipt.started_tick>=0 and receipt.completed_tick>=receipt.started_tick and receipt.completed_tick<=source.tick and row.get("acquired_tick")==receipt.completed_tick

## 只接受成功本人投影中的本局取得，不能把外部discovered或任意卡ID当成新发现。
func observe(view: Dictionary, creation_mode: String) -> Dictionary:
	if not _loaded: return {"ok":false,"code":"DiscoveryNotLoaded"}
	if view.get("ok",true)!=true or not creation_mode in ["standard","free"] or not view.get("actor_id") is String or str(view.actor_id).is_empty() or not view.get("cards") is Array: return {"ok":false,"code":"InvalidDiscoveryObservation"}
	var normal: Array[String]=_normal.duplicate(); var free: Array[String]=_free.duplicate(); var changed: bool=false
	for row in view.cards:
		if not row is Dictionary: return {"ok":false,"code":"InvalidDiscoveryObservation"}
		if row.get("acquired_this_world")!=true: continue
		var id: Variant=row.get("card_id")
		if not id is String or id.is_empty() or id.length()>256: return {"ok":false,"code":"InvalidDiscoveryObservation"}
		if row.get("category")=="insight":
			if not _valid_insight(view,row): return {"ok":false,"code":"InvalidDiscoveryObservation"}
		elif row.get("learned")!=true: return {"ok":false,"code":"InvalidDiscoveryObservation"}
		var target: Array[String]=free if creation_mode=="free" else normal
		if not id in target: target.append(id); changed=true
	if _union(normal,free).size()>MAX_CARDS: return {"ok":false,"code":"DiscoveryLimit"}
	normal.sort(); free.sort(); _normal=normal; _free=free; _dirty=_dirty or changed
	return {"ok":true,"changed":changed}

## 先写同目录临时文件再替换，失败不删原档；图鉴写失败不得回滚已经成功的世界命令。
func save_profile() -> Dictionary:
	if not _loaded: return {"ok":false,"code":"DiscoveryNotLoaded"}
	# 在当前对象载入后被其他实例或用户改动的图鉴必须重新读，不能用旧快照覆盖。
	if FileAccess.file_exists(_path)!=_file_present: return {"ok":false,"code":"DiscoveryChanged"}
	if _file_present:
		var current=FileAccess.open(_path,FileAccess.READ)
		if current==null: return {"ok":false,"code":"DiscoveryReadFailed"}
		if current.get_length()>MAX_BYTES or current.get_as_text()!=_file_snapshot: return {"ok":false,"code":"DiscoveryChanged"}
		current.close()
	var absolute: String=ProjectSettings.globalize_path(_path)
	if DirAccess.make_dir_recursive_absolute(absolute.get_base_dir())!=OK: return {"ok":false,"code":"DiscoveryDirectoryFailed"}
	var encoded: String=JSON.stringify({"version":VERSION,"discovered_card_ids":_normal,"free_discovered_card_ids":_free})
	var temporary: String=absolute+".tmp"
	var file=FileAccess.open(temporary,FileAccess.WRITE)
	if file==null: return {"ok":false,"code":"DiscoveryWriteFailed"}
	file.store_string(encoded); file.flush()
	if file.get_error()!=OK: file.close(); return {"ok":false,"code":"DiscoveryWriteFailed"}
	file.close()
	if DirAccess.rename_absolute(temporary,absolute)!=OK: return {"ok":false,"code":"DiscoveryReplaceFailed"}
	_file_present=true; _file_snapshot=encoded; _dirty=false
	return {"ok":true}

## 自由发现标记只加在展示副本，不能把曾发现改成本局取得或实际学会。
func decorate(view: Dictionary) -> Dictionary:
	var result: Dictionary=view.duplicate(true)
	for row in result.get("cards",[]): row["discovered_free"]=row.get("card_id","") in _free and not row.get("card_id","") in _normal
	return result
