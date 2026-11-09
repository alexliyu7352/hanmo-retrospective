## 个人战定义的跨记录语义校验；不替代结构 Schema，不建立装备、技能或世界实例。
extends RefCounted
const AIScore=preload("res://core/domains/personal/personal_ai_score.gd")
const Objectives=preload("res://core/domains/personal/personal_objectives.gd")
const Guard=preload("res://core/domains/personal/personal_guard.gd")
const Area=preload("res://core/domains/personal/personal_area.gd")
const Strike=preload("res://core/domains/personal/personal_strike.gd")
const Stamina=preload("res://core/domains/personal/personal_stamina.gd")
const Geometry=preload("res://core/personal_geometry.gd")
const Counter=preload("res://core/domains/personal/personal_counter.gd")
const FacingDamage=preload("res://core/domains/personal/personal_facing_damage.gd")
const Hit=preload("res://core/domains/personal/personal_hit.gd")
const ArmorDefinition=preload("res://core/domains/personal/personal_armor_content.gd")
const BODY_RADIUS: int=18
const BAG_CAPACITY: int=20
const TEACH_MASTERY: int=80

## 对已经通过结构检查的索引表复核关系；旧包没有新表时直接兼容且不注入缺省字段。
static func validate(data: Dictionary) -> Dictionary:
	for item in data.get("items",{}).values():
		var armor: Dictionary=ArmorDefinition.validate_item(item,"/items/"+item.id)
		if not armor.ok: return armor
		if item.has("weapon") and (item.weapon.minimum_range>item.weapon.maximum_range or item.kind!="commodity"):
			return _fail("PERSONAL_WEAPON_RANGE","/items/"+item.id+"/weapon")
	for skill in data.get("personal_skills",{}).values():
		var facing_guard: Dictionary=Guard.validate_skill(skill,"/personal_skills/"+skill.id)
		if not facing_guard.ok: return facing_guard
		var recovery: Dictionary=Stamina.validate_skill(skill,"/personal_skills/"+skill.id)
		if not recovery.ok: return recovery
		var strike: Dictionary=Strike.validate_skill(skill,"/personal_skills/"+skill.id)
		if not strike.ok: return strike
		var area: Dictionary=Area.validate_skill(skill,"/personal_skills/"+skill.id)
		if not area.ok: return area
		var hit: Dictionary=Hit.validate_skill(skill,"/personal_skills/"+skill.id)
		if not hit.ok: return hit
		var counter: Dictionary=Counter.validate_skill(skill,"/personal_skills/"+skill.id)
		if not counter.ok: return counter
		if (skill.status=="none")!=(skill.status_rounds==0): return _fail("PERSONAL_STATUS_DURATION","/personal_skills/"+skill.id+"/status_rounds")
		var seen: Array=[]
		for weapon in skill.weapons:
			if weapon in seen: return _fail("PERSONAL_DUPLICATE_WEAPON","/personal_skills/"+skill.id+"/weapons")
			seen.append(weapon)
	for objective in data.get("personal_objectives",{}).values():
		var ai_model: Dictionary=AIScore.validate_objective(objective,"/personal_objectives/"+objective.id)
		if not ai_model.ok: return ai_model
		var escort_model: Dictionary=Objectives.validate_objective(objective,"/personal_objectives/"+objective.id)
		if not escort_model.ok: return escort_model
		var stamina: Dictionary=Stamina.validate_objective(objective,"/personal_objectives/"+objective.id)
		if not stamina.ok: return stamina
		var hit: Dictionary=Hit.validate_objective(objective,"/personal_objectives/"+objective.id)
		if not hit.ok: return hit
		var facing: Dictionary=FacingDamage.validate_objective(objective,"/personal_objectives/"+objective.id)
		if not facing.ok: return facing
	for arena in data.get("personal_battlefields",{}).values():
		var result: Dictionary=_battlefield(arena,"/personal_battlefields/"+arena.id)
		if not result.ok: return result
	for actor in data.get("actors",{}).values():
		if data.has("active_static_actor_ids") and not actor.id in data.active_static_actor_ids: continue
		var result: Dictionary=_actor(actor,data,"/actors/"+actor.id)
		if not result.ok: return result
	for role in data.get("roles",{}).values():
		var result: Dictionary=_role(role,data,"/roles/"+role.id)
		if not result.ok: return result
	return {"ok":true}

## 用正式几何检查出生身体和退出中心；双方均有入口，退出触发区可重合或触及场地边缘。
static func _battlefield(arena: Dictionary, path: String) -> Dictionary:
	for number in range(arena.obstacles.size()):
		var obstacle: Dictionary=arena.obstacles[number]
		if obstacle.x+obstacle.width>arena.width or obstacle.y+obstacle.height>arena.height:
			return _fail("PERSONAL_OBSTACLE_BOUNDS",path+"/obstacles/"+str(number))
	for field in ["spawns","exits"]:
		var sides: Array=[]
		for number in range(arena[field].size()):
			var row: Dictionary=arena[field][number]
			if not row.side in sides: sides.append(row.side)
			if not Geometry.point_valid(arena,row.position,BODY_RADIUS): return _fail("PERSONAL_POINT_BLOCKED",path+"/"+field+"/"+str(number)+"/position")
			if field=="spawns":
				for previous in range(number):
					var other: Array=arena.spawns[previous].position
					var dx: int=row.position[0]-other[0]; var dy: int=row.position[1]-other[1]
					if dx*dx+dy*dy<(BODY_RADIUS*2)*(BODY_RADIUS*2): return _fail("PERSONAL_POINT_OVERLAP",path+"/spawns/"+str(number)+"/position")
		if sides.size()!=2 or not 0 in sides or not 1 in sides: return _fail("PERSONAL_BOTH_SIDES",path+"/"+field)
	return {"ok":true}

## 武器和护具均接受本人初始背包实物；显式空武器允许仅穿护具，敌对来源与切磋提供者另守武装条件。
static func _actor(actor: Dictionary, data: Dictionary, path: String) -> Dictionary:
	var goods: Dictionary={}; var weight: int=0
	var initial: Array=actor.get("initial_goods",[])
	for number in range(initial.size()):
		var good: Dictionary=initial[number]
		var at: String=path+"/initial_goods/"+str(number)+"/item_id"
		if not data.get("items",{}).has(good.item_id): return _fail("PERSONAL_REFERENCE",at)
		if goods.has(good.item_id): return _fail("PERSONAL_DUPLICATE_GOODS",at)
		goods[good.item_id]=good.quantity
		weight+=good.quantity*data.items[good.item_id].weight
	if weight>BAG_CAPACITY: return _fail("PERSONAL_BAG_CAPACITY",path+"/initial_goods")
	if not actor.get("combat",{}).is_empty():
		var combat: Dictionary=actor.combat
		if not combat.get("weapon_id") is String: return _fail("PERSONAL_REFERENCE",path+"/combat/weapon_id")
		if not combat.weapon_id.is_empty():
			if not data.get("items",{}).has(combat.weapon_id): return _fail("PERSONAL_REFERENCE",path+"/combat/weapon_id")
			if not data.items[combat.weapon_id].has("weapon") or goods.get(combat.weapon_id,0)<1: return _fail("PERSONAL_WEAPON_OWNERSHIP",path+"/combat/weapon_id")
		var armor: Dictionary=ArmorDefinition.validate_initial(combat.get("armor_id",""),data.get("items",{}),goods,path+"/combat/armor_id")
		if not armor.ok: return armor
		var learned: Array=[]
		for number in range(combat.learned.size()):
			var skill_id: String=combat.learned[number].skill_id
			var at: String=path+"/combat/learned/"+str(number)+"/skill_id"
			if not data.get("personal_skills",{}).has(skill_id): return _fail("PERSONAL_REFERENCE",at)
			if skill_id in learned: return _fail("PERSONAL_DUPLICATE_SKILL",at)
			learned.append(skill_id)
	if actor.has("road_conflict"):
		if actor.get("combat",{}).get("weapon_id","").is_empty(): return _fail("PERSONAL_COMBAT_REQUIRED",path+"/road_conflict")
		return _battle_link(actor.road_conflict,data,path+"/road_conflict",false)
	return {"ok":true}

## 当面切磋和授课必须具备真实条款、教师能力及收费授权；不把岗位名称当作自动权限。
static func _role(role: Dictionary, data: Dictionary, path: String) -> Dictionary:
	var lessons: Array=role.get("lessons",[])
	var courses: Array=role.get("skill_courses",[])
	if (not courses.is_empty())!=("study_skill" in role.services): return _fail("SKILL_STUDY_AUTHORITY",path+"/skill_courses")
	var course_ids: Array=[]
	for number in range(courses.size()):
		var course: Dictionary=courses[number]; var at: String=path+"/skill_courses/"+str(number)
		if course.skill_id in course_ids: return _fail("SKILL_STUDY_DUPLICATE",at+"/skill_id")
		course_ids.append(course.skill_id)
		var wallet: Dictionary=_wallet(role,course.wallet_id,data,at+"/wallet_id")
		if not wallet.ok: return wallet
	if role.has("sparring")!=("duel_start" in role.services): return _fail("PERSONAL_SERVICE_AUTHORITY",path+"/sparring")
	if (not lessons.is_empty())!=("learn_skill" in role.services): return _fail("PERSONAL_SERVICE_AUTHORITY",path+"/lessons")
	if not role.has("sparring") and lessons.is_empty() and courses.is_empty(): return {"ok":true}
	if not data.get("actors",{}).has(role.initial_actor): return _fail("PERSONAL_REFERENCE",path+"/initial_actor")
	var actor: Dictionary=data.actors[role.initial_actor]
	if actor.initial_place!=role.place_id: return _fail("PERSONAL_PROVIDER_ABSENT",path+"/place_id")
	if role.has("sparring"):
		if actor.get("combat",{}).get("weapon_id","").is_empty(): return _fail("PERSONAL_COMBAT_REQUIRED",path+"/sparring")
		var result: Dictionary=_battle_link(role.sparring,data,path+"/sparring",true)
		if not result.ok: return result
		result=_wallet(role,role.sparring.wallet_id,data,path+"/sparring/wallet_id")
		if not result.ok: return result
	var learned: Dictionary={}; var seen: Array=[]
	for row in actor.get("combat",{}).get("learned",[]): learned[row.skill_id]=row.mastery
	for number in range(lessons.size()):
		var lesson: Dictionary=lessons[number]; var at: String=path+"/lessons/"+str(number)
		if not data.get("personal_skills",{}).has(lesson.skill_id): return _fail("PERSONAL_REFERENCE",at+"/skill_id")
		if learned.get(lesson.skill_id,0)<TEACH_MASTERY: return _fail("PERSONAL_TEACHER_MASTERY",at+"/skill_id")
		if lesson.skill_id in seen: return _fail("PERSONAL_DUPLICATE_LESSON",at+"/skill_id")
		seen.append(lesson.skill_id)
		var result: Dictionary=_wallet(role,lesson.wallet_id,data,at+"/wallet_id")
		if not result.ok: return result
	return {"ok":true}

## 已知场地须配合正确的切磋或危险目标；护卫至少保留两个己方出生位，不能用同点替身充数。
static func _battle_link(terms: Dictionary, data: Dictionary, path: String, sparring: bool) -> Dictionary:
	if not data.get("personal_battlefields",{}).has(terms.battlefield_id): return _fail("PERSONAL_REFERENCE",path+"/battlefield_id")
	if not data.get("personal_objectives",{}).has(terms.objective_id): return _fail("PERSONAL_REFERENCE",path+"/objective_id")
	var objective: Dictionary=data.personal_objectives[terms.objective_id]
	if (objective.kind=="sparring")!=sparring: return _fail("PERSONAL_OBJECTIVE_KIND",path+"/objective_id")
	if objective.kind=="escort":
		var friendly: int=0
		for spawn in data.personal_battlefields[terms.battlefield_id].spawns:
			if spawn.side==0: friendly+=1
		if friendly<2: return _fail("PERSONAL_ESCORT_SPAWNS",path+"/battlefield_id")
	return {"ok":true}

## 验证费用接收钱包属于岗位代理的公家或本人；免费条款也不能引用外人钱包。
static func _wallet(role: Dictionary, wallet_id: String, data: Dictionary, path: String) -> Dictionary:
	if not data.get("wallets",{}).has(wallet_id): return _fail("PERSONAL_REFERENCE",path)
	var owner: Dictionary={"kind":"actor","id":role.initial_actor} if role.organization_id==null else {"kind":"organization","id":role.organization_id}
	if data.wallets[wallet_id].owner!=owner: return _fail("PERSONAL_FEE_AUTHORITY",path)
	return {"ok":true}

## 保留索引字段路径与稳定错误码，调用方据此拒绝内容；不修补定义后继续运行。
static func _fail(code: String, path: String) -> Dictionary:
	return {"ok":false,"code":code,"path":path}
