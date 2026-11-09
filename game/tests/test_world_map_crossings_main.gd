## 原作者关渡经正规Main和实际半日旅行；不用测试setter开放关口、移动玩家或代答自然遭遇。
extends "test_world_map_company_formed_main.gd"
const TimeRules=preload("res://core/base/time_rules.gd")

## JSON读口坐标可为float；比较真实数值，不把容器中的int/float差异当成玩家移动。
func same_point(actual: Array, expected: Array) -> bool:
	return actual.size()==2 and expected.size()==2 and float(actual[0])==float(expected[0]) and float(actual[1])==float(expected[1])

## 原checkpoint只保存本轮实际世界；成功、失败或自然待决停点均保原件与来源。
func finish(main: Node) -> void:
	if is_instance_valid(main) and main._started and main.kernel!=null:
		evidence.final_identity=main.kernel.source_identity()
		evidence.final_sidebar=main.kernel.journey_sidebar()
		if not bank_path.is_empty():
			var bytes: PackedByteArray=await checkpoint(main)
			if not bytes.is_empty():
				var bank=FileAccess.open(bank_path,FileAccess.WRITE)
				if bank!=null:bank.store_buffer(bytes);bank.close();evidence.bank_sha256=FileAccess.get_sha256(bank_path)
	evidence.commands=commands;evidence.checks=checks;evidence.failures=failures;evidence.gui="NOT_TESTED";evidence.ui_save_slot="NOT_TESTED"
	if not output_path.is_empty():
		var file=FileAccess.open(output_path,FileAccess.WRITE)
		if file!=null:file.store_string(JSON.stringify(evidence,"\t",false,true)+"\n")
	if is_instance_valid(main):main.queue_free()
	await process_frame
	print("MAP_CROSSINGS_MAIN_CHECKS=",checks," FAILURES=",failures," SOURCE=FORMAL_AUTHOR_NATIVE_MAIN GUI=NOT_TESTED")
	quit(0 if failures==0 else 1)

## 地图只将真实关渡标记变成候选，封闭原码和开放路线均由原travel报价决定。
func _run() -> void:
	var catalog: String=""
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--catalog="):catalog=arg.trim_prefix("--catalog=")
		if arg.begins_with("--output="):output_path=arg.trim_prefix("--output=")
		if arg.begins_with("--bank-output="):bank_path=arg.trim_prefix("--bank-output=")
	if not check(ClassDB.class_exists("HanmoResidentBridge") and not catalog.is_empty(),"原扩展及正式安装关渡目录就绪"):quit(1);return
	var main=Main.new();root.add_child(main);main.size=Vector2(1280,800)
	var installed: Dictionary=main.start_content("crossings::scenario.crossings",catalog,{},"society::actor.apprentice")
	evidence.installed=installed.duplicate(true)
	if not check(installed.get("ok",false),"从原青川起点正常安装关渡作者情景："+str(installed)) or not await settled(main):await finish(main);return
	var initial: Dictionary=main.kernel.source_identity()
	var before: PackedByteArray=await checkpoint(main)
	main._open_map()
	if not await settled(main):await finish(main);return
	var canvas=main.find_child("WorldMapCanvas",true,false)
	if canvas!=null:
		evidence.initial_identity=initial;evidence.initial_map=canvas.view.duplicate(true);evidence.crossings=canvas._crossings.duplicate(true);evidence.crossing_error=canvas._crossing_error
	if not check(canvas!=null and canvas._crossings.size()==3 and same_point(canvas.view.observer_position,[35,60]),"原起点与三处作者关渡真实投影一致"):await finish(main);return
	var closed: String="";var opened: String=""
	for key in canvas._crossings:
		var row: Dictionary=canvas._crossings[key]
		if row.kind=="ford" and canvas.view.crossing_closed.get(row.flag_id)==true:closed=key
		if row.kind=="bridge" and canvas.view.crossing_closed.get(row.flag_id)==false:opened=key
	if not check(not closed.is_empty() and not opened.is_empty(),"关闭渡口与开放北桥状态来自本次原旗标"):await finish(main);return
	var closed_point: Array=canvas._crossings[closed].point.duplicate()
	canvas._crossing_markers[closed].pressed.emit()
	if not await settled(main):await finish(main);return
	var denied: Dictionary=main._map_quote();evidence.closed_quote=denied.duplicate(true)
	check(main.journey_view.selected_point==closed_point and not denied.get("ok",false) and button(main,"map_confirm").disabled,"封闭渡口仍可查看，但原报价拒绝并禁用确认")
	canvas=main.find_child("WorldMapCanvas",true,false)
	var open_point: Array=canvas._crossings[opened].point.duplicate()
	canvas._crossing_markers[opened].pressed.emit()
	if not await settled(main):await finish(main);return
	var allowed: Dictionary=main._map_quote();evidence.open_quote=allowed.duplicate(true)
	check(main.journey_view.selected_point==open_point and allowed.get("ok",false) and allowed.get("terrain",false),"开放北桥交原地形路径报价，不把符号当通行许可")
	canvas=main.find_child("WorldMapCanvas",true,false);canvas.point_selected.emit([830,290])
	if not await settled(main):await finish(main);return
	var quote: Dictionary=main._map_quote();evidence.route_quote=quote.duplicate(true)
	if not check(quote.get("ok",false) and quote.get("terrain",false) and quote.get("points",[]).size()>2,"原作者东侧落点使用实际绕行关桥路径"):await finish(main);return
	var context=main.find_child("WorldMapRouteContext",true,false)
	var finish_tick: int=initial.tick+quote.duration
	var expected: String="预计到达 第%d日%s"%[TimeRules.day_index(finish_tick)+1,"上半日" if TimeRules.half_day(finish_tick)==0 else "下半日"]
	check(context is Label and context.text.contains(expected) and context.text.contains("启程负重"),"正式Main用同源时钟和原地形报价显示完成时刻与负重")
	check(main.kernel.source_identity()==initial and await checkpoint(main)==before,"查看关渡和多次报价保持完整checkpoint不变")
	if not await press(main,"map_confirm"):await finish(main);return
	evidence.steps=[]
	for _step in range(quote.duration+1):
		var sidebar: Dictionary=main.kernel.journey_sidebar()
		if not sidebar.travel.active:break
		if not sidebar.get("encounter",{}).is_empty():
			evidence.natural_stop=sidebar.encounter.duplicate(true);check(false,"自然相逢已停，测试不代替玩家回应");await finish(main);return
		if not await press(main,"advance"):await finish(main);return
		evidence.steps.append({"identity":main.kernel.source_identity(),"map":main.kernel.map_view()})
	var final_map: Dictionary=main.kernel.map_view();var final_sidebar: Dictionary=main.kernel.journey_sidebar()
	check(not final_sidebar.travel.active and final_sidebar.location.kind=="Wilderness" and same_point(final_map.observer_position,[830,290]),"实际逐半日通过合法地形并抵达原落点")
	check(final_map.crossing_closed==evidence.initial_map.crossing_closed,"行走不暗改关渡旗标")
	check(main.kernel.source_identity().tick==finish_tick,"无停顿的实际完成半日与原报价相等")
	await finish(main)
