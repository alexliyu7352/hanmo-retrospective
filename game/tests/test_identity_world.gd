## 验证动态人物在现有世界中工作、学习和读档；创建来源是领域测试前提，不冒充玩家世代入口验收。
extends SceneTree
const Kernel=preload("res://core/world_kernel.gd")
const Identity=preload("res://core/actor_identity.gd")
const Codec=preload("res://core/save_codec.gd")
const F=preload("res://tests/companion_fixture.gd")
var checks: int=0
var failures: int=0

## 等待引擎完成初始化再载入同一正式内容包。
func _init() -> void:
	call_deferred("run_checks")

## 记录明确失败，最终以非零进程状态阻止错误集成。
func check(value: bool, label: String) -> void:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)

## 通过版本化人物命令驱动新增实例，所有工资、学习费用、时间与知识仍走原规则。
func run_checks() -> void:
	var k=Kernel.new()
	check(k.new_game("scenario.alpha_life","res://data/generated/alpha_life.hmcatalog.json").ok,"实际作者包载入")
	var candidate: Dictionary=k.snapshot()
	candidate.facts["identity-test-source"]={"kind":"family_adoption"}
	var created: Dictionary=Identity.instantiate(candidate,k.content().data,"actor.apprentice","林生",-18*Identity.TICKS_PER_YEAR,"west.street","identity-test-source")
	check(created.ok,"领域创建有独立原型与事实来源的真人")
	if not created.ok: finish(); return
	var person: String=created.actor_id
	var accepted: Dictionary=k.restore(candidate)
	check(accepted.ok,"正式世界校验接受有依据的动态人物："+str(accepted))
	if not accepted.ok: finish(); return
	check(k.snapshot().actors.size()==k.content().data.actors.size()+1 and not k.content().data.actors.has(person),"新增身份不要求在不可变作者目录中增加写死 ID")
	check(k.snapshot().wallets[k.snapshot().actors[person].wallet_id].balance==0 and k.snapshot().actors[person].combat.learned.is_empty(),"新人物没有复制原型钱包或武学")
	check(F.send(k,"move_local",{"place_id":"west.tavern"},person).ok and F.send(k,"talk",{"role_id":"role.host"},person).ok,"动态人物可以实际走访并开始对话")
	check(F.send(k,"end_interaction",{},person).ok,"动态人物正常结束对白")
	var host_money: int=k.snapshot().wallets["wallet.host"].balance
	check(F.send(k,"service",{"activity":"work","context":F.context(k,"role.host")},person).ok and k.advance_one_tick().ok,"动态人物通过真实店家短工谋生")
	var worker: Dictionary=k.snapshot().actors[person]
	check(k.snapshot().wallets[worker.wallet_id].balance==80 and k.snapshot().wallets["wallet.host"].balance==host_money-80,"工资来自雇主，进入新人物独立的钱包")
	check(F.send(k,"move_local",{"place_id":"west.street"},person).ok and F.send(k,"move_local",{"place_id":"west.training"},person).ok,"新增人物使用现有场所通行")
	check(F.send(k,"learn_skill",{"skill_id":"skill.cleave","context":F.context(k,"role.wei")},person).ok,"使用自己挣得的钱向在场教师受教")
	check(k.advance_one_tick().ok and k.advance_one_tick().ok,"双方共同经过两个真实半日")
	worker=k.snapshot().actors[person]
	check(worker.combat.learned.get("skill.cleave",-1)==0 and k.snapshot().wallets[worker.wallet_id].balance==45,"新增人物获得自己的初学知识并支付束脩")
	check(k.snapshot().actors["actor.apprentice"].combat.learned.is_empty(),"原型人物不会共享新增人物的修行进度")
	var state: Dictionary=k.snapshot()
	var decoded: Dictionary=Codec.decode(Codec.encode(state,0,"dynamic-person",k.content()),k.content())
	check(decoded.ok and k.restore(decoded.state).ok and k.snapshot()==state,"新增人物、来源、对话与资产通过完整世界存档往返")
	candidate=state.duplicate(true); candidate.player_actor_id=person
	check(not k.restore(candidate).ok and k.snapshot()==state,"动态身份本身不能越过尚未履行的合法继任来切换玩家")
	candidate=state.duplicate(true); candidate.facts.erase(person)
	check(not k.restore(candidate).ok,"丢失永久创建事实的动态人物不能恢复")
	finish()

## 只报告身份集成范围，不将测试前提中的创建来源当作已完成世代玩法。
func finish() -> void:
	print("IDENTITY_WORLD_CHECKS=",checks," FAILURES=",failures)
	quit(0 if failures==0 else 1)
