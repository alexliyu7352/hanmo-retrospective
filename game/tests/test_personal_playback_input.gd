## 验证有限公开页到动作片段的映射；不调用世界结算，也不以夹具替代图形验收。
extends SceneTree
var checks: int = 0
var failures: int = 0

## 原红在模块缺失时明确失败，避免把预加载语法错误当作行为红。
func _init() -> void:
	if not ResourceLoader.exists("res://ui/personal_battle_playback_input.gd"):
		check(false, "缺少个人战公开动作映射")
		quit(1)
		return
	var mapper = load("res://ui/personal_battle_playback_input.gd")
	if mapper == null:
		check(false, "动作映射须可解析")
		quit(1)
		return
	run_source_checks(mapper)
	if not "--source-only" in OS.get_cmdline_user_args(): run_checks(mapper)
	print("PERSONAL_PLAYBACK_INPUT checks=", checks, " failures=", failures)
	quit(1 if failures else 0)

## 失败退出非零并保留具名边界，便于精确定位退化。
func check(value: bool, label: String) -> void:
	checks += 1
	if not value:
		failures += 1
		printerr("FAIL: ", label)

## 使用五个既有身份戳，不新增世界标识。
func binding() -> Dictionary:
	return {"kernel_id":7,"load_generation":0,"actor_id":"p","world_id":"","execution_epoch":0}

## 页夹具仅携带公开身体和事件；恶意附加字段用于验证严格过滤。
func page(revision: int = 10, battle_round: int = 1, phase: String = "planning") -> Dictionary:
	return {"ok":true,"revision":revision,"actor_id":"p","battle_id":"b","battle":{"id":"b","controller_id":"p","round":battle_round,"phase":phase,"units":{"p":{"position":[10,20],"hp":100,"heading":0,"side":0,"protected":false,"statuses":{}},"e":{"position":[80,20],"hp":100,"heading":180,"side":1,"protected":false,"statuses":{},"weapon_id":"SECRET"}}},"present_actor_ids":["p","e"],"events":[],"ai_plans":{"secret":true}}

## 汇总阶段事件，供伤害与脱离的真实语义断言。
func events_of(clip: Dictionary, kind: String) -> Array:
	for phase in clip.get("phases",[]):
		if phase.kind == kind: return phase.events
	return []

## 覆盖相邻结算、同轮战果、来源隔离、重复页与未知字段；所有输入均须保持字节稳定。
func run_checks(mapper) -> void:
	var first: Dictionary = page()
	var context: Dictionary = binding()
	var before: String = JSON.stringify([first,context])
	var anchor: Dictionary = mapper.capture(first,context)
	check(anchor.get("ok",false), "捕获公开锚点")
	check(JSON.stringify([first,context]) == before, "capture不改输入")
	var settled: Dictionary = page(11,2)
	settled.battle.units.p.position = [40,20]
	settled.battle.units.p.hp = 95
	settled.battle.units.e.hp = 80
	settled.events = [{"actor_id":"p","target_id":"e","action":"attack","code":"Hit","damage":20,"roll":19,"ai_plans":{"secret":true}},{"actor_id":"e","target_id":"p","action":"counter","code":"Hit","damage":5},{"actor_id":"e","action":"defend","code":"Guarded"}]
	var original: String = JSON.stringify([anchor,settled,context])
	var clip: Dictionary = mapper.prepare(anchor,settled,context)
	check(clip.get("ok",false) and clip.get("playable",false), "相邻公开轮次可播放")
	check(clip.actors.p.from_position == [10,20] and clip.actors.p.to_position == [40,20], "只保真实起止锚点")
	check(clip.actors.p.from_hp == 100 and clip.actors.p.hp == 95, "HP保结算前后真实值")
	check(events_of(clip,"impact").size() == 3, "同批伤害与守势同阶段呈现")
	check(events_of(clip,"strike").size() == 2, "仅真实出手进入strike")
	check(not JSON.stringify(clip).contains("SECRET") and not JSON.stringify(clip).contains("\"roll\"") and not JSON.stringify(clip).contains("ai_plans"), "过滤敌私密装备、roll与未知字段")
	check(JSON.stringify([anchor,settled,context]) == original, "prepare不改锚点、页面或来源")
	check(mapper.prepare(anchor,settled,context).id == clip.id, "同一前后来源生成稳定片段id")
	var cold: Dictionary = mapper.prepare({},settled,context)
	check(cold.ok and not cold.playable and cold.phases.is_empty(), "冷恢复直接终态")
	check(cold.actors.p.from_position == cold.actors.p.to_position and cold.actors.p.from_hp == cold.actors.p.hp, "终态不能伪造旧位置或旧HP")
	check(not mapper.prepare(anchor,first,context).playable, "重复页不回放")
	check(not mapper.prepare(anchor,page(12,3),context).playable, "缺中间轮次直接终态")
	check(not mapper.prepare(anchor,page(12,2),context).playable, "缺中间revision直接终态")
	for key in context:
		var changed: Dictionary = context.duplicate(true)
		changed[key] = "different" if context[key] is String else context[key]+1
		if key == "actor_id":
			check(not mapper.prepare(anchor,settled,changed).get("playable",false), "换玩家拒绝旧锚点")
		else:
			check(not mapper.prepare(anchor,settled,changed).get("playable",false), "来源隔离："+key)
	var result: Dictionary = page(11,1,"result")
	result.battle.units.e.hp = 0
	result.events = [{"actor_id":"p","target_id":"e","action":"attack","code":"Hit","damage":100},{"action":"outcome","code":"victory"}]
	check(mapper.prepare(anchor,result,context).playable, "最终结算同轮planning到result可播放")
	check(not mapper.prepare(mapper.capture(result,context),page(12,1,"result"),context).playable, "战果同轮刷新不回放")
	var withdraw: Dictionary = page(11,2)
	withdraw.present_actor_ids = ["p"]
	withdraw.events = [{"actor_id":"e","action":"withdraw","code":"Withdrawn"}]
	var departed: Dictionary = mapper.prepare(anchor,withdraw,context)
	check(not departed.actors.e.present and departed.actors.e.from_present, "退离只认真实公开在场名单")
	check(events_of(departed,"state").size() == 1, "真实退离事件进入state")
	withdraw.present_actor_ids = ["p","e"]
	check(events_of(mapper.prepare(anchor,withdraw,context),"state").is_empty(), "无终态佐证不宣称已退离")
	var malformed: Dictionary = settled.duplicate(true)
	malformed.battle.units.p.position = [1]
	check(not mapper.prepare(anchor,malformed,context).ok, "坏终态坐标明确失败")
	malformed = settled.duplicate(true)
	malformed.events.append({"actor_id":"e","target_id":"p","action":"future","damage":999,"code":"Hit"})
	check(events_of(mapper.prepare(anchor,malformed,context),"impact").size() == 3, "未知动作不穿透白名单")
	var copy: Dictionary = mapper.capture(first,context)
	copy.actors.p.to_position[0] = 999
	check(first.battle.units.p.position == [10,20], "返回锚点与页面解别名")
	check(not mapper.prepare(anchor,page(11,1),context).playable, "同轮planning的其他revision不是结算")
	check(not mapper.prepare(anchor,page(11,2,"result"),context).playable, "跳过最终轮初页不能伪造终结轮动作")
	var miss: Dictionary = page(11,2)
	miss.events = [{"actor_id":"p","target_id":"e","action":"attack","code":"Miss","damage":0},{"actor_id":"e","target_id":"p","action":"attack","code":"OutOfRange","damage":0}]
	var attempts: Dictionary = mapper.prepare(anchor,miss,context)
	check(events_of(attempts,"strike").size() == 1 and events_of(attempts,"impact").size() == 2, "真实落空可出手，未达射程仅显示原失败理由")
	check(events_of(attempts,"impact")[0].code == "Miss" and attempts.actors.e.hp == 100, "落空不产生受伤或伤害")
	miss.events = [{"actor_id":"p","action":"skill","code":"Guarded","skill_id":"guard","damage":999,"target_id":"e","blocked":true}]
	var guard: Dictionary = mapper.prepare(anchor,miss,context)
	check(events_of(guard,"strike").is_empty() and events_of(guard,"impact") == [{"action":"skill","actor_id":"p","code":"Guarded","skill_id":"guard"}], "守势不接受伪造格挡成功或伤害字段")
	miss.events = [{"actor_id":"p","target_id":"e","action":"status","status":"slow","expires_round":3}]
	check(events_of(mapper.prepare(anchor,miss,context),"state").is_empty(), "缺真实状态不显示虚假减速")
	miss.battle.units.e.statuses["slow"] = 3
	check(events_of(mapper.prepare(anchor,miss,context),"state").size() == 1, "已发生状态有终态佐证")
	for invalid in [{"actor_id":"p","target_id":"missing","action":"attack","code":"Hit","damage":20},{"actor_id":"p","target_id":"e","action":"attack","code":"Hit","damage":20.0},{"actor_id":"p","target_id":"e","action":"attack","code":"Miss","damage":20}]:
		miss.events = [invalid]
		check(events_of(mapper.prepare(anchor,miss,context),"impact").is_empty(), "坏目标、浮点伤害或带伤害落空拒绝")
	var invalid_binding: Dictionary = context.duplicate(true)
	invalid_binding.erase("execution_epoch")
	check(not mapper.capture(first,invalid_binding).ok, "缺来源戳不能冒充GD零epoch")
	var poisoned: Dictionary = anchor.duplicate(true)
	poisoned.actors.p.to_position = [1]
	check(not mapper.prepare(poisoned,settled,context).playable, "损坏旧锚点直接终态")
	clip.actors.p.to_position[0] = 999
	clip.phases[3].events[0].damage = 999
	check(settled.battle.units.p.position == [40,20] and settled.events[0].damage == 20, "片段返回值与公开页深度解别名")
	check_actual_settlement(mapper)

## 实际纯规则先结算并经既有事件投影裁剪，再交映射；用反击同步伤害和同轮倒地验证真实调用者。
func check_actual_settlement(mapper) -> void:
	var fixture = load("res://tests/test_personal_counter.gd")
	var projection = load("res://core/domains/personal/personal_projection.gd")
	var data: Dictionary = fixture.content()
	for fatal in [false,true]:
		var battle: Dictionary = fixture.battle()
		battle.id = "battle.actual"
		if fatal:
			battle.units.defender.hp = 10
			battle.units.attacker.hp = 8
		var current: Dictionary = {"ok":true,"actor_id":"defender","revision":1,"battle_id":battle.id,"battle":battle.duplicate(true),"present_actor_ids":projection.present_actor_ids(battle),"events":projection.events(battle)}
		var context: Dictionary = binding()
		context.actor_id = "defender"
		var prior: Dictionary = mapper.capture(current,context)
		var result: Dictionary = fixture.resolve(battle,data)
		check(result.ok, "真实纯规则反击夹具完成结算")
		var settled: Dictionary = current.duplicate(true)
		settled.revision = 2
		settled.battle = result.battle
		settled.present_actor_ids = projection.present_actor_ids(result.battle)
		settled.events = projection.events(result.battle)
		var immutable: String = JSON.stringify(result)
		var actual: Dictionary = mapper.prepare(prior,settled,context)
		check(actual.playable, "真实公开事件可映射："+("同轮result" if fatal else "下一planning"))
		check(actual.actors.defender.hp == result.battle.units.defender.hp and actual.actors.attacker.hp == result.battle.units.attacker.hp, "反击双方HP精确保持真实结算")
		check(events_of(actual,"strike").size() == 2 and events_of(actual,"impact").size() == 3, "真实guard声明、主攻击和反击同批呈现")
		check(JSON.stringify(result) == immutable, "实际结算输出保持字节不变")

## 真实RefCounted实例ID可能以有符号负数表示；只拒绝零及非整数，不重新编码对象身份。
func run_source_checks(mapper) -> void:
	var kernel: RefCounted = RefCounted.new()
	var context: Dictionary = binding()
	context.kernel_id = kernel.get_instance_id()
	print("PERSONAL_PLAYBACK_INPUT kernel_instance_id=",context.kernel_id)
	check(context.kernel_id != 0, "真实RefCounted实例ID非零")
	var anchor: Dictionary = mapper.capture(page(),context)
	check(anchor.ok, "真实RefCounted实例ID可捕获锚点")
	check(mapper.prepare(anchor,page(11,2),context).playable, "真实RefCounted来源可播放相邻轮次")
	context.kernel_id = 0
	check(not mapper.capture(page(),context).ok, "零kernel身份仍拒绝")
	context.kernel_id = 7.0
	check(not mapper.capture(page(),context).ok, "浮点kernel身份仍拒绝")
	context.kernel_id = kernel.get_instance_id()
	context.load_generation = -1
	check(not mapper.capture(page(),context).ok, "不放宽其他来源字段")
