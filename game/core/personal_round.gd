## 个人战的纯轮次规则：消费已保存意图、同步移动与伤害；显式命中模型仅推进传入duel流副本，世界统一提交。
extends RefCounted
const AIScore = preload("res://core/domains/personal/personal_ai_score.gd")
const Area = preload("res://core/domains/personal/personal_area.gd")
const Strike = preload("res://core/domains/personal/personal_strike.gd")
const Stamina = preload("res://core/domains/personal/personal_stamina.gd")
const Withdrawal = preload("res://core/domains/personal/personal_withdrawal.gd")
const Geometry = preload("res://core/personal_geometry.gd")
const Guard = preload("res://core/domains/personal/personal_guard.gd")
const EscortAI = preload("res://core/domains/personal/personal_escort_ai.gd")
const Objectives = preload("res://core/domains/personal/personal_objectives.gd")
const Counter = preload("res://core/domains/personal/personal_counter.gd")
const FacingDamage = preload("res://core/domains/personal/personal_facing_damage.gd")
const Hit = preload("res://core/domains/personal/personal_hit.gd")
const ArmorDefinition = preload("res://core/domains/personal/personal_armor_content.gd")
const ACTIONS: Array = ["attack", "defend", "wait", "skill", "withdraw"]

## 提供界面与作者预览共用的有效移动距离；普通防御和非法会话返回零，可选action兼容旧技能查询。
static func movement_distance(battle: Dictionary, data: Dictionary, actor_id: String, skill_id: String = "", action: String = "") -> int:
	if not _battle_error(battle, data).is_empty() or not battle.units.has(actor_id): return 0
	if action == "defend" or (not action.is_empty() and not action in ACTIONS): return 0
	var unit: Dictionary = battle.units[actor_id]
	if unit.hp <= 0 or Withdrawal.departed(unit) or _recovering(unit, battle.round): return 0
	var skill: Dictionary = {}
	if not skill_id.is_empty():
		var learned: Dictionary = _skill(unit, data, skill_id)
		if not learned.ok or not learned.skill.kind in ["guard", "strike"] or unit.stamina < learned.skill.stamina_cost: return 0
		skill = learned.skill
	return _move_distance(unit, data, battle.round, skill)

## 仅按当前公开人物状态预定非玩家意图；不读取 ai_plans、玩家草案或未来移动。
static func make_ai(battle: Dictionary, data: Dictionary) -> Dictionary:
	if not _battle_error(battle, data).is_empty(): return {}
	var plans: Dictionary = {}
	for id in _ids(battle.units):
		var unit: Dictionary = battle.units[id]
		if id == battle.controller_id or unit.hp <= 0 or Withdrawal.departed(unit): continue
		if AIScore.enabled(data.personal_objectives[battle.objective_id]):
			var scored: Dictionary = ai_explanation(battle,data,id)
			if scored.ok: plans[id] = scored.plan
			continue
		var idle: Dictionary = _plan(unit.position, unit.heading, "wait" if unit.stamina < 35 else "defend")
		var weapon: Dictionary = _weapon(unit, data)
		var target_id: String = _nearest_enemy(battle, id)
		if unit.protected and unit.side == battle.units[battle.controller_id].side and data.personal_objectives[battle.objective_id].get("kind") == "escort":
			if not _recovering(unit, battle.round): idle.destination = EscortAI.destination(battle, data, id, _move_distance(unit, data, battle.round))
			if not target_id.is_empty(): idle.heading = _heading(idle.destination, battle.units[target_id].position, unit.heading)
			if idle.destination != unit.position: idle.action = "wait"
			plans[id] = idle; continue
		if weapon.is_empty() or target_id.is_empty() or _recovering(unit, battle.round):
			plans[id] = _safer_withdrawal(battle, data, id, _safer_guard(battle, data, id, idle)); continue
		var best: Dictionary = _best_attack(battle,data,id,target_id,true)
		if not best.is_empty(): plans[id] = best.plan
		else:
			idle.destination = _ai_destination(battle,data,id,target_id)
			idle.heading = _heading(idle.destination,battle.units[target_id].position,unit.heading)
			if idle.destination != unit.position: idle.action = "wait"
			plans[id] = idle
		plans[id] = _safer_withdrawal(battle, data, id, _safer_guard(battle, data, id, plans[id]))
	return plans

## 受信调试纯查询：仅重算轮初可见候选和理由，不把未来计划/随机状态泄露给普通玩家解释入口。
static func ai_explanation(battle: Dictionary, data: Dictionary, actor_id: String) -> Dictionary:
	var error: String=_battle_error(battle,data)
	if not error.is_empty(): return _fail(error)
	if not AIScore.enabled(data.personal_objectives[battle.objective_id]): return _fail("ScoredAIUnavailable")
	if not battle.units.has(actor_id) or battle.units[actor_id].hp<=0 or Withdrawal.departed(battle.units[actor_id]): return _fail("UnavailableAIActor")
	var candidates: Array=_scored_candidates(battle,data,actor_id)
	var baseline: int=_protected_risk_q(battle,data,actor_id,{})
	var evaluations: Array=[]
	for index in range(mini(candidates.size(),AIScore.MAX_CANDIDATES)):
		var assessed: Dictionary=_assess_plan(battle,data,actor_id,candidates[index],baseline)
		if not assessed.is_empty(): evaluations.append(assessed)
	var best: Dictionary=AIScore.choose(evaluations)
	if best.is_empty(): return _fail("NoLegalAICandidate")
	var unit: Dictionary=battle.units[actor_id]
	var original: Dictionary={"plan":best.plan.duplicate(true),"score_q":best.score_q,"maximum_incoming":best.maximum_incoming,"lethal":not best.survives_maximum}
	var filtering: Dictionary={"applied":false,"reason":"BestAlreadySurvives" if best.survives_maximum else "NotLowHealth","original":original,"surviving_candidates":0}
	if unit.hp*100<=unit.initial_hp*Withdrawal.LOW_HP_PERCENT and not best.survives_maximum:
		var survivors: Array=[]
		for row in evaluations:
			var defensive: bool=row.plan.action in ["defend","withdraw"] or (row.plan.action=="skill" and data.personal_skills.get(row.plan.skill_id,{}).get("kind")=="guard")
			if defensive and row.survives_maximum: survivors.append(row)
		filtering.surviving_candidates=survivors.size(); filtering.reason="NoSurvivableDefense"
		if not survivors.is_empty():
			best=AIScore.choose(survivors); filtering.applied=true; filtering.reason="VisibleLethalRiskAvoided"
			best.reason_codes.append("SurvivalOverride")
	best.survival_filter=filtering
	best.ok=true; best.model=AIScore.MODEL; best.candidates_considered=evaluations.size(); best.candidate_total=candidates.size(); best.truncated=candidates.size()>AIScore.MAX_CANDIDATES
	return best

## 复用已存在的合法攻击点、出口首段与防守朝向；有限预算先保留防御/等待/撤离，不使大量进攻卡吞掉安全候选。
static func _scored_candidates(battle: Dictionary, data: Dictionary, actor_id: String) -> Array:
	var unit: Dictionary=battle.units[actor_id]
	var endpoints: Array=[unit.position.duplicate()]; var headings: Array=[unit.heading]; var attacks: Array=[]
	for target in battle.units.values():
		if target.side==unit.side or target.hp<=0 or Withdrawal.departed(target): continue
		var heading: int=_heading(unit.position,target.position,unit.heading)
		if not heading in headings: headings.append(heading)
	if unit.protected and unit.side==battle.units[battle.controller_id].side:
		var escort: Array=EscortAI.destination(battle,data,actor_id,0 if _recovering(unit,battle.round) else _move_distance(unit,data,battle.round))
		if not escort in endpoints: endpoints.append(escort)
	else:
		for target_id in _ids(battle.units):
			var target: Dictionary=battle.units[target_id]
			if target.side==unit.side or target.hp<=0 or Withdrawal.departed(target): continue
			var heading: int=_heading(unit.position,target.position,unit.heading)
			if not heading in headings: headings.append(heading)
			if not _weapon(unit,data).is_empty() and not _recovering(unit,battle.round):
				var point: Array=_ai_destination(battle,data,actor_id,target_id)
				if not point in endpoints: endpoints.append(point)
				attacks.append_array(_attack_plans(battle,data,actor_id,target_id))
	headings.sort()
	var raw: Array=[]
	for heading in headings: raw.append(_plan(unit.position,heading,"defend"))
	for point in endpoints: raw.append(_plan(point,unit.heading,"wait"))
	if Withdrawal.low_hp(unit) and data.personal_objectives[battle.objective_id].allow_withdrawal:
		var point: Array=Withdrawal.destination(battle,data,actor_id,0 if _recovering(unit,battle.round) else _move_distance(unit,data,battle.round))
		if point!=unit.position or Withdrawal.at_exit(battle,data,actor_id): raw.append(_plan(point,unit.heading,"withdraw"))
	for skill_id in _ids(unit.learned):
		if data.personal_skills.get(skill_id,{}).get("kind")!="guard": continue
		for point in endpoints:
			for heading in headings: raw.append(_plan(point,heading,"skill","",skill_id))
	raw.append_array(attacks)
	var result: Array=[]; var seen: Dictionary={}
	for candidate in raw:
		var key: String=AIScore.action_key(candidate,unit.position)
		if seen.has(key) or not validate_plan(battle,data,actor_id,candidate).ok: continue
		seen[key]=true; result.append(candidate)
	return result

## 只投影本候选的真实几何，其他人物停在轮初可见位置；预计击倒不抹除其同步攻击威胁。
static func _assess_plan(battle: Dictionary, data: Dictionary, actor_id: String, plan: Dictionary, baseline_protected_q: int) -> Dictionary:
	var projected: Dictionary=battle.duplicate(true)
	var moved: Dictionary=Geometry.resolve_moves(data.personal_battlefields[battle.battlefield_id],Withdrawal.bodies(battle.units),{actor_id:plan})
	if moved.has("code"): return {}
	projected.units[actor_id].position=moved.positions[actor_id]; projected.units[actor_id].heading=plan.heading
	var actual_plan: Dictionary=plan.duplicate(true); actual_plan.destination=moved.positions[actor_id]
	var incoming_q: int=_scored_incoming_q(projected,data,actor_id,actual_plan)
	var maximum_incoming: int=_incoming_damage(projected.units[actor_id],actual_plan,_incoming_sources(projected,data,actor_id,moved.positions[actor_id]),data,battle.round,data.personal_objectives[battle.objective_id].get("facing_damage",{}))
	var protection_q: int=_protected_risk_q(projected,data,actor_id,actual_plan)
	var damage_q: int=0; var cost: int=0; var outgoing: Dictionary={}
	var skill: Dictionary=data.personal_skills.get(plan.skill_id,{})
	if plan.action=="skill" and skill.get("kind")=="guard": cost=skill.stamina_cost
	elif plan.action=="attack" or skill.get("kind")=="strike":
		var condition: Dictionary=Strike.condition(skill,battle.units[actor_id].position,plan.destination,moved.positions[actor_id])
		var preview: Dictionary=preview_attack(projected,data,actor_id,moved.positions[actor_id],plan.heading,plan.target_id,plan.skill_id) if condition.ok else condition
		if preview.ok:
			cost=skill.stamina_cost if not skill.is_empty() else _weapon(projected.units[actor_id],data).stamina_cost
			for target in preview.get("targets",[{"target_id":plan.target_id,"damage":preview.damage,"hit_bp":preview.get("hit_bp",10000)}]):
				var expected: int=mini(target.damage,projected.units[target.target_id].hp)*target.get("hit_bp",10000)*4/10000
				damage_q+=expected; outgoing[target.target_id]=expected
	# 期望受伤只在目标完成投影里回写；保护风险计算始终保留所有尚活攻击者，避免杀敌取消同步伤害。
	var result_projection: Dictionary=projected.duplicate(true)
	result_projection.units[actor_id].hp=maxi(0,result_projection.units[actor_id].hp-ceili(incoming_q/4.0))
	for target_id in outgoing: result_projection.units[target_id].hp=maxi(0,result_projection.units[target_id].hp-outgoing[target_id]/4)
	var goal: bool=false
	var outcome: String=_outcome(result_projection,data)
	if projected.units[actor_id].side==projected.units[battle.controller_id].side: goal=outcome=="victory"
	else: goal=outcome in ["defeat","protected_lost"]
	if plan.action=="withdraw" and Withdrawal.at_exit(projected,data,actor_id) and result_projection.units[actor_id].hp>0:
		var safe: bool=true
		for source in _incoming_sources(projected,data,actor_id,projected.units[actor_id].position):
			if source.weapon.category in ["sword","spear"]: safe=false
		if safe: goal=true
	var result: Dictionary=AIScore.evaluate(plan,battle.units[actor_id].position,goal,protection_q<baseline_protected_q,damage_q,incoming_q,cost)
	result.maximum_incoming=maximum_incoming; result.survives_maximum=maximum_incoming<battle.units[actor_id].hp
	result.projected_position=moved.positions[actor_id].duplicate(); result.protected_risk_q={"before":baseline_protected_q,"after":protection_q}
	return result

## 公开合法最大威胁转成当前模型期望值；方向守势仍用同一伤害叶，既不抽骰也不偷看对方承诺。
static func _scored_incoming_q(battle: Dictionary, data: Dictionary, actor_id: String, plan: Dictionary) -> int:
	var target: Dictionary=battle.units[actor_id].duplicate(true); target.heading=plan.get("heading",target.heading)
	var total: int=0
	for source in _incoming_sources(battle,data,actor_id,target.position,true):
		var guard_bp: int=10000
		if plan.get("action")=="defend": guard_bp=Guard.multiplier(source.unit.position,target.position,target.heading,5000)
		elif plan.get("action")=="skill" and data.personal_skills.get(plan.skill_id,{}).get("kind")=="guard":
			var skill: Dictionary=data.personal_skills[plan.skill_id]
			guard_bp=_mastered_bp(skill.defense_bp,target.learned[plan.skill_id])
			if skill.get("guard_facing","")=="front": guard_bp=Guard.multiplier(source.unit.position,target.position,target.heading,guard_bp)
		var damage: int=_damage(source.unit,target,source.weapon,source.skill,data,battle.round,guard_bp,data.personal_objectives[battle.objective_id].get("facing_damage",{}))
		var chance_bp: int=10000
		if Hit.enabled(data.personal_objectives[battle.objective_id]): chance_bp=Hit.chance(source.unit,target,source.weapon,source.skill).get("hit_bp",0)
		total+=mini(damage,target.hp)*chance_bp*4/10000
	return mini(target.hp*4,total)

## 只评估当前友方保护者真实风险变化；没有保护者时为零，不发明世界NPC战略目标。
static func _protected_risk_q(battle: Dictionary, data: Dictionary, actor_id: String, acting_plan: Dictionary) -> int:
	var total: int=0; var side: int=battle.units[actor_id].side
	for id in _ids(battle.units):
		var unit: Dictionary=battle.units[id]
		if unit.side!=side or not unit.protected or unit.hp<=0 or Withdrawal.departed(unit): continue
		var plan: Dictionary=acting_plan if id==actor_id and not acting_plan.is_empty() else _plan(unit.position,unit.heading,"wait")
		total+=_scored_incoming_q(battle,data,id,plan)
	return total

## 低体力只优先安全且有实际出口路径的撤离；威胁比较不读未来意图，堵死或更危险时保留原合法候选。
static func _safer_withdrawal(battle: Dictionary, data: Dictionary, actor_id: String, original: Dictionary) -> Dictionary:
	var unit: Dictionary=battle.units[actor_id]
	if not Withdrawal.low_hp(unit) or not data.personal_objectives[battle.objective_id].allow_withdrawal: return original
	var maximum: int=0 if _recovering(unit,battle.round) else _move_distance(unit,data,battle.round)
	var destination: Array=Withdrawal.destination(battle,data,actor_id,maximum)
	var at_exit: bool=Withdrawal.point_at_exit(data.personal_battlefields[battle.battlefield_id],unit.side,destination)
	if destination==unit.position and not at_exit: return original
	var candidate: Dictionary=_plan(destination,original.heading,"withdraw")
	if not validate_plan(battle,data,actor_id,candidate).ok: return original
	var incoming: Array=_incoming_sources(battle,data,actor_id,destination)
	var facing: Dictionary=data.personal_objectives[battle.objective_id].get("facing_damage",{})
	var risk: int=_incoming_damage(unit,candidate,incoming,data,battle.round,facing)
	if risk>=unit.hp: return original
	# 出口处仍可能受近战时不把离场当成已成功；安全路径本身可比较，但没有额外胜出加分。
	var melee_risk: bool=false
	for source in incoming:
		if source.weapon.category in ["sword","spear"]: melee_risk=true
	if at_exit and not melee_risk: return candidate
	var prior: int=_incoming_damage(unit,original,_incoming_sources(battle,data,actor_id,original.destination),data,battle.round,facing)
	return candidate if destination!=unit.position and risk<=prior else original

## 在已有合法候选会承受致命威胁时比较守势；只有真实降低到可生存才替换，不把低血写成固定防御。
static func _safer_guard(battle: Dictionary, data: Dictionary, actor_id: String, original: Dictionary) -> Dictionary:
	var unit: Dictionary = battle.units[actor_id]
	var incoming: Array = _incoming_sources(battle, data, actor_id, original.destination)
	var facing: Dictionary = data.personal_objectives[battle.objective_id].get("facing_damage", {})
	var danger: int = _incoming_damage(unit, original, incoming, data, battle.round, facing)
	if danger < unit.hp: return original
	var stationary_incoming: Array = incoming if original.destination == unit.position else _incoming_sources(battle, data, actor_id, unit.position)
	var headings: Array = [original.heading]
	for source in stationary_incoming:
		var heading: int = _heading(unit.position, source.unit.position, original.heading)
		if not heading in headings: headings.append(heading)
	headings.sort()
	var candidates: Array = []
	for heading in headings: candidates.append(_plan(unit.position, heading, "defend"))
	for skill_id in _ids(unit.learned):
		var skill: Dictionary = data.personal_skills.get(skill_id,{})
		if skill.get("kind") != "guard": continue
		var guard_headings: Array = [original.heading]
		if skill.get("guard_facing","") == "front":
			for source in incoming:
				var heading: int = _heading(original.destination,source.unit.position,original.heading)
				if not heading in guard_headings: guard_headings.append(heading)
			guard_headings.sort()
		for heading in guard_headings: candidates.append(_plan(original.destination,heading,"skill","",skill_id))
	var best: Dictionary = original; var best_damage: int = danger; var best_cost: int = 2147483647; var best_key: String = ""
	for candidate in candidates:
		if not validate_plan(battle, data, actor_id, candidate).ok: continue
		var predicted: Array = incoming if candidate.destination == original.destination else stationary_incoming
		var damage: int = _incoming_damage(unit, candidate, predicted, data, battle.round, facing)
		if damage >= unit.hp: continue
		var cost: int = data.personal_skills[candidate.skill_id].stamina_cost if candidate.action == "skill" else 0
		var key: String = JSON.stringify(candidate, "", true)
		if damage < best_damage or (damage == best_damage and (cost < best_cost or (cost == best_cost and key < best_key))):
			best = candidate; best_damage = damage; best_cost = cost; best_key = key
	return best

## 只用轮初可见能力和原合法移动推算可能攻击；不读取ai_plans或玩家草案，也不假设装填者或气势不足者能出手。
static func _incoming_sources(battle: Dictionary, data: Dictionary, actor_id: String, destination: Array, expected: bool=false) -> Array:
	var projected: Dictionary = battle.duplicate(true)
	projected.units[actor_id].position = destination.duplicate()
	var target: Dictionary = projected.units[actor_id]
	var sources: Array = []
	for id in _ids(projected.units):
		var opponent: Dictionary = projected.units[id]
		if opponent.side == target.side or opponent.hp <= 0 or Withdrawal.departed(opponent) or _recovering(opponent, projected.round) or _weapon(opponent, data).is_empty(): continue
		var strongest: Dictionary = _best_attack(projected,data,id,actor_id,expected,false)
		if strongest.is_empty(): continue
		var skill: Dictionary = data.personal_skills.get(strongest.plan.skill_id,{})
		var attacker: Dictionary = opponent.duplicate(true); attacker.position = strongest.plan.destination; attacker.heading = strongest.plan.heading
		sources.append({"unit":attacker, "weapon":Strike.weapon(_weapon(opponent,data),skill), "skill":skill})
	return sources

## 合计公开威胁在候选姿态下的原规则伤害；前后夹击分别应用普通方向防御，守势与心法沿原公式。
static func _incoming_damage(unit: Dictionary, plan: Dictionary, incoming: Array, data: Dictionary, current_round: int, facing: Dictionary = {}) -> int:
	var target: Dictionary = unit.duplicate(true); target.position = plan.destination; target.heading = plan.heading
	var total: int = 0
	for source in incoming:
		var guard_bp: int = 10000
		if plan.action == "defend": guard_bp = Guard.multiplier(source.unit.position, target.position, target.heading, 5000)
		elif plan.action == "skill" and data.personal_skills.get(plan.skill_id, {}).get("kind") == "guard":
			var skill: Dictionary = data.personal_skills[plan.skill_id]
			guard_bp = _mastered_bp(skill.defense_bp,target.learned[plan.skill_id])
			if skill.get("guard_facing","") == "front": guard_bp = Guard.multiplier(source.unit.position,target.position,target.heading,guard_bp)
		total += _damage(source.unit, target, source.weapon, source.skill, data, current_round, guard_bp, facing)
	return total

## 校验意图的身份、消耗和可达位置；射程在双方共同移动后重查，不能把敌人当前位置当未来承诺。
static func validate_plan(battle: Dictionary, data: Dictionary, actor_id: String, params: Dictionary) -> Dictionary:
	var error: String = _battle_error(battle, data)
	if not error.is_empty(): return _fail(error)
	if not battle.units.has(actor_id): return _fail("UnknownCombatant")
	var unit: Dictionary = battle.units[actor_id]
	if unit.hp <= 0: return _fail("ActorDown")
	if Withdrawal.departed(unit): return _fail("ActorWithdrawn")
	if not params.get("action") in ACTIONS: return _fail("InvalidAction")
	if not _point(params.get("destination")) or typeof(params.get("heading")) != TYPE_INT or params.heading < 0 or params.heading > 359: return _fail("InvalidPlan")
	var action: String = params.action
	if action == "defend" and params.destination != unit.position: return _fail("DefendRequiresStationary")
	var skill_id: String = str(params.get("skill_id", ""))
	var skill: Dictionary = {}
	if action == "skill":
		var learned: Dictionary = _skill(unit, data, skill_id)
		if not learned.ok: return learned
		skill = learned.skill
		if skill.kind == "heart": return _fail("PassiveHeart")
	elif not skill_id.is_empty(): return _fail("UnexpectedSkill")
	var condition: Dictionary = Strike.condition(skill,unit.position,params.destination)
	if not condition.ok: return condition
	if _recovering(unit, battle.round) and (params.destination != unit.position or action == "attack" or skill.get("kind") == "strike"): return _fail("WeaponRecovering")
	var movement: Dictionary = Geometry.can_move(data.personal_battlefields[battle.battlefield_id], unit.position, params.destination, _move_distance(unit, data, battle.round, skill), unit.radius)
	if not movement.ok: return movement
	if action == "withdraw" and not data.personal_objectives[battle.objective_id].allow_withdrawal: return _fail("WithdrawalForbidden")
	if action == "withdraw" and unit.protected: return _fail("ProtectedWithdrawalRequiresController")
	var target_id: String = str(params.get("target_id", ""))
	if action == "attack" or skill.get("kind") == "strike":
		var attack: Dictionary = _attack_actor(battle, data, actor_id, target_id, skill_id)
		if not attack.ok: return attack
	elif not target_id.is_empty(): return _fail("UnexpectedTarget")
	if action == "skill" and unit.stamina < skill.stamina_cost: return _fail("NotEnoughStamina")
	return {"ok": true, "plan": _plan(params.destination, params.heading, action, target_id, skill_id)}

## 预测指定公开落点的攻击；复用结算伤害公式，不预读敌方意图或消耗气势，未来防御与移动仍可改变结果。
static func preview_attack(battle: Dictionary, data: Dictionary, actor_id: String, destination: Array, heading: int, target_id: String, skill_id: String = "") -> Dictionary:
	var error: String = _battle_error(battle, data)
	if not error.is_empty(): return _fail(error)
	var attack: Dictionary = _attack_actor(battle, data, actor_id, target_id, skill_id)
	if not attack.ok: return attack
	var params: Dictionary = _plan(destination, heading, "attack" if skill_id.is_empty() else "skill", target_id, skill_id)
	var plan: Dictionary = validate_plan(battle, data, actor_id, params)
	if not plan.ok: return plan
	var projected: Dictionary = battle.duplicate(true)
	projected.units[actor_id].position = destination.duplicate(); projected.units[actor_id].heading = heading
	var selected: Dictionary = Area.select(projected,data,actor_id,target_id,attack.skill,attack.weapon)
	if not selected.ok: return selected
	var result: Dictionary = {"ok":true}
	var targets: Array = []
	for selected_id in selected.target_ids:
		var target_preview: Dictionary = {"target_id":selected_id,"damage":_damage(projected.units[actor_id],projected.units[selected_id],attack.weapon,attack.skill,data,battle.round,10000,data.personal_objectives[battle.objective_id].get("facing_damage",{}))}
		if Hit.enabled(data.personal_objectives[battle.objective_id]):
			var chance: Dictionary = Hit.chance(projected.units[actor_id],projected.units[selected_id],attack.weapon,attack.skill)
			if not chance.ok: return chance
			target_preview.hit_bp = chance.hit_bp
		if selected_id == target_id:
			result.damage = target_preview.damage
			if target_preview.has("hit_bp"): result.hit_bp = target_preview.hit_bp
		targets.append(target_preview)
	if attack.skill.has("area"): result.targets = targets
	return result

## 原子结算一轮；验证保存的全部 AI 意图后移动，攻击阶段同时扣血，失败返回原因并保留所有输入。
static func resolve(battle: Dictionary, data: Dictionary, player_plan: Dictionary, duel_stream: Dictionary = {}) -> Dictionary:
	var error: String = _battle_error(battle, data)
	if not error.is_empty(): return _fail(error)
	var stream: Dictionary = {}
	if Hit.enabled(data.personal_objectives[battle.objective_id]):
		var stream_valid: Dictionary = Hit.validate_stream(duel_stream)
		if not stream_valid.ok: return stream_valid
		stream = duel_stream.duplicate(true)
	var next: Dictionary = battle.duplicate(true)
	next.last_events = []
	var initial_outcome: String = _outcome(next,data)
	if not initial_outcome.is_empty(): return _finish(next,initial_outcome,stream,data.personal_objectives[next.objective_id])
	var player: Dictionary = validate_plan(battle, data, battle.controller_id, player_plan)
	if not player.ok: return player
	var plans: Dictionary = {battle.controller_id: player.plan}
	for id in battle.ai_plans:
		if not battle.units.has(id) or id == battle.controller_id or battle.units[id].hp <= 0 or Withdrawal.departed(battle.units[id]): return _fail("UnexpectedAIPlan")
	for id in _ids(battle.units):
		if id == battle.controller_id or battle.units[id].hp <= 0 or Withdrawal.departed(battle.units[id]): continue
		if not battle.ai_plans.get(id) is Dictionary: return _fail("MissingAIPlan")
		var checked: Dictionary = validate_plan(battle, data, id, battle.ai_plans[id])
		if not checked.ok: return {"ok": false, "code": "InvalidAIPlan", "actor_id": id, "reason": checked.code}
		plans[id] = checked.plan
	# 倒地者仍占据场地；其计划为空，几何层保持原位，活人不能在后续轮次与其重叠。
	var moved: Dictionary = Geometry.resolve_moves(data.personal_battlefields[battle.battlefield_id], Withdrawal.bodies(next.units), plans)
	if moved.has("code"): return _fail(moved.code)
	var guards: Dictionary = {}
	var directional_guards: Dictionary = {}
	var counter_declarations: Dictionary = {}
	for id in _ids(plans):
		var unit: Dictionary = next.units[id]
		var plan: Dictionary = plans[id]
		unit.position = moved.positions[id]; unit.heading = plan.heading
		_prune(unit, next.round)
		next.last_events.append({"actor_id": id, "action": "move", "position": unit.position.duplicate(), "code": "MoveStopped" if id in moved.blocked else "Moved"})
		if plan.action == "defend":
			guards[id] = 5000; directional_guards[id] = true
			if not Stamina.enabled(data.personal_objectives[next.objective_id]): unit.stamina = mini(100, unit.stamina + 18)
			next.last_events.append({"actor_id": id, "action": "defend", "code": "Guarded"})
		elif plan.action == "wait":
			if not Stamina.enabled(data.personal_objectives[next.objective_id]): unit.stamina = mini(100, unit.stamina + 25)
			next.last_events.append({"actor_id": id, "action": "wait", "code": "Rested"})
		elif plan.action == "skill" and data.personal_skills[plan.skill_id].kind == "guard":
			var skill: Dictionary = data.personal_skills[plan.skill_id]
			var counter_valid: Dictionary = Counter.validate_skill(skill)
			if not counter_valid.ok: return counter_valid
			unit.stamina -= skill.stamina_cost
			guards[id] = _mastered_bp(skill.defense_bp, unit.learned[plan.skill_id])
			if skill.get("guard_facing","") == "front": directional_guards[id] = true
			if skill.has("counter"): counter_declarations[id] = skill.counter.damage_bp
			next.last_events.append({"actor_id": id, "action": "skill", "skill_id": plan.skill_id, "code": "Guarded"})
	var damage_totals: Dictionary = {}
	var melee_hits: Dictionary = {}
	var status_effects: Array = []
	var primary_hits: Array = []
	for id in _ids(plans):
		var plan: Dictionary = plans[id]
		if plan.action != "attack" and not (plan.action == "skill" and data.personal_skills[plan.skill_id].kind == "strike"): continue
		var unit: Dictionary = next.units[id]
		var skill: Dictionary = data.personal_skills.get(plan.skill_id, {})
		var weapon: Dictionary = Strike.weapon(_weapon(unit,data),skill)
		# 一次尝试只设置一次装填；主目标/原地条件成立才发动并支付一次，附加目标不能资助失败主目标。
		unit.recovery_until = next.round + weapon.recovery_rounds + 1
		var selected: Dictionary = Strike.condition(skill,battle.units[id].position,plan.destination,unit.position)
		if selected.ok: selected = Area.select(next,data,id,plan.target_id,skill,weapon)
		if not selected.ok:
			next.last_events.append({"actor_id":id,"target_id":plan.target_id,"action":plan.action,"skill_id":plan.skill_id,"damage":0,"code":selected.code})
			continue
		unit.stamina -= weapon.stamina_cost if skill.is_empty() else skill.stamina_cost
		for selected_id in selected.target_ids:
			var target: Dictionary = next.units[selected_id]
			var event: Dictionary = {"actor_id":id,"target_id":selected_id,"action":plan.action,"skill_id":plan.skill_id,"damage":0}
			if not stream.is_empty():
				var chance: Dictionary = Hit.chance(unit,target,weapon,skill)
				if not chance.ok: return chance
				var sampled: Dictionary = Hit.draw(stream,chance.hit_bp)
				if not sampled.ok: return sampled
				stream = sampled.duel_stream
				event.hit_bp = chance.hit_bp; event.roll = sampled.roll
				if not sampled.hit:
					event.code = "Miss"; next.last_events.append(event); continue
			# 普通防御分别按每个目标的实际朝向处理；所有目标的伤害与状态仍留待同批应用。
			var guard_bp: int = guards.get(selected_id,10000)
			if directional_guards.has(selected_id): guard_bp = Guard.multiplier(unit.position,target.position,target.heading,guard_bp)
			var damage: int = _damage(unit,target,weapon,skill,data,next.round,guard_bp,data.personal_objectives[next.objective_id].get("facing_damage",{}))
			damage_totals[selected_id] = damage_totals.get(selected_id,0)+damage
			event.damage = damage; event.code = "Hit"; next.last_events.append(event)
			primary_hits.append(event)
			if weapon.category in ["sword","spear"]: melee_hits[selected_id] = true
			if not skill.is_empty() and skill.status != "none" and skill.status_rounds > 0:
				status_effects.append({"actor_id":id,"target_id":selected_id,"action":"status","status":skill.status,"expires_round":next.round+skill.status_rounds})
	var counters: Dictionary = Counter.select(next, data, counter_declarations, primary_hits)
	if not counters.ok: return counters
	for reaction in counters.reactions:
		var counter: Dictionary = next.units[reaction.actor_id]
		var target: Dictionary = next.units[reaction.target_id]
		var guard_bp: int = guards.get(reaction.target_id, 10000)
		if directional_guards.has(reaction.target_id): guard_bp = Guard.multiplier(counter.position, target.position, target.heading, guard_bp)
		# 明确实施约定：已有进攻/心法修正后乘40%，再进入目标减伤；不再套反击技能掌握倍率。
		var power: int = _scale(_attack_power(counter, target, _weapon(counter, data), {}, data, data.personal_objectives[next.objective_id].get("facing_damage", {})), reaction.damage_bp)
		var damage: int = _mitigate_damage(power, target, data, next.round, guard_bp)
		damage_totals[reaction.target_id] = damage_totals.get(reaction.target_id, 0) + damage
		melee_hits[reaction.target_id] = true
		next.last_events.append({"actor_id":reaction.actor_id, "target_id":reaction.target_id, "action":"counter", "skill_id":plans[reaction.actor_id].skill_id, "damage":damage, "code":"Hit"})
	for id in damage_totals: next.units[id].hp = maxi(0, next.units[id].hp - damage_totals[id])
	for effect in status_effects:
		var unit: Dictionary = next.units[effect.target_id]
		unit.statuses[effect.status] = maxi(unit.statuses.get(effect.status, -1), effect.expires_round)
		next.last_events.append(effect)
	var outcome: String = _outcome(next,data)
	if outcome.is_empty(): Withdrawal.apply_npcs(next,data,plans,melee_hits)
	if outcome.is_empty() and plans[next.controller_id].action == "withdraw":
		# 只认本轮实际成立的近战命中，不把计划、射空或远射记为缠斗。
		if not _at_exit(next, data, next.controller_id): next.last_events.append({"actor_id": next.controller_id, "action": "withdraw", "code": "ExitNotReached"})
		elif melee_hits.has(next.controller_id): next.last_events.append({"actor_id": next.controller_id, "action": "withdraw", "code": "WithdrawalMeleeHit"})
		else:
			var escort_ready: bool = true
			for id in _ids(next.units):
				var protected_unit: Dictionary = next.units[id]
				if protected_unit.side != next.units[next.controller_id].side or not protected_unit.protected: continue
				if not _at_exit(next, data, id) or melee_hits.has(id):
					escort_ready = false
					next.last_events.append({"actor_id": id, "action": "withdraw", "code": "WithdrawalMeleeHit" if melee_hits.has(id) else "ProtectedExitNotReached"})
			if escort_ready:
				Withdrawal.apply_group(next)
				outcome = "withdrawn"
	if outcome.is_empty(): outcome = _outcome(next,data)
	if outcome.is_empty() and next.round >= data.personal_objectives[next.objective_id].max_rounds: outcome = Objectives.at_limit(next,data.personal_objectives[next.objective_id],data.personal_battlefields[next.battlefield_id])
	if Stamina.enabled(data.personal_objectives[next.objective_id]):
		var defend_bonuses: Dictionary = {}
		for id in plans:
			if plans[id].action != "defend": continue
			var heart: Dictionary = _heart(next.units[id],data)
			if heart.has("defend_recovery_bonus"): defend_bonuses[id] = heart.defend_recovery_bonus
		Stamina.apply(next,plans,battle.units,defend_bonuses)
	if not outcome.is_empty(): return _finish(next,outcome,stream,data.personal_objectives[next.objective_id])
	next.round += 1
	for unit in next.units.values():
		if not Withdrawal.departed(unit): _prune(unit, next.round)
	next.ai_plans = make_ai(next, data)
	return _resolved(next, stream)

## 检查当前轮次所需上下文，不将坏引用当成空场地或替坏档修人物。
static func _battle_error(battle: Dictionary, data: Dictionary) -> String:
	if battle.get("phase") != "planning": return "BattleNotPlanning"
	if typeof(battle.get("round")) != TYPE_INT or battle.round < 1 or not battle.get("units") is Dictionary or not battle.get("ai_plans") is Dictionary: return "InvalidBattle"
	for key in ["items", "personal_skills", "personal_battlefields", "personal_objectives"]:
		if not data.get(key) is Dictionary: return "InvalidCombatData"
	if not data.personal_battlefields.has(battle.get("battlefield_id")) or not data.personal_objectives.has(battle.get("objective_id")): return "UnknownCombatContent"
	var ai_model: Dictionary = AIScore.validate_objective(data.personal_objectives[battle.objective_id])
	if not ai_model.ok: return ai_model.code
	var objective_model: Dictionary = Objectives.validate_objective(data.personal_objectives[battle.objective_id])
	if not objective_model.ok: return objective_model.code
	var stamina_model: Dictionary = Stamina.validate_objective(data.personal_objectives[battle.objective_id])
	if not stamina_model.ok: return stamina_model.code
	var hit_model: Dictionary = Hit.validate_objective(data.personal_objectives[battle.objective_id])
	if not hit_model.ok: return hit_model.code
	var facing: Dictionary = FacingDamage.validate_objective(data.personal_objectives[battle.objective_id])
	if not facing.ok: return facing.code
	if not battle.units.has(battle.get("controller_id")): return "MissingController"
	for id in battle.units:
		var unit: Variant = battle.units[id]
		if not id is String or not unit is Dictionary or unit.get("actor_id") != id or not _point(unit.get("position")): return "InvalidCombatant"
		for key in ["side", "heading", "radius", "hp", "initial_hp", "stamina", "martial", "recovery_until"]:
			if typeof(unit.get(key)) != TYPE_INT or unit[key] < 0: return "InvalidCombatant"
		if unit.side > 1 or unit.heading > 359 or unit.stamina > 100 or unit.hp > unit.initial_hp: return "InvalidCombatant"
		if not unit.get("weapon_id") is String or not unit.get("heart_id") is String or not unit.get("protected") is bool or not unit.get("learned") is Dictionary or not unit.get("statuses") is Dictionary: return "InvalidCombatant"
		var withdrawn: Dictionary = Withdrawal.validate_unit(battle,data,id)
		if not withdrawn.ok: return withdrawn.code
		var armor_snapshot: Dictionary = ArmorDefinition.snapshot(unit, data.items)
		if not armor_snapshot.ok: return armor_snapshot.code
		var hit_snapshot: Dictionary = Hit.validate_unit(unit, data.personal_objectives[battle.objective_id])
		if not hit_snapshot.ok: return hit_snapshot.code
		for skill_id in unit.learned:
			if not skill_id is String or typeof(unit.learned[skill_id]) != TYPE_INT or unit.learned[skill_id] < 0 or unit.learned[skill_id] > 100: return "InvalidMastery"
		for status in unit.statuses:
			if not status in ["slow", "exposed"] or typeof(unit.statuses[status]) != TYPE_INT: return "InvalidStatus"
	var objective_units: Dictionary = Objectives.validate_battle(battle,data.personal_objectives[battle.objective_id])
	return "" if objective_units.ok else objective_units.code

## 检查攻击来源和代价；未装备者、倒下者、同侧目标和未掌握功法不得构成攻击。
static func _attack_actor(battle: Dictionary, data: Dictionary, actor_id: String, target_id: String, skill_id: String) -> Dictionary:
	if not battle.units.has(actor_id): return _fail("UnknownCombatant")
	var unit: Dictionary = battle.units[actor_id]
	if unit.hp <= 0: return _fail("ActorDown")
	if Withdrawal.departed(unit): return _fail("ActorWithdrawn")
	if not battle.units.has(target_id) or battle.units[target_id].hp <= 0 or Withdrawal.departed(battle.units[target_id]): return _fail("InvalidTarget")
	if battle.units[target_id].side == unit.side: return _fail("FriendlyTarget")
	var weapon: Dictionary = _weapon(unit, data)
	if weapon.is_empty(): return _fail("NoWeapon")
	if _recovering(unit, battle.round): return _fail("WeaponRecovering")
	var skill: Dictionary = {}
	if not skill_id.is_empty():
		var learned: Dictionary = _skill(unit, data, skill_id)
		if not learned.ok: return learned
		skill = learned.skill
		if skill.kind != "strike": return _fail("NotAttackSkill")
	if unit.stamina < (weapon.stamina_cost if skill.is_empty() else skill.stamina_cost): return _fail("NotEnoughStamina")
	return {"ok": true, "weapon": Strike.weapon(weapon,skill), "skill": skill}

## 已掌握本领才可读取效果；学习记录不等于当前武器兼容，心法也使用同一资格规则。
static func _skill(unit: Dictionary, data: Dictionary, skill_id: String) -> Dictionary:
	if not data.personal_skills.has(skill_id): return _fail("UnknownSkill")
	if unit.learned.get(skill_id, 0) <= 0: return _fail("SkillNotLearned")
	var weapon: Dictionary = _weapon(unit, data)
	var skill: Dictionary = data.personal_skills[skill_id]
	var contract: Dictionary = Strike.validate_skill(skill)
	if not contract.ok: return contract
	contract = Area.validate_skill(skill)
	if not contract.ok: return contract
	contract = Guard.validate_skill(skill)
	if not contract.ok: return contract
	contract = Stamina.validate_skill(skill)
	if not contract.ok: return contract
	if weapon.is_empty() or not weapon.category in skill.weapons: return _fail("SkillWeaponMismatch")
	return {"ok": true, "skill": skill}

## 伤害只依赖公开人物、能力、当前状态和本轮已成立防御；预览与结算共享定点倍率及取整。
static func _damage(attacker: Dictionary, target: Dictionary, weapon: Dictionary, skill: Dictionary, data: Dictionary, current_round: int, guard_bp: int = 10000, facing: Dictionary = {}) -> int:
	return _mitigate_damage(_attack_power(attacker, target, weapon, skill, data, facing), target, data, current_round, guard_bp)

## 唯一进攻基数含既有主动招式和攻击心法修正；普通攻击、预览及反击共用，顺序不改。
static func _attack_power(attacker: Dictionary, target: Dictionary, weapon: Dictionary, skill: Dictionary, data: Dictionary, facing: Dictionary = {}) -> int:
	var result: int = maxi(1, weapon.damage + attacker.martial / 8 - target.martial / 20 - target.get("armor_defense", 0))
	if not skill.is_empty(): result = _scale(result, _mastered_bp(skill.power_bp, attacker.learned[skill.id]))
	var heart: Dictionary = _heart(attacker, data)
	if not heart.is_empty(): result = _scale(result, _mastered_bp(heart.power_bp, attacker.learned[heart.id]))
	return _scale(result, FacingDamage.multiplier(attacker, target, facing))

## 目标心法、暴露与本轮守势仍按原顺序取整；反击也不能跳过目标的真实减伤路径。
static func _mitigate_damage(power: int, target: Dictionary, data: Dictionary, current_round: int, guard_bp: int) -> int:
	var result: int = power
	var heart: Dictionary = _heart(target, data)
	if not heart.is_empty(): result = _scale(result, _mastered_bp(heart.defense_bp, target.learned[heart.id]))
	if _status(target, "exposed", current_round): result = _scale(result, 12500)
	return maxi(1, _scale(result, guard_bp))

## 移动采用武器基础距离、已掌握心法和本次技能加成；无武器人物仍能移动，迟缓持续期间减半。
static func _move_distance(unit: Dictionary, data: Dictionary, current_round: int, skill: Dictionary = {}) -> int:
	if skill.get("requires_stationary",false): return 0
	var distance: int = _weapon(unit, data).get("move_distance", 90)
	var heart: Dictionary = _heart(unit, data)
	if not heart.is_empty(): distance += heart.move_bonus * unit.learned[heart.id] / 100
	if not skill.is_empty(): distance += skill.move_bonus * unit.learned[skill.id] / 100
	if _status(unit, "slow", current_round): distance /= 2
	return distance

## 只有装备、已掌握且兼容武器的心法才作为被动；不会每轮暗扣其通用技能消耗字段。
static func _heart(unit: Dictionary, data: Dictionary) -> Dictionary:
	if unit.heart_id.is_empty(): return {}
	var learned: Dictionary = _skill(unit, data, unit.heart_id)
	return learned.skill if learned.ok and learned.skill.kind == "heart" else {}

## 武器定义从真实装备引用取得；空装备或无武器定义返回空字典，不临时制造拳脚武器。
static func _weapon(unit: Dictionary, data: Dictionary) -> Dictionary:
	return data.items.get(unit.weapon_id, {}).get("weapon", {})

## 装填截至 recovery_until 前一轮，零装填武器下一轮即可再用；装填时仍可原地防御等待。
static func _recovering(unit: Dictionary, current_round: int) -> bool:
	return current_round < unit.recovery_until

## 状态到期轮包含自身；施加后下一轮仍生效，已过期条目对查询没有效果。
static func _status(unit: Dictionary, status: String, current_round: int) -> bool:
	return unit.statuses.has(status) and unit.statuses[status] >= current_round

## 只清理已经过去的期限，保留仍生效状态供保存和下一轮预览。
static func _prune(unit: Dictionary, current_round: int) -> void:
	for status in unit.statuses.keys():
		if unit.statuses[status] < current_round: unit.statuses.erase(status)

## 保护目标倒下优先于双方死亡或歼敌；主角死亡不能被同轮击倒全部敌人掩盖。
static func _outcome(battle: Dictionary, data: Dictionary) -> String:
	return Objectives.current_outcome(battle,data.personal_objectives[battle.objective_id],data.personal_battlefields[battle.battlefield_id])

## 撤离以共同移动后的实际中心进入本方出口圆为准，计划终点抵达而被截停不能成功。
static func _at_exit(battle: Dictionary, data: Dictionary, actor_id: String) -> bool:
	return Withdrawal.at_exit(battle,data,actor_id)

## 锁定结果并丢弃未来意图；世界伤势、报酬与行程由会话拥有者另行一次性结算。
static func _finish(battle: Dictionary, outcome: String, duel_stream: Dictionary = {}, objective: Dictionary = {}) -> Dictionary:
	battle.phase = "result"; battle.outcome = outcome; battle.ai_plans = {}
	var event: Dictionary = Objectives.result_event(battle,objective)
	if not event.is_empty(): battle.last_events.append(event)
	battle.last_events.append({"action": "outcome", "code": outcome})
	return _resolved(battle, duel_stream)

## 新模型只返回随机副本供同一世界候选提交；旧目标不增加回执字段或隐含随机真值。
static func _resolved(battle: Dictionary, duel_stream: Dictionary) -> Dictionary:
	var result: Dictionary = {"ok":true, "battle":battle}
	if not duel_stream.is_empty(): result.duel_stream = duel_stream.duplicate(true)
	return result

## 攻击候选比较公开期望伤害；生存分支仍按合法最大伤害保守比较，两个查询都不读取随机流。
static func _expected_damage(preview: Dictionary) -> int:
	var total: int=0
	for target in preview.get("targets",[preview]):
		@warning_ignore("integer_division")
		total+=target.damage*target.get("hit_bp",10000)/10000
	return total

## 选择最近公开敌人，同距用稳定人物 ID；不偏爱玩家控制身份或读取未公开计划。
static func _nearest_enemy(battle: Dictionary, actor_id: String) -> String:
	var chosen: String = ""; var nearest: float = INF
	var unit: Dictionary = battle.units[actor_id]
	for id in _ids(battle.units):
		var target: Dictionary = battle.units[id]
		if target.hp <= 0 or Withdrawal.departed(target) or target.side == unit.side: continue
		var distance: float = _distance(unit.position, target.position)
		if distance < nearest: nearest = distance; chosen = id
	return chosen

## 普通与每个已学主动技能独立枚举自身射程内候选；原地射击不会依赖普通攻击先能命中，预期/最大伤害共用合法性。
## 生成现有普通/主动技能有限攻击候选，不按普通攻击先命中才开放技能区域。
static func _attack_plans(battle: Dictionary, data: Dictionary, actor_id: String, target_id: String) -> Array:
	var unit: Dictionary=battle.units[actor_id]
	var target: Dictionary=battle.units[target_id]
	var ordinary_point: Array=_ai_destination(battle,data,actor_id,target_id)
	var candidates: Array=[_plan(ordinary_point,_heading(ordinary_point,target.position,unit.heading),"attack",target_id)]
	for skill_id in _ids(unit.learned):
		var skill: Dictionary=data.personal_skills.get(skill_id,{})
		if skill.get("kind")!="strike" or not Strike.validate_skill(skill).ok or not Area.validate_skill(skill).ok: continue
		# 无新条件的旧技能保留原普通落点；只有显式扩展才增加自身候选，避免默改旧AI移动选择。
		var points: Array=[ordinary_point]
		if skill.get("requires_stationary",false): points=[unit.position.duplicate()]
		elif skill.has("range_modifier"):
			for point in [_ai_destination(battle,data,actor_id,target_id,skill),unit.position]:
				if not point in points: points.append(point)
		for point in points: candidates.append(_plan(point,_heading(point,target.position,unit.heading),"skill",target_id,skill_id))
	return candidates

## 旧AI与可见威胁共用同一有限攻击集合；不读取已保存ai_plans或玩家未提交计划。
static func _best_attack(battle: Dictionary, data: Dictionary, actor_id: String, target_id: String, expected: bool, aggregate: bool=true) -> Dictionary:
	var candidates: Array=_attack_plans(battle,data,actor_id,target_id)
	var best: Dictionary={}; var damage: int=-1
	for candidate in candidates:
		var preview: Dictionary=preview_attack(battle,data,actor_id,candidate.destination,candidate.heading,target_id,candidate.skill_id)
		if not preview.ok: continue
		var value: int=_expected_damage(preview) if expected else preview.damage
		if expected and not aggregate: value=mini(preview.damage,battle.units[target_id].hp)*preview.get("hit_bp",10000)*4/10000
		if value>damage:
			damage=value; best={"plan":candidate,"preview":preview}
	return best

## 用固定方向候选接近武器有效距离，弓与长枪过近会后退；障碍用同一几何检查，人员相遇留给同步碰撞。
static func _ai_destination(battle: Dictionary, data: Dictionary, actor_id: String, target_id: String, skill: Dictionary = {}) -> Array:
	var unit: Dictionary = battle.units[actor_id]; var target: Dictionary = battle.units[target_id]
	var weapon: Dictionary = Strike.weapon(_weapon(unit,data),skill)
	var arena: Dictionary = data.personal_battlefields[battle.battlefield_id]
	var limit: int = _move_distance(unit, data, battle.round,skill)
	var occupied: Array = []
	for id in battle.units:
		if id != actor_id and not Withdrawal.departed(battle.units[id]): occupied.append({"position": battle.units[id].position, "radius": battle.units[id].radius})
	var preferred: float = maxf(unit.radius + target.radius + 4, weapon.minimum_range + (weapon.maximum_range - weapon.minimum_range) * 0.55)
	var candidates: Array = [unit.position.duplicate()]
	var delta: Vector2 = Vector2(unit.position[0] - target.position[0], unit.position[1] - target.position[1])
	var desired: Vector2 = Vector2(target.position[0], target.position[1]) + delta.normalized() * preferred
	var origin: Vector2 = Vector2(unit.position[0], unit.position[1])
	var travel: Vector2 = (desired - origin).limit_length(limit)
	candidates.append([roundi(origin.x + travel.x), roundi(origin.y + travel.y)])
	for step in range(16):
		for fraction in [0.5, 1.0]:
			var offset: Vector2 = Vector2.from_angle(TAU * step / 16.0) * limit * fraction
			candidates.append([roundi(origin.x + offset.x), roundi(origin.y + offset.y)])
	var best: Array = unit.position.duplicate(); var score: float = INF
	for candidate in candidates:
		if not Geometry.can_move(arena, unit.position, candidate, limit, unit.radius, occupied).ok: continue
		var distance: float = _distance(candidate, target.position)
		var value: float = absf(distance - preferred) + _distance(candidate, unit.position) * 0.02
		if distance < weapon.minimum_range: value += 500
		if not Geometry.line_of_sight(arena, candidate, target.position): value += 200
		if value < score: best = candidate.duplicate(); score = value
	return best

## 规范化可保存计划，只复制已知字段，丢弃界面的临时预览数据。
static func _plan(destination: Array, heading: int, action: String, target_id: String = "", skill_id: String = "") -> Dictionary:
	return {"destination": destination.duplicate(), "heading": heading, "action": action, "target_id": target_id, "skill_id": skill_id}

## 熟练度从无加成线性靠近内容倍率，以万分比表达并采用整数运算。
static func _mastered_bp(value: int, mastery: int) -> int:
	return 10000 + (value - 10000) * mastery / 100

## 按万分比向下取整，伤害是否至少为一由最终伤害入口统一决定。
static func _scale(value: int, multiplier: int) -> int:
	return value * multiplier / 10000

## 以双精度求距离，避免与规则整数坐标共同平方时溢出。
static func _distance(a: Array, b: Array) -> float:
	return Vector2(float(a[0]) - b[0], float(a[1]) - b[1]).length()

## 朝向零度向右、九十度向下；同点时保留原朝向，不产生未定义角度。
static func _heading(from: Array, to: Array, fallback: int) -> int:
	if from == to: return fallback
	return posmod(roundi(rad_to_deg(atan2(float(to[1]) - from[1], float(to[0]) - from[0]))), 360)

## 返回稳定 ID 顺序，避免字典加载或插入顺序改变 AI 和伤害事件。
static func _ids(values: Dictionary) -> Array:
	var ids: Array = values.keys(); ids.sort(); return ids

## 坐标仅接受两个整数，拒绝小数或布尔经隐式转换进入已提交计划。
static func _point(value: Variant) -> bool:
	return value is Array and value.size() == 2 and typeof(value[0]) == TYPE_INT and typeof(value[1]) == TYPE_INT

## 返回可定位的稳定失败码，失败不输出半轮状态供调用方误提交。
static func _fail(code: String) -> Dictionary:
	return {"ok": false, "code": code}
