## 用真实引擎验证个人战轮次的空间、技能、护卫与确定性边界，不写世界或玩家存档。
extends SceneTree
const Rules = preload("res://core/personal_round.gd")
const Strict = preload("res://core/strict_json.gd")
var checks: int = 0
var failures: int = 0

## 初始化完成后执行独立规则用例，避免场景启动顺序影响结果。
func _init() -> void:
	call_deferred("run_checks")

## 汇总实际断言，失败保留具体标签且最终返回非零退出码。
func check(value: bool, label: String) -> void:
	checks += 1
	if not value: failures += 1; printerr("FAIL: ", label)

## 建立与作者字段一致的两场地和三武器，能力数值故意不同以检验配置复用。
func content() -> Dictionary:
	var skills: Dictionary = {}
	for row in [["skill.strike", "strike", 20000, 10000, 0, "slow", 1], ["skill.guard", "guard", 10000, 2000, 0, "none", 0], ["skill.heart", "heart", 12000, 8000, 20, "none", 0], ["skill.expose", "strike", 13000, 10000, 10, "exposed", 2]]:
		skills[row[0]] = {"id": row[0], "kind": row[1], "weapons": ["sword", "spear"], "stamina_cost": 15, "power_bp": row[2], "defense_bp": row[3], "move_bonus": row[4], "status": row[5], "status_rounds": row[6]}
	var data: Dictionary = {"items": {}, "personal_skills": skills, "personal_battlefields": {}, "personal_objectives": {"objective.spar": {"kind": "sparring", "max_rounds": 10, "allow_withdrawal": true}, "objective.escort": {"kind": "escort", "max_rounds": 6, "allow_withdrawal": true}}}
	for row in [["sword", 0, 70, 120, 18, 10, 0, 90], ["spear", 45, 135, 70, 22, 12, 0, 80], ["bow", 100, 400, 80, 25, 14, 1, 70]]:
		data.items["item." + row[0]] = {"weapon": {"category": row[0], "minimum_range": row[1], "maximum_range": row[2], "arc_degrees": row[3], "damage": row[4], "stamina_cost": row[5], "recovery_rounds": row[6], "move_distance": row[7]}}
	data.personal_battlefields["field.yard"] = {"width": 600, "height": 400, "obstacles": [], "exits": [{"side": 0, "position": [30, 200], "radius": 30}, {"side": 1, "position": [570, 200], "radius": 30}]}
	data.personal_battlefields["field.ford"] = {"width": 800, "height": 500, "obstacles": [{"x": 320, "y": 160, "width": 40, "height": 120, "blocks_movement": true, "blocks_sight": true}], "exits": [{"side": 0, "position": [30, 400], "radius": 30}, {"side": 1, "position": [760, 400], "radius": 30}]}
	return data

## 用任意人物 ID 建立真实参战者形状，测试不借助初始剧本人物名。
func unit(id: String, side: int, position: Array, weapon: String = "item.sword") -> Dictionary:
	return {"actor_id": id, "side": side, "position": position, "heading": 0 if side == 0 else 180, "radius": 18, "hp": 100, "initial_hp": 100, "stamina": 100, "weapon_id": weapon, "martial": 40, "learned": {}, "heart_id": "", "statuses": {}, "recovery_until": 0, "protected": false}

## 建立可保存会话，默认敌人等待，便于把规则差异和 AI 策略分开检查。
func battle() -> Dictionary:
	var result: Dictionary = {"id": "battle.test", "controller_id": "actor.p", "battlefield_id": "field.yard", "objective_id": "objective.spar", "round": 1, "phase": "planning", "units": {"actor.p": unit("actor.p", 0, [200, 200]), "actor.e": unit("actor.e", 1, [260, 200])}, "ai_plans": {}, "outcome": "", "last_events": []}
	wait_ai(result)
	return result

## 固定敌方公开意图为原地等待，测试者仍通过正常 resolve 接口提交玩家计划。
func wait_ai(state: Dictionary) -> void:
	state.ai_plans = {}
	for id in state.units:
		if id != state.controller_id and state.units[id].hp > 0: state.ai_plans[id] = plan(state.units[id], "wait")

## 创建界面所需最小参数；不直接修改会话中的行动、伤害或结局。
func plan(actor: Dictionary, action: String, target_id: String = "", skill_id: String = "") -> Dictionary:
	return {"destination": actor.position.duplicate(), "heading": actor.heading, "action": action, "target_id": target_id, "skill_id": skill_id}

## 执行所有用例并报告真实计数；数量只表示规则覆盖，不冒充图形试玩。
func run_checks() -> void:
	check_space_and_input()
	check_ai_and_simultaneous()
	check_skills_and_expiry()
	check_objectives_and_withdrawal()
	check_recovery_and_replay()
	check_ai_full_battles()
	print("PERSONAL_ROUND_CHECKS=", checks, " FAILURES=", failures)
	quit(0 if failures == 0 else 1)

## 验证内外射程、朝向、障碍以及未装备和非法输入不会产生攻击。
func check_space_and_input() -> void:
	var data: Dictionary = content(); var state: Dictionary = battle()
	var preview: Dictionary = Rules.preview_attack(state, data, "actor.p", [200, 200], 0, "actor.e")
	check(Rules.movement_distance(state, data, "actor.p") == 90, "公开移动查询复用实际武器距离")
	check(Rules.movement_distance(state, data, "actor.missing") == 0 and Rules.movement_distance(state, data, "actor.p", "skill.missing") == 0, "非法人物与技能没有可移动范围")
	check(preview.ok and preview.damage == 21, "刀剑采用真实武器和武艺计算确定伤害")
	check(Rules.preview_attack(state, data, "actor.p", [200, 200], 180, "actor.e").get("code") == "WrongFacing", "背向目标不能命中")
	state.units["actor.p"].weapon_id = "item.bow"
	check(Rules.preview_attack(state, data, "actor.p", [200, 200], 0, "actor.e").get("code") == "InsideMinimumRange", "弓近身攻击受限")
	state.units["actor.e"].position = [400, 200]
	check(Rules.preview_attack(state, data, "actor.p", [200, 200], 0, "actor.e").ok, "弓能射及刀剑不可达的远处")
	state.units["actor.p"].weapon_id = "item.sword"
	check(Rules.preview_attack(state, data, "actor.p", [200, 200], 0, "actor.e").get("code") == "OutOfRange", "刀剑不继承弓的射程")
	state.units["actor.p"].weapon_id = "item.spear"; state.units["actor.e"].position = [240, 200]
	check(Rules.preview_attack(state, data, "actor.p", [200, 200], 0, "actor.e").get("code") == "InsideMinimumRange", "长兵有独立的近身空隙")
	state.units["actor.e"].position = [300, 200]
	check(Rules.preview_attack(state, data, "actor.p", [200, 200], 0, "actor.e").ok, "长兵中距攻击真实成立")
	state.battlefield_id = "field.ford"; state.units["actor.p"].weapon_id = "item.bow"; state.units["actor.e"].position = [450, 200]
	check(Rules.preview_attack(state, data, "actor.p", [200, 200], 0, "actor.e").get("code") == "LineBlocked", "第二场地障碍挡住远射视线")
	state = battle(); state.units["actor.p"].weapon_id = ""
	check(Rules.validate_plan(state, data, "actor.p", plan(state.units["actor.p"], "attack", "actor.e")).get("code") == "NoWeapon", "未装备不凭空生成攻击")
	var move: Dictionary = plan(state.units["actor.p"], "defend"); move.destination = [150, 200]
	check(Rules.validate_plan(state, data, "actor.p", move).get("code") == "DefendRequiresStationary", "普通防御只能原地转向，不因未装备而例外")
	move.action = "wait"
	check(Rules.validate_plan(state, data, "actor.p", move).ok, "未装备人物仍可正常移动等待")
	move.destination = [150.0, 200]
	check(Rules.validate_plan(state, data, "actor.p", move).get("code") == "InvalidPlan", "小数坐标不能进入轮次")
	move.destination = [150, 200]; move.heading = 360
	check(Rules.validate_plan(state, data, "actor.p", move).get("code") == "InvalidPlan", "界面越界朝向明确失败")
	state = battle(); state.units["actor.p"].stamina = 0
	check(Rules.validate_plan(state, data, "actor.p", plan(state.units["actor.p"], "attack", "actor.e")).get("code") == "NotEnoughStamina", "气势不足不能提交攻击")
	check(Rules.validate_plan(state, data, "actor.p", plan(state.units["actor.p"], "attack", "actor.p")).get("code") == "FriendlyTarget", "同侧不能攻击自己")

## 保存 AI 的意图不受玩家草案影响，双方致命攻击在同一伤害阶段成立。
func check_ai_and_simultaneous() -> void:
	var data: Dictionary = content(); var state: Dictionary = battle()
	var ai: Dictionary = Rules.make_ai(state, data)
	check(ai.size() == 1 and not ai.has("actor.p") and Rules.validate_plan(state, data, "actor.e", ai["actor.e"]).ok, "AI只给非控制者选择合法意图")
	var hidden: Dictionary = state.duplicate(true); hidden.player_draft = {"destination": [20, 20], "action": "withdraw"}; hidden.ai_plans = {"untrusted": {"attack": "future"}}
	check(Rules.make_ai(hidden, data) == ai, "AI不读取草案或已有意图字典")
	state.ai_plans = ai.duplicate(true)
	var moving: Dictionary = plan(state.units["actor.p"], "wait"); moving.destination = [120, 200]
	var result: Dictionary = Rules.resolve(state, data, moving)
	check(result.ok and result.battle.units["actor.e"].position == ai["actor.e"].destination, "结算使用保存的AI目标而非追读玩家新位置")
	check(result.ok and result.battle.units["actor.p"].hp == 100, "玩家离开射程后敌方已提交攻击不能隔空命中")
	check(result.ok and result.battle.units["actor.e"].stamina == 100, "共同移动后目标脱离射程，未发动进攻不支付气势")
	state = battle(); state.units["actor.p"].hp = 10; state.units["actor.e"].hp = 10
	state.ai_plans["actor.e"] = plan(state.units["actor.e"], "attack", "actor.p")
	result = Rules.resolve(state, data, plan(state.units["actor.p"], "attack", "actor.e"))
	check(result.ok and result.battle.units["actor.p"].hp == 0 and result.battle.units["actor.e"].hp == 0, "同阶段致命伤害同时回写，不按ID抢先取消反击")
	check(result.ok and result.battle.outcome == "defeat" and result.battle.phase == "result", "双方倒下以主角败北优先于歼敌")
	state = battle(); state.ai_plans = {}
	check(Rules.resolve(state, data, plan(state.units["actor.p"], "wait")).get("code") == "MissingAIPlan", "缺失保存AI意图不能现场重新抽取")
	state = battle(); state.ai_plans["actor.e"].destination = [599, 200]
	check(Rules.resolve(state, data, plan(state.units["actor.p"], "wait")).get("code") == "InvalidAIPlan", "损坏AI意图原子拒绝")
	state = battle(); state.ai_plans["actor.p"] = plan(state.units["actor.p"], "wait")
	check(Rules.resolve(state, data, plan(state.units["actor.p"], "wait")).get("code") == "UnexpectedAIPlan", "AI不能占据玩家控制槽")

## 实际学习程度、技能气势、心法和下一轮状态都有可观测效果，到期以后停止影响规则。
func check_skills_and_expiry() -> void:
	var data: Dictionary = content(); var state: Dictionary = battle()
	var attack: Dictionary = plan(state.units["actor.p"], "skill", "actor.e", "skill.strike")
	check(Rules.validate_plan(state, data, "actor.p", attack).get("code") == "SkillNotLearned", "未掌握本领不能使用")
	state.units["actor.p"].learned["skill.strike"] = 10
	var partial: Dictionary = Rules.preview_attack(state, data, "actor.p", [200, 200], 0, "actor.e", "skill.strike")
	state.units["actor.p"].learned["skill.strike"] = 100
	check(Rules.movement_distance(state, data, "actor.p", "skill.strike") == 90, "合法主动技能查询保留其实际移动配置")
	var full: Dictionary = Rules.preview_attack(state, data, "actor.p", [200, 200], 0, "actor.e", "skill.strike")
	check(partial.ok and full.ok and full.damage > partial.damage and partial.damage > 21, "修炼掌握程度改变真实伤害")
	var result: Dictionary = Rules.resolve(state, data, attack)
	check(result.ok and result.battle.units["actor.e"].hp == 100 - full.damage, "相同静态目标预览和实际伤害一致")
	check(result.ok and result.battle.units["actor.p"].stamina == 85, "技能采用一次技能气势成本，不重复叠扣普攻")
	check(result.ok and result.battle.round == 2 and result.battle.units["actor.e"].statuses.slow == 2, "一轮迟缓在接下来的完整轮有效")
	state = result.battle
	var slow_move: Dictionary = plan(state.units["actor.e"], "wait"); slow_move.destination = [306, 200]
	check(Rules.validate_plan(state, data, "actor.e", slow_move).get("code") == "MoveTooFar", "迟缓实际减半移动距离")
	wait_ai(state); result = Rules.resolve(state, data, plan(state.units["actor.p"], "wait")); state = result.battle
	check(result.ok and state.round == 3 and not state.units["actor.e"].statuses.has("slow"), "状态期限结束后删除已过期标记")
	check(Rules.validate_plan(state, data, "actor.e", slow_move).ok, "到期后恢复完整移动能力")
	state = battle(); state.units["actor.p"].learned["skill.heart"] = 100; state.units["actor.p"].heart_id = "skill.heart"
	var heart: Dictionary = Rules.preview_attack(state, data, "actor.p", [200, 200], 0, "actor.e")
	check(heart.ok and heart.damage > 21, "装备已掌握心法自动增强攻击")
	var long_move: Dictionary = plan(state.units["actor.p"], "wait"); long_move.destination = [100, 200]
	check(Rules.validate_plan(state, data, "actor.p", long_move).ok, "心法移动加成改变实际可达距离")
	check(Rules.movement_distance(state, data, "actor.p") == 110 and Rules.movement_distance(state, data, "actor.p", "skill.heart") == 0, "移动圈包含已装备心法，但心法不能伪装主动技能")
	check(Rules.validate_plan(state, data, "actor.p", plan(state.units["actor.p"], "skill", "", "skill.heart")).get("code") == "PassiveHeart", "心法不冒充主动攻击按钮")
	state.units["actor.p"].weapon_id = "item.bow"
	check(Rules.validate_plan(state, data, "actor.p", long_move).get("code") == "MoveTooFar", "不兼容武器时心法加成失效")
	state.units["actor.p"].learned["skill.strike"] = 100
	check(Rules.validate_plan(state, data, "actor.p", attack).get("code") == "SkillWeaponMismatch", "刀枪武功不能用弓强行施展")
	state = battle(); state.units["actor.e"].learned["skill.guard"] = 100
	state.ai_plans["actor.e"] = plan(state.units["actor.e"], "skill", "", "skill.guard")
	result = Rules.resolve(state, data, plan(state.units["actor.p"], "attack", "actor.e"))
	check(result.ok and result.battle.units["actor.e"].hp == 96 and result.battle.units["actor.e"].stamina == 85, "防御功法用实际掌握倍率减伤并支付气势")
	state = battle(); state.ai_plans["actor.e"] = plan(state.units["actor.e"], "defend"); state.units["actor.e"].stamina = 20
	result = Rules.resolve(state, data, plan(state.units["actor.p"], "attack", "actor.e"))
	check(result.ok and result.battle.units["actor.e"].hp == 90 and result.battle.units["actor.e"].stamina == 38, "普通防御减伤一半且恢复气势")
	state = battle(); state.units["actor.e"].statuses.exposed = 1
	var exposed: Dictionary = Rules.preview_attack(state, data, "actor.p", [200, 200], 0, "actor.e")
	check(exposed.ok and exposed.damage == 26, "破绽使当前公开状态伤害提高四分之一")
	state.units["actor.e"].statuses.exposed = 0
	check(Rules.preview_attack(state, data, "actor.p", [200, 200], 0, "actor.e").damage == 21, "已过期状态即使未清理也不影响只读预览")

## 护卫失败优先、出口归属和途中身体碰撞共同决定结局，而非只看计划终点。
func check_objectives_and_withdrawal() -> void:
	var data: Dictionary = content(); var state: Dictionary = battle()
	state.objective_id = "objective.escort"
	state.units["actor.guest"] = unit("actor.guest", 0, [260, 260], "")
	state.units["actor.guest"].protected = true; state.units["actor.guest"].hp = 1; state.units["actor.e"].hp = 1
	wait_ai(state); state.ai_plans["actor.e"] = plan(state.units["actor.e"], "attack", "actor.guest"); state.ai_plans["actor.e"].heading = 90
	var result: Dictionary = Rules.resolve(state, data, plan(state.units["actor.p"], "attack", "actor.e"))
	check(result.ok and result.battle.outcome == "protected_lost", "敌人同轮倒下也不能掩盖保护对象死亡")
	check(result.ok and result.battle.units["actor.p"].hp == 100, "主角活着不是护卫成功的充分条件")
	state = battle(); state.units["actor.e"].position = [400, 200]; wait_ai(state)
	var withdrawal: Dictionary = plan(state.units["actor.p"], "withdraw"); withdrawal.destination = [110, 200]
	result = Rules.resolve(state, data, withdrawal)
	check(result.ok and result.battle.phase == "planning" and result.battle.outcome.is_empty(), "选择撤离但尚未抵达出口继续战斗")
	state = result.battle; wait_ai(state); withdrawal = plan(state.units["actor.p"], "withdraw"); withdrawal.destination = [30, 200]
	result = Rules.resolve(state, data, withdrawal)
	check(result.ok and result.battle.outcome == "withdrawn" and result.battle.units["actor.p"].position == [30, 200], "实际走入本方出口才撤离成功")
	state = battle(); state.units["actor.p"].position = [540, 200]; state.units["actor.e"].position = [400, 200]; wait_ai(state)
	withdrawal = plan(state.units["actor.p"], "withdraw"); withdrawal.destination = [570, 200]
	result = Rules.resolve(state, data, withdrawal)
	check(result.ok and result.battle.phase == "planning", "敌方出口不能冒充本方撤离点")
	state = battle(); data.personal_objectives["objective.spar"].allow_withdrawal = false
	check(Rules.validate_plan(state, data, "actor.p", plan(state.units["actor.p"], "withdraw")).get("code") == "WithdrawalForbidden", "目标可配置禁止撤离且预览即明确失败")
	data = content(); state = battle(); state.units["actor.e"].position = [400, 200]
	state.units["actor.friend"] = unit("actor.friend", 0, [150, 200], ""); wait_ai(state)
	data.personal_battlefields["field.yard"].exits[0] = {"side": 0, "position": [110, 200], "radius": 20}
	withdrawal = plan(state.units["actor.p"], "withdraw"); withdrawal.destination = [110, 200]
	result = Rules.resolve(state, data, withdrawal)
	check(result.ok and result.battle.units["actor.p"].position == [186, 200], "移动途中被真实人物截停，不互穿或重叠")
	check(result.ok and result.battle.phase == "planning", "终点写到出口但途中受阻不能撤离成功")
	state.units["actor.friend"].hp = 0; wait_ai(state)
	result = Rules.resolve(state, data, withdrawal)
	check(result.ok and result.battle.units["actor.p"].position == [186, 200], "倒地人物保留占位，后续轮次也不会与活人重叠")
	state = battle(); state.round = 10
	result = Rules.resolve(state, data, plan(state.units["actor.p"], "wait"))
	check(result.ok and result.battle.outcome == "draw", "耗尽目标回合预算仍未分胜负则平局")
	state = battle(); state.units["actor.e"].hp = 1
	result = Rules.resolve(state, data, plan(state.units["actor.p"], "attack", "actor.e"))
	check(result.ok and result.battle.outcome == "victory", "对手全部倒下且没有更优先失败才胜利")
	check(Rules.resolve(result.battle, data, {}).get("code") == "BattleNotPlanning", "结果阶段不能重复结算攻击和消耗")

## 装填跨轮保存并限制移动；严格 JSON 恢复和重复查询给出相同完整结果，全部输入保持纯性。
func check_recovery_and_replay() -> void:
	var data: Dictionary = content(); var state: Dictionary = battle()
	state.units["actor.p"].weapon_id = "item.bow"; state.units["actor.e"].position = [400, 200]; wait_ai(state)
	var attack: Dictionary = plan(state.units["actor.p"], "attack", "actor.e")
	var result: Dictionary = Rules.resolve(state, data, attack)
	check(result.ok and result.battle.units["actor.p"].recovery_until == 3, "弓射击后保存下一完整轮的装填期限")
	state = result.battle; wait_ai(state)
	check(Rules.movement_distance(state, data, "actor.p") == 0, "装填轮公开移动距离归零")
	check(Rules.validate_plan(state, data, "actor.p", attack).get("code") == "WeaponRecovering", "装填轮不能再次射箭")
	var moving: Dictionary = plan(state.units["actor.p"], "wait"); moving.destination = [201, 200]
	check(Rules.validate_plan(state, data, "actor.p", moving).get("code") == "WeaponRecovering", "装填轮不能借等待指令移动")
	result = Rules.resolve(state, data, plan(state.units["actor.p"], "wait")); state = result.battle; wait_ai(state)
	check(result.ok and state.round == 3 and Rules.validate_plan(state, data, "actor.p", attack).ok, "装填完成后正常恢复射击")
	state = battle(); state.ai_plans = Rules.make_ai(state, data)
	var params: Dictionary = plan(state.units["actor.p"], "attack", "actor.e")
	var original_state: Dictionary = state.duplicate(true); var original_data: Dictionary = data.duplicate(true); var original_params: Dictionary = params.duplicate(true)
	var first: Dictionary = Rules.resolve(state, data, params)
	var second: Dictionary = Rules.resolve(state, data, params)
	check(first.ok and first == second, "相同保存意图重复恢复产生完全相同整轮结果")
	var restored: Dictionary = Strict.parse_text(JSON.stringify(state))
	check(restored.ok and Rules.resolve(restored.value, data, params) == first, "严格JSON读档不改变整数坐标、意图和伤害结果")
	Rules.make_ai(state, data); Rules.validate_plan(state, data, "actor.p", params); Rules.preview_attack(state, data, "actor.p", [200, 200], 0, "actor.e")
	check(state == original_state and data == original_data and params == original_params, "预览、AI、校验、结算全部不修改输入")
	var reversed: Dictionary = state.duplicate(true); reversed.units = {"actor.e": state.units["actor.e"].duplicate(true), "actor.p": state.units["actor.p"].duplicate(true)}
	check(Rules.resolve(reversed, data, params) == first, "人物字典顺序不改变结算和AI下一轮计划")

## 跨武器与带障碍第二场地连续推进整场，验证 AI 生成的每轮意图合法且兵员状态有界。
func check_ai_full_battles() -> void:
	var data: Dictionary = content()
	for weapon in ["item.sword", "item.spear", "item.bow"]:
		var state: Dictionary = battle()
		state.battlefield_id = "field.ford"
		state.units["actor.p"].weapon_id = weapon
		state.units["actor.p"].position = [220, 300]
		state.units["actor.e"].weapon_id = "item.spear"
		state.units["actor.e"].position = [420, 300]
		state.units["actor.e"].learned["skill.expose"] = 60
		state.units["actor.p"].learned["skill.strike"] = 80
		var valid: bool = true
		var turns: int = 0
		while state.phase == "planning" and turns < 12:
			state.ai_plans = Rules.make_ai(state, data)
			var other_controller: Dictionary = state.duplicate(true); other_controller.controller_id = "actor.e"
			var player_candidates: Dictionary = Rules.make_ai(other_controller, data)
			if not player_candidates.has("actor.p"):
				valid = false; break
			var next: Dictionary = Rules.resolve(state, data, player_candidates["actor.p"])
			if not next.ok:
				printerr("AI battle failure: ", weapon, " ", next); valid = false; break
			for id in next.battle.units:
				var actor: Dictionary = next.battle.units[id]
				if actor.hp < 0 or actor.hp > state.units[id].hp or actor.stamina < 0 or actor.stamina > 100: valid = false
			var a: Array = next.battle.units["actor.p"].position; var b: Array = next.battle.units["actor.e"].position
			if Vector2(a[0] - b[0], a[1] - b[1]).length() < 36: valid = false
			state = next.battle; turns += 1
		check(valid and turns > 0, "连续AI战斗合法且资源与身体不越界：" + weapon)
		check(state.phase == "result" and state.outcome in ["victory", "defeat", "draw"], "第二场地按正常轮次到达真实终态：" + weapon)
