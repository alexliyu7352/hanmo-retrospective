## 真实成长页护具门：正常穿卸与售出走Main/Kernel，产权边界只构造隔离副本，不写用户存档。
extends SceneTree
const Main=preload("res://ui/main.gd")
const Armor=preload("res://core/domains/personal/personal_armor.gd")
const Interaction=preload("res://core/interaction_rules.gd")
var checks: int=0
var failures: int=0
var main

## 主场景不入树以免启动个人存储和音频，实际page入树供正常Composer/按钮工作。
func _init() -> void: call_deferred("run")

## 前置失败立即停对应流程，所有失败反映在最终退出状态。
func check(value: bool, label: String) -> bool:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)
	return value

## 仅查当前活页面，旧控件专测明确保存原引用。
func control(label: String) -> Control: return main.page.find_child(label,true,false)

## 标准下拉信号只更新草案和正式预览。
func choose(id: String) -> bool:
	var choice: OptionButton=control("GrowthArmorChoice")
	for index in range(choice.item_count):
		if choice.get_item_metadata(index)==id:
			choice.select(index); choice.item_selected.emit(index); return true
	check(false,"护具候选存在："+id); return false

## 按钮沿现有Main._perform真实提交；重绘递延释放不影响同步结果检查。
func press() -> void:
	var button: Button=control("GrowthArmorEquip")
	if check(button!=null and not button.disabled,"护具按钮可操作"): button.pressed.emit()

## 使用当前玩家身份和版本的原注册命令，不绕过提交入口。
func command(kind: String, params: Dictionary) -> Dictionary:
	var context: Dictionary=main.kernel.command_context()
	return {"kind":kind,"params":params,"actor_id":context.actor_id,"expected_revision":context.revision,"request_seq":context.next_request_seq}

## 新局按正式作者装载，再从正常成长页验证零消耗配置及失效来源。
func run() -> void:
	main=Main.new()
	for node in [main.storage,main.journey_view,main._ui_font_scale,main._ambience]: main.add_child(node)
	main._ambience.add_child(main._ambience.music); main._ambience.add_child(main._ambience.effect)
	main.page=VBoxContainer.new(); root.add_child(main.page)
	var index: Dictionary=JSON.parse_string(FileAccess.get_file_as_string("res://data/generated/installed-content.json"))
	var path: String="res://data/generated/"+index.sets[0].directory+"/catalog.hmcatalog.json"
	if not check(main.start_content("base::scenario.procurement",path).ok,"正式Main作者护具开局"): finish(); return
	main._open_growth()
	if not check(control("GrowthArmorChoice")!=null and control("GrowthArmorEquip")!=null and control("GrowthArmorEffective")!=null,"正常成长页具有护具选择、穿卸与实际防御控件"): finish(); return
	var state: Dictionary=main.kernel.snapshot(); var actor: String=state.player_actor_id
	var current: Dictionary=main.kernel.character_summary().equipment
	check(current.armor_id=="base::item.coat" and current.armor_effective==Armor.effective(state,main._data,actor),"选中引用与实际效果沿同一护具规则")
	check(control("GrowthArmorChoice").get_selected_metadata()==current.armor_id and control("GrowthArmorEffective").text.contains("6"),"初始实际已穿护具及防御可见")
	choose(""); check(main.kernel.snapshot()==state,"仅选择卸下不改世界")
	var old_button: Button=control("GrowthArmorEquip")
	press()
	check(main.kernel.snapshot().actors[actor].combat.armor_id=="" and main.kernel.snapshot().inventories==state.inventories,"正常卸下不移动或复制实物")
	var once: Dictionary=main.kernel.snapshot(); old_button.pressed.emit()
	check(main.kernel.snapshot()==once,"同帧旧页面重复点击不借新版本再次提交")
	choose("base::item.coat"); press()
	check(main.kernel.snapshot().actors[actor].combat.armor_id=="base::item.coat" and control("GrowthArmorEffective").text.contains("6"),"重新穿戴本人护具立即显示真实防御")
	check(main.kernel.snapshot().tick==state.tick and main.kernel.snapshot().rng==state.rng and main.kernel.snapshot().inventories==state.inventories,"穿卸不消耗半日、随机流或实物")
	state=main.kernel.snapshot()
	control("GrowthTabInventory").pressed.emit()
	check(control("GrowthArmorChoice")!=null and control("GrowthInventory")!=null and control("GrowthArmorEffective").text.contains("6"),"正常行囊页沿同一护具选择与实际防御")
	choose(""); var canceled: Button=control("GrowthArmorEquip")
	main._close_growth(); canceled.pressed.emit()
	check(main.kernel.snapshot()==state,"关闭未提交草案和旧按钮均不卸下当前护具")
	main.set_meta("growth_page","skills"); main._open_growth()
	var denied: Dictionary=main.kernel.submit(command("equip_armor",{"armor_id":"base::item.public_coat"}))
	check(denied.get("code")=="ArmorNotOwned" and main.kernel.snapshot()==state,"非本人公共库存由原命令拒绝且整态不变")
	old_button=control("GrowthArmorEquip")
	var context: Dictionary=Interaction.for_role(state,main._data,"base::role.merchant")
	main._perform("trade",{"shop_id":"base::shop.grain","item_id":"base::item.coat","quantity":1,"side":"sell","payer_wallet_id":state.actors[actor].wallet_id,"context":context})
	for _frame in range(6): await process_frame
	state=main.kernel.snapshot(); current=main.kernel.character_summary().equipment
	check(main._message.is_empty() and current.armor_id=="base::item.coat" and current.armor_effective.armor_defense==0,"真实卖出最后一件后选择保留但实际防御为0")
	check(control("GrowthArmorChoice").get_selected_metadata()==current.armor_id and control("GrowthArmorEffective").text.contains("0") and control("GrowthArmorEquip").disabled,"售出后的原选择可辨且不能重新穿上")
	check(control("GrowthArmorStatus").text.contains("私人行囊"),"售出失败解释沿原所有权报价")
	if is_instance_valid(old_button): old_button.pressed.emit()
	check(main.kernel.snapshot()==state,"交易重绘后的旧穿戴按钮不会恢复卖出物品")
	choose(""); press(); check(main.kernel.snapshot().actors[actor].combat.armor_id=="","失效选择仍可显式卸下")
	boundaries()
	finish()

## 从新局复制出的预留、组织和代管lot只验投影与原报价，不伪造正常玩家获取流程。
func boundaries() -> void:
	var index: Dictionary=JSON.parse_string(FileAccess.get_file_as_string("res://data/generated/installed-content.json"))
	var path: String="res://data/generated/"+index.sets[0].directory+"/catalog.hmcatalog.json"
	check(main.start_content("base::scenario.procurement",path).ok,"边界从同一正式作者重启")
	var source: Dictionary=main.kernel.snapshot(); var actor: String=source.player_actor_id
	for kind in ["reserved","organization","other_actor"]:
		var state: Dictionary=source.duplicate(true)
		var bag: Dictionary=state.inventories[state.actors[actor].inventory_id]
		for lot in bag.lots:
			if lot.item_id!="base::item.coat": continue
			if kind=="reserved": lot.reserved_by="action.test"
			else: lot.owner={"kind":"organization","id":"base::org.office"} if kind=="organization" else {"kind":"actor","id":"base::actor.chen"}
		main.kernel._state=state; main._open_growth()
		var before: Dictionary=state.duplicate(true)
		var equipment: Dictionary=main.kernel.character_summary().equipment
		check(equipment.armor_effective.armor_defense==0 and main.kernel.preview("equip_armor",{"armor_id":"base::item.coat"}).get("code")=="ArmorNotOwned","原规则拒绝不可用护具 "+kind)
		check(equipment.armor_options.size()==(1 if kind=="reserved" else 0),"只保留本人候选，预留资格仍交报价 "+kind)
		check(control("GrowthArmorEquip").disabled and control("GrowthArmorStatus").text.contains("预留"),"禁用原因可见 "+kind)
		equipment.armor_effective.armor_defense=99; equipment.armor_options.append({"item_id":"forged","quantity":99})
		check(main.kernel.snapshot()==before and main.kernel.character_summary().equipment.armor_effective.armor_defense==0,"投影深隔离且不纠正原选择 "+kind)
	main.kernel._state=source; main._advancing=true; main._open_growth()
	check(control("GrowthArmorEquip").disabled and control("GrowthArmorStatus").text=="正在办理当前行动。","办理期间沿原忙碌反馈禁用护具操作")
	main._advancing=false; main._render()
	var button: Button=control("GrowthArmorEquip")
	var changed: Dictionary=main.kernel.submit(command("equip_armor",{"armor_id":""}))
	check(changed.ok,"真实外部命令改变页面所绑定版本")
	var before: Dictionary=main.kernel.snapshot(); button.pressed.emit()
	check(main.kernel.snapshot()==before,"仍挂载但版本陈旧的穿戴草案不能借当前revision")
	main._render(); choose(""); main._close_growth()
	check(control("GrowthArmorChoice")==null,"关闭返回原场所，无残留装备面板")

## 释放未入树的主场景与独立真实页面，汇总本门实际结果。
func finish() -> void:
	main.page.free(); main.free()
	await process_frame
	print("ARMOR_EQUIPMENT_CONTROLS_CHECKS=",checks," FAILURES=",failures)
	quit(0 if failures==0 else 1)
