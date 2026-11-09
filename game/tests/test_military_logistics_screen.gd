## 真实Alpha的编组、军营、行旅、野战与围城共享口粮提示；复用正常出征前序，不注入军团或口粮。
extends "res://tests/test_auto_travel_boundaries.gd"
const Assets=preload("res://core/military_assets.gd")
const Location=preload("res://core/world_location.gd")

## 每个军粮入口都必须与同一只读报价相等，重复渲染不扣粮、不改请求号，也不暗示可以远程补给。
func check_logistics(label: String) -> void:
	var state: Dictionary=main.kernel.snapshot(); var id: String=Location.army_id(state,state.player_actor_id)
	var quote: Dictionary=Assets.supply_preview(state,main._data,id)
	var text: Label=main.find_child("ArmyLogistics",true,false)
	check(text!=null and quote.ok,label+"显示真实本人军团口粮")
	if text==null or not quote.ok: return
	check(text.text.contains("军粮 %d份"%quote.available) and text.text.contains("每半日需 %d份"%quote.required) and text.text.contains("按当前 %d口估算 %d个完整半日"%[quote.mouth_count,quote.full_ticks]),label+"现粮、名册、定额与估算均来自同一查询")
	check(text.text.contains("下个半日缺粮")==bool(quote.shortage>0),label+"仅在下一半日确实短缺时警示")
	check(text.tooltip_text.contains("伤员") and text.tooltip_text.contains("行军、野战与围城"),label+"说明实际计口范围与共享半日消费")
	main._render()
	check(main.kernel.snapshot()==state,label+"重绘全世界状态保持不变")

## 正常出征命令完成后即检查真实地图页面，不从军团面板伪造一份单独库存。
func send(kind: String, params: Dictionary={}) -> bool:
	var ok: bool=super.send(kind,params)
	if ok and kind=="campaign_depart": main._render(); check_logistics("行军地图")
	return ok

## 父测试以真实军令打完野战并进入围城；在每个公开决策边界检查可见粮秣，个人战不冒用军事提示。
func boundary(label: String) -> void:
	super.boundary(label)
	if label.contains("野战") or label.contains("围城"): check_logistics(label)

## 名册选择、粮食数量和办理按钮使用当前控件回调，不能在编组成功前制造军团或领用口粮。
func formation_ui() -> void:
	if not fresh(): return
	check(main.find_child("ArmyLogistics",true,false)==null,"无军团时不显示虚构口粮")
	if not send("move_local",{"place_id":"west.camp"}): return
	if not send("office_accept",{"office_id":"office.qingchuan.army","context":F.context(main.kernel,"role.qingchuan.0")}): return
	check(main.kernel.advance_one_tick().ok,"真实半日任官")
	if not send("campaign_accept",{"template_id":"order.qingchuan.army","context":F.context(main.kernel,"role.qingchuan.0")}): return
	main._open_military_camp(); await settle(); await click("camp_tab","formation")
	for id in ["unit.qingchuan.infantry","unit.qingchuan.archer","unit.qingchuan.cavalry"]:
		for control in main.find_children("*","CheckBox",true,false):
			if control.get_meta("unit_id","")==id: control.button_pressed=true; break
		await settle()
	var food: SpinBox=main.find_child("CampFoodQuantity",true,false); var money: SpinBox=main.find_child("CampMoneyQuantity",true,false)
	check(food!=null and money!=null,"真实编组数量控件存在")
	if food==null or money==null: return
	var before: Dictionary=main.kernel.snapshot(); food.value=500; money.value=300
	var quote: Label=main.find_child("CampFormationProvisions",true,false)
	check(quote!=null and quote.visible and quote.text.contains("每半日需 27份") and quote.text.contains("按当前 27口估算 18个完整半日"),"正式三部草案500粮明确预计当前27口18个完整半日")
	food.value=1000000
	check(not quote.visible and quote.text.is_empty(),"超出真实公库的无效报价清除旧续航，不沿用500粮估算")
	food.value=500
	check(quote.visible and quote.text.contains("军粮 500份") and main.kernel.snapshot()==before,"数量改回后恢复最新纯报价，整个数量编辑没有提前领粮")
	await click("army_form"); check_logistics("已编组军营")
	await click("camp_tab","supply"); check_logistics("军营补给页")
	await click("camp_close"); await click("move_local","west.street"); await click("map")
	check_logistics("城门地图")

## 每个入口的回归均经过正常世界，无图形运行只检控件与数据；真实字体、构图和鼠标另由根层试玩。
func run_checks() -> void:
	main=Host.new(); root.add_child(main); main.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT); await settle()
	await formation_ui()
	await military_boundary()
	await finish()

## 释放宿主与声音后报告实际断言，不把控件回归当作截图验收。
func finish() -> void:
	if is_instance_valid(main):
		main._ambience.music.stop(); main._ambience.effect.stop()
		main._ambience.music.stream=null; main._ambience.effect.stream=null
		main.queue_free()
	await settle()
	print("MILITARY_LOGISTICS_SCREEN_CHECKS=",checks," FAILURES=",failures)
	quit(1 if failures else 0)
