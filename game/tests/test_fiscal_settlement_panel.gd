## 财政UI合同夹具只验呈现与原pending链；不作为原生政策/转账或Main运行证据。
extends SceneTree
const Economic=preload("res://ui/economic_world_panel.gd")
const Navigation=preload("res://tests/test_economic_world_navigation.gd")
class FiscalKernel extends Navigation.Kernel:
    var fiscal_pending=false
    var fiscal_queries=0
    var fiscal={}
    func fiscal_settlement(_place,_cursor,_authority):
        fiscal_queries+=1
        if fiscal_pending: return {"ok":false,"code":"NativeAsyncRequired"}
        return fiscal.duplicate(true)
func _initialize(): call_deferred("run")
func run():
    create_timer(5).timeout.connect(func(): quit(2))
    var host=Navigation.Host.new();host.kernel=FiscalKernel.new();root.add_child(host)
    var identity={"world_id":[0,0],"execution_epoch":1,"revision":2,"tick":2}
    host.kernel.actor_id="mayor";host.kernel.identity=identity
    host.kernel.view={"identity":identity,"observer_actor_id":"mayor","place_id":"town","authorities":[],"rows":[],"next_cursor":null,"fiscal_available":true,"configuration_available":false}
    host.kernel.fiscal={"identity":identity,"observer_actor_id":"mayor","place_id":"town","rows":[{"kind":"tax_assessment","due_tick":2,"mode":"normal","cash_due":20,"cash_paid":7,"cash_outstanding":13,"grain_due":8,"grain_paid":8,"grain_outstanding":0,"produced_grain":20,"civilian_grain_reserve":4}],"next_cursor":{"offset":32}}
    host.kernel.fiscal_pending=true;assert(not Economic.prepare(host));assert(host.commands.is_empty())
    host.kernel.fiscal_pending=false;assert(Economic.prepare(host))
    var panel=VBoxContainer.new();panel.name="DynamicWorldPanel";host.add_child(panel)
    var scroll=ScrollContainer.new();scroll.name="DynamicWorldScroll";panel.add_child(scroll);scroll.add_child(VBoxContainer.new())
    Economic.build(host,host);await process_frame
    var row=host.find_child("FiscalSettlementRow",true,false);assert(row!=null and "应缴20钱，实缴7钱，尚欠13钱" in row.text and "本期实际粮产20份" in row.text)
    assert(host.find_child("ProductionConfiguration",true,false)==null and host.commands.is_empty())
    var nav=host.find_child("FiscalSettlementSection",true,false).get_children()[-1]
    var next=nav.get_child(0);next.emit_signal("pressed")
    assert(host.get_meta(Economic.META)["fiscal_cursor"]=={"offset":32} and host.get_meta(Economic.META)["cursor"].is_empty())
    Economic.select_authority(host,{});assert(host.get_meta(Economic.META)["fiscal_cursor"].is_empty())
    host.kernel.fiscal["identity"]={"revision":99};assert(not Economic.prepare(host) and host._resident_page_error["code"]=="StaleFiscalSettlementView")
    host.free()
    print("fiscal_settlement_panel: PASS (explicit UI DTO fixture, pending pure read, original due/paid/outstanding, independent cursor, stale source rejected)")
    quit()
