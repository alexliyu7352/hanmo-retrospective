## 野战伤害纯叶：只消费已验证战场投影，不查世界、推进随机流或改兵员；旧模式保留逐步整数语义。
extends RefCounted
## T43贴身候选50%沿既有整数下限计算，尚非平衡定论。
const ARCHER_CONTACT_POWER_BP: int=5000

## 模型缺省与显式legacy均沿旧算法；未知模型严格拒绝，不静默降代或接受任意公式。
static func validate_model(model: Variant) -> Dictionary:
	if not model is Dictionary: return _fail("InvalidMilitaryDamageModel")
	if model.is_empty() or (model.size()==1 and model.get("kind")=="legacy_v1"): return {"ok":true}
	if model.get("kind")!="legacy_base_factors_v1": return _fail("UnknownMilitaryDamageModel")
	var fields: Array=["kind","side_bp","rear_bp","forest_ranged_bp","hill_melee_bp","min_total_bp","max_total_bp"]
	if model.size()!=fields.size(): return _fail("InvalidMilitaryDamageModel")
	for key in model:
		if not key in fields: return _fail("InvalidMilitaryDamageModel",str(key))
	for key in ["side_bp","rear_bp","forest_ranged_bp","hill_melee_bp"]:
		var low: int=10000 if key in ["side_bp","rear_bp"] else 4000
		var high: int=18000 if key in ["side_bp","rear_bp"] else 10000
		if not _whole(model.get(key),low,high) or model[key]%100!=0: return _fail("InvalidMilitaryDamageParameter",key)
	if model.rear_bp<model.side_bp: return _fail("InvalidMilitaryDamageParameter","rear_bp")
	if not _whole(model.get("min_total_bp"),4000,4000) or not _whole(model.get("max_total_bp"),18000,18000): return _fail("InvalidMilitaryDamageParameter","total_bounds")
	return {"ok":true}

## 查询只返回原报价字段；范围/占用/战法资格由Round先验，RNG及伤亡分类仍归原结算边界。
static func quote(attacker: Dictionary, defender: Dictionary, context: Dictionary, model: Variant={}, source_factors: Variant={}) -> Dictionary:
	var checked: Dictionary=validate_model(model)
	if not checked.ok: return checked
	if not _inputs(attacker,defender,context): return _fail("InvalidMilitaryDamageInput")
	if not _sources(source_factors): return _fail("InvalidMilitaryDamageSource")
	if model.get("kind", "legacy_v1")=="legacy_base_factors_v1": return _candidate(attacker,defender,context,model,source_factors)
	return _legacy(attacker,defender,context)

## 输入是有限整数和显式布尔事实，拒缺字段/坏类型而不是猜默认数或从世界补快照。
static func _inputs(attacker: Dictionary, defender: Dictionary, context: Dictionary) -> bool:
	for row in [[attacker,"fit_count",1,4096],[attacker,"attack",1,100],[attacker,"accuracy",0,100],[attacker,"morale",0,100],[defender,"fit_count",1,4096],[defender,"defense",0,100],[defender,"guard",0,100],[defender,"cover",0,100],[context,"facing",0,3],[context,"power_bp",1000,30000],[context,"accuracy_bonus",-100,100]]:
		if not _whole(row[0].get(row[1]),row[2],row[3]): return false
	for row in [[attacker,"inspired"],[attacker,"suppressed"],[attacker,"cavalry_in_forest"],[defender,"exposed"],[context,"height_advantage"],[context,"ranged"],[context,"close_combat"]]:
		if not row[0].get(row[1]) is bool: return false
	if not defender.get("terrain") in ["plain","forest","hill","water","wall"]: return false
	if typeof(context.get("incoming_damage_bp"))!=TYPE_INT or not context.incoming_damage_bp in [10000,11000]: return false
	return not (context.ranged and context.close_combat)

## 旧命中近似原样保留；显式候选把侧后/林丘移到伤害，其他命中项仍沿旧定义，不双计同一地形。
static func _chance(attacker: Dictionary, defender: Dictionary, context: Dictionary, legacy: bool=true) -> int:
	var flank: int=20 if context.facing==3 else (10 if context.facing==2 else 0)
	var terrain_factor_applies: bool=(context.ranged and defender.terrain=="forest") or (not context.ranged and defender.terrain=="hill")
	var cover: int=defender.cover if legacy or not terrain_factor_applies else 0
	var chance: int=attacker.accuracy+(attacker.morale-50)/10+(flank if legacy else 0)-cover+context.accuracy_bonus
	if context.height_advantage: chance+=10
	if attacker.inspired: chance+=10
	if attacker.suppressed: chance-=15
	if defender.exposed: chance+=15
	if attacker.cavalry_in_forest: chance-=15
	return clampi(chance,0,100)

## 完整保持旧ceil基础、每阶段最低1及逐次截断；目标名册上限在同一位置应用。
static func _legacy(attacker: Dictionary, defender: Dictionary, context: Dictionary) -> Dictionary:
	var potential: int=maxi(1,(attacker.fit_count*attacker.attack+99)/100)
	potential=maxi(1,potential*(50+attacker.morale/2)/100)
	potential=maxi(1,potential*context.power_bp/10000)
	if context.close_combat: potential=maxi(1,potential*ARCHER_CONTACT_POWER_BP/10000)
	potential=maxi(1,potential*(100-mini(95,defender.defense+defender.guard))/100)
	potential=maxi(1,potential*context.incoming_damage_bp/10000)
	var flank: int=10 if context.facing==3 else (5 if context.facing==2 else 0)
	return {"ok":true,"hit_chance":_chance(attacker,defender,context),"potential":mini(potential,defender.fit_count),"flank":flank}

## 显式子集保留现攻防基础，七个有限因子先精确合并；墙来源仅由中央可信投影传入，身份不在此重判。
static func _candidate(attacker: Dictionary, defender: Dictionary, context: Dictionary, model: Dictionary, source_factors: Dictionary) -> Dictionary:
	var base: int=maxi(1,(attacker.fit_count*attacker.attack+99)/100)
	base=maxi(1,base*(100-mini(95,defender.defense+defender.guard))/100)
	var facing_bp: int=model.rear_bp if context.facing==3 else (model.side_bp if context.facing==2 else 10000)
	var terrain_bp: int=10000
	if context.ranged and defender.terrain=="forest": terrain_bp=model.forest_ranged_bp
	elif not context.ranged and defender.terrain=="hill": terrain_bp=model.hill_melee_bp
	var wall_bp: int=source_factors.get("siege_wall_bp",10000) if context.ranged else 10000
	var factors: Array=[5000+50*attacker.morale,context.power_bp,ARCHER_CONTACT_POWER_BP if context.close_combat else 10000,context.incoming_damage_bp,facing_bp,terrain_bp,wall_bp]
	var numerator: int=1; var denominator: int=1
	for factor in factors:
		var top: int=factor; var bottom: int=10000
		var divisor: int=_gcd(top,bottom); top/=divisor; bottom/=divisor
		divisor=_gcd(numerator,bottom); numerator/=divisor; bottom/=divisor
		divisor=_gcd(denominator,top); denominator/=divisor; top/=divisor
		numerator*=top; denominator*=bottom
	if numerator*5<2*denominator: numerator=model.min_total_bp; denominator=10000
	elif numerator*5>9*denominator: numerator=model.max_total_bp; denominator=10000
	var potential: int=maxi(1,_scaled_floor(base,numerator,denominator))
	var flank: int=10 if context.facing==3 else (5 if context.facing==2 else 0)
	return {"ok":true,"hit_chance":_chance(attacker,defender,context,false),"potential":mini(potential,defender.fit_count),"flank":flank}

## 可选来源只容纳一项有限墙倍率，作者模型不能塞来源；合法性/守方身份必须由中央桥和来源叶先验。
static func _sources(source_factors: Variant) -> bool:
	return source_factors is Dictionary and (source_factors.is_empty() or (source_factors.size()==1 and _whole(source_factors.get("siege_wall_bp"),10000,30000)))

## 基础最多4096，以13个二进制位完成精确乘除；不直接乘大分子，避免有效第七因子导致64位溢出。
static func _scaled_floor(base: int, numerator: int, denominator: int) -> int:
	var result: int=0; var remainder: int=0; var bit: int=4096
	while bit>0:
		var current: int=remainder*2+(numerator if (base & bit)!=0 else 0)
		result=result*2+current/denominator; remainder=current%denominator; bit>>=1
	return result

## 固定七因子范围保证约分中间项及5倍比较在64位内；欧几里得只处理有限正整数。
static func _gcd(left: int, right: int) -> int:
	while right!=0:
		var remainder: int=left%right; left=right; right=remainder
	return left

## 布尔值不充当整数；纯叶不作隐式类型强转。
static func _whole(value: Variant, minimum: int, maximum: int) -> bool:
	return typeof(value)==TYPE_INT and value>=minimum and value<=maximum

## 统一失败形状供原Round向外传播，调用者不得把失败当成零伤害成功。
static func _fail(code: String, path: String="") -> Dictionary:
	return {"ok":false,"code":code,"path":path}
