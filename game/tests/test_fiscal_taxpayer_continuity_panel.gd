## 有限暂停页UI夹具：只核DTO解释、异步预读、独立游标和过期拒绝，不代Main实玩。
extends SceneTree
const Economic=preload("res://ui/economic_world_panel.gd")
const Navigation=preload("res://tests/test_economic_world_navigation.gd")
class PausedKernel extends Navigation.Kernel:
	var taxpayer_pending=false
	var paused={}
	func fiscal_taxpayer_continuity(_place,_cursor,_authority):
		return {"ok":false,"code":"NativeAsyncRequired"} if taxpayer_pending else paused.duplicate(true)
func _initialize(): call_deferred("run")
func run():
	create_timer(5).timeout.connect(func(): quit(2))
	var host=Navigation.Host.new();host.kernel=PausedKernel.new();root.add_child(host)
	var identity={"world_id":[0,0],"execution_epoch":1,"revision":2,"tick":4}
	host.kernel.actor_id="mayor";host.kernel.identity=identity
	host.kernel.view={"identity":identity,"observer_actor_id":"mayor","place_id":"town","authorities":[],"rows":[],"next_cursor":null,"fiscal_taxpayer_continuity_available":true,"fiscal_available":false}
	host.kernel.paused={"identity":identity,"observer_actor_id":"mayor","place_id":"town","rows":[{"kind":"fiscal_taxpayer_period_suspended","due_tick":4},{"kind":"fiscal_taxpayer_collection_paused","due_tick":2,"cash_outstanding":17,"grain_outstanding":0}],"next_cursor":{"offset":32}}
	host.kernel.taxpayer_pending=true;assert(not Economic.prepare(host) and host.commands.is_empty());host.kernel.taxpayer_pending=false;assert(Economic.prepare(host))
	var panel=VBoxContainer.new();panel.name="DynamicWorldPanel";host.add_child(panel)
	var scroll=ScrollContainer.new();scroll.name="DynamicWorldScroll";panel.add_child(scroll);scroll.add_child(VBoxContainer.new())
	Economic.build(host,host);await process_frame
	var section=host.find_child("FiscalTaxpayerContinuitySection",true,false);assert(section!=null)
	var rows=section.find_children("*","Label",true,false).filter(func(label): return "原纳税组织资产已换属" in label.text);assert(rows.size()==2 and "本期暂停评税" in rows[0].text and "不补征本期" in rows[0].text and "尚欠17钱" in rows[1].text and "不向新所有人转派旧税" in rows[1].text)
	assert(host.commands.is_empty());section.get_children()[-1].get_child(0).emit_signal("pressed")
	assert(host.get_meta(Economic.META)["fiscal_taxpayer_cursor"]=={"offset":32} and host.get_meta(Economic.META)["fiscal_cursor"].is_empty())
	Economic.select_authority(host,{});assert(host.get_meta(Economic.META)["fiscal_taxpayer_cursor"].is_empty())
	host.kernel.paused["place_id"]="elsewhere";assert(not Economic.prepare(host) and host._resident_page_error["code"]=="StaleFiscalTaxpayerView")
	host.free();print("fiscal_taxpayer_continuity_panel: PASS (explicit pause, frozen debt, no catchup, pure async/cursor, stale source)");quit()
