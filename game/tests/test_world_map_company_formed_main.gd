## 原清溪任命、受令与编组实际控件产生军团后，由地图只消费本人摘要；不注入军队或权源。
extends "test_world_map_cargo_main.gd"
const Company=preload("res://ui/world_map_company.gd")
var commands: Array=[]
var bank_path: String=""

## 目标与命令一起匹配，避免误点另一个角色或同页不同军令。
func target_button(parent: Node,kind: String,target: String="") -> Button:
	if parent is Button and parent.is_visible_in_tree() and parent.get_meta("command_kind","")==kind and (target.is_empty() or parent.get_meta("target_id","")==target):return parent
	for child in parent.get_children():
		var found: Button=target_button(child,kind,target)
		if found!=null:return found
	return null

## 每次都从新页面重新定位真实控件，保存原业务参数和提交前后身份。
func press(main: Node,kind: String,target: String="",business: bool=true) -> bool:
	var control: Button=target_button(main,kind,target)
	if not check(control!=null and not control.disabled,"原控件可用："+kind+" / "+target):return false
	var before: Dictionary=main.kernel.source_identity()
	var params: Dictionary=control.get_meta("camp_params",{}).duplicate(true)
	control.pressed.emit()
	if not await settled(main):return false
	var after: Dictionary=main.kernel.source_identity()
	if business:
		commands.append({"kind":kind,"target":target,"params":params,"before":before,"after":after,"message":main._message})
		return check(after.revision>before.revision,"原业务实际提交："+kind+" · "+main._message)
	return check(after==before,"界面导航不提交世界："+kind)

## 仅继续原办理余时，最多四个半日；异常待办不自动代答。
func finish_action(main: Node) -> bool:
	for _step in range(4):
		if main.kernel.action_context().get("action_id","").is_empty():return true
		if not await press(main,"advance"):return false
	return check(false,"原营务办理未在已知短工期内结束")

## 选择军部只操作既有CheckBox，不直接写MilitaryState或界面选择私表。
func unit_control(parent: Node,id: String) -> CheckBox:
	if parent is CheckBox and parent.get_meta("unit_id","")==id:return parent
	for child in parent.get_children():
		var found: CheckBox=unit_control(child,id)
		if found!=null:return found
	return null

## 保完整提交轨迹和首错；存档是原worker字节，UI磁盘槽及GUI没有被本门验证。
func finish(main: Node) -> void:
	evidence.checks=checks;evidence.failures=failures;evidence.commands=commands;evidence.gui="NOT_TESTED";evidence.ui_save_slots="NOT_TESTED"
	if is_instance_valid(main):evidence.last_message=main._message
	if not output_path.is_empty():
		var file=FileAccess.open(output_path,FileAccess.WRITE)
		if file!=null:file.store_string(JSON.stringify(evidence,"\t",false,true)+"\n")
	if is_instance_valid(main):main.queue_free()
	await process_frame
	print("MAP_FORMED_COMPANY_MAIN_CHECKS=",checks," FAILURES=",failures," SOURCE=ORIGINAL_QINGXI_MILITARY_COMMANDS GUI=NOT_TESTED")
	quit(0 if failures==0 else 1)

## 先照原作者办理出征准备，再观察地图；军令内容、部队与装运数值全部沿原README。
func _run() -> void:
	var catalog: String=""
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--catalog="):catalog=arg.trim_prefix("--catalog=")
		if arg.begins_with("--output="):output_path=arg.trim_prefix("--output=")
		if arg.begins_with("--bank-output="):bank_path=arg.trim_prefix("--bank-output=")
	var main=Main.new();root.add_child(main);main.size=Vector2(1280,800)
	var installed: Dictionary=main.start_content("qingxi::scenario.grain_road_integration",catalog,{},"qingxi::actor.player")
	evidence.install=installed
	if not check(installed.get("ok",false),"原清溪正常Main安装") or not await settled(main):await finish(main);return
	if not await press(main,"move_local","qingxi::place.north.camp"):await finish(main);return
	main._open_military_camp()
	if not await settled(main):await finish(main);return
	if not await press(main,"office_accept","qingxi::office.north_leader") or not await finish_action(main):await finish(main);return
	if not await press(main,"campaign_accept","qingxi::order.north_clear") or not await finish_action(main):await finish(main);return
	if not await press(main,"camp_tab","formation",false):await finish(main);return
	for id in ["qingxi::unit.north.work_detail","qingxi::unit.north.blockade"]:
		var unit: CheckBox=unit_control(main,id)
		if not check(unit!=null and not unit.disabled,"原部队可以选入编组："+id):await finish(main);return
		unit.button_pressed=true
		if not await settled(main):await finish(main);return
	var food: SpinBox=main.find_child("CampFoodQuantity",true,false)
	var money: SpinBox=main.find_child("CampMoneyQuantity",true,false)
	if not check(food!=null and money!=null,"原装运数量控件存在"):await finish(main);return
	food.value=360;money.value=100
	if not await settled(main):await finish(main);return
	if not await press(main,"army_form") or not await finish_action(main):await finish(main);return
	var before: PackedByteArray=await checkpoint(main)
	var source: Dictionary=main.kernel.source_identity()
	if not bank_path.is_empty() and not before.is_empty():
		var file=FileAccess.open(bank_path,FileAccess.WRITE);file.store_buffer(before);file.close()
		evidence.bank_sha256=FileAccess.get_sha256(bank_path)
	main._open_map()
	if not await settled(main):await finish(main);return
	var company: Dictionary=main.get_meta(Company.META,{})
	evidence.company=company.duplicate(true);evidence.location=main.kernel.journey_sidebar().location.duplicate(true)
	var army: Dictionary=company.get("page",{}).get("army",{})
	check(company.get("source",{}).get("identity")==source and not army.is_empty() and army.get("commander_actor_id")=="qingxi::actor.player" and army.get("commanded_by_observer")==true,"正式编组结果进入同源本人统领军团摘要")
	var canvas=main.find_child("WorldMapCanvas",true,false)
	check(canvas!=null and canvas.observer_label=="所领军团","原本人锚点显示所领军团身份")
	var after: PackedByteArray=await checkpoint(main)
	check(not before.is_empty() and before==after and main.kernel.source_identity()==source,"地图观察不改变军团、位置或checkpoint字节")
	evidence.checkpoint_unchanged=before==after
	await finish(main)
