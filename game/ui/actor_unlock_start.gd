## 人物选择只展示收藏资格；最终新局仍由原生核真实人物与收藏门槛。
extends RefCounted
const Profile=preload("res://core/actor_unlock_profile.gd")
## 当前原生人物卡只准入现存人物政策；外貌生成不改变人生创建模式。
static func creation_mode(scenario: Dictionary) -> String:
	if scenario.get("actor_card_unlocks",{}).get("profile","") not in ["completed_procurement_actor_cards_v1","completed_task_actor_cards_v2"]: return ""
	return "standard" if scenario.get("dynamic_world",{}).get("founder_policy","") in ["selected_existing_actor_v1","selected_existing_actor_roster_v1"] else ""

## 确认新局时重读原文件；锁定人物不能把损坏来源折成空资格。
static func request_qualification(scenario: Dictionary,selected: String,path: String="user://profiles/actor_unlock_v1.json") -> Dictionary:
	if creation_mode(scenario).is_empty(): return {"ok":false,"code":"UnsupportedActorUnlockCreationMode"}
	var profile=Profile.new(path);var loaded: Dictionary=profile.load_profile()
	var actor: String=selected if not selected.is_empty() else str(scenario.get("player_actor",""))
	if not loaded.get("ok",false) and not actor in scenario.get("actor_card_unlocks",{}).get("initially_available_actor_ids",[]): return loaded
	return {"ok":true,"unlocked_actor_ids":profile.unlocked_actor_ids(),"profile_available":loaded.get("ok",false)}
static func _ids(value: Variant) -> bool:
	if not value is Array or value.is_empty() or value.size()>4096: return false
	var seen: Dictionary={}
	for id in value:
		if not Profile._id(id) or id.to_utf8_buffer().size()>256 or seen.has(id): return false
		seen[id]=true
	return true
static func resolve(data: Dictionary,scenario: Dictionary,path: String) -> Dictionary:
	var config: Variant=scenario.get("actor_card_unlocks")
	if not config is Dictionary or config.size()!=3 or not config.get("profile") in ["completed_procurement_actor_cards_v1","completed_task_actor_cards_v2"] or not _ids(config.get("card_ids")) or not _ids(config.get("initially_available_actor_ids")) or not _ids(scenario.get("playable_actor_ids")): return {"ok":false,"code":"InvalidActorStartPolicy"}
	var roster: Array=scenario.playable_actor_ids; var initial: Array=config.initially_available_actor_ids
	if not scenario.get("player_actor") in initial: return {"ok":false,"code":"InvalidActorStartInitialActor"}
	for actor in initial:
		if not actor in roster: return {"ok":false,"code":"InvalidActorStartInitialActor"}
	if not data.get("actor_cards") is Dictionary: return {"ok":false,"code":"MissingActorCardCatalog"}
	var targets: Dictionary={}
	for id in config.card_ids:
		var card: Variant=data.actor_cards.get(id)
		if not card is Dictionary or card.get("id")!=id or card.get("category")!="actor" or card.get("prerequisites")!={"kind":"current_player_completed_tasks"} or card.get("effect_kind")!="unlock_actor" or card.get("repeat_policy")!="once_per_actor_profile": return {"ok":false,"code":"InvalidActorStartCard"}
		var condition: Variant=card.get("acquire_condition")
		if condition!={"kind":"completed_procurement_twice_v1"} and not (config.profile=="completed_task_actor_cards_v2" and condition=={"kind":"completed_envoy_round_trip_v1"}): return {"ok":false,"code":"InvalidActorStartCardCondition"}
		var effect: Variant=card.get("effect_params")
		if not effect is Dictionary or effect.size()!=1 or not effect.get("target_actor_id") in roster or targets.has(effect.target_actor_id) or not data.get("texts",{}).get(card.get("name_key")) is String: return {"ok":false,"code":"InvalidActorStartCardTarget"}
		targets[effect.target_actor_id]=data.texts[card.name_key]
	for actor in roster:
		if not actor in initial and not targets.has(actor): return {"ok":false,"code":"MissingActorStartCard"}
	var profile=Profile.new(path); var loaded: Dictionary=profile.load_profile(); var unlocked: Array[String]=profile.unlocked_actor_ids()
	var actors: Dictionary={}
	for actor in roster:
		var available: bool=actor in initial or actor in unlocked
		var reason: String="初始可选人物"
		if not actor in initial:
			if available: reason="人物卡已解锁"+(" · 自由创建记录" if profile.source_for(actor).is_empty() else "")
			else: reason=("解锁记录暂不可用。" if not loaded.ok else "")+"需取得人物卡「"+str(targets[actor])+"」。"
		actors[actor]={"available":available,"reason":reason}
	return {"ok":true,"actors":actors}
