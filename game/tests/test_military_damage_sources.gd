## 可信墙来源的纯乘区接缝：这里只验数学/范围，守方与墙来源身份由FortificationContext另门先验。
extends SceneTree
const Damage=preload("res://core/domains/military/military_damage.gd")
var checks: int=0
var failures: int=0

## 有界纯叶测试不启动战争、世界或GUI，不把手造投影称为真实城墙来源。
func _init() -> void:
	call_deferred("_run")

## 失败退出码用于外部run_checked，所有倍率判断须有明确输入证据。
func check(value: bool, label: String) -> void:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)

## 夹具只表达已经冻结的数学投影，真实来源校验不在此复制。
func facts() -> Dictionary:
	return {"attacker":{"fit_count":100,"attack":20,"accuracy":60,"morale":100,"inspired":false,"suppressed":false,"cavalry_in_forest":false},"defender":{"fit_count":4096,"defense":0,"guard":0,"cover":0,"exposed":false,"terrain":"plain"},"context":{"facing":0,"height_advantage":false,"ranged":true,"close_combat":false,"power_bp":10000,"accuracy_bonus":0,"incoming_damage_bp":10000},"model":{"kind":"legacy_base_factors_v1","side_bp":11000,"rear_bp":12000,"forest_ranged_bp":8000,"hill_melee_bp":9000,"min_total_bp":4000,"max_total_bp":18000}}

## 只把中央已经先验的来源倍率传纯叶，接口不收作者脚本、单位名单或保存来源标志。
func quote(f: Dictionary, sources: Variant={}) -> Dictionary:
	return Damage.quote(f.attacker,f.defender,f.context,f.model,sources)

## 单次来源倍率、近战中性、统一夹取与大整数取整构成此窄门的全部范围。
func _run() -> void:
	var f: Dictionary=facts(); var before: Dictionary=f.duplicate(true)
	var neutral: Dictionary=quote(f)
	var wall: Dictionary=quote(f,{"siege_wall_bp":12000})
	check(neutral.potential==20 and wall.potential==24 and neutral.hit_chance==wall.hit_chance,"墙120%只乘一次伤害，不同时加命中")
	check(quote(f,{"siege_wall_bp":10000})==neutral and f==before,"缺省与来源中性10000同义，纯叶不改输入")
	f.context.ranged=false
	check(quote(f,{"siege_wall_bp":12000})==quote(f),"非远射即使收到墙倍率也强制中性")
	f=facts(); f.model={"kind":"legacy_v1"}
	check(quote(f,{"siege_wall_bp":12000})==quote(f),"legacy旧模型不隐式启用墙增伤")
	f.model={}
	check(quote(f,{"siege_wall_bp":30000})==quote(f),"缺省旧模型同样保留原结果")
	f=facts(); f.attacker.fit_count=7; f.attacker.attack=100; f.context.power_bp=12500; f.context.facing=2; f.defender.terrain="forest"
	check(quote(f,{"siege_wall_bp":12000}).potential==9,"7×1.25×1.1×0.8×1.2最终只取整一次9，不后乘成8")
	f=facts(); f.context.power_bp=15000; f.context.facing=3
	check(quote(f,{"siege_wall_bp":12000}).potential==36,"墙与已有180%合成后仍统一夹180%，不绕上限")
	f=facts(); f.attacker.fit_count=4093; f.attacker.attack=100; f.attacker.morale=99; f.context.power_bp=2999; f.context.incoming_damage_bp=11000; f.context.facing=2; f.model.side_bp=17900; f.model.rear_bp=17900; f.model.forest_ranged_bp=9900; f.defender.terrain="forest"
	check(quote(f,{"siege_wall_bp":29999}).potential==4096,"合法七因子直接乘基础会超过int64，精确有界乘除仍正确封顶")
	f.attacker.fit_count=3001; f.context.power_bp=1111
	check(quote(f,{"siege_wall_bp":29999}).potential==1939,"非整百墙倍率精确分数参考1939，不用浮点近似")
	for sources in [null,12000,{"siege_wall_bp":9999},{"siege_wall_bp":30001},{"siege_wall_bp":12000.0},{"siege_wall_bp":true},{"other":12000},{"siege_wall_bp":12000,"kind":"author_claim"}]:
		check(quote(f,sources).code=="InvalidMilitaryDamageSource","拒坏来源参数而不吞成中性 "+str(sources))
	f=facts(); f.model.siege_wall_bp=12000
	check(quote(f).code=="InvalidMilitaryDamageModel","作者damage_model不能伪装可信墙来源")
	print("MILITARY_DAMAGE_SOURCE_CHECKS=",checks," FAILURES=",failures)
	quit(0 if failures==0 else 1)
