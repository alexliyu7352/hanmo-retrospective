## 命中候选公式与唯一duel随机副本局部门；此纯叶不自行激活作者目标或提交世界。
extends SceneTree
const Hit=preload("res://core/domains/personal/personal_hit.gd")
var checks: int=0
var failures: int=0

## 汇总精确公式与随机推进断言，失败退出非零。
func check(value: bool, label: String) -> void:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)

## 用引擎原随机对象生成已有世界合同的规范字符串流。
static func stream(seed_value: int=123) -> Dictionary:
	var rng=RandomNumberGenerator.new(); rng.seed=seed_value
	return {"seed":str(rng.seed),"state":str(rng.state)}

## 只传真实技能XP给共享等级服务，属性不充当等级。
func _initialize() -> void:
	var captured: Dictionary=Hit.snapshot({"skills":{"martial":650,"archery":100},"attributes":{"martial":110}})
	check(captured.ok and captured.skill_levels=={"martial":3,"archery":1},"snapshot derives exact levels from XP")
	check(not Hit.snapshot({"skills":{"martial":650.0,"archery":100}}).ok,"fractional XP is not silently converted")
	var attacker: Dictionary={"martial":60,"skill_levels":captured.skill_levels}
	var target: Dictionary={"martial":50,"skill_levels":{"martial":2,"archery":4}}
	check(Hit.chance(attacker,target,{"category":"sword"}).hit_bp==7900,"sword uses martial3 versus defense martial2")
	check(Hit.chance(attacker,target,{"category":"bow"}).hit_bp==7500,"bow uses archery1 rather than attacker martial3")
	check(Hit.chance(attacker,target,{"category":"sword"},{"kind":"strike","hit_bonus_bp":1500}).hit_bp==9400,"strike bonus composes with exact candidate formula")
	check(Hit.chance(attacker,target,{"category":"sword"},{"kind":"strike","hit_bonus_bp":10000}).hit_bp==9500,"upper chance stays bounded")
	check(Hit.chance(attacker,target,{"category":"sword"},{"kind":"strike","hit_bonus_bp":-10000}).hit_bp==1500,"lower chance stays bounded")
	check(not Hit.chance(attacker,target,{"category":"sword"},{"kind":"guard","hit_bonus_bp":1}).ok,"guard cannot add an offensive hit bonus")
	check(Hit.validate_objective({}).ok and not Hit.enabled({}),"legacy content is not implicitly opted in")
	check(Hit.validate_objective({"hit_model":"unknown"}).get("code")=="PERSONAL_HIT_MODEL","unknown model does not fall back to legacy success")
	var rng_stream: Dictionary=stream(); var before: Dictionary=rng_stream.duplicate(true)
	var manual=RandomNumberGenerator.new(); manual.seed=int(rng_stream.seed); manual.state=int(rng_stream.state)
	var expected: int=manual.randi_range(0,9999)
	var drawn: Dictionary=Hit.draw(rng_stream,7900)
	check(drawn.ok and drawn.roll==expected and drawn.hit==(expected<7900),"one engine integer draw determines one hit")
	check(drawn.duel_stream.state==str(manual.state) and drawn.duel_stream.seed==rng_stream.seed,"returned state is exactly the existing duel stream successor")
	check(rng_stream==before and Hit.draw(rng_stream,7900)==drawn,"draw preserves source and replay is deterministic")
	expected=manual.randi_range(0,9999)
	var second: Dictionary=Hit.draw(drawn.duel_stream,7900)
	check(second.roll==expected and second.duel_stream.state==str(manual.state),"second legal attack consumes exactly the next draw")
	for invalid in [{"seed":"01","state":"1"},{"seed":"1","state":"1.5"},{"seed":1,"state":"1"},{"seed":"1","state":"9223372036854775808"},{}]:
		check(not Hit.validate_stream(invalid).ok,"bad stream is rejected without inventing seed")
	check(not Hit.draw(rng_stream,10001).ok and rng_stream==before,"bad chance cannot mutate input stream")
	print("PERSONAL_HIT_LEAF checks=",checks," failures=",failures)
	quit(1 if failures else 0)
