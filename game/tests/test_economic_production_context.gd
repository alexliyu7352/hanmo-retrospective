## 有限DTO显示门：上期真实收据和下期系数不能混成同一生产原因；不实例化世界。
extends SceneTree
class Host extends RefCounted:
	var _data: Dictionary={}
	func _name(record: Dictionary) -> String: return record.get("name","")
var checks: int=0
var failures: int=0
func _initialize() -> void: call_deferred("run")
func check(value: bool, label: String) -> void:
	checks+=1
	if not value: failures+=1;printerr("FAIL: ",label)
func run() -> void:
	var path: String="res://ui/economic_world_panel.gd"
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--panel="): path=arg.trim_prefix("--panel=")
	var panel=load(path)
	if panel==null: printerr("Cannot load actual economic panel");quit(2);return
	var host=Host.new()
	var row: Dictionary={"kind":"production","allocated_workers":6,"capacity":10,"next_due_tick":8,"last_result":{"due_tick":4,"effective_workers":4,"batches":2,"outputs":[],"production_context":{"capacity":12,"region_factor_bp":9000,"season_factor_bp":7500}},"context":{"capacity":10,"region_factor_bp":11000,"season_factor_bp":12500}}
	var before: Dictionary=row.duplicate(true)
	var text: String=panel.describe(host,row)
	check("最近半日4：有效劳力4，完成2批" in text,"原已结产出仍沿原收据")
	check("该次结算：土地设施上限12批，地区系数90%，季节系数75%" in text,"上期显示原收据系数")
	check("下次半日8适用：土地设施上限10批，地区系数110%，季节系数125%" in text,"下期系数必须明确标时点")
	check(row==before,"呈现不改DTO或旧结果")
	row["last_result"]["production_context"]=null
	text=panel.describe(host,row)
	check(not "该次结算：" in text and "下次半日8适用：" in text,"旧收据无上下文时不补造上期系数")
	row["last_result"]=null
	text=panel.describe(host,row)
	check("尚无已结算产出" in text and not "该次结算：" in text,"新局未发生生产不冒已结结果")
	var disturbance: Dictionary={"remaining_yield_bp":5000,"causes":[{"reason_key":"reason.rain","from_tick":4,"through_tick":8,"remaining_yield_bp":5000}],"outputs":[{"item_id":"millet","original_potential":10,"unproduced":5,"adjusted_potential":5,"stored":4}]}
	var original_disturbance: Dictionary=disturbance.duplicate(true)
	text=panel._disruption(host,disturbance,false)
	check("当前减产安排" in text and "半日4至8" in text and "50.00%" in text and "届满后不补回减产" in text,"明确时窗与原潜在产量系数")
	text=panel._disruption(host,disturbance,true)
	check("原潜在10份，减产未形成5份，减产后5份；实际入库4份" in text,"历史损失与实际入库分别取真实DTO")
	check(disturbance==original_disturbance,"减产展示不回写库存或历史来源")
	print("ECONOMIC_PRODUCTION_CONTEXT_CHECKS=",checks," FAILURES=",failures," WORLD_INSTANTIATIONS=0 GUI=NOT_TESTED")
	quit(0 if failures==0 else 1)
