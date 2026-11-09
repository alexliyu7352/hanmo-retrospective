## 原Personal.preview的可玩只读出口：当前目标/范围/命中与已发生结果，不携带AI计划或额外权威状态。
extends SceneTree
const Personal=preload("res://core/personal_combat.gd")
const Tactical=preload("res://core/domains/personal/personal_projection.gd")
const Round=preload("res://core/personal_round.gd")
const AreaFixture=preload("res://tests/test_personal_area_strike.gd")
const StrikeFixture=preload("res://tests/test_personal_strike_conditions.gd")
const CounterFixture=preload("res://tests/test_personal_counter.gd")
const ExitFixture=preload("res://tests/test_personal_escort_exit.gd")
const NpcFixture=preload("res://tests/test_personal_npc_withdrawal.gd")
const Source=preload("res://tests/test_personal_condition_snapshot.gd")
var checks: int=0
var failures: int=0

## 检查公开报价的兼容字段与新增可选信息；坏输入仍使用原拒绝出口。
func check(value: bool, label: String) -> void:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)

## 只构造原preview需要的已存在会话，世界资产与命令结算在另一个真实入口用例核验。
static func wrapper(battle: Dictionary) -> Dictionary:
	battle.id="battle.preview"
	var actors: Dictionary={}
	for id in battle.units: actors[id]={"alive":true}
	return {"actors":actors,"battles":{battle.id:battle},"rng":{"duel":{"seed":"11","state":"23"}}}

## 报价仍通过现battle_plan入口，不发明新动作或把只读字段塞进提交权限。
static func preview(state: Dictionary, data: Dictionary, intent: Dictionary) -> Dictionary:
	var params: Dictionary=intent.duplicate(true); params.battle_id="battle.preview"
	return Personal.preview(state,data,"defender","battle_plan",params)

## 覆盖新范围/命中、退出过滤、结果语义、权限与不读取对方未来意图。
func _initialize() -> void:
	var data: Dictionary=AreaFixture.content(); data.required_capabilities=["personal_combat"]; data.personal_objectives.spar.hit_model="skill_v1"
	var battle: Dictionary=AreaFixture.battle()
	for unit in battle.units.values(): unit.skill_levels={"martial":2,"archery":0}
	var state: Dictionary=wrapper(battle); var intent: Dictionary=AreaFixture.plan(battle); var before: Dictionary=state.duplicate(true)
	var quote: Dictionary=preview(state,data,intent)
	check(quote.ok and quote.plan==intent and quote.duration==0 and quote.battle_id==battle.id,"existing public plan quote fields remain compatible")
	var view: Dictionary=quote.tactical_preview
	check(view.ok and view.attack.targets==[{"target_id":"b.near","damage":17,"hit_bp":7500},{"target_id":"z.primary","damage":17,"hit_bp":7500}],"public preview carries exact current area set and known hit chances")
	check(view.nominal_stamina_cost==25 and view.attackable_target_ids==["a.far","b.near","z.primary"],"one nominal skill cost and geometrically valid selectable primaries are exposed")
	check(view.basis=="current_snapshot_candidate" and view.may_change_after_simultaneous_movement,"projection does not claim future enemy positions or committed damage")
	check(state==before,"preview leaves HP stamina random state and source objects unchanged")
	var hidden: Dictionary=state.duplicate(true); hidden.battles[battle.id].ai_plans={"hidden":{"future_destination":[2,3],"secret":"not_public"}}; hidden.battles[battle.id].player_draft={"hidden":"not_public"}
	check(preview(hidden,data,intent)==quote,"opponent committed plans and arbitrary drafts cannot influence tactical projection")
	check(not JSON.stringify(quote).contains("not_public") and not JSON.stringify(quote).contains("ai_plans"),"public response does not serialize hidden plans")
	var params: Dictionary=intent.duplicate(true); params.battle_id=battle.id
	var denied: Dictionary=Personal.preview(state,data,"b.near","battle_plan",params)
	check(denied.get("code")=="NotBattleController" and not denied.has("tactical_preview"),"existing controller authority rejects before projection")
	battle.round=2; battle.units["b.near"].withdrawn_round=1; data.personal_objectives.spar.allow_withdrawal=true; data.personal_battlefields.yard.exits=[{"side":1,"position":[300,200],"radius":25}]
	quote=preview(state,data,intent)
	check(quote.ok and quote.tactical_preview.selectable_target_ids==["a.far","z.primary"] and quote.tactical_preview.attack.targets[0].target_id=="a.far","departed positive-HP enemy is not selectable or an area target")
	intent.target_id="b.near"
	check(preview(state,data,intent).get("code")=="InvalidTarget","stale UI target remains rejected by actual command after departure")
	data=StrikeFixture.content(); data.required_capabilities=["personal_combat"]; battle=StrikeFixture.battle(); state=wrapper(battle)
	intent={"destination":[250,200],"heading":0,"action":"skill","target_id":"attacker","skill_id":"long"}
	quote=preview(state,data,intent)
	check(quote.ok and quote.tactical_preview.effective_weapon.minimum_range==110 and quote.tactical_preview.effective_weapon.maximum_range==300 and quote.tactical_preview.effective_weapon.arc_degrees==85,"effective skill outer range is exposed without erasing inner zone or arc")
	battle.units.attacker.position=[410,200]; intent.skill_id="aim"; quote=preview(state,data,intent)
	check(quote.ok and quote.tactical_preview.requires_stationary and quote.tactical_preview.movement_distance==0,"stationary skill exposes same zero movement rule to original input adapter")
	intent={"destination":[250,200],"heading":0,"action":"defend","target_id":"","skill_id":""}; quote=preview(state,data,intent)
	check(quote.ok and quote.tactical_preview.movement_distance==0 and quote.tactical_preview.nominal_stamina_cost==0,"normal defend projection passes action into shared movement query")
	intent={"destination":[250,200],"heading":0,"action":"attack","target_id":"attacker","skill_id":""}; battle.units.attacker.position=[500,200]; quote=preview(state,data,intent)
	check(quote.ok and not quote.tactical_preview.attack.ok and quote.tactical_preview.attack.code=="OutOfRange","current geometric failure is advisory rather than rejecting otherwise lawful simultaneous plan")
	data=CounterFixture.content(); battle=CounterFixture.resolve(CounterFixture.battle(),data).battle
	var actual: Array=Tactical.events(battle); var found: bool=false
	for event in actual:
		if event.action=="counter" and event.damage==8: found=true
	check(found,"actual counter is projected during following planning round")
	battle.last_events.append({"action":"debug_ai","private_plan":{"secret":"not_public"}})
	battle.last_events[0].secret="not_public"
	check(not JSON.stringify(Tactical.events(battle)).contains("not_public"),"event projection strips unknown action and unknown internal fields")
	data=ExitFixture.content(); battle=ExitFixture.battle(); battle.round=20; battle.units["protected"].position=[450,300]
	battle=ExitFixture.step(battle,data).battle
	var result_view: Dictionary=Tactical.result(battle,data)
	check(result_view.ok and result_view.reason_codes==["EscortExitTimeout"] and result_view.objective.goal_rule=="protected_exit" and result_view.objective.timeout_rule=="protected_exit_or_truce","result clearly names explicit escort goal and timeout truce instead of generic defeat-opponents")
	data=Source.data(); data.personal_objectives.spar.max_rounds=1; state=Source.state(); state.facts={}; state.tasks={}
	quote=Personal.preview(state,data,"a","duel_start",Source.params()); Personal.begin(state,data,quote); battle=state.battles.values()[0]
	params={"battle_id":battle.id,"destination":battle.units.a.position.duplicate(),"heading":0,"action":"wait"}
	quote=Personal.preview(state,data,"a","battle_plan",params)
	var canonical: Dictionary=state.duplicate(true); Personal.apply(canonical,data,"a","battle_plan",quote.duplicate(true))
	quote.tactical_preview={"attack":{"damage":999999},"nominal_stamina_cost":0,"ai_plans":{"forged":{}}}
	check(quote.ok and Personal.apply(state,data,"a","battle_plan",quote).ok and state==canonical,"forged advisory values cannot alter actual round, cost or authority")
	before=state.duplicate(true); quote=Personal.preview(state,data,"a","battle_close",{"battle_id":battle.id})
	check(quote.ok and quote.duration==0 and quote.battle_id==battle.id and quote.result_preview.ok,"original close preview carries optional actual result projection")
	check(state==before and quote.result_preview.withdrawn_actor_ids==[] and not quote.result_preview.has("ai_plans"),"result preview cannot settle twice or disclose future intentions")
	data=NpcFixture.content(); battle=NpcFixture.battle(false); NpcFixture.commit(battle,data); battle=NpcFixture.resolve(battle,data).battle
	result_view=Tactical.result(battle,data)
	check(result_view.withdrawn_actor_ids==["attacker"] and result_view.defeated_actor_ids==[] and result_view.participants[0].status=="withdrawn" and result_view.participants[0].hp==25,"result explicitly distinguishes living NPC departure from defeated or dead identity")

	print("PERSONAL_TACTICAL_PROJECTION checks=",checks," failures=",failures)
	quit(1 if failures else 0)
