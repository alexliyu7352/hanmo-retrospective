## 请求身份与有限摘要的协议门；不创建World，不将模拟摘要当作真实采购成果。
extends SceneTree
const Backend=preload("res://ui/native_resident_backend.gd")
var checks: int=0
var failures: int=0
func _init() -> void: call_deferred("run")
func check(value: bool,label: String) -> void:
	checks+=1
	if not value: failures+=1; printerr("FAIL "+label)
func run() -> void:
	var scenario: Dictionary={"dynamic_world":{"founder_policy":"selected_existing_actor_roster_v1"},"seed_text":"registration-contract"}
	var catalog: Dictionary={"data":{"scenarios":{"s":scenario}}}
	var old: Dictionary=Backend.native_request(catalog,"s","a")
	check(not old.has("unlocked_actor_ids") and not {"name":"actor_card_unlocks","version":"1"} in old.expected_catalog_identity.module_versions,"旧情景请求不增加资格字段或模块")
	scenario["actor_card_unlocks"]={"profile":"completed_procurement_actor_cards_v1"}
	var current: Dictionary=Backend.native_request(catalog,"s","a",{},["b"])
	check(current.unlocked_actor_ids==["b"] and current.expected_catalog_identity.module_versions.back()=={"name":"actor_card_unlocks","version":"1"},"显式新情景请求绑定人物卡模块和独立资格")
	check(current.expected_catalog_identity.module_versions.slice(0,-1)==old.expected_catalog_identity.module_versions,"新模块不更改原模块顺序和版本")
	var backend=Backend.new();backend._catalog=catalog;backend._scenario_id="s";backend._source={"revision":0};backend._stage={"views":{"command_context":{"actor_id":"a"}}}
	var frame: Dictionary={"ok":true,"world_scene_legacy":false,"personal_battle_view":{},"military_battle_page":{},"siege_page":{},"study_activity":{},"presentation_location":{}}
	check(not backend._accept_base("screen_context",frame).get("ok",true),"显式新情景缺人物卡摘要拒绝发布")
	frame["actor_cards_summary"]={"enabled":true,"actor_id":"a","completed_count":1,"acquired_count":0}
	check(backend._accept_base("screen_context",frame).get("ok",false),"仅人物卡情景接受真实有限摘要形状")
	frame.actor_cards_summary.acquired_count=1
	check(not backend._accept_base("screen_context",frame).get("ok",true),"未完成两次时不能宣称取得")
	frame.actor_cards_summary.completed_count=2
	check(backend._accept_base("screen_context",frame).get("ok",false),"完成摘要支持人物资格读取")
	frame.actor_cards_summary.actor_id="b"
	check(not backend._accept_base("screen_context",frame).get("ok",true),"另一人物的摘要不能发布为本人")
	frame.actor_cards_summary.actor_id="a";frame.actor_cards_summary.completed_count=3
	check(not backend._accept_base("screen_context",frame).get("ok",true),"有限进度上界严格拒绝")
	scenario.erase("actor_card_unlocks");frame.erase("actor_cards_summary")
	check(backend._accept_base("screen_context",frame).get("ok",false),"旧七字段frame保持准入")
	backend.free()
	print("ACTOR_CARD_REGISTRATION ",checks," checks ",failures," failures; WORLD_NOT_CREATED")
	quit(1 if failures else 0)
