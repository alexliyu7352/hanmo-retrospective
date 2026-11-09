## 新税账UI DTO夹具只验必要解析/预读/游标；不代替Main和真实原生账。
extends SceneTree
const Economic=preload("res://ui/economic_world_panel.gd")
const Navigation=preload("res://tests/test_economic_world_navigation.gd")
class FiscalKernel extends Navigation.Kernel:
	var fiscal_pending=false
	var fiscal={}
	func occupation_fiscal_settlement(_place,_cursor,_authority):
		if fiscal_pending: return {"ok":false,"code":"NativeAsyncRequired"}
		return fiscal.duplicate(true)
func _initialize(): call_deferred("run")
func run():
	create_timer(5).timeout.connect(func(): quit(2))
	var host=Navigation.Host.new();host.kernel=FiscalKernel.new();root.add_child(host)
	var identity={"world_id":[0,0],"execution_epoch":1,"revision":2,"tick":4}
	host.kernel.actor_id="mayor";host.kernel.identity=identity
	host.kernel.view={"identity":identity,"observer_actor_id":"mayor","place_id":"town","authorities":[],"rows":[],"next_cursor":null,"occupation_fiscal_available":true,"fiscal_available":false}
	host.kernel.fiscal={"identity":identity,"observer_actor_id":"mayor","place_id":"town","rows":[{"kind":"occupation_tax_assessment","due_tick":2,"income_basis_points":5000,"cash_due":10,"cash_paid":3,"cash_need":{"need_id":"need:1","status":"partial","revision":3},"cash_outstanding":7,"grain_due":0,"grain_paid":0,"grain_outstanding":0,"cash_bucket":{"coarse":70000000,"fine":1},"grain_bucket":{"coarse":0,"fine":0},"collection_pause_reason":"original_taxpayer_assets_captured"}],"suspended_periods":[{"due_tick":4}],"next_cursor":{"offset":32}}
	host.kernel.fiscal_pending=true;assert(not Economic.prepare(host));assert(host.commands.is_empty())
	host.kernel.fiscal_pending=false;assert(Economic.prepare(host))
	var panel=VBoxContainer.new();panel.name="DynamicWorldPanel";host.add_child(panel)
	var scroll=ScrollContainer.new();scroll.name="DynamicWorldScroll";panel.add_child(scroll);scroll.add_child(VBoxContainer.new())
	Economic.build(host,host);await process_frame
	var row=host.find_child("OccupationFiscalRow",true,false);assert(row!=null and "应缴10钱，实缴3钱，尚欠7钱" in row.text and "50.00%" in row.text and "70000000/1亿" in row.text and "暂停收缴" in row.text and "税钱部分清偿" in row.text)
	var pause=host.find_child("OccupationFiscalSuspension",true,false);assert(pause!=null and "没有生成新应缴额" in pause.text)
	assert(host.find_child("FiscalSettlementSection",true,false)==null and host.commands.is_empty())
	var nav=host.find_child("OccupationFiscalSection",true,false).get_children()[-1]
	var next=nav.get_child(0);next.emit_signal("pressed")
	assert(host.get_meta(Economic.META)["occupation_fiscal_cursor"]=={"offset":32} and host.get_meta(Economic.META)["cursor"].is_empty() and host.get_meta(Economic.META)["fiscal_cursor"].is_empty())
	Economic.select_authority(host,{});assert(host.get_meta(Economic.META)["occupation_fiscal_cursor"].is_empty())
	host.kernel.fiscal["place_id"]="other";assert(not Economic.prepare(host) and host._resident_page_error["code"]=="StaleOccupationFiscalView")
	host.free();print("occupation_fiscal_panel: PASS (typed UI fixture, pending pure read, original debt, fractional rights, explicit suspension, independent cursor, stale source)");quit()
