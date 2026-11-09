## 人物解锁只从同世界、同本人、同提交代的完整有限投影同步，使用独立跨局账。
extends RefCounted
const Profile=preload("res://core/actor_unlock_profile.gd")
var profile: RefCounted
var _loaded: bool=false
func _init(path: String="user://profiles/actor_unlock_v1.json") -> void: profile=Profile.new(path)
func _ensure() -> Dictionary:
	if _loaded: return {"ok":true}
	var result: Dictionary=profile.load_profile(); _loaded=result.get("ok",false); return result
## 身份数字规范成整型再比较，仍逐项绑定世界、执行代、revision及半日。
static func _identity(value: Variant) -> Dictionary:
	if not value is Dictionary or not Profile._world(value.get("world_id")): return {}
	var out: Dictionary={"world_id":value.world_id}
	for key in ["execution_epoch","revision","tick"]:
		if not Profile._whole(value.get(key)): return {}
		out[key]=int(value[key])
	return out

## 先完整收齐同次投影再观察，分页错误或过期不得保存前几页的部分资格。
func sync_after_commit(query: Callable,context: Callable,mode: String) -> Dictionary:
	var loaded: Dictionary=_ensure()
	if not loaded.ok: return loaded
	var source: Variant=context.call()
	if not source is Dictionary or not source.get("ok",false) or not Profile._id(source.get("actor_id")): return {"ok":false,"code":"ActorUnlockSourceUnavailable"}
	source=source.duplicate(true)
	var identity: Dictionary=_identity(source)
	if identity.is_empty(): return {"ok":false,"code":"ActorUnlockSourceUnavailable"}
	var offset: int=0; var total: int=-1; var rows: Array=[]; var source_version: int=0
	for _page in range(65):
		if source!=context.call(): return {"ok":false,"code":"ActorUnlockSourceChanged"}
		var result: Variant=query.call({"actor_id":source.actor_id,"offset":offset,"limit":64})
		if not result is Dictionary: return {"ok":false,"code":"InvalidActorUnlockPage"}
		if not result.get("ok",true): return result
		if result.get("actor_id")!=source.actor_id or _identity(result.get("source_identity"))!=identity or not Profile._whole(result.get("offset")) or result.offset!=offset or not Profile._whole(result.get("total")) or result.total>4096 or not result.get("cards") is Array or result.cards.size()>64: return {"ok":false,"code":"InvalidActorUnlockPage"}
		var page_version: Variant=result.get("source_version",1)
		if not Profile._whole(page_version) or not int(page_version) in [1,2]: return {"ok":false,"code":"UnsupportedActorUnlockSourceVersion"}
		if source_version==0: source_version=int(page_version)
		if source_version!=int(page_version): return {"ok":false,"code":"ActorUnlockSourceChanged"}
		if total<0: total=int(result.total)
		if result.total!=total: return {"ok":false,"code":"ActorUnlockSourceChanged"}
		rows.append_array(result.cards)
		if rows.size()>total: return {"ok":false,"code":"InvalidActorUnlockPage"}
		if result.get("next_offset")==null:
			if rows.size()!=total or source!=context.call(): return {"ok":false,"code":"ActorUnlockSourceChanged"}
			var observation: Dictionary={"actor_id":source.actor_id,"source_identity":identity,"cards":rows}
			if source_version==2: observation["source_version"]=2
			var observed: Dictionary=profile.observe(observation,mode)
			if not observed.ok: return observed
			if profile.needs_save():
				var saved: Dictionary=profile.save_profile()
				if not saved.ok: return saved
			return {"ok":true,"changed":observed.changed}
		if not Profile._whole(result.next_offset) or result.next_offset!=offset+result.cards.size() or result.next_offset<=offset: return {"ok":false,"code":"InvalidActorUnlockPage"}
		offset=int(result.next_offset)
	return {"ok":false,"code":"ActorUnlockLimit"}

## 当前成果与跨局资格分别呈现；坏文件不把当前真实委托成果改成零。
func page_view(query: Callable,offset: int=0) -> Dictionary:
	var loaded: Dictionary=_ensure()
	var result: Variant=query.call({"offset":offset,"limit":32})
	if not result is Dictionary or not result.get("ok",true) or not result.get("cards") is Array: return result if result is Dictionary else {"ok":false,"code":"InvalidActorUnlockPage"}
	var view: Dictionary=result.duplicate(true); var ids: Array[String]=profile.unlocked_actor_ids()
	for row in view.cards:
		if not row is Dictionary or not Profile._id(row.get("target_actor_id")): return {"ok":false,"code":"InvalidActorUnlockPage"}
		var actor: String=row.target_actor_id; row["unlocked"]=actor in ids
		row["unlocked_free"]=row.unlocked and profile.source_for(actor).is_empty()
		row["unlock_source"]=profile.source_for(actor,row.unlocked_free) if row.unlocked else {}
	if not loaded.ok: view["unlock_notice"]="人物解锁记录暂不可用；本局委托成果仍可查看。"
	elif profile.needs_save(): view["unlock_notice"]="本局条件已达成，跨局人物资格尚未保存。"
	return view
