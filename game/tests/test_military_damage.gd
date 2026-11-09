## 统一伤害纯叶的旧行为基线：先捕获真实旧Round，再逐案比较完整结果/RNG，不复制第二套伤害公式。
extends SceneTree
const Round=preload("res://core/military_round.gd")
const Damage=preload("res://core/domains/military/military_damage.gd")
const F=preload("res://tests/military_fixture.gd")
const Strict=preload("res://core/strict_json.gd")
var checks: int=0
var failures: int=0

## 使用既有轻工程执行；捕获文件仅本地，不提交或伪造历史存档。
func _init() -> void:
	call_deferred("_run")

## 每次失败均计入最终进程退出，不从诊断输出猜成功。
func check(value: bool, label: String) -> void:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)

## 有限病例覆盖旧整数截断、命中分支及真实伤害结果，使用原合法部队/动作协议。
func cases() -> Dictionary:
	var result: Dictionary={}
	for name in ["plain","odd_morale","rear","forest","hill","high_ground","statuses","defend","tiny","armor","zero_accuracy","charge","archer_contact","volley"]:
		var data: Dictionary=F.data(); var battle: Dictionary=F.battle(data)
		var order: Dictionary=F.order([2,1],"attack","unit.b")
		match name:
			"odd_morale": battle.units["unit.a"].morale=51
			"rear": battle.units["unit.b"].heading=0
			"forest","hill":
				for cell in data.battlefields["field.open"].cells:
					if cell.q==3 and cell.r==1: cell.terrain=name
			"high_ground":
				data.troop_types["troop.foot"].movement=3
				for cell in data.battlefields["field.open"].cells:
					if cell.q==2 and cell.r==1: cell.terrain="hill"
			"statuses": battle.units["unit.a"].statuses={"inspired":1,"suppressed":1}; battle.units["unit.a"].position=[1,1]; battle.units["unit.b"].statuses={"exposed":1}
			"tiny": battle.units["unit.a"]=F.unit("unit.a",0,[0,1],"troop.foot",1)
			"armor": data.troop_types["troop.foot"].defense=100
			"zero_accuracy": data.troop_types["troop.foot"].accuracy=0; battle.units["unit.a"].morale=50
			"charge": order=F.order([2,1],"tactic","unit.b","tactic.charge")
			"archer_contact": battle.units["unit.a"].troop_type_id="troop.bow"
			"volley": battle.units["unit.a"].troop_type_id="troop.bow"; order=F.order([0,1],"tactic","unit.b","tactic.volley")
		F.wait_ai(battle)
		if name=="defend": battle.ai_plans["unit.b"]=F.order([3,1],"defend","","",3)
		var before: Dictionary=battle.duplicate(true)
		var quote: Dictionary=Round.preview(battle,data,"unit.a",order)
		var resolved: Dictionary=Round.resolve(battle,data,battle.controller_actor_id,{"unit.a":order})
		check(quote.ok and resolved.ok,name+" 真实旧协议可执行")
		check(battle==before,name+" 查询/结算原输入不变")
		result[name]={"quote":quote,"resolved":resolved}
	return result

## 显式作者子集模式，有限倍率不是脚本或任意公式。
func model() -> Dictionary:
	return {"kind":"legacy_base_factors_v1","side_bp":11000,"rear_bp":12000,"forest_ranged_bp":8000,"hill_melee_bp":9000,"min_total_bp":4000,"max_total_bp":18000}

## 纯投影只含当前真实规则已能提供的量，不填不存在的统率/训练/部队疲劳。
func facts() -> Dictionary:
	return {"attacker":{"fit_count":100,"attack":20,"accuracy":60,"morale":100,"inspired":false,"suppressed":false,"cavalry_in_forest":false},"defender":{"fit_count":1000,"defense":0,"guard":0,"cover":0,"exposed":false,"terrain":"plain"},"context":{"facing":0,"height_advantage":false,"ranged":true,"close_combat":false,"power_bp":10000,"accuracy_bonus":0,"incoming_damage_bp":10000}}

## 有限新模式经纯叶查询；调用不具有消费兵员或RNG的能力。
func candidate(f: Dictionary) -> Dictionary:
	return Damage.quote(f.attacker,f.defender,f.context,model())

## 检验伤害与命中拆分、精确合成/夹界、严格模型形状及真实Round整场选择。
func model_checks() -> void:
	var f: Dictionary=facts(); var before: Dictionary=f.duplicate(true)
	check(candidate(f).potential==20 and candidate(f).hit_chance==65 and f==before,"中性倍率保持现基础，纯叶不改投影")
	check(Damage.quote(f.attacker,f.defender,f.context)==Damage.quote(f.attacker,f.defender,f.context,{"kind":"legacy_v1"}),"缺省与显式legacy同义")
	f.context.facing=2
	check(candidate(f).potential==22 and candidate(f).hit_chance==65,"侧110%进damage，不再额外+10命中")
	f.context.facing=3
	check(candidate(f).potential==24 and candidate(f).hit_chance==65,"背120%进damage，不再额外+20命中")
	f.context.facing=2; f.defender.terrain="forest"; f.defender.cover=20
	check(candidate(f).potential==17 and candidate(f).hit_chance==65,"侧后与远射林地合成88%，林地不再同时扣命中")
	f.context.facing=0; f.context.ranged=false
	check(candidate(f).potential==20 and candidate(f).hit_chance==45,"近战林地不套远射80%，原cover命中语义保留")
	f.defender.terrain="hill"; f.defender.cover=10
	check(candidate(f).potential==18 and candidate(f).hit_chance==65,"近战丘陵90%只应用一次")
	f.context.ranged=true
	check(candidate(f).potential==20 and candidate(f).hit_chance==55,"远射不套近战丘陵倍率，原cover命中语义保留")
	f.defender.terrain="plain"; f.defender.cover=20
	check(candidate(f).potential==20 and candidate(f).hit_chance==45,"其他作者掩护仍保持原命中语义")
	f=facts(); f.attacker.fit_count=7; f.attacker.attack=100; f.attacker.morale=51; f.context.power_bp=12500; f.context.facing=2; f.defender.terrain="forest"
	check(candidate(f).potential==5,"7×0.755×1.25×1.1×0.8最终取整5，不逐因子截断成4")
	f=facts(); f.context.power_bp=30000; f.context.incoming_damage_bp=11000; f.context.facing=3
	check(candidate(f).potential==36,"合成396%在最终处夹180%，20基础得36")
	f=facts(); f.attacker.fit_count=1000; f.attacker.attack=100; f.attacker.morale=0; f.context.power_bp=1000; f.context.close_combat=true; f.context.ranged=false
	check(candidate(f).potential==400,"2.5%合成后夹40%，不逐项提前夹取")
	f.defender.fit_count=3
	check(candidate(f).potential==3,"最终减员不超过目标真实有效名册")
	f=facts(); f.defender.defense=20; f.defender.guard=25
	check(candidate(f).potential==11,"当前defense+普通guard构成基础，不偷偷加付费固守")
	var malformed: Array=[null,"legacy_v1",{"kind":"future"},{"kind":"legacy_v1","side_bp":11000}]
	var bad: Dictionary=model(); bad.erase("side_bp"); malformed.append(bad)
	bad=model(); bad.formula="free-form"; malformed.append(bad)
	for pair in [["side_bp",11050],["side_bp",10000.0],["side_bp",true],["rear_bp",10000],["forest_ranged_bp",3999],["hill_melee_bp",10100],["min_total_bp",3999],["max_total_bp",18001]]:
		bad=model(); bad[pair[0]]=pair[1]; malformed.append(bad)
	for entry in malformed: check(not Damage.validate_model(entry).ok,"坏模型/参数明确拒绝 "+str(entry))
	f=facts(); f.attacker.morale=true
	check(candidate(f).code=="InvalidMilitaryDamageInput","投影坏类型不脚本错或补默认值")
	var data: Dictionary=F.data(); data.battlefields["field.open"].damage_model=model()
	for troop in data.troop_types.values(): troop.attack=20; troop.accuracy=100; troop.defense=0
	for cell in data.battlefields["field.open"].cells:
		if cell.q==3 and cell.r==1: cell.terrain="forest"
	var battle: Dictionary=F.battle(data); battle.units["unit.a"]=F.unit("unit.a",0,[0,1],"troop.bow",100); battle.units["unit.b"]=F.unit("unit.b",1,[3,1],"troop.foot",100)
	for unit in battle.units.values(): unit.morale=100; unit.heading=0
	F.wait_ai(battle)
	var orders: Dictionary={"unit.a":F.order([1,1],"attack","unit.b")}; var original: Dictionary={"battle":battle.duplicate(true),"data":data.duplicate(true),"orders":orders.duplicate(true)}
	var quote: Dictionary=Round.preview(battle,data,"unit.a",orders["unit.a"])
	var result: Dictionary=Round.resolve(battle,data,battle.controller_actor_id,orders)
	check(quote.ok and quote.casualties_min==19 and result.ok and result.battle.units["unit.b"].fit.size()==81,"整场显式模型实际执行背120%×林80%，真实减员19")
	check(battle==original.battle and data==original.data and orders==original.orders,"整场模型预览/结算不改输入或RNG")
	if result.ok: check(Round.validate(result.battle,data,true).ok,"新模型结果及自主下一轮AI计划合法")
	var restored: Dictionary=Strict.parse_text(JSON.stringify(battle))
	check(restored.ok and result.ok and Round.resolve(restored.value,data,battle.controller_actor_id,orders)==result,"显式模型下保存计划重放完整结果/RNG相等")
	data.battlefields["field.open"].damage_model={"kind":"future"}; before=battle.duplicate(true)
	check(Round.resolve(battle,data,battle.controller_actor_id,orders).code=="UnknownMilitaryDamageModel" and battle==before,"未知整场模型在执行前拒绝且不改兵员/RNG")

## 外部显式提供本地基线；捕获只对旧源码执行，正常门逐个比较真实输出整态及RNG。
func _run() -> void:
	var path: String=""; var capture: bool=false
	for argument in OS.get_cmdline_user_args():
		if argument.begins_with("--baseline-path="): path=argument.trim_prefix("--baseline-path=")
		if argument=="--capture-legacy": capture=true
	check(not path.is_empty(),"必须显式提供本地旧源码基线路径")
	if path.is_empty(): quit(1); return
	var actual: Dictionary=cases()
	if capture:
		check(not FileAccess.file_exists(path),"不得覆盖已有基线证据")
		if failures==0:
			var file: FileAccess=FileAccess.open(path,FileAccess.WRITE)
			check(file!=null,"基线原件可写")
			if file!=null: file.store_string(JSON.stringify(actual)); file.close()
	else:
		var saved: Dictionary=Strict.parse_text(FileAccess.get_file_as_string(path))
		check(saved.ok,"真实旧基线可解析")
		if saved.ok:
			for name in actual: check(saved.value.get(name)==actual[name],name+" 旧报价/完整结果/RNG逐字段相同")
	if not capture: model_checks()
	print("MILITARY_DAMAGE_CHECKS=",checks," FAILURES=",failures)
	quit(0 if failures==0 else 1)
