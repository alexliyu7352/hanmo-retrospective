## 真实Alpha庄园经原Main提交与短行动推进，headless只替整页组合和纹理，不替命令或世界规则。
extends SceneTree
const EstateTarget=preload("res://ui/estate_screen.gd")
const Art=preload("res://ui/art_catalog.gd")
const Strict=preload("res://core/strict_json.gd")
var checks: int=0
var failures: int=0

## 不启动主窗口、音频或Native；原Main._perform/_apply_command_result/_finish_short保持不变。
class MainHost extends "res://ui/main.gd":
	var test_stage: Control
	## 局部控件门不走整局初始化。
	func _ready() -> void: pass
	## 只组合同一庄务页；该回调替代绘制范围，不替换原提交或阶段推进。
	func _render() -> void:
		_scene_counter+=1
		if not is_instance_valid(test_stage): return
		for child in test_stage.get_children(): test_stage.remove_child(child); child.queue_free()
		EstateTarget.render(self,test_stage,kernel.estate_page(_estate_id,get_meta("estate_tab","people")))

## 原表现描述仍读取真实清单，缺纹理明确返回null，绝不把本门称作人物像素验收。
class NoPixels extends RefCounted:
	var manifest: Dictionary
	## 保存独立真实清单。
	func _init(value: Dictionary) -> void: manifest=value.duplicate(true)
	## 描述不变形或补造人物。
	func describe() -> Dictionary: return manifest.duplicate(true)
	## 禁止无图门伪造素材。
	func texture(_group: String, _id: String) -> Texture2D: return null

## 进入引擎后运行真实命令链。
func _init() -> void: call_deferred("run")
## 失败影响退出码。
func check(value: bool, label: String) -> bool:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)
	return value
## 递延短行动走原Main，不人工修改时间。
func settle() -> void:
	for _frame in range(6): await process_frame
## 当前按钮命名即原设施ID。
func control(host: MainHost, id: String) -> Control: return host.test_stage.find_child(id,true,false)
## 从真实庄院入口、选中设施、营造到返回，核实资源只支付一次。
func run() -> void:
	var old_registry: RefCounted=Art._registry
	Art._registry=NoPixels.new(Strict.parse_text(FileAccess.get_file_as_string("res://data/presentation.json")).value)
	var host=MainHost.new(); host.size=Vector2(800,800)
	for node in [host.storage,host.journey_view,host._ui_font_scale]: host.add_child(node)
	host._ambience.add_child(host._ambience.music); host._ambience.add_child(host._ambience.effect)
	root.add_child(host); host.test_stage=Control.new(); host.add_child(host.test_stage); host.test_stage.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	var started: Dictionary=host.kernel.new_game("scenario.alpha_life","res://data/generated/alpha_life.hmcatalog.json")
	if check(started.ok,"原受信Alpha目录新建世界 "+str(started)):
		host._data=host.kernel.content().data; host._started=true
		host._perform("move_local",{"place_id":"west.estate"}); await settle()
		check(host.kernel.estate_available("estate.riverside") and host._message.is_empty(),"原Main命令实际进入庄院")
		host.set_meta("estate_tab","build"); host._open_estate("estate.riverside"); await settle()
		var before_select: Dictionary=host.kernel.snapshot()
		var selector: Button=control(host,"EstateFacility_granary")
		if check(selector!=null,"原Main打开后显示真实仓廪选择"):
			selector.pressed.emit(); await settle()
			check(host.kernel.snapshot()==before_select,"选择设施没有交易、时间或RNG变化")
			var request: Dictionary={"estate_id":"estate.riverside","facility":"granary"}
			var quote: Dictionary=host.kernel.preview("estate_build",request)
			var before: Dictionary=host.kernel.snapshot(); var definition: Dictionary=host._data.estates["estate.riverside"]
			var old_level: int=before.estates["estate.riverside"].facilities.granary
			var old_cash: int=before.wallets[definition.wallet_id].balance
			var build: Button=control(host,"EstateBuild_granary")
			if check(quote.ok and build!=null and not build.disabled,"当前正式报价和营造按钮一致"):
				build.pressed.emit()
				var first_submit: Dictionary=host.kernel.snapshot()
				build.pressed.emit()
				check(host.kernel.snapshot()==first_submit,"同帧旧按钮不能重复支付")
				await settle()
				var remaining: int=int(quote.duration)+1
				while not host.kernel.action_context().action_id.is_empty() and remaining>0:
					var advance: Button=control(host,"EstateAdvance")
					if not check(advance!=null and not advance.disabled,"未完营造仅用原继续半日按钮"): break
					advance.pressed.emit(); await settle(); remaining-=1
				var after: Dictionary=host.kernel.snapshot()
				check(host._message.is_empty() and host.kernel.action_context().action_id.is_empty(),"营造按原Main短行动完成")
				check(after.estates["estate.riverside"].facilities.granary==old_level+1,"原世界仓廪真实升一级")
				check(after.wallets[definition.wallet_id].balance==old_cash-int(quote.terms.money_cost),"只从本庄钱包支付正式工钱一次")
				check(control(host,"EstateFacility_granary").button_pressed and control(host,"EstateFacilityDetail").text.contains(str(old_level+1)+"级"),"完成重绘保原选中并显示真实新等级")
				check(control(host,"EstateCompletion")!=null,"完成信息来自真实营造事实")
				control(host,"EstateClose").pressed.emit(); await settle()
				check(host._estate_id.is_empty() and host.kernel.snapshot()==after,"回到庄院不改变刚完成世界")
	host._ambience.free(); host.free(); Art._registry=old_registry; await settle()
	print("ESTATE_OVERVIEW_MAIN_CHECKS=",checks," FAILURES=",failures)
	quit(0 if failures==0 else 1)
