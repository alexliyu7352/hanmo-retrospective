## 本域跨局文件的真实读写门；不替代原生任务完成、World注册或标题资格门。
extends SceneTree
const Profile=preload("res://core/actor_unlock_profile.gd")
var checks: int=0
var failures: int=0
func _init() -> void: call_deferred("run")
func check(value: bool,label: String) -> bool:
	checks+=1
	if not value: failures+=1; printerr("FAIL "+label)
	return value
func receipt() -> Dictionary:
	return {"card_id":"pc03","target_actor_id":"steward","earner_actor_id":"player","world_id":"01000000000000000000000000000000","scenario_id":"scenario:1","acquired_tick":8,"tasks":[{"task_id":"task:1","template_id":"procurement","settlement_id":"settlement:1","reported_tick":5},{"task_id":"task:2","template_id":"procurement","settlement_id":"settlement:2","reported_tick":8}]}
func observation() -> Dictionary:
	return {"actor_id":"player","source_identity":{"world_id":"01000000000000000000000000000000","execution_epoch":1,"revision":20,"tick":10},"cards":[{"card_id":"pc03","target_actor_id":"steward","completed_count":2,"acquired":receipt()}]}
func run() -> void:
	var folder: String="user://tests/actor-unlock-"+str(OS.get_process_id())+"-"+str(Time.get_ticks_usec())
	var path: String=folder.path_join("profile.json")
	var profile=Profile.new(path)
	if not check(profile.load_profile().get("ok",false) and profile.unlocked_actor_ids().is_empty(),"新独立资格文件不默认解锁"):
		quit(1); return
	var initial: Dictionary=observation()
	check(profile.observe(initial,"standard").get("ok",false) and profile.needs_save() and profile.unlocked_actor_ids()==["steward"],"实际完成证据只授新局人物ID")
	check(profile.source_for("steward")==receipt(),"保存世界本人及两次结算的完整来源")
	check(profile.save_profile().get("ok",false) and FileAccess.file_exists(path) and not profile.needs_save(),"真实落盘并清除待保存标记")
	var original: String=FileAccess.get_file_as_string(path)
	var loaded=Profile.new(path)
	check(loaded.load_profile().get("ok",false) and loaded.unlocked_actor_ids()==["steward"] and loaded.source_for("steward")==receipt(),"另对象读取同一真实跨局资格")
	check(loaded.observe(initial,"standard").get("ok",false) and not loaded.needs_save() and FileAccess.get_file_as_string(path)==original,"重复观察不改首次来源或文件")
	var later: Dictionary=observation();later.source_identity.world_id="02000000000000000000000000000000";later.cards[0].acquired.world_id=later.source_identity.world_id
	check(loaded.observe(later,"standard").get("ok",false) and loaded.source_for("steward")==receipt(),"另一世界再次达成仍保留首次来源")
	var invalid: Dictionary=observation();invalid.cards[0].acquired.tasks[1].task_id="task:1"
	check(not loaded.observe(invalid,"standard").get("ok",true) and loaded.source_for("steward")==receipt(),"同一任务不能重复计成两次")
	invalid=observation();invalid.cards[0].acquired.earner_actor_id="other"
	check(not loaded.observe(invalid,"standard").get("ok",true),"他人的完成来源不被当前本人观察接受")
	invalid=observation();invalid.cards[0].acquired.world_id="02000000000000000000000000000000"
	check(not loaded.observe(invalid,"standard").get("ok",true),"旧世界完成源与当前页面不匹配时拒绝")
	invalid=observation();invalid.cards[0].acquired.acquired_tick=11
	check(not loaded.observe(invalid,"standard").get("ok",true),"未来或未对应末次完成时刻不能授资格")
	var free=Profile.new(folder.path_join("free.json"));free.load_profile()
	check(free.observe(initial,"free").get("ok",false) and free.source_for("steward").is_empty() and free.source_for("steward",true)==receipt() and free.unlocked_actor_ids()==["steward"],"自由创建来源单列而不冒普通取得")
	check(free.save_profile().get("ok",false),"自由来源真实保存")
	var partial=Profile.new(folder.path_join("partial.json"));partial.load_profile()
	var mixed: Dictionary=observation();var bad_row: Dictionary=mixed.cards[0].duplicate(true);bad_row.card_id="bad";bad_row.target_actor_id="different";mixed.cards.append(bad_row)
	check(not partial.observe(mixed,"standard").get("ok",true) and partial.unlocked_actor_ids().is_empty() and not partial.needs_save(),"同一观察中后项错误不留下前项的半份资格")
	var discovered_path: String=folder.path_join("discovery.json")
	var discovered=FileAccess.open(discovered_path,FileAccess.WRITE);discovered.store_string(JSON.stringify({"version":1,"discovered_card_ids":["pc03"],"free_discovered_card_ids":[]}));discovered.close()
	var discovery_as_unlock=Profile.new(discovered_path)
	check(not discovery_as_unlock.load_profile().get("ok",true) and discovery_as_unlock.unlocked_actor_ids().is_empty(),"卡片发现文件不能被当成人物解锁文件")
	var foreign=FileAccess.open(path,FileAccess.WRITE);foreign.store_string("broken original");foreign.close()
	check(not loaded.save_profile().get("ok",true) and FileAccess.get_file_as_string(path)=="broken original","已被外部更改的资格档拒绝覆盖")
	var broken=Profile.new(path)
	check(not broken.load_profile().get("ok",true) and not broken.observe(initial,"standard").get("ok",true) and not broken.save_profile().get("ok",true) and FileAccess.get_file_as_string(path)=="broken original","损坏原件不按空档补写")
	print("ACTOR_UNLOCK_PROFILE ",checks," checks ",failures," failures; WORLD_MAIN_NOT_RUN; originals=",folder)
	quit(1 if failures else 0)
