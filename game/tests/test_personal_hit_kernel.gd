## 唯一Kernel命中随机回滚接缝；内存目录仅隔离开启模型做故障注入，不冒称新作者包/Schema已发布。
extends SceneTree
const Round=preload("res://core/personal_round.gd")
const Sessions=preload("res://core/combat_session.gd")
const F=preload("res://tests/companion_fixture.gd")
## 故障回调通过原描述构建注册器，保持validator索引与执行计划一致，不篡改已经编译的注册合同。
class ProbeKernel extends "res://core/world_kernel.gd":
	var reject_tail: bool=false
	var previous_stream: Dictionary={}
	var observed: bool=false
	var captured_stream: Dictionary={}
	## 只包装真实个人校验；新局和入战先正常通过，指定回合的尾部才拒绝。
	func _module_descriptors() -> Array:
		var rows: Array=super._module_descriptors()
		for descriptor in rows:
			for entry in descriptor.validators:
				if entry.id!="personal.state": continue
				var original: Callable=entry.callback
				entry.callback=func(candidate: Dictionary,data: Dictionary) -> Dictionary:
					var checked: Dictionary=original.call(candidate,data)
					if not checked.ok or not reject_tail: return checked
					observed=candidate.rng.duel!=previous_stream
					captured_stream=candidate.rng.duel.duplicate(true)
					return {"ok":false,"code":"InjectedHitTailValidation"}
		return rows

var checks: int=0
var failures: int=0

## 只断言该高风险接口，不重跑整套旧世界回归。
func check(value: bool, label: String) -> void:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)

## 复用已有小目录和正常开战命令；真正消费RNG后在最终个人validator拒绝候选。
func _initialize() -> void:
	var kernel=ProbeKernel.new()
	var created: Dictionary=kernel.new_game("scenario.alpha_life","res://data/generated/alpha_life.hmcatalog.json")
	check(created.ok,"existing runtime fixture loads without new prepare")
	if not created.ok: printerr(created); quit(1); return
	kernel._catalog.data.personal_objectives["objective.spar"].hit_model="skill_v1"
	check(F.send(kernel,"move_local",{"place_id":"west.training"}).ok,"normal actor reaches real sparring role")
	var entered: Dictionary=F.send(kernel,"duel_start",{"context":F.context(kernel,"role.wei")})
	check(entered.ok,"normal registered command creates opted battle")
	if not entered.ok: printerr(entered); quit(1); return
	var before: Dictionary=kernel.snapshot(); var actor_id: String=before.player_actor_id
	var battle: Dictionary=Sessions.for_actor(before,actor_id)
	var opponent: String=""
	for id in battle.units:
		if id!=actor_id: opponent=id
	var unit: Dictionary=battle.units[actor_id]
	var origin: Vector2=Vector2(unit.position[0],unit.position[1])
	var target: Array=battle.ai_plans[opponent].destination
	var delta: Vector2=Vector2(target[0],target[1])-origin
	var weapon: Dictionary=kernel.content().data.items[unit.weapon_id].weapon
	var stride: float=minf(Round.movement_distance(battle,kernel.content().data,actor_id),maxf(0.0,delta.length()-weapon.maximum_range*0.65))
	var point: Vector2=origin+delta.normalized()*stride
	var params: Dictionary={"battle_id":battle.id,"destination":[roundi(point.x),roundi(point.y)],"heading":posmod(roundi(rad_to_deg(atan2(delta.y,delta.x))),360),"action":"attack","target_id":opponent,"skill_id":""}
	check(kernel.preview("battle_plan",params).ok and kernel.snapshot()==before,"planning preview is pure before failure injection")
	kernel.previous_stream=before.rng.duel.duplicate(true)
	kernel.reject_tail=true
	var command: Dictionary={"kind":"battle_plan","params":params,"actor_id":actor_id,"request_seq":before.request_sequences[actor_id]+1,"expected_revision":before.revision}
	var denied: Dictionary=kernel.submit(command)
	if denied.get("code")!="InjectedHitTailValidation" or not kernel.observed: printerr("HIT_TAIL_RESULT ",denied," observed=",kernel.observed," params=",params," ai=",battle.ai_plans)
	check(denied.get("code")=="InjectedHitTailValidation" and kernel.observed,"tail validator rejects only after real candidate RNG advancement")
	check(kernel.snapshot()==before,"full failed candidate including RNG HP sequence facts and revision is discarded")
	kernel.reject_tail=false
	var accepted: Dictionary=kernel.submit(command)
	if not accepted.ok: printerr("HIT_RETRY_RESULT ",accepted)
	check(accepted.ok and kernel.snapshot().rng.duel==kernel.captured_stream,"same request retry commits exact same random successor once")
	var committed: Dictionary=kernel.snapshot()
	check(kernel.submit(command)==accepted and kernel.snapshot()==committed,"duplicate request cannot draw again")
	# 探针闭包持有本测试Kernel；结束时解除测试注册引用，避免把回滚通过掩盖为资源泄漏。
	kernel._registry={}
	kernel=null
	print("PERSONAL_HIT_KERNEL checks=",checks," failures=",failures)
	quit(1 if failures else 0)
