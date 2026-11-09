## 见闻只保存跨局发现ID；实际军营经历须绑定本人、世界与正式完成时刻。
extends SceneTree
const Profile=preload("res://core/card_discovery_profile.gd")
var checks: int=0
var failures: int=0
func check(value: bool,message: String) -> void:
	checks+=1
	if not value: failures+=1;printerr("FAIL "+message)

## 这是组件投影输入，不构造或声称正式军事World事实。
static func view(id: String="VI04") -> Dictionary:
	var world: String="0123456789abcdef0123456789abcdef"
	return {"actor_id":"person","source_identity":{"world_id":world,"execution_epoch":2,"revision":9,"tick":8},"cards":[{"card_id":id,"category":"insight","acquired_this_world":true,"experienced_this_world":true,"acquired_tick":7,"experience":{"kind":"completed_camp_training_v1","world_id":world,"scenario_id":"scenario","actor_id":"person","fact_id":"training:1","garrison_id":"camp","place_id":"field","unit_id":"unit","started_tick":3,"completed_tick":7}}]}

## 新独占文件保原始普通/自由标记与坏档，未取得的图鉴显示不会生成本局经历。
func _initialize() -> void:
	var path: String="user://insight-profile-"+str(Time.get_unix_time_from_system()).replace(".","-")+".json"
	var journal=Profile.new(path)
	check(journal.load_profile().ok,"load independent profile")
	var good: Dictionary=view()
	var result: Dictionary=journal.observe(good,"standard")
	check(result.get("ok",false) and result.get("changed",false),"actual insight discovery needs experience, not learned ability")
	if not result.get("ok",false): print("INSIGHT_DISCOVERY_PROFILE %d checks %d failures"%[checks,failures]);quit(1);return
	check(not journal.observe(good,"standard").changed,"repeat observation is idempotent")
	for mutation in ["actor","world","future","started","kind","ability","epoch","empty","float_tick"]:
		var bad: Dictionary=view("bad-"+mutation)
		match mutation:
			"actor": bad.cards[0].experience.actor_id="other"
			"world": bad.cards[0].experience.world_id="abcdef0123456789abcdef0123456789"
			"future": bad.cards[0].experience.completed_tick=9;bad.cards[0].acquired_tick=9
			"started": bad.cards[0].experience.started_tick=8
			"kind": bad.cards[0].experience.kind="appointed_officer"
			"ability": bad.cards[0].learned=true
			"epoch": bad.cards[0].experience.execution_epoch=2
			"empty": bad.cards[0].experience.fact_id=""
			"float_tick": bad.cards[0].acquired_tick=7.0
		check(not journal.observe(bad,"standard").ok and not "bad-"+mutation in journal.discovered_ids(),"reject mismatched insight "+mutation)
	check(journal.observe(view("VI04-free"),"free").changed and journal.save_profile().ok,"free insight keeps separate record")
	var restored=Profile.new(path)
	check(restored.load_profile().ok and restored.discovered_ids()==["VI04","VI04-free"],"new profile reader restores stable IDs")
	var later: Dictionary=view("VI04-free");later.cards[0].acquired_this_world=false;later.cards[0].experienced_this_world=false;later.cards[0].experience=null;later.cards[0].acquired_tick=null
	check(not restored.observe(later,"standard").changed,"new-world discovery does not become current experience")
	var decorated: Dictionary=restored.decorate(later)
	check(decorated.cards[0].discovered_free and not decorated.cards[0].experienced_this_world and decorated.cards[0].experience==null,"free label does not grant current experience")
	var stored: Dictionary=JSON.parse_string(FileAccess.get_file_as_string(path))
	check(stored.size()==3 and not str(stored).contains("training:1"),"profile persists only original three fields, no world or fact ledger")
	var file=FileAccess.open(path,FileAccess.WRITE);file.store_string("{broken-insight-profile");file.close()
	check(not restored.save_profile().ok and FileAccess.get_file_as_string(path)=="{broken-insight-profile","bad external file remains unchanged")
	print("INSIGHT_DISCOVERY_PROFILE %d checks %d failures"%[checks,failures]);quit(1 if failures else 0)
