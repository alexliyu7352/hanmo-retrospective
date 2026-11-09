## 通过真实工坊和作者进程编辑军事局部引用，验证原生下拉、数组、错误定位、撤销与保存重开。
extends "res://tests/test_editor_structured.gd"
const WorldFixture=preload("res://tests/companion_fixture.gd")
const WorldCodec=preload("res://core/save_codec.gd")
const WorldLocation=preload("res://core/world_location.gd")
const WorldInventory=preload("res://core/inventory_rules.gd")
var world_commands: Array=[]

## 用正常工坊首建地形并选择军令目标，使第二配置能够实际去程交粮，兼容原源尚未定义地图。
func configure_world(panel) -> void:
	select_record(panel,"world_maps")
	await press(panel,"new_record")
	var map_id: String=panel.selected_id
	await edit(panel,"/name_key","name.scenario.alpha_life")
	await edit(panel,"/width",1200); await edit(panel,"/height",1200)
	await edit(panel,"/cell_size",600)
	await edit(panel,"/cells",[{"terrain":"plain"},{"terrain":"plain"},{"terrain":"plain"},{"terrain":"plain"}])
	for mode in ["foot","army","boat"]: await edit(panel,"/budgets/"+mode,4000)
	await edit(panel,"/money_per_tick",1); await edit(panel,"/fatigue_per_tick",1)
	select_record(panel,"scenarios","scenario.alpha_life")
	await choose(panel,"/world_map_id",map_id)
	select_record(panel,"military_orders","order.military.a")
	await choose(panel,"/target_stronghold_id","stronghold.military.b")
	await choose(panel,"/objective/recipient_organization_id","org.military.b")

## 记录普通玩家命令的输入、时间和真实结果；失败立即停止依赖步骤，不写运行世界快照。
func world_send(kernel, kind: String, params: Dictionary={}) -> bool:
	var result: Dictionary=WorldFixture.send(kernel,kind,params)
	world_commands.append({"kind":kind,"params":params.duplicate(true),"tick":kernel.snapshot().tick,"result":result.duplicate(true)})
	check(result.ok,"第二配置正常命令 "+kind+"："+str(result))
	return result.ok

## 每次只走共享世界的一个半日，验证军需与庄务采用同一日期，不直接增加 tick。
func world_tick(kernel) -> bool:
	var before: int=kernel.snapshot().tick
	var result: Dictionary=kernel.advance_one_tick()
	world_commands.append({"kind":"advance_one_tick","tick":kernel.snapshot().tick,"result":result.duplicate(true)})
	check(result.ok,"第二配置实际半日："+str(result))
	if not result.ok: return false
	var state: Dictionary=kernel.snapshot()
	check(state.tick==before+1 and state.estates["estate.military.private"].last_tick==state.tick,"军务与同世界私庄各结算一个真实半日")
	return true

## 军团沿工坊配置的地形正常行军，有限推进并保留实际经过荒野的证据。
func world_arrive(kernel, settlement_id: String) -> bool:
	var saw_wilderness: bool=false
	for _half in range(40):
		var state: Dictionary=kernel.snapshot()
		var location: Dictionary=WorldLocation.resolve(state,state.player_actor_id)
		if location.get("settlement_id")==settlement_id:
			check(saw_wilderness,"第二配置行军实际经过地形荒野")
			return true
		saw_wilderness=saw_wilderness or state.actors[state.player_actor_id].location.get("kind")=="Wilderness"
		if not world_tick(kernel): return false
	check(false,"第二配置军团未在有限半日抵达："+settlement_id)
	return false

## 正式存档编解码后恢复到同一核心；人物、资产、占用和军令不能依靠测试修补。
func world_restore(kernel) -> bool:
	var before: Dictionary=kernel.snapshot()
	var decoded: Dictionary=WorldCodec.decode(WorldCodec.encode(before,0,"editor-second-military",kernel.content()),kernel.content())
	check(decoded.ok,"第二配置军事存档可正式解码："+str(decoded.get("code","")))
	if not decoded.ok: return false
	var restored: Dictionary=kernel.restore(decoded.state)
	check(restored.ok and kernel.snapshot()==before,"第二配置读档完整保持军事、人产与时间："+str(restored))
	return restored.ok

## 从保存重开的军事第二配置出发，实际受任、编组、去程交付与回营卸任，全程只用 Kernel 公共入口。
func verify_world(kernel) -> void:
	var original: Dictionary=kernel.snapshot(); var player: String=original.player_actor_id
	check(kernel.content().data.actors["actor.military.b.commander"].military_plan.unit_ids==["unit.military.b","unit.local.second"],"工坊数组下拉选择的第二真实部队进入游戏目录")
	var context: Dictionary=WorldFixture.context(kernel,"role.military.a")
	if not world_send(kernel,"office_accept",{"office_id":"office.military.a","context":context}): return
	check(not kernel.snapshot().offices["office.military.a"].enabled,"正常受任须等待实际工期")
	if not world_tick(kernel): return
	check(kernel.snapshot().offices["office.military.a"].holder_actor_id==player,"完成受任获得本人真实公职")
	if not world_send(kernel,"campaign_accept",{"template_id":"order.military.a","context":context}): return
	var order_id: String=""
	for order in kernel.snapshot().campaigns.values():
		if order.actor_id==player and order.template_id=="order.military.a": order_id=order.id
	check(not order_id.is_empty(),"正常受令生成本人军令实例")
	if order_id.is_empty(): return
	if not world_send(kernel,"army_form",{"order_id":order_id,"unit_ids":["unit.military.a"],"food_quantity":160,"money":100}): return
	if not world_restore(kernel) or not world_tick(kernel): return
	var state: Dictionary=kernel.snapshot(); var army_id: String=state.campaigns[order_id].army_id
	check(not army_id.is_empty(),"实际半日完成编组并绑定原军令")
	if army_id.is_empty(): return
	var army: Dictionary=state.armies[army_id]
	check(army.unit_ids==["unit.military.a"] and state.military_units["unit.military.a"].fit.size()==8,"编组使用原驻军八名稳定兵员")
	check(state.wallets[army.wallet_id].balance==100 and state.wallets["wallet.military.a"].balance==original.wallets["wallet.military.a"].balance-100,"编组从真实公库领取军费")
	check(state.wallets[state.actors[player].wallet_id].balance==original.wallets[original.actors[player].wallet_id].balance,"受任编组保持私人钱包独立")
	var military_owner: Array=[{"kind":"organization","id":"org.military.a"}]
	var food_before: int=WorldInventory.count(state.inventories[army.inventory_id],"item.military.a.food",military_owner)
	if not world_send(kernel,"campaign_depart",{"campaign_id":order_id}): return
	state=kernel.snapshot()
	var action: Dictionary=state.actions[state.actors[player].action_id]
	check(action.kind=="travel" and action.params.mode=="army" and action.params.payer_wallet_id==army.wallet_id,"工坊地形形成真实军团行旅并由军柜付费")
	if not world_restore(kernel) or not world_arrive(kernel,"village.east"): return
	state=kernel.snapshot()
	check(state.campaigns[order_id].status=="operating" and WorldInventory.count(state.inventories[army.inventory_id],"item.military.a.food",military_owner)<food_before,"真实行军抵达后执行军令且消耗随军粮食")
	var recipient_owner: Array=[{"kind":"organization","id":"org.military.b"}]
	var received: int=WorldInventory.count(state.inventories["inventory.military.b"],"item.military.a.food",recipient_owner)
	if not world_send(kernel,"campaign_deliver",{"campaign_id":order_id}): return
	check(WorldInventory.count(kernel.snapshot().inventories["inventory.military.b"],"item.military.a.food",recipient_owner)==received+20,"工坊选择的接收组织实际收到二十份粮食")
	check(not kernel.preview("campaign_deliver",{"campaign_id":order_id}).ok,"已交付的军令不能再次领取实物")
	if not world_send(kernel,"campaign_return",{"campaign_id":order_id}) or not world_arrive(kernel,"city.west"): return
	if not world_send(kernel,"army_return",{"army_id":army_id}) or not world_tick(kernel): return
	state=kernel.snapshot()
	check(state.armies[army_id].status=="closed" and state.campaigns[order_id].status=="awaiting_report" and state.inventories[army.inventory_id].lots.is_empty(),"归营按实际人物钱粮清算后才等待当面回报")
	if not world_restore(kernel): return
	context=WorldFixture.context(kernel,"role.military.a")
	if not world_send(kernel,"campaign_report",{"campaign_id":order_id,"context":context}): return
	if not world_send(kernel,"office_resign",{"office_id":"office.military.a","context":context}): return
	state=kernel.snapshot()
	check(state.campaigns[order_id].status=="closed" and not state.offices["office.military.a"].enabled and state.estates["estate.military.private"].owner_actor_id==player,"第二配置完整军令结束，卸任保留私人庄园")
	var second_army: Dictionary={}
	for candidate in state.armies.values():
		if candidate.commander_actor_id=="actor.military.b.commander": second_army=candidate
	check(second_army.get("unit_ids")==["unit.military.b","unit.local.second"],"NPC 通过正常受任受令实际编入工坊新选的第二部队")
	if not second_army.is_empty():
		check(state.actors["actor.local.second_commander"].location=={"kind":"InArmy","army_id":second_army.id} and state.military_units["unit.local.second"].fit.size()==12,"第二部队十二名真实兵员和军官进入同一军团，未复制人物")
		check(second_army.last_supply_tick==state.tick,"NPC 编组也只按共享半日结算随军粮食")
	world_restore(kernel)

## 原生引用选择器只按稳定 ID 选择，不向字段缓存或工作进程直接注入选中值。
func option(panel, id: String) -> int:
	for index in range(panel.reference_picker.item_count):
		if panel.reference_picker.get_item_metadata(index)==id: return index
	return -1

## 按正常字段树和应用按钮提交一次下拉选择，缺少选项时留下明确失败。
func choose(panel, pointer: String, id: String) -> void:
	select_field(panel,pointer)
	check(panel.reference_picker.visible and not panel.value_input.visible,"局部引用确实使用原生下拉："+pointer)
	var index: int=option(panel,id)
	check(index>=0 and not panel.reference_picker.is_item_disabled(index),"实际唯一目标可以选择："+id)
	if index<0: return
	panel.reference_picker.select(index); panel.reference_picker.item_selected.emit(index)
	await press(panel,"apply")

## 在已有错误列表选择精确字段问题，编辑器须跳到记录并刷新引用选择器。
func locate(panel, code: String, suffix: String) -> bool:
	for index in range(panel.diagnostics.item_count):
		var issue: Dictionary=panel.diagnostics.get_item_metadata(index)
		if issue.code==code and str(issue.path).ends_with(suffix):
			panel.diagnostics.select(index); panel.diagnostics.item_selected.emit(index)
			check(panel._field_pointer==suffix,"错误列表定位具体数组或嵌套引用："+suffix)
			return true
	check(false,"存在可定位的局部引用诊断："+code+suffix)
	return false

## 用既有 Python 军事作者夹具生成临时源，之后所有修改均由正常面板及作者进程执行。
func _run() -> void:
	root.size=Vector2i(1440,900)
	var panel=load("res://addons/hanmo_author/panel.gd").new(); root.add_child(panel)
	panel.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	await process_frame
	var folder: String=OS.get_user_data_dir().path_join("local-reference-author-"+str(Time.get_ticks_usec()))
	DirAccess.make_dir_recursive_absolute(folder)
	var source: String=folder.path_join("military-second.json")
	var output: Array=[]
	var generated: int=OS.execute(OS.get_environment("HANMO_PYTHON"),PackedStringArray(["-c","import sys,json;sys.path.insert(0,sys.argv[1]);from tests.content.test_local_references import local_pack;open(sys.argv[2],'w',encoding='utf-8').write(json.dumps(local_pack(),ensure_ascii=False))",panel.root_path,source]),output,true)
	check(generated==0,"独立军事作者源生成成功："+str(output))
	if generated!=0: panel.queue_free(); await process_frame; quit(1); return
	panel.path_input.text=source; await press(panel,"connect")
	select_record(panel,"siege_topologies","topology.military.a")
	select_field(panel,"/edges/1/blocker_id")
	check(panel.reference_picker.visible and not panel.value_input.visible,"阻挡节点使用实际下拉控件")
	check(option(panel,"gate")>=0 and option(panel,"east_supply")==-1,"当前城防不能误选另一城防独有节点")
	check(panel.reference_picker.get_item_text(option(panel,"gate")).contains("正门 · gate"),"局部节点显示作者中文名和稳定 ID")
	check(option(panel,"")>=0 and not panel.reference_picker.is_item_disabled(option(panel,"")),"可选阻挡保留明确空值选项")
	select_record(panel,"siege_topologies","topology.military.b"); select_field(panel,"/supply_node_ids/0")
	check(option(panel,"east_supply")>=0 and option(panel,"supply")==-1,"切换记录后局部选项立即切换作用域")
	select_record(panel,"siege_topologies","topology.military.a")
	select_field(panel,"/edges/0/blocker_id")
	check(panel._field_missing and panel.reference_picker.visible,"嵌套可选局部引用尚未设置时也使用下拉")
	await choose(panel,"/edges/0/blocker_id","gate")
	check(not panel._field_missing and panel._field_value=="gate","下拉选择通过原 add_field 新增可选字段")
	await press(panel,"undo")
	check(panel._field_missing,"撤销恢复可选字段缺失状态")
	await edit(panel,"/nodes/1/id","renamed_gate")
	await press(panel,"validate")
	if locate(panel,"LOCAL_REFERENCE_MISSING","/edges/1/blocker_id"):
		check(panel.reference_picker.visible and panel.reference_picker.get_item_text(panel.reference_picker.selected).contains("当前无效引用：gate"),"失效节点保留原值和明确提示")
		var revision: int=int(panel.client.document.revision)
		await press(panel,"apply")
		check(int(panel.client.document.revision)==revision,"不能把失效占位项当作真实选择提交")
	await press(panel,"undo")
	await choose(panel,"/edges/1/blocker_id","")
	check(panel._field_pointer=="/edges/1/blocker_id","局部引用写入后保留嵌套编辑位置")
	await press(panel,"undo"); await press(panel,"redo")
	check(panel._field_value=="","明确空值经过撤销与重做恢复")
	select_record(panel,"actors","actor.military.b.commander")
	select_field(panel,"/military_plan/unit_ids")
	await press(panel,"array_add")
	check(panel._field_pointer=="/military_plan/unit_ids/1" and panel._field_value=="","数组引用首建为空，不自动取第一部队")
	check(panel.reference_picker.visible and panel.reference_picker.is_item_disabled(panel.reference_picker.selected),"新数组项等待真实下拉选择")
	var index: int=option(panel,"unit.local.second")
	check(index>=0 and panel.reference_picker.get_item_text(index).contains("东营 / 后部 · unit.local.second"),"全局部队显示所属驻军、中文名称与稳定 ID")
	await choose(panel,"/military_plan/unit_ids/1","unit.local.second")
	await press(panel,"array_remove"); await press(panel,"undo")
	check(panel._nodes.has("/military_plan/unit_ids/1"),"数组删除撤销恢复实际已选引用")
	select_record(panel,"garrisons","garrison.military.a")
	await edit(panel,"/units/0/id","unit.local.second")
	select_record(panel,"actors","actor.military.b.commander"); select_field(panel,"/military_plan/unit_ids/1")
	index=option(panel,"unit.local.second")
	check(index>=0 and panel.reference_picker.is_item_disabled(index),"同名部队歧义不能任选其一")
	await press(panel,"validate")
	locate(panel,"LOCAL_REFERENCE_AMBIGUOUS","/military_plan/unit_ids/1")
	await press(panel,"undo")
	if "--verify-world" in OS.get_cmdline_user_args(): await configure_world(panel)
	await press(panel,"validate")
	check(panel.diagnostics.item_count==0,"保存前实际作者草稿通过原校验")
	await press(panel,"save"); await press(panel,"open")
	select_record(panel,"actors","actor.military.b.commander"); select_field(panel,"/military_plan/unit_ids/1")
	check(panel._field_value=="unit.local.second" and panel.reference_picker.visible,"保存重开保留数组目标并重建真实下拉")
	select_record(panel,"siege_topologies","topology.military.a"); select_field(panel,"/edges/1/blocker_id")
	check(panel._field_value=="" and option(panel,"")>=0,"保存重开保留合法空阻挡")
	if "--capture" in OS.get_cmdline_user_args():
		panel.reference_picker.show_popup()
		await RenderingServer.frame_post_draw
		var capture: String=folder.path_join("military-local-picker.png")
		check(root.get_texture().get_image().save_png(capture)==OK,"记录正常工坊下拉界面")
		panel.reference_picker.get_popup().hide()
		print("CAPTURE=",capture)
		select_record(panel,"actors","actor.military.b.commander"); select_field(panel,"/military_plan/unit_ids/1")
		await process_frame
		await process_frame
		panel.reference_picker.show_popup()
		await process_frame
		check(panel.reference_picker.get_popup().visible,"数组部队下拉已实际展开")
		await RenderingServer.frame_post_draw
		capture=folder.path_join("military-unit-picker.png")
		check(root.get_texture().get_image().save_png(capture)==OK,"记录正常工坊数组部队下拉界面")
		panel.reference_picker.get_popup().hide(); print("CAPTURE=",capture)
	if "--verify-world" in OS.get_cmdline_user_args():
		panel.prepared_path=folder.path_join("military-second.hmcatalog.json")
		await press(panel,"prepare")
		var kernel=load("res://core/world_kernel.gd").new()
		var started: Dictionary=kernel.new_game("scenario.alpha_life",panel.prepared_path)
		check(started.ok,"第二军事配置实际进入原游戏核心："+str(started))
		if started.ok: verify_world(kernel)
		var evidence=FileAccess.open(folder.path_join("world-commands.json"),FileAccess.WRITE)
		evidence.store_string(JSON.stringify(world_commands,"  ")); evidence.close()
		evidence=FileAccess.open(folder.path_join("world-last.json"),FileAccess.WRITE)
		evidence.store_string(JSON.stringify(kernel.snapshot(),"  ")); evidence.close()
		if started.ok:
			var save_path: String=folder.path_join("world-save.json")
			evidence=FileAccess.open(save_path,FileAccess.WRITE)
			evidence.store_string(WorldCodec.encode(kernel.snapshot(),0,"editor-second-military",kernel.content())); evidence.close()
			var reopened: Dictionary=WorldCodec.decode(FileAccess.get_file_as_string(save_path),kernel.content())
			check(reopened.ok and reopened.state==kernel.snapshot(),"完整军事第二配置存档落盘重开保持同一世界")
		print("WORLD_COMMANDS=",folder.path_join("world-commands.json"))
	else: print("SECOND_CONFIG_RUNTIME_PENDING=需根层军事 Catalog/Kernel 集成后使用 --verify-world 核验")
	print("AUTHOR_SOURCE=",source)
	panel.queue_free(); await process_frame
	print("EDITOR_LOCAL_REFERENCES_CHECKS=",checks," FAILURES=",failures)
	quit(0 if failures==0 else 1)
