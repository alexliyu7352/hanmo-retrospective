## 新个人作者可选字段跨正式目录/Kernel/v2与公开预览的一次接缝，不复跑owner战斗与AI矩阵。
extends SceneTree
const Kernel=preload("res://core/world_kernel.gd")
const Codec=preload("res://core/save_codec.gd")
const Interaction=preload("res://core/interaction_rules.gd")
const Explain=preload("res://core/runtime/explanation_query.gd")
var checks: int=0
var failures: int=0

## 无界面、无用户槽，只运行准备器生成的新锁小目录。
func _init() -> void:
	call_deferred("run")

## 每项断言影响退出码，前置失败不继续解引用。
func check(value: bool, label: String) -> bool:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)
	return value

## 原请求身份和去重规则保持唯一权威。
func command(k, kind: String, params: Dictionary={}) -> Dictionary:
	var s: Dictionary=k.snapshot();var actor: String=s.player_actor_id
	return {"kind":kind,"params":params,"actor_id":actor,"expected_revision":s.revision,"request_seq":s.request_sequences[actor]+1}

## 正式初值与技能进入战斗，查询零副作用，一轮后全状态冷恢复并保持原请求结果。
func run() -> void:
	var index: Dictionary=JSON.parse_string(FileAccess.get_file_as_string("res://data/generated/installed-content.json"))
	var path: String="res://data/generated/"+index.sets[0].directory+"/catalog.hmcatalog.json"
	var k=Kernel.new();var made: Dictionary=k.new_game("base::scenario.procurement",path)
	if not check(made.ok,"新typed个人字段正式目录启动 "+str(made)): finish();return
	var s: Dictionary=k.snapshot();var actor: String=s.player_actor_id
	if not check(k.submit(command(k,"configure_heart",{"skill_id":"base::skill.heart"})).ok,"原合法心法配置入口"): finish();return
	var context: Dictionary=Interaction.for_role(s,k.content().data,"base::role.office")
	var entered: Dictionary=k.submit(command(k,"duel_start",{"context":context}))
	if not check(entered.ok,"真实角色切磋入口 "+str(entered)): finish();return
	s=k.snapshot();var battle: Dictionary=s.battles.values()[0]
	check(battle.units[actor].stamina==70 and battle.units["base::actor.chen"].stamina==70,"显式初始气势只在真实入战固定")
	var params: Dictionary={"battle_id":battle.id,"action":"skill","skill_id":"base::skill.area","target_id":"base::actor.chen","destination":battle.units[actor].position.duplicate(),"heading":0}
	var quote: Dictionary=k.preview("battle_plan",params)
	if not check(quote.ok,"同一正常计划可预览 "+str(quote)): finish();return
	check(quote.tactical_preview.attack.targets.size()==1 and quote.tactical_preview.effective_weapon.maximum_range==200 and quote.tactical_preview.movement_distance==0,"正式射程/区域/原地投影来自同一合法规则 "+str(quote.tactical_preview))
	check(k.preview("battle_plan",params)==quote and k.snapshot()==s,"公开查询不抽duel、不改AI或草案")
	var saved: Dictionary=k.save_context();var decoded: Dictionary=Codec.decode(Codec.encode(saved.state,1,"personal-planning",saved.catalog,saved.registry),saved.catalog,saved.registry)
	if not check(decoded.ok and decoded.state==s,"真实保存AI/初值新锁v2 "+str(decoded.get("code",""))): finish();return
	var recovered=Kernel.new();var restored: Dictionary=recovered.restore_with_catalog(decoded.state,saved.catalog,saved.registry)
	if not check(restored.ok and recovered.snapshot()==s,"另一Kernel只验证已保存规划不重抽"): finish();return
	var request: Dictionary=command(recovered,"battle_plan",params);var applied: Dictionary=recovered.submit(request)
	if not check(applied.ok,"实际同步一轮 "+str(applied)): finish();return
	var after: Dictionary=recovered.snapshot()
	check(after.rng.duel!=s.rng.duel and recovered.submit(request)==applied and recovered.snapshot()==after,"正常命中抽流一次提交且相同请求不再结算")
	saved=recovered.save_context();decoded=Codec.decode(Codec.encode(saved.state,2,"personal-after",saved.catalog,saved.registry),saved.catalog,saved.registry)
	check(decoded.ok and decoded.state==after,"下一轮新模型/AI及原人物全Schema存读")
	var known: Dictionary={"kind":"personal_battle","tick":1,"participants":[actor,"other"],"withdrawn":["other"],"outcome":"victory"}
	check(Explain._fact_projection(known,true).get("withdrawn")==["other"] and not Explain._fact_projection(known,false).has("withdrawn"),"仅既有亲历层解释撤离，不扩大传闻可见性")
	finish()

## 只报告本门接缝结果；真实章节通关和集中GUI依旧另验。
func finish() -> void:
	print("PERSONAL_MODELS_KERNEL_CHECKS=",checks," FAILURES=",failures)
	quit(0 if failures==0 else 1)
