## 验证共用历法的真实边界与生产HUD/手记文本；日期投影不改世界、身份或旧见闻。
extends SceneTree
const HudView=preload("res://core/views/hud_view.gd")
const TimeRules=preload("res://core/base/time_rules.gd")
const Identity=preload("res://core/actor_identity.gd")
const Family=preload("res://core/family_rules.gd")
const Host=preload("res://ui/main.gd")
const Screen=preload("res://ui/screen_composer.gd")
const Notes=preload("res://ui/journey_notes.gd")
var checks: int=0
var failures: int=0

## 在引擎就绪后检查纯查询及真正用于玩家界面的日期控件。
func _init() -> void:
	call_deferred("run_checks")

## 每个边界失败均影响退出码，不能仅凭引擎成功启动宣布通过。
func check(value: bool, label: String) -> void:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)

## 日序、生日和期限都使用独立字面预期，不借待测查询自己生成答案。
func check_boundaries() -> void:
	check(TimeRules.TICKS_PER_YEAR==720 and TimeRules.TICKS_PER_DAY==2 and Identity.TICKS_PER_YEAR==720,"保留720/2历法与旧身份常量别名")
	for row in [[0,0,0],[1,0,1],[719,359,1],[720,360,0],[2147483647,1073741823,1]]:
		check(TimeRules.day_index(row[0])==row[1] and TimeRules.half_day(row[0])==row[2],"世界日序和半日边界："+str(row[0]))
	for row in [[0,0,0],[719,0,0],[720,0,1],[0,-720,1],[719,-720,1],[720,-720,2],[2147483647,-2147483647,5965232]]:
		check(TimeRules.age_years(row[0],row[1])==row[2],"负生日与整岁边界："+str(row))
	for row in [[9,10,true,true],[10,10,true,false],[11,10,false,false],[0,0,true,false],[2147483647,2147483647,true,false]]:
		check(TimeRules.deadline_met(row[0],row[1])==row[2] and TimeRules.deadline_met(row[0],row[1],true)==row[2] and TimeRules.deadline_met(row[0],row[1],false)==row[3],"期限相等时明确包含或排除："+str(row))
	var state: Dictionary={"tick":720,"actors":{"known":{"birth_tick":0},"unknown":{}}}
	check(Identity.age_years(state,"known")==1 and Family._age_at(state,"known",719)==0 and Family._age_at(state,"known",720)==1,"身份与历史资格都沿用共享生日边界")
	check(Identity.age_years(state,"unknown")==-1 and Identity.age_years(state,"missing")==-1 and Family._age_at(state,"unknown",720)==-1,"缺出生字段及未知人物仍返回负一")
	for birth in [null,true,0.5,"0",721,-2147483648]:
		state.actors.known.birth_tick=birth
		var before: Dictionary=state.duplicate(true)
		check(Identity.age_years(state,"known")==-1 and Family._age_at(state,"known",720)==-1 and state==before,"调用方继续拒绝非法生日且无回写："+str(birth))

## 调用生产HUD和手记，以准确日期文本核对世界原点、跨年半日及旧消息获知日期。
func check_projections() -> void:
	var host=Host.new(); root.add_child(host)
	await process_frame
	var started: Dictionary=host.start_content("scenario.alpha_life","res://data/generated/alpha_life.hmcatalog.json")
	check(started.ok,"正式生活内容可建立同一世界")
	if not started.ok: host.queue_free(); await process_frame; return
	var original: Dictionary=host.kernel.snapshot()
	var data_before: Dictionary=host._data.duplicate(true)
	for row in [[0,"第1日上半日"],[1,"第1日下半日"],[719,"第360日下半日"],[720,"第361日上半日"]]:
		var state: Dictionary=original.duplicate(true); state.tick=row[0]
		var actor: Dictionary=state.actors[state.player_actor_id]
		var before: Dictionary=state.duplicate(true)
		var stage=Control.new(); root.add_child(stage)
		Screen.hud(host,stage,HudView.query(state,host._data))
		var status: Label=stage.find_child("WorldStatus",true,false)
		var expected: String=row[1]+"　%s　 钱%d　体力%d"%[host._name(host._actor(actor.id)),state.wallets[actor.wallet_id].balance,actor.hp]
		check(status!=null and status.text==expected,"真实HUD日期文本："+row[1])
		check(Notes.describe(state,host._data).split("\n")[1]==row[1],"手记与HUD同日同半日："+row[1])
		check(state==before,"读取日期不会推进或修补世界："+str(row[0]))
		stage.queue_free(); await process_frame
	var state: Dictionary=original.duplicate(true); state.tick=720
	var actor_id: String=state.player_actor_id
	state.social.events["calendar-memory"]={"kind":"refusal","actor_id":actor_id,"target_actor_id":"actor.apprentice"}
	for row in [[0,"第1日"],[1,"第1日"],[719,"第360日"],[720,"第361日"]]:
		state.social.memories[actor_id]={"calendar-memory":{"learned_tick":row[0],"source_actor_id":actor_id}}
		var before: Dictionary=state.duplicate(true)
		var lines: Array[String]=Notes.social_notes(state,host._data,actor_id)
		check(lines.size()==2 and lines[1].ends_with("（"+row[1]+" · 亲历）"),"旧消息按首次获知日期显示："+str(row[0]))
		check(state==before,"见闻投影不重写历史日期")
	check(host.kernel.snapshot()==original and host._data==data_before,"全部投影保留正式世界及作者目录原样")
	host.queue_free(); await process_frame

## 汇总真实断言；本测试不宣称跨年世界推进、图形输入或完整成长验收。
func run_checks() -> void:
	check_boundaries()
	await check_projections()
	print("TIME_RULES_CHECKS=",checks," FAILURES=",failures)
	quit(0 if failures==0 else 1)
