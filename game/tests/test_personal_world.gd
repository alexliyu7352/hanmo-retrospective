## 从真实作者目录和人物对白进入个人战，验证同一世界的收费、占用、读档与终局回写。
extends SceneTree
const Kernel=preload("res://core/world_kernel.gd")
const F=preload("res://tests/companion_fixture.gd")
const Round=preload("res://core/personal_round.gd")
const Codec=preload("res://core/save_codec.gd")
const Sessions=preload("res://core/combat_session.gd")
var checks: int=0
var failures: int=0

## 内容装载完成后执行正常命令流程；无图形结论不计入本脚本。
func _init() -> void:
	call_deferred("run_checks")

## 保存具体失败原因，不能用进程成功掩盖领域失败。
func check(value: bool, label: String) -> void:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)

## 与实际玩家入口相同地读取扩展生活包，保留旧城市和旅行人物。
func fresh():
	var k=Kernel.new()
	var result: Dictionary=k.new_game("scenario.alpha_life","res://data/generated/alpha_life.hmcatalog.json")
	check(result.ok,"集成生活包可创建："+str(result))
	return k

## 完成当面切磋流程，并用保存的整轮意图核验重复查询、重启和永久结算。
func run_checks() -> void:
	var k=fresh()
	if failures: finish(); return
	var player: String=k.snapshot().player_actor_id
	check(k.preview("duel_start",{"context":F.context(k,"role.wei")}).get("code")=="NotAtPlace","远方武师不提供全局切磋")
	check(F.send(k,"move_local",{"place_id":"west.training"}).ok,"实际走入武馆")
	check(F.send(k,"talk",{"role_id":"role.wei"}).ok,"实际武师对白可开始")
	check(F.send(k,"dialogue_next").ok and F.send(k,"dialogue_choose",{"option_id":"spar"}).ok,"当面选择切磋约定")
	var before: Dictionary=k.snapshot()
	var preview: Dictionary=k.preview("dialogue_confirm")
	check(preview.ok and k.preview("dialogue_confirm")==preview and before==k.snapshot(),"询价预览不扣费不推进时间")
	var start: Dictionary=F.send(k,"dialogue_confirm")
	check(start.ok,"对白确认真实进入战斗："+str(start))
	if not start.ok: finish(); return
	var state: Dictionary=k.snapshot(); var b: Dictionary=Sessions.for_actor(state,player)
	check(state.tick==before.tick+1 and state.wallets["wallet.player"].balance==before.wallets["wallet.player"].balance-12 and state.wallets["wallet.wei"].balance==before.wallets["wallet.wei"].balance+12,"一场切磋只推进所属半日且费用实转给授艺人")
	check(b.units.keys().size()==2 and b.units.has("actor.wei") and not state.interactions.has(player),"参战者为同一世界人物，对话让位于真实战斗")
	check(k.preview("move_local",{"place_id":"west.street"}).get("code")=="BattlePending" and k.preview("wait",{},"actor.wei").get("code")=="BattlePending","战中控制者和服务提供者不能另做活动")
	check(not F.send(k,"battle_plan",{"battle_id":b.id},"actor.chen").ok,"局外人不能代替控制者提交计划")
	var encoded: String=Codec.encode(state,1,"personal-world",k.content())
	var decoded: Dictionary=Codec.decode(encoded,k.content())
	check(not encoded.is_empty() and decoded.ok and decoded.state==state,"稳定轮间全状态经过真实存档编解码")
	var damaged: Dictionary=state.duplicate(true)
	damaged.battles[b.id].ai_plans["actor.wei"].heading=7
	check(not k.restore(damaged).ok and k.snapshot()==state,"篡改已保存AI意图拒绝且保留旧世界")
	var rounds: int=0
	while b.phase=="planning" and rounds<26:
		var policy: Dictionary=b.duplicate(true); policy.controller_id="actor.wei"
		var plan: Dictionary=Round.make_ai(policy,k.content().data)[player]
		plan.battle_id=b.id
		var saved: Dictionary=k.snapshot()
		var step: Dictionary=F.send(k,"battle_plan",plan)
		check(step.ok,"实际世界提交完整轮次："+str(step))
		if not step.ok: break
		var first: Dictionary=k.snapshot()
		check(k.restore(saved).ok and F.send(k,"battle_plan",plan)==step and k.snapshot()==first,"相同存档意图恢复后产生相同结果")
		b=Sessions.for_actor(k.snapshot(),player); rounds+=1
	state=k.snapshot()
	check(b.phase=="result" and b.applied and state.facts.has(b.id),"正常战斗结束建立永久事实")
	if b.phase=="result":
		check(state.tick==before.tick+1 and state.wallets["wallet.player"].balance==before.wallets["wallet.player"].balance-12,"轮数不额外推进世界半日，不从面板刷取钱财")
		check(state.actors[player].hp==maxi(1,b.units[player].hp) and state.actors["actor.wei"].hp==maxi(1,b.units["actor.wei"].hp),"双方伤势回到原人物而非战斗替身")
		check(k.restore(state).ok and k.snapshot()==state,"未关闭结果恢复不重复扣费或加伤")
		var saved_result: Dictionary=Codec.decode(Codec.encode(state,2,"personal-world",k.content()),k.content())
		check(saved_result.ok and k.restore(saved_result.state).ok,"结果经过排序JSON重启后永久参与者事实仍匹配")
		check(F.send(k,"battle_close",{"battle_id":b.id}).ok and not F.send(k,"battle_close",{"battle_id":b.id}).ok,"结果只关闭一次")
		check(F.send(k,"move_local",{"place_id":"west.street"}).ok,"终局后能继续原城市生活")
	check_legacy()
	check_growth()
	finish()

## 当面受教后分段修炼，再向真实在场人物传授；学习中与完成后均经过完整存档检查。
func check_growth() -> void:
	var k=fresh()
	if k.snapshot().is_empty(): return
	F.send(k,"move_local",{"place_id":"west.training"})
	F.send(k,"talk",{"role_id":"role.wei"}); F.send(k,"dialogue_next")
	F.send(k,"dialogue_choose",{"option_id":"learn0"})
	var learned: Dictionary=F.send(k,"dialogue_confirm")
	check(learned.ok,"真实授课动作开始："+str(learned))
	if not learned.ok: return
	var s: Dictionary=k.snapshot(); var player: String=s.player_actor_id
	check(s.wallets["wallet.player"].balance==765 and s.wallets["wallet.wei"].balance==335 and not s.actors[player].combat.learned.has("skill.cleave"),"学习先实际付费，占用时间结束前不授予武功")
	check(not F.send(k,"move_local",{"place_id":"west.street"},"actor.wei").ok,"授课教师不能同时离馆")
	check(k.advance_one_tick().ok,"学习推进第一个半日")
	var saved: Dictionary=Codec.decode(Codec.encode(k.snapshot(),0,"training-world",k.content()),k.content())
	check(saved.ok and k.restore(saved.state).ok,"学习中真实保存恢复保留双方占用与收费事实")
	check(k.advance_one_tick().ok and k.snapshot().actors[player].combat.learned.get("skill.cleave",-1)==0,"完整学习后只取得招式，并未免费精通")
	F.send(k,"end_interaction")
	check(F.send(k,"practice_skill",{"skill_id":"skill.cleave","ticks":8}).ok,"选择八个半日实际修炼")
	var valid: bool=true
	for tick in range(8):
		var advance: Dictionary=k.advance_one_tick()
		if not advance.ok: printerr("PRACTICE: ",advance); valid=false; break
	check(valid and k.snapshot().actors[player].combat.learned.get("skill.cleave",0)==100,"正常半日推进改变真实熟练度")
	if not valid: return
	F.send(k,"move_local",{"place_id":"west.street"}); F.send(k,"move_local",{"place_id":"west.grain"})
	var taught: Dictionary=F.send(k,"teach_skill",{"skill_id":"skill.cleave","context":F.context(k,"role.clerk")})
	check(taught.ok,"以自身已掌握本领向当面人物传授："+str(taught))
	if not taught.ok: return
	check(k.advance_one_tick().ok and k.advance_one_tick().ok and k.snapshot().actors["actor.apprentice"].combat.learned.get("skill.cleave",-1)==10,"传授消耗双方时间，只改变该学员该项本领")
	check(Codec.decode(Codec.encode(k.snapshot(),1,"training-world",k.content()),k.content()).ok,"修炼和传授事实可随同一世界继续保存")

## 旧目录与旧规则的空战斗迁移只补必要字段，不注入新武器或改变目录摘要。
func check_legacy() -> void:
	var k=Kernel.new(); check(k.new_game("scenario.lived_city","res://data/generated/journey_life.hmcatalog.json").ok,"旧生活内容继续可运行")
	var state: Dictionary=k.snapshot(); var old: Dictionary=state.duplicate(true)
	old.ruleset_id="f04-companions-1"; old.erase("battles"); old.erase("facts")
	for actor in old.actors.values(): actor.erase("combat"); actor.erase("injury"); actor.erase("appearance")
	check(k.restore(old).ok and k.snapshot()==state,"旧规则恢复保留人物、资源、目录摘要及行旅")

## 输出领域验证范围并退出，视觉与平台发行仍需另外验收。
func finish() -> void:
	print("PERSONAL_WORLD_CHECKS=",checks," FAILURES=",failures)
	quit(0 if failures==0 else 1)
