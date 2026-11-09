## 成功提交后同源见闻分页进入原图鉴；错页或换世界不保存任何部分发现。
extends SceneTree
const Controller=preload("res://ui/growth_cards_controller.gd")
const Samples=preload("res://tests/test_insight_discovery_profile.gd")
var checks: int=0
var failures: int=0
func check(value: bool,message: String) -> void:
	checks+=1
	if not value: failures+=1;printerr("FAIL "+message)
func _initialize() -> void:
	var original: Dictionary=Samples.view()
	var identity: Dictionary=original.source_identity.duplicate(true);identity["ok"]=true;identity["actor_id"]="person"
	var context: Callable=func(): return identity
	var path: String="user://insight-controller-"+str(Time.get_unix_time_from_system()).replace(".","-")+".json"
	var controller=Controller.new(path)
	var query: Callable=func(args: Dictionary):
		var page: Dictionary=original.duplicate(true);page["offset"]=args.offset;page["next_offset"]=null;return page
	var result: Dictionary=controller.sync_insights_after_commit(query,context,"standard")
	check(result.ok and result.changed and controller.profile.discovered_ids()==["VI04"],"same-source actual insight reaches existing profile")
	check(not controller.sync_insights_after_commit(query,context,"standard").changed,"repeated successful frame remains idempotent")
	var wrong: Callable=func(args: Dictionary):
		var page: Dictionary=query.call(args);page.source_identity.execution_epoch+=1;return page
	check(controller.sync_insights_after_commit(wrong,context,"standard").get("code")=="CardSourceChanged","another execution epoch cannot supply current observation")
	var changing: Callable=func(args: Dictionary):
		var page: Dictionary=query.call(args);identity.revision+=1;return page
	check(controller.sync_insights_after_commit(changing,context,"standard").get("code")=="CardSourceChanged","identity change after query rejects observation")
	identity.revision-=1
	var failed: Callable=func(args: Dictionary):
		if args.offset>0: return {"ok":false,"code":"ReadFailed"}
		var page: Dictionary=query.call(args);page.cards[0].card_id="partial";page.next_offset=1;return page
	check(not controller.sync_insights_after_commit(failed,context,"standard").ok and not "partial" in controller.profile.discovered_ids(),"later-page failure never writes partial experience")
	var disabled: Callable=func(_args: Dictionary): return {"ok":false,"code":"InsightCardsUnavailable"}
	result=controller.sync_insights_after_commit(disabled,context,"standard")
	check(result.ok and not result.supported and controller.profile.discovered_ids()==["VI04"],"old world without profile preserves existing discovery")
	var before: String=FileAccess.get_file_as_string(path)
	check(not controller.sync_insights_after_commit(query,context,"unknown").ok and FileAccess.get_file_as_string(path)==before,"unknown creation mode cannot silently write standard discovery")
	print("INSIGHT_CARDS_CONTROLLER %d checks %d failures"%[checks,failures]);quit(1 if failures else 0)
