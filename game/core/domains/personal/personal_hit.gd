## 个人战显式skill_v1候选命中模型；纯查询与随机副本推进分开，旧目标不启用也不消费duel流。
extends RefCounted
const Levels=preload("res://core/domains/cultivation/skill_levels.gd")
const Values=preload("res://core/base/value_checks.gd")
const MODEL: String="skill_v1"
const SNAPSHOT_SKILLS: Array[String]=["martial","archery"]

## 显式未知模型不能落回必中；缺省保持历史目标的原行为。
static func validate_objective(objective: Dictionary, path: String="") -> Dictionary:
	if objective.has("hit_model") and (typeof(objective.hit_model)!=TYPE_STRING or objective.hit_model!=MODEL): return {"ok":false,"code":"PERSONAL_HIT_MODEL","path":path+"/hit_model"}
	return {"ok":true}

## 命中修正只属于主动进攻技能，缺省零；不读取任意表达式或把守势当主动攻击。
static func validate_skill(skill: Dictionary, path: String="") -> Dictionary:
	if skill.has("hit_bonus_bp") and (skill.get("kind")!="strike" or not Values.whole(skill.hit_bonus_bp,-10000,10000)): return {"ok":false,"code":"PERSONAL_HIT_BONUS","path":path+"/hit_bonus_bp"}
	return {"ok":true}

## 检查作者是否显式启用新模型，不按目标或人物ID白名单开启。
static func enabled(objective: Dictionary) -> bool:
	return objective.get("hit_model")==MODEL

## 等级仅从唯一普通技艺XP服务派生；无效或缺失XP不能由人物属性替代。
static func snapshot(actor: Dictionary) -> Dictionary:
	var levels: Dictionary={}
	var experience: Variant=actor.get("skills")
	if not experience is Dictionary: return {"ok":false,"code":"InvalidHitSkillExperience"}
	for skill_id in SNAPSHOT_SKILLS:
		var level: int=Levels.level(experience.get(skill_id))
		if level<0: return {"ok":false,"code":"InvalidHitSkillExperience","skill_id":skill_id}
		levels[skill_id]=level
	return {"ok":true,"skill_levels":levels}

## 入场前纯核所有参与者和已有唯一duel流；失败发生在费用与实例编号写入前。
static func prepare(state: Dictionary, objective: Dictionary, participants: Array) -> Dictionary:
	var checked: Dictionary=validate_objective(objective)
	if not checked.ok: return checked
	var snapshots: Dictionary={}
	if not enabled(objective): return {"ok":true,"snapshots":snapshots}
	var streams: Variant=state.get("rng")
	if not streams is Dictionary: return {"ok":false,"code":"InvalidDuelRandomState"}
	var stream: Dictionary=validate_stream(streams.get("duel"))
	if not stream.ok: return stream
	for actor_id in participants:
		if not state.actors.has(actor_id): return {"ok":false,"code":"UnknownCombatant"}
		var captured: Dictionary=snapshot(state.actors[actor_id])
		if not captured.ok: return captured
		snapshots[actor_id]=captured.skill_levels
	return {"ok":true,"snapshots":snapshots}

## 新快照只含武艺与弓弩两项派生等级；不是新的可写人物技能账。
static func valid_levels(value: Variant) -> bool:
	if not Values.fields(value,{"martial":TYPE_INT,"archery":TYPE_INT}) or value.size()!=2: return false
	return Values.whole(value.martial,0,4) and Values.whole(value.archery,0,4)

## 新模型必须有明确快照，旧模型不得偷偷增添该字段；仅planning核当前XP来源，历史结果不追改。
static func validate_unit(unit: Dictionary, objective: Dictionary, actor: Dictionary={}, check_source: bool=false) -> Dictionary:
	var model: Dictionary=validate_objective(objective)
	if not model.ok: return model
	if not enabled(objective): return {"ok":false,"code":"UnexpectedPersonalHitSnapshot"} if unit.has("skill_levels") else {"ok":true}
	if not valid_levels(unit.get("skill_levels")): return {"ok":false,"code":"InvalidPersonalHitSnapshot"}
	if check_source:
		var expected: Dictionary=snapshot(actor)
		if not expected.ok or expected.skill_levels!=unit.skill_levels: return {"ok":false,"code":"PersonalHitSourceMismatch"}
	return {"ok":true}

## 既有候选公式仅用于显式模型：7500起点、等级差200、有效武力差20及技能修正，限制15%—95%，尚待调平。
static func chance(attacker: Dictionary, target: Dictionary, weapon: Dictionary, skill: Dictionary={}) -> Dictionary:
	if not valid_levels(attacker.get("skill_levels")) or not valid_levels(target.get("skill_levels")): return {"ok":false,"code":"InvalidPersonalHitSnapshot"}
	if not Values.whole(attacker.get("martial"),1,110) or not Values.whole(target.get("martial"),1,110): return {"ok":false,"code":"InvalidHitCombatant"}
	if not weapon.get("category") in ["sword","spear","bow"]: return {"ok":false,"code":"InvalidHitWeapon"}
	var checked: Dictionary=validate_skill(skill)
	if not checked.ok: return checked
	var attack_skill: String="archery" if weapon.category=="bow" else "martial"
	var chance_bp: int=7500+200*(attacker.skill_levels[attack_skill]-target.skill_levels.martial)+20*(attacker.martial-target.martial)+skill.get("hit_bonus_bp",0)
	return {"ok":true,"hit_bp":clampi(chance_bp,1500,9500)}

## 与已有世界随机合同一样只收规范十进制64位字符串；不产生新种子或悄悄修复坏流。
static func validate_stream(value: Variant) -> Dictionary:
	if not Values.fields(value,{"seed":TYPE_STRING,"state":TYPE_STRING}): return {"ok":false,"code":"InvalidDuelRandomState"}
	for key in ["seed","state"]:
		var number: String=value[key]
		if not number.is_valid_int() or str(number.to_int())!=number: return {"ok":false,"code":"InvalidDuelRandomState"}
	return {"ok":true}

## 每次真正出手仅抽一个0—9999结果并返回duel副本；输入不变，是否提交由个人战候选拥有者决定。
static func draw(stream: Dictionary, hit_bp: int) -> Dictionary:
	var checked: Dictionary=validate_stream(stream)
	if not checked.ok: return checked
	if not Values.whole(hit_bp,0,10000): return {"ok":false,"code":"InvalidHitChance"}
	var rng=RandomNumberGenerator.new()
	rng.seed=int(stream.seed); rng.state=int(stream.state)
	var roll: int=rng.randi_range(0,9999)
	var next: Dictionary=stream.duplicate(true); next.state=str(rng.state)
	return {"ok":true,"hit":roll<hit_bp,"roll":roll,"duel_stream":next}
