## 验证已作者化事件的语义取消、纯查询与真实再入场边界，不建设新事件平台。
extends SceneTree
const P=preload("res://tests/policy_fixture.gd")
const F=preload("res://tests/companion_fixture.gd")
const Events=preload("res://core/event_rules.gd")
const Codec=preload("res://core/save_codec.gd")
const KEY="actor.player|event.letter"
var checks: int=0
var failures: int=0

## 尾部故障只存在于测试子类；保留候选引用以证明清理发生在Schema拒绝之前。
class FaultKernel extends "res://core/world_kernel.gd":
	var armed: bool=false
	var observed: Dictionary={}
	## 正常领域动作后破坏余额，外层提交必须丢弃整个候选。
	func _apply(state: Dictionary, kind: String, params: Dictionary, actor: String, offer: Dictionary) -> Dictionary:
		var result: Dictionary=super._apply(state,kind,params,actor,offer)
		if armed and result.ok:
			observed=state
			state.wallets["wallet.player"].balance=-1
		return result

## 引擎初始化完成后执行实际规则。
func _init() -> void:
	call_deferred("run_checks")

## 记录具体行为失败，缺失合同不能以脚本错误冒充红测试。
func check(value: bool, label: String) -> bool:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)
	return value

## 对每种真实语义失效分别使用通过Schema的隔离恢复，不把忙碌等同死亡。
func cancellation_matrix() -> void:
	for reason in ["sender_moved","sender_dead","sender_unavailable","role_disabled","role_changed","player_moved","player_dead","player_unavailable","condition"]:
		var k=P.queued(); var s: Dictionary=k.snapshot(); var player: String=s.player_actor_id
		check(s.events[KEY].status=="queued","真实入场入队："+reason)
		if reason=="sender_moved": s.actors["actor.shen"].location.place_id="west.street"
		if reason=="sender_dead": s.actors["actor.shen"].alive=false
		if reason=="sender_unavailable": s.actors["actor.shen"].available=false
		if reason=="role_disabled": s.roles["role.sender"].enabled=false
		if reason=="role_changed":
			s.roles["role.sender"].actor_id="actor.zheng"
			s.actors["actor.zheng"].location=s.actors["actor.shen"].location.duplicate(true)
		if reason=="player_moved": s.actors[player].location.place_id="west.street"
		if reason=="player_dead": s.actors[player].alive=false
		if reason=="player_unavailable": s.actors[player].available=false
		if reason=="condition": s.knowledge[player]["offer.letter"]={"source":{"kind":"actor","id":"actor.shen"},"tick":s.tick}
		if not check(k.restore(s).ok,"失效夹具合法恢复："+reason): continue
		var preview_state: Dictionary=k.snapshot(); var preview_before: Dictionary=preview_state.duplicate(true)
		for _i in range(3): check(not Events.preview(preview_state,k.content().data,player,"event.letter").ok,"语义失效preview必须失败："+reason)
		check(preview_state==preview_before and preview_state.rng==preview_before.rng and k.snapshot()==s,"失效preview传入对象及其RNG完全纯读："+reason)
		# 死亡玩家不能行动，由另一活人提交零时长正常命令完成同一共享尾部。
		var actor: String="actor.chen" if reason in ["player_dead","player_unavailable"] else player
		check(F.send(k,"end_interaction",{},actor).ok,"普通提交完成："+reason)
		var after: Dictionary=k.snapshot()
		check(after.events[KEY].status=="cancelled" and after.events[KEY].last_tick==after.tick,"queued语义失效必须取消："+reason)
		check(after.wallets==s.wallets and after.inventories==s.inventories and after.rng==s.rng,"取消不发奖不改资产随机流："+reason)
		check(not Codec.encode(after,0,"event-policy",k.content()).is_empty(),"取消状态可保存："+reason)

## 等待或忙碌不取消一次事件；忙碌不能遮蔽同时存在的死亡。
func transient_and_pure() -> void:
	var k=P.queued(); var preview_state: Dictionary=k.snapshot(); var before: Dictionary=preview_state.duplicate(true)
	for _i in range(5): check(Events.preview(preview_state,k.content().data,"actor.player","event.letter").ok,"有效preview")
	check(preview_state==before and preview_state.rng==before.rng and k.snapshot()==before,"有效preview传入对象全部状态与RNG无变化")
	check(F.send(k,"wait").ok and k.snapshot().events[KEY].status=="queued","玩家正常等待不取消一次事件")
	check(k.advance_one_tick().ok and k.snapshot().events[KEY].status=="queued","等待结束仍可查看")
	check(F.send(k,"wait",{},"actor.shen").ok and k.snapshot().events[KEY].status=="queued","provider暂忙仍排队")
	var s: Dictionary=k.snapshot(); s.actors["actor.shen"].alive=false
	check(k.restore(s).ok and F.send(k,"end_interaction").ok,"忙碌且死亡夹具可提交")
	check(k.snapshot().events[KEY].status=="cancelled","死亡不被ActorBusy遮蔽")

## 只真实入场可重新排非一次事件；冷却时钟不被读取或重复清理刷新。
func cooldown_and_triggers() -> void:
	var k=P.queued(); F.send(k,"move_local",{"place_id":"west.street"})
	var cancelled: Dictionary=k.snapshot(); check(cancelled.events[KEY].status=="cancelled","玩家离场取消")
	for _i in range(3): F.send(k,"wait"); k.advance_one_tick()
	F.send(k,"move_local",{"place_id":"west.tavern"})
	check(k.snapshot().events[KEY].status=="cancelled" and k.snapshot().events[KEY].last_tick==cancelled.events[KEY].last_tick,"once取消永不复活，取消时间不漂移")
	var preview_state: Dictionary=k.snapshot(); var after_cancel: Dictionary=preview_state.duplicate(true)
	for _i in range(3): check(Events.preview(preview_state,k.content().data,"actor.player","event.letter").get("code")=="EventCancelled","已取消事件仅报通用取消状态")
	check(preview_state==after_cancel and preview_state.rng==after_cancel.rng and k.snapshot()==after_cancel,"已取消preview不改传入对象、历史原因或RNG")
	var data: Dictionary=k.content().data; data.events["event.letter"].once_per_actor=false
	var state: Dictionary=cancelled.duplicate(true); state.actors["actor.player"].location.place_id="west.tavern"
	state.tick=1; Events.on_enter(state,data,"actor.player")
	check(state.events[KEY].status=="cancelled","非一次事件冷却之前不入队")
	state.tick=2
	var old: Dictionary=state.duplicate(true)
	for _i in range(4): Events.preview(state,data,"actor.player","event.letter")
	check(state==old and state.rng==old.rng,"冷却边界preview不改传入对象或RNG，也不复活")
	Events.on_enter(state,data,"actor.player")
	check(state.events[KEY].status=="queued" and state.events[KEY].last_tick==2,"非一次事件边界真实入场才入队")
	for trigger in ["on_tick","after_action"]:
		state.events={}; data.events["event.letter"].trigger=trigger; Events.on_enter(state,data,"actor.player")
		check(state.events.is_empty(),"非入场触发不能在入场排队："+trigger)
	data.events["event.letter"].trigger="on_enter"; data.events["event.letter"].on_unavailable="reassign"
	state.events={}; Events.on_enter(state,data,"actor.player")
	check(state.events.is_empty(),"未知策略不得入队")
	check(Events.preview(state,data,"actor.player","event.letter").get("code")=="UnsupportedEventPolicy","未知策略预览明确拒绝")

## 隔离单元只提供占用判定所读字段，验证BattlePending不遮蔽语义；不冒充合法世界战斗存档。
func battle_mask_and_rejected_submits() -> void:
	var k=P.queued(); var state: Dictionary=k.snapshot(); var data: Dictionary=k.content().data
	state.battles["unit-occupation"]={"kind":"personal","phase":"planning","units":{"actor.player":{}}}
	check(Events.preview(state,data,"actor.player","event.letter").get("code")=="BattlePending","占用单元确实返回BattlePending")
	check(Events.reconcile(state,data).ok and state.events[KEY].status=="queued","纯暂时战斗占用仍排队")
	state.actors["actor.player"].alive=false
	check(Events.reconcile(state,data).ok and state.events[KEY].status=="cancelled","死亡不能被BattlePending遮蔽")
	state=k.snapshot(); state.actors["actor.shen"].available=false; check(k.restore(state).ok,"拒绝事务测试夹具可恢复")
	var invalid: Dictionary=P.command(k,"unknown-policy-command")
	check(not k.submit(invalid).ok and k.snapshot()==state,"失败preview不偷偷取消事件")
	invalid=P.command(k,"end_interaction"); invalid.expected_revision-=1
	check(k.submit(invalid).get("code")=="StalePreview" and k.snapshot()==state,"过时revision不执行清理")
	data.events["event.letter"].on_unavailable="reassign"
	check(Events.reconcile(state,data).get("code")=="UnsupportedEventPolicy","未知取消策略在reconcile明确拒绝")

## 清理发生后Schema失败须回滚所有字段，修复再提交及重放只结算一次。
func tail_rollback() -> void:
	var k=FaultKernel.new(); check(k.new_game().ok,"故障子类正常开局")
	F.send(k,"move_local",{"place_id":"west.tavern"})
	var s: Dictionary=k.snapshot(); s.actors["actor.shen"].available=false; check(k.restore(s).ok,"尾部失效夹具恢复")
	var cmd: Dictionary=P.command(k,"end_interaction"); k.armed=true
	check(not k.submit(cmd).ok and k.snapshot()==s,"尾部Schema故障完整回滚")
	check(k.observed.events[KEY].status=="cancelled","失败候选已执行reconcile才被Schema拒绝")
	k.armed=false; var result: Dictionary=k.submit(cmd); var after: Dictionary=k.snapshot()
	check(result.ok and after.events[KEY].status=="cancelled","同一命令修复后可重试")
	check(k.submit(cmd)==result and k.snapshot()==after,"重复request_seq不二次清理或改历史")

## 分组执行全部窄语义回归，并报告可核验断言数。
func run_checks() -> void:
	cancellation_matrix(); transient_and_pure(); cooldown_and_triggers(); battle_mask_and_rejected_submits(); tail_rollback()
	print("EVENT_POLICY_CHECKS=",checks," FAILURES=",failures)
	quit(1 if failures else 0)
