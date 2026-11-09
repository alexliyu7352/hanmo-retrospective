## 下一统一节点的真实Main围城攻具片；调用方提供同源原Kernel与native Main，不伪造库存或独立重跑整场战争。
extends RefCounted

## 把每个必要断言附到统一回执；失败立即停止本片，保留当前真实世界供诊断。
static func check(harness,value: bool,label: String) -> bool:
	harness.rows.append({"stage":"siege_engine_"+label,"ok":value})
	if not value: harness.failures.append("siege_engine_"+label)
	return value

## 每次查当前存活页面，不持有已被异步同源刷新移除的旧控件。
static func screen(main):
	return main.find_child("SiegeScreen",true,false)

## 实际下拉选项的信号走产品回调；禁用或不存在的选项明确失败，不能直接补写草案。
static func choose(harness,main,control_name: String,value: Variant) -> bool:
	var view=screen(main)
	var choice: OptionButton=view.find_child(control_name,true,false) if view!=null else null
	if choice!=null:
		for i in range(choice.item_count):
			if choice.get_item_metadata(i)!=value: continue
			if not check(harness,not choice.is_item_disabled(i),control_name+"_enabled"): return false
			choice.select(i); choice.item_selected.emit(i)
			return await harness.settled(main)
	return check(harness,false,control_name+"_missing")

## 实际城防图的选点信号包含native草案未到时的重建路径，不越过页面直接替换命令。
static func target(harness,main,node_id: String) -> bool:
	var view=screen(main)
	if not check(harness,view!=null,"target_screen"): return false
	view.canvas.node_chosen.emit(node_id)
	return await harness.settled(main)

## 只用当前树中可用的整军确认按钮，不用调用Kernel冒充玩家确认。
static func submit_button(node: Node) -> Button:
	if node is Button and node.get_meta("command_kind","")=="siege_submit": return node
	for child in node.get_children():
		var found: Button=submit_button(child)
		if found!=null: return found
	return null

## 原Kernel独立执行同一完整草案及一次advance；实际Main负责自己的native提交、半日和重绘。
static func submit_round(harness,main,baseline,label: String) -> bool:
	var view=screen(main)
	var button: Button=submit_button(view) if view!=null else null
	if not check(harness,button!=null and not button.disabled,label+"_submit_enabled"): return false
	var context: Dictionary=baseline.command_context()
	var submitted: Dictionary=baseline.submit({"kind":"siege_plan","params":{"siege_id":view.siege.id,"orders":view.orders.duplicate(true)},"actor_id":context.actor_id,"request_seq":context.next_request_seq,"expected_revision":context.revision})
	if not check(harness,submitted.get("ok",false),label+"_original_plan"): return false
	if not check(harness,baseline.advance_one_tick().get("ok",false),label+"_original_half"): return false
	button.pressed.emit()
	if not check(harness,await harness.settled(main),label+"_settled"): return false
	harness.record(main,"siege_engine_"+label,JSON.stringify(baseline.snapshot(),"",false,true))
	return check(harness,main.kernel.siege_page()==baseline.siege_page(),label+"_finite_page")

## 同源原Kernel的正式单部报价提供期待值，不从UI文案反推伤害或自算攻具加成。
static func quote(baseline,unit_id: String,order: Dictionary) -> Dictionary:
	var page: Dictionary=baseline.siege_page()
	var result: Dictionary=baseline.siege_draft(page.identity,[{"unit_id":unit_id,"order":order}])
	return result.quotes[0].quote if result.ok else result

## 以真实公库候选选具；没有现成器械时经营造命令和作者规定的半日数产出，不能改短围城期限。
static func prepare_engine(harness,main,baseline,unit_id: String) -> bool:
	var view=screen(main)
	if not view.page_input.engine_choices.get(unit_id,[]).is_empty(): return true
	if not await target(harness,main,view.siege.unit_nodes[unit_id]): return false
	if not await choose(harness,main,"SiegeAction","build_engine"): return false
	var ticks: int=screen(main).topology.rules.engine_ticks
	if not await submit_round(harness,main,baseline,"manufacture"): return false
	for i in range(1,ticks):
		view=screen(main)
		if not check(harness,view!=null and view.siege.phase=="active","manufacture_active"): return false
		if not check(harness,view.page_input.engine_choices.get(unit_id,[]).is_empty(),"not_ready_"+str(i)): return false
		if not await submit_round(harness,main,baseline,"manufacture_wait_"+str(i)): return false
	return check(harness,not screen(main).page_input.engine_choices.get(unit_id,[]).is_empty(),"manufactured_public_stock")

## 调用前须完成真实任官、编组、行军与开围城；本片涵盖制造、选择/取消、异步选点、改令、确认与轮后回执。
static func run(harness,main,baseline) -> bool:
	if not check(harness,await harness.settled(main),"entry_settled"): return false
	var view=screen(main)
	if not check(harness,view!=null and view.siege.phase=="active" and not view.selected_id.is_empty(),"actual_active_entry"): return false
	if not check(harness,main._resident_mode and main.kernel.siege_page()==baseline.siege_page(),"native_finite_entry"): return false
	var unit_id: String=view.selected_id
	if not await prepare_engine(harness,main,baseline,unit_id): return false
	view=screen(main)
	var source: Dictionary=baseline.snapshot()
	var choices: Array=view.page_input.engine_choices[unit_id]
	var use: Dictionary=choices[0].engine_use
	var army_id: String=source.military_units[unit_id].army_id
	if not check(harness,use.inventory_id==source.armies[army_id].inventory_id and use.quantity==1 and choices[0].available_quantity>0,"owned_actual_source"): return false
	for other in view.siege.unit_nodes:
		if view.orders.has(other): continue
		if not check(harness,not view.page_input.engine_choices.has(other),"no_uncontrolled_stock"): return false
	var node_id: String=""
	for node in view.topology.nodes:
		if quote(baseline,unit_id,{"action":"assault","node_id":node.id,"engine_use":use}).get("ok",false): node_id=node.id; break
	if not check(harness,not node_id.is_empty(),"real_assault_target"): return false
	if not await target(harness,main,node_id) or not await choose(harness,main,"SiegeAction","assault"): return false
	var plain: Dictionary=quote(baseline,unit_id,{"action":"assault","node_id":node_id})
	var equipped: Dictionary=quote(baseline,unit_id,{"action":"assault","node_id":node_id,"engine_use":use})
	if not await choose(harness,main,"SiegeEngine",use): return false
	view=screen(main)
	var damage: Label=view.find_child("SiegeDamagePreview",true,false)
	if not check(harness,view.orders[unit_id].engine_use==use and equipped.damage>plain.damage and damage!=null and damage.text=="本部预计破坏城防 %d" % equipped.damage,"explicit_quote"): return false
	if not await choose(harness,main,"SiegeEngine",{}): return false
	view=screen(main); damage=view.find_child("SiegeDamagePreview",true,false)
	if not check(harness,not view.orders[unit_id].has("engine_use") and damage.text=="本部预计破坏城防 %d" % plain.damage,"cancel_no_bonus"): return false
	if not await choose(harness,main,"SiegeEngine",use): return false
	main._render()
	if not check(harness,await harness.settled(main),"redraw_settled"): return false
	if not await target(harness,main,node_id): return false
	view=screen(main)
	if not check(harness,view.orders[unit_id].get("engine_use")==use,"redraw_async_retarget_keeps_use"): return false
	if not await target(harness,main,view.siege.unit_nodes[unit_id]) or not await choose(harness,main,"SiegeAction","wait"): return false
	if not check(harness,not screen(main).orders[unit_id].has("engine_use"),"different_action_clears_use"): return false
	if not await target(harness,main,node_id) or not await choose(harness,main,"SiegeAction","assault") or not await choose(harness,main,"SiegeEngine",use): return false
	harness.record(main,"siege_engine_selections_readonly",JSON.stringify(baseline.snapshot(),"",false,true))
	var before_stock: int=screen(main).page_input.engine_choices[unit_id][0].available_quantity
	var before_tick: int=source.tick
	if not await submit_round(harness,main,baseline,"actual_assault"): return false
	view=screen(main)
	if not check(harness,view!=null and baseline.snapshot().tick==before_tick+1,"one_actual_half"): return false
	var events: Array=view.page_input.last_damage_events
	var damage_event: Dictionary={}
	for event in events:
		if event.unit_id==unit_id and event.node_id==node_id: damage_event=event
	if not check(harness,not damage_event.is_empty() and damage_event.damage==mini(source.sieges[view.siege.id].node_hp[node_id],equipped.damage),"actual_damage_receipt"): return false
	if not check(harness,view.find_child("SiegeRoundFeedback",true,false)!=null,"rendered_round_feedback"): return false
	if not check(harness,view.page_input.engine_choices[unit_id][0].available_quantity==before_stock,"use_does_not_consume_engine"): return false
	return true
