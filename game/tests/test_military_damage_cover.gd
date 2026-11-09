## 地形乘区收窄门：只有同一地形确实移入damage时才去掉hit中的cover，不重复完整伤害套件。
extends SceneTree
const Damage=preload("res://core/domains/military/military_damage.gd")
var failures: int=0
var checks: int=0

## 两个反例及对应分支仅调用纯叶，不加载世界/旧存档或GUI。
func _init() -> void:
	var a: Dictionary={"fit_count":100,"attack":20,"accuracy":60,"morale":100,"inspired":false,"suppressed":false,"cavalry_in_forest":false}
	var d: Dictionary={"fit_count":100,"defense":0,"guard":0,"cover":20,"exposed":false,"terrain":"forest"}
	var c: Dictionary={"facing":0,"height_advantage":false,"ranged":false,"close_combat":false,"power_bp":10000,"accuracy_bonus":0,"incoming_damage_bp":10000}
	var m: Dictionary={"kind":"legacy_base_factors_v1","side_bp":11000,"rear_bp":12000,"forest_ranged_bp":8000,"hill_melee_bp":9000,"min_total_bp":4000,"max_total_bp":18000}
	check(Damage.quote(a,d,c,m),20,45,"近战林地保留原cover")
	c.ranged=true
	check(Damage.quote(a,d,c,m),16,65,"远射林地只扣伤害，不双计")
	d.terrain="hill"; d.cover=10
	check(Damage.quote(a,d,c,m),20,55,"远射丘陵保留原cover")
	c.ranged=false
	check(Damage.quote(a,d,c,m),18,65,"近战丘陵只扣伤害，不双计")
	check(Damage.quote(a,d,c),20,55,"缺省legacy完全保留原命中")
	print("MILITARY_DAMAGE_COVER_CHECKS=",checks," FAILURES=",failures)
	quit(0 if failures==0 else 1)

## 同时检查潜在伤害和命中，避免一边修正另一边仍双计。
func check(result: Dictionary, potential: int, chance: int, label: String) -> void:
	checks+=1
	if not result.get("ok",false) or result.get("potential")!=potential or result.get("hit_chance")!=chance:
		failures+=1; printerr("FAIL: ",label," ",result)
