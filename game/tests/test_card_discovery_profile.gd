## 跨局图鉴独立档的真实文件门；只写本测试专属路径，不读取世界银行。
extends SceneTree
const Profile=preload("res://core/card_discovery_profile.gd")
var checks: int=0
var failures: int=0

## 每个断言均保留普通、自由和局内能力之间的明确边界。
func check(value: bool, message: String) -> void:
	checks+=1
	if not value: failures+=1; printerr("FAIL "+message)

## 观察输入来自有限本人卡片页，未取得的已发现卡片不能被重复记录为新来源。
func view(id: String, acquired: bool=true) -> Dictionary:
	return {"actor_id":"person","cards":[{"card_id":id,"acquired_this_world":acquired,"learned":acquired,"discovered":true}]}

## 使用专属文件实际保存和新对象读取，坏档原文在失败后仍保持不变。
func _initialize() -> void:
	var path: String="user://profile-tests/discovery-"+str(OS.get_process_id())+".json"
	# 容器会复用小PID，已有本测试原件保留并选择下一独占文件。
	var suffix: int=0
	while FileAccess.file_exists(path):
		suffix+=1; path="user://profile-tests/discovery-"+str(OS.get_process_id())+"-"+str(suffix)+".json"
	var journal=Profile.new(path)
	check(journal.load_profile().ok and journal.discovered_ids().is_empty(),"missing profile starts an empty discovery record")
	check(journal.observe(view("card.normal"),"standard").changed,"actual standard acquisition records discovery")
	check(not journal.observe(view("card.normal"),"standard").changed,"repeated source observation is idempotent")
	check(not journal.observe(view("card.seen",false),"standard").changed and not "card.seen" in journal.discovered_ids(),"discovery-only card is not a new acquisition")
	check(journal.observe(view("card.free"),"free").changed and journal.save_profile().ok,"free acquisition stores separately")
	var restored=Profile.new(path)
	check(restored.load_profile().ok and restored.discovered_ids()==["card.free","card.normal"],"new profile object restores exact cross-run discoveries")
	var decorated: Dictionary=restored.decorate(view("card.free",false))
	check(decorated.cards[0].discovered_free and not decorated.cards[0].learned and not decorated.cards[0].acquired_this_world,"free discovery never becomes a current-world ability")
	check(restored.observe(view("card.free"),"standard").changed and restored.save_profile().ok,"normal acquisition can later give same card a normal record")
	check(not restored.decorate(view("card.free",false)).cards[0].discovered_free,"normal and free sources remain distinguishable")
	var malformed=view("bad"); malformed.cards[0].learned=false
	check(not restored.observe(malformed,"standard").ok and not restored.observe(view("bad"),"unrecognized").ok,"invalid source and unknown creation mode cannot write discovery")
	var file=FileAccess.open(path,FileAccess.WRITE); file.store_string("{broken-profile"); file.close()
	check(not restored.save_profile().ok,"loaded old instance cannot overwrite externally changed discovery")
	var broken=Profile.new(path)
	check(not broken.load_profile().ok and not broken.save_profile().ok,"unreadable profile cannot be overwritten as empty")
	file=FileAccess.open(path,FileAccess.READ)
	check(file.get_as_text()=="{broken-profile","failed recovery preserves original corrupt bytes"); file.close()
	check(not Profile.new("res://world-bank.json").load_profile().ok,"discovery service refuses resource or bank paths")
	print("CARD_DISCOVERY_PROFILE %d checks %d failures"%[checks,failures]); quit(1 if failures else 0)
