## 成功提交后图鉴同步的有限查询与真实文件门，不调用世界命令或整态导出。
extends SceneTree
const Controller=preload("res://ui/growth_cards_controller.gd")
const Profile=preload("res://core/card_discovery_profile.gd")
var checks: int=0
var failures: int=0

## 失败保持具体同步语义，不以查询次数代替真实来源判断。
func check(value: bool, message: String) -> void:
	checks+=1
	if not value: failures+=1; printerr("FAIL "+message)

## 模拟已由正式查询投影的一条真实取得，不包含可授能力的写接口。
func row(id: String) -> Dictionary: return {"card_id":id,"acquired_this_world":true,"learned":true}

## 同一源分页完整成功后才保存；失败、换人、变revision和落盘失败均保留可重试边界。
func _initialize() -> void:
	var path: String="user://controller-tests/discovery-"+str(OS.get_process_id())+".json"
	# 容器会复用小PID，已有本测试原件保留并选择下一独占文件。
	var suffix: int=0
	while FileAccess.file_exists(path):
		suffix+=1; path="user://controller-tests/discovery-"+str(OS.get_process_id())+"-"+str(suffix)+".json"
	var controller=Controller.new(path)
	var identity: Dictionary={"ok":true,"actor_id":"person","revision":1,"world_id":"world"}
	var context: Callable=func(): return identity
	var query: Callable=func(args: Dictionary): return {"actor_id":"person","offset":args.offset,"cards":[row("card."+str(args.offset))],"next_offset":1.0 if args.offset==0 else null}
	var result: Dictionary=controller.sync_after_commit(query,context,"standard")
	check(result.ok and result.changed and controller.profile.discovered_ids()==["card.0","card.1"],"complete same-source pages store actual discoveries")
	check(not controller.profile.needs_save(),"successful sync clears pending disk write")
	result=controller.sync_after_commit(query,context,"standard")
	check(result.ok and not result.changed,"same completed source is idempotent")
	var failed: Callable=func(args: Dictionary): return {"actor_id":"person","offset":0,"cards":[row("card.partial")],"next_offset":1} if args.offset==0 else {"ok":false,"code":"ReadFailed"}
	check(not controller.sync_after_commit(failed,context,"standard").ok and not "card.partial" in controller.profile.discovered_ids(),"failed later page cannot partially observe discoveries")
	var changed: Callable=func(args: Dictionary):
		identity.revision+=1
		return {"actor_id":"person","offset":args.offset,"cards":[row("card.stale")],"next_offset":null}
	check(not controller.sync_after_commit(changed,context,"standard").ok and not "card.stale" in controller.profile.discovered_ids(),"query changing aliased identity cannot mutate bound snapshot")
	var wrong_actor: Callable=func(_args: Dictionary): return {"actor_id":"other","offset":0,"cards":[row("card.other")],"next_offset":null}
	check(not controller.sync_after_commit(wrong_actor,context,"standard").ok,"page for another actor is rejected")
	var wrong_offset: Callable=func(_args: Dictionary): return {"actor_id":"person","offset":0,"cards":[],"next_offset":1}
	check(not controller.sync_after_commit(wrong_offset,context,"standard").ok,"nonprogressing empty page is rejected")
	var unavailable: Callable=func(_args: Dictionary): return {"ok":false,"code":"CardsUnavailable"}
	result=controller.sync_after_commit(unavailable,context,"standard")
	check(result.ok and not result.supported and controller.profile.discovered_ids().size()==2,"legacy world keeps prior discoveries without silently enabling cards")
	var file=FileAccess.open(path,FileAccess.READ); var before: String=file.get_as_text(); file.close()
	file=FileAccess.open(path,FileAccess.WRITE); file.store_string("external edit"); file.close()
	var extra: Callable=func(_args: Dictionary): return {"actor_id":"person","offset":0,"cards":[row("card.retry")],"next_offset":null}
	result=controller.sync_after_commit(extra,context,"standard")
	check(not result.ok and controller.profile.needs_save(),"failed profile save retains pending observation")
	file=FileAccess.open(path,FileAccess.WRITE); file.store_string(before); file.close()
	result=controller.sync_after_commit(extra,context,"standard")
	var restored=Profile.new(path)
	check(result.ok and not result.changed and restored.load_profile().ok and "card.retry" in restored.discovered_ids(),"retry writes pending discovery even when there is no newly observed card")
	print("GROWTH_CARDS_CONTROLLER %d checks %d failures"%[checks,failures]); quit(1 if failures else 0)
