## 营务整用途保持性门：真实Main控件/Kernel预览和四tab旧oracle；不启动GUI、存档或实际远征。
extends "res://tests/test_family_estate_views.gd"
const Camp=preload("res://ui/military_camp_screen.gd")
const CF=preload("res://tests/military_camp_view_fixture.gd")
const PAGE_PATH="res://core/views/military_camp_page.gd"
const CAMP_ORACLE_SHA="fa6943e2868a03ec7a0ca03e4e62abbddcb49a51853d0e4f1b11024bbc7aae3c"
var camp_snapshots: Dictionary={}
var old_camp: Script
const EDGE_ORACLE_SHA="f08916a21c1b622ae3724335005ada726ec2f2828ba37d7dac4839c470729d73"

## 页面草案回调只记录原重绘请求，本门主动重挂同页；真实Main路由另作集成检查。
class CampHost extends "res://ui/main.gd":
	var render_requests: int=0
	## 阻止草案勾选触发无关HUD/Native初始化，不替换预览、命名或按钮生成。
	func _render() -> void: render_requests+=1

## 记录公开preview的精确actor与顺序，仍执行原Kernel全部守卫和正式领域报价。
class CampKernel extends "res://core/world_kernel.gd":
	var calls: Array=[]
	var snapshots: int=0
	## 返回原报价并只保存观察副本。
	func preview(kind: String, params: Dictionary={}, actor_id: String="") -> Dictionary:
		var quote: Dictionary=super.preview(kind,params,actor_id)
		calls.append({"kind":kind,"params":params.duplicate(true),"actor_id":actor_id,"quote":quote.duplicate(true)}); return quote
	## 统计专用快照，实际仍深复制原世界。
	func snapshot() -> Dictionary: snapshots+=1; return super.snapshot()

## 真Main方法离树运行，原资产/课程接口均由同一个真实Kernel实例提供。
func camp_host(f: Dictionary):
	var main=CampHost.new()
	for node in [main.storage,main.journey_view,main._ui_font_scale,main._ambience]: main.add_child(node)
	main._ambience.add_child(main._ambience.music); main._ambience.add_child(main._ambience.effect)
	main.kernel=CampKernel.new(); main.kernel._state=f.state; main.kernel._catalog={"data":f.data}; main.kernel._registry=registrations; main._data=f.data
	return main

## 保原序采集实际可读控件、命令参数/报价与选择，排除对象地址及自动节点编号。
func camp_controls(node: Node) -> Array:
	var output: Array=[]
	if node is Label: output.append({"text":node.text,"tooltip":node.tooltip_text,"visible":node.visible})
	if node is SpinBox: output.append({"number":node.name,"value":node.value,"min":node.min_value,"max":node.max_value,"step":node.step})
	if node is OptionButton:
		var rows: Array=[]
		for index in range(node.item_count): rows.append({"label":node.get_item_text(index),"id":node.get_item_metadata(index),"disabled":node.is_item_disabled(index)})
		output.append({"select":node.name,"options":rows,"selected":node.selected})
	elif node is Button:
		var row: Dictionary={"button":node.text,"kind":node.get_meta("command_kind",""),"target":node.get_meta("target_id",""),"disabled":node.disabled,"pressed":node.button_pressed,"tooltip":node.tooltip_text}
		for key in ["unit_id","retainer_id","pledge_actor_id","camp_params","camp_quote","activity_params","activity_quote"]:
			if node.has_meta(key): row[key]=node.get_meta(key).duplicate(true) if node.get_meta(key) is Dictionary else node.get_meta(key)
		output.append(row)
	if node.has_meta("render_result"): output.append({"portrait_result":node.get_meta("render_result"),"tooltip":node.tooltip_text})
	for child in node.get_children(): output.append_array(camp_controls(child))
	return output

## owner生成一般成功阶段，边界case只改变明确显示输入，不冒称每个反例为可存完整世界。
func camp_fixture(kind: String) -> Dictionary:
	var f: Dictionary=CF.create()
	if not check(f.ok,"真实初始军事人产 "+kind+" "+str(f.get("code",""))): return {}
	var status: Dictionary={"ok":true}
	if kind in ["orders_accepted","orders_disabled","formation_accepted","formation_stale","formation_pledged","training_auto","training_manual","training_duplicate","training_profile_missing"]: status=CF.accept(f)
	elif kind=="formation_single": status=CF.accept(f,true)
	elif kind in ["orders_mustered","orders_gate","orders_target","orders_no_route","orders_returned","formation_progress","supply_default","supply_empty","supply_foreign","supply_reserved"]: status=CF.form(f)
	if not check(status.ok,"原owner准备 "+kind+" "+str(status)): return {}
	if kind=="orders_provider_bad": f.state.roles["role.office.z"].enabled=false
	if kind=="orders_disabled": f.state.offices["office.z"].enabled=false
	if kind in ["orders_gate","orders_no_route"]: f.state.actors.player.location={"kind":"Settlement","settlement_id":"town","place_id":"gate"}
	if kind=="orders_target": f.state.actors.player.location={"kind":"Settlement","settlement_id":"target","place_id":"target.room"}
	if kind=="orders_no_route": f.data.routes={}
	if kind=="orders_returned":
		status=CF.action(f,"army_return",{"army_id":f.army_id,"garrison_id":"garrison.z"})
		if not check(status.ok,"真实归营来源"): return {}
		f.state.campaigns[f.campaign_id].status="awaiting_report"
	if kind=="formation_pledged":
		var quote: Dictionary=CF.Assets.preview(f.state,f.data,"player","army_pledge",{"order_id":f.campaign_id,"estate_id":"estate.z","actor_ids":["guard"]})
		if not check(quote.ok and CF.Assets.apply(f.state,f.data,"player","army_pledge",quote).ok,"真实庄卫借调来源"): return {}
	if kind=="supply_reserved":
		f.state.wallets["cash.garrison.z"].reserved.other=90
		f.state.inventories["store.garrison.z"].lots[0].reserved_by="other"
		f.state.wallets[f.state.armies[f.army_id].wallet_id].reserved.other=9
	if kind=="training_manual": CF.add_drill(f)
	if kind=="training_duplicate":
		f.state.offices["office.a"]=f.state.offices["office.z"].duplicate(true); f.state.offices["office.a"].id="office.a"
	if kind=="training_profile_missing": f.state.military_units["unit.z"].erase("training")
	return f

## 页面所有选择保原合法meta键，缺值和显式空必须能区别。
func prepare_selection(main, f: Dictionary, kind: String) -> void:
	if kind in ["formation_accepted","formation_stale","formation_pledged","formation_single"]:
		main.set_meta(Camp._key("camp_units_"+f.campaign_id),["unit.z"] if kind!="formation_stale" else ["missing-unit"])
	if kind=="formation_stale": main.set_meta("camp_formation_order","missing-order")
	if kind=="formation_pledged": main.set_meta(Camp._key("camp_retainers_"+f.campaign_id),["guard"])
	if kind in ["supply_empty","supply_foreign"]: main.set_meta(Camp._key("camp_supply_garrison_"+f.army_id),"" if kind=="supply_empty" else "garrison.foreign")

## 原四参页与新三参页由运行模式选择；旧oracle在产品写入前固定一次。
func render_page(main, stage: Control, f: Dictionary, old: bool) -> void:
	if old: (old_camp if old_camp!=null else Camp).new().call("render",main,stage,f.state.duplicate(true),f.context)
	else: Camp.new().call("render",main,stage,main.kernel.military_camp_page(main.get_meta("camp_tab","orders"),Camp.new().call("selection",main),f.context))

## 完整页面和数量编辑只读，按钮提交仍共享原params，不把一次初始预览当永久授权。
func camp_case(kind: String, old: bool) -> Dictionary:
	var f: Dictionary=camp_fixture(kind)
	if f.is_empty(): return {}
	var main=camp_host(f); var stage=Control.new(); root.add_child(stage)
	var tab: String=kind.get_slice("_",0)
	main.set_meta("camp_tab",tab); prepare_selection(main,f,kind)
	Character.clear_cache(); registry.reads=[]
	var before: Dictionary=f.duplicate(true); seed(971); var expected: int=randi(); seed(971)
	render_page(main,stage,f,old)
	var output: Dictionary={"controls":camp_controls(stage),"calls":main.kernel.calls.duplicate(true),"descriptors":descriptors(),"reads":registry.reads.duplicate(true)}
	if kind in ["formation_accepted","supply_default"]:
		main.kernel.calls=[]
		var food: SpinBox=stage.find_child("CampFoodQuantity",true,false); var money: SpinBox=stage.find_child("CampMoneyQuantity",true,false)
		food.value=25; money.value=7
		output.edited={"controls":camp_controls(stage),"calls":main.kernel.calls.duplicate(true)}
	check(f==before and randi()==expected,kind+" world/data/global RNG unchanged")
	camp_snapshots[kind]=main.kernel.snapshots
	stage.free(); main.free()
	return output

## 两个补充旧源反例独立冻结，不更新原24case摘要；回执与他人友军都走实际显示/预览。
func edge_case(kind: String, old: bool) -> Dictionary:
	var f: Dictionary=CF.create(); check(f.ok,"边界初始人产 "+kind)
	if kind=="return_receipts": check(CF.returned_retainer(f).ok,"真实借调编组归营供给同人后续回执")
	else: f.state.armies.allied={"id":"allied","status":"active","organization_id":"org","commander_actor_id":"other","unit_ids":[]}
	var main=camp_host(f); var stage=Control.new(); root.add_child(stage)
	main.set_meta("camp_tab","supply" if kind=="return_receipts" else "orders")
	Character.clear_cache(); registry.reads=[]; var before: Dictionary=f.duplicate(true)
	render_page(main,stage,f,old)
	var output: Dictionary={"controls":camp_controls(stage),"calls":main.kernel.calls.duplicate(true),"descriptors":descriptors(),"reads":registry.reads.duplicate(true)}
	check(f==before,"补充反例显示仍只读 "+kind)
	if kind=="return_receipts": check(main.kernel.calls.size()==3 and not main.kernel.calls[0].quote.ok and main.kernel.calls[1].quote.ok and main.kernel.calls[2].quote.ok,"失败旧回执不吞掉同人后续成功行，原按钮再次报价")
	else: check(button(stage,"campaign_accept","order.supply").get_meta("camp_params").recipient_army_id=="allied","非本人友军仍是原受援候选")
	stage.free(); main.free(); return output

## 递归有限载荷不包含任何完整世界领域根表；允许既有画像公开输入和正式供给标量。
func bounded_page(value: Variant) -> bool:
	if value is Dictionary:
		for key in ["actors","facts","wallets","inventories","military_units","armies","state","data"]:
			if value.has(key): return false
		for item in value.values():
			if not bounded_page(item): return false
	elif value is Array:
		for item in value:
			if not bounded_page(item): return false
	return true

## 有限返回不携带领域根表或可写引用；值查询不代替资格，错误仍来自真实public preview。
func finite_checks() -> void:
	var leaf=load(PAGE_PATH); var f: Dictionary=CF.create(); var before: Dictionary=f.duplicate(true)
	var context: Dictionary=leaf.provider_context(f.state,f.data)
	check(context==f.context,"开窗取原首作者公职岗位")
	f.state.roles["role.office.z"].enabled=false
	check(leaf.provider_context(f.state,f.data)==context,"首失效岗位不改选第二可办人")
	f.state.roles["role.office.z"].enabled=true
	check(CF.accept(f).ok,"有限页实际受任受令")
	before=f.duplicate(true)
	var view: Dictionary=leaf.query(f.state,f.data,"formation",{},context)
	check(bounded_page(view),"编组页不携带世界或领域根容器")
	check(view.formation.units.map(func(row): return row.unit_id)==["unit.a","unit.z"],"驻军沿原Assets排序")
	check(view.formation.pledge_estate_choices.map(func(row): return row.id)==["estate.z","estate.a"],"本人远地庄园仍保插入序")
	view.entry_provider.context.role_id="mutated"; view.formation.units.clear()
	check(f==before,"页面嵌套返回与世界深隔离")
	f.state.military_units["unit.z"].training_action_id="busy-training"
	view=leaf.query(f.state,f.data,"formation",{},context)
	check(not view.formation.units[1].blocked,"编组显示不提前增加训练占用过滤")
	f.state.military_units["unit.z"].army_id="other-army"
	var rows: Array=leaf.training_rows(f.state,f.data)
	check(rows.size()==2,"训练原候选不提前排除army与training占用")
	check(not rows[0].has("member_ids") and rows[0].keys().size()==6,"训练仅六项候选标量及profile")
	f=CF.create(); check(CF.form(f).ok,"有限军粮实际编组")
	view=leaf.query(f.state,f.data,"supply",{},f.context)
	check(view.selected_garrison_id=="garrison.z","营地metadata缺失保军团原驻营")
	view=leaf.query(f.state,f.data,"supply",{"supply_garrison_army_id":f.army_id,"supply_garrison_id":""},f.context)
	check(view.selected_garrison_id=="garrison.a","显式空营地选择回原首项")
	view=leaf.query(f.state,f.data,"supply",{"supply_army_id":"stale","supply_garrison_army_id":"stale","supply_garrison_id":"garrison.foreign"},f.context)
	check(view.selected_army_id==f.army_id and view.selected_garrison_id=="garrison.z","失效军团键不能污染回落军团营地")
	var main=camp_host(f); before=f.duplicate(true); main.kernel.calls=[]
	var offer: Dictionary=main.kernel.military_camp_offer("campaign_depart",{"campaign_id":f.campaign_id})
	check(offer.quote.code=="NotAtCityExit" and offer.departure_guide_place_id=="gate" and main.kernel.calls.map(func(row): return row.kind)==["campaign_depart","travel"],"真实第二travel失败与城口引导保原序")
	check(f==before,"实时组合报价不改变世界/钱货/RNG")
	f.state.offices["office.z"].enabled=false; main.kernel.calls=[]
	offer=main.kernel.military_camp_offer("campaign_depart",{"campaign_id":f.campaign_id})
	check(offer.quote.code=="CampaignAuthorityLost" and main.kernel.calls.size()==1 and offer.departure_guide_place_id.is_empty(),"首报价拒绝不读下一出征或guide分支")
	f.state.offices["office.z"].enabled=true
	f.state.actors.player.location={"kind":"Settlement","settlement_id":"target","place_id":"target.room"}; main.kernel.calls=[]
	offer=main.kernel.military_camp_offer("campaign_depart",{"campaign_id":f.campaign_id})
	check(offer.quote.ok and main.kernel.calls.size()==1,"已到目标保持首报价早退")
	f.data.settlements.target.map_xy=[3,4]; f.data.settlements.target.map_id="map"; f.state.actors.player.location={"kind":"Wilderness","point":[3,4],"map_id":"map"}
	check(leaf.departure_source(f.state,f.data,f.campaign_id).at_target,"到目标仍复用原荒野地图点规则")
	f.state.actors.manager.appearance=false
	check(leaf.participant_names(f.state,f.data,["manager","missing","manager"])==["实际管事","无人","实际管事"],"参与者姓名保重复/缺人回退，坏外观不增姓名验证")
	main.free()

## 旧训练二参入口继续原Session身份和候选快照顺序，原其它Study主体不在本门改动。
func training_compatibility(expected: Dictionary) -> void:
	var f: Dictionary=camp_fixture("training_profile_missing"); var main=camp_host(f); var stage=Control.new(); root.add_child(stage)
	var training=load("res://ui/study_screen.gd").render_training(main,stage)
	var original: Array=expected.training_profile_missing.controls; var start: int=0
	for index in range(original.size()):
		if original[index].get("text")=="驻军训练": start=index; break
	check(camp_controls(training)==original.slice(start) and main.kernel.calls==expected.training_profile_missing.calls,"旧训练二参入口保完整失败按钮/文字与报价序")
	check(main.kernel.snapshots==2,"兼容训练仍Session身份后候选snapshot两次")
	stage.free(); main.free()

## 实际Main开窗与Composer四页口验证；只挂真实Control，不启动Native、GUI或代提交流程。
func main_camp_routes(expected: Dictionary) -> void:
	var f: Dictionary=camp_fixture("orders_mustered"); var main=host_for(f); main.kernel=CampKernel.new(); main.kernel._state=f.state; main.kernel._catalog={"data":f.data}; main.kernel._registry=registrations
	main._started=true; main.page=VBoxContainer.new(); root.add_child(main.page)
	main._open_military_camp()
	var frame: Control=main.page.find_child("MilitaryCampScreen",true,false)
	if check(frame!=null,"真实Main打开Camp经Composer有限页"):
		check(camp_controls(frame)==expected.orders_mustered.controls and main.kernel.calls==expected.orders_mustered.calls and main.kernel.snapshots==2,"真实orders同旧oracle，开窗专用snapshot消除")
		for pair in [["formation","formation_progress"],["supply","supply_default"]]:
			main.kernel.calls=[]; main.kernel.snapshots=0
			button(main.page,"camp_tab",pair[0]).pressed.emit()
			check(camp_controls(main.page.find_child("MilitaryCampScreen",true,false))==expected[pair[1]].controls and main.kernel.calls==expected[pair[1]].calls and main.kernel.snapshots==2,"真实页签完整原控件/报价 "+pair[0])
		var context: Dictionary=main._military_context.duplicate(true)
		f.state.roles["role.office.z"].actor_id="manager"; button(main.page,"camp_tab","formation").pressed.emit()
		check(main._military_context==context,"页签不重选开窗provider")
	main.page.free(); main.free()
	f=camp_fixture("training_auto"); main=host_for(f); main.kernel=CampKernel.new(); main.kernel._state=f.state; main.kernel._catalog={"data":f.data}; main.kernel._registry=registrations
	main._started=true; main.page=VBoxContainer.new(); root.add_child(main.page); main.set_meta("camp_tab","training"); main._open_military_camp()
	frame=main.page.find_child("MilitaryCampScreen",true,false)
	if check(frame!=null,"真实Main训练页存在"):
		check(camp_controls(frame)==expected.training_auto.controls and main.kernel.calls==expected.training_auto.calls and main.kernel.snapshots==3,"真实训练页只留Main/Composer两次与Session身份一次")
	main.page.free(); main.free()

## 固定旧页完整摘要，再对有限输入与同一公开预览链判断保持性。
func _run() -> void:
	registrations=Registry.build(Builtin.descriptors())
	if not check(registrations.get("ok",false),"完整受信registry"): quit(1); return
	registry=DescriptorRegistry.new(Strict.parse_text(FileAccess.get_file_as_string("res://../tests/fixtures/character_presentation/manifest.json")).value)
	var previous: RefCounted=Art._registry; Art._registry=registry
	var capture: String=""; var edge_capture: String=""
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--capture-oracle="): capture=arg.trim_prefix("--capture-oracle=")
		if arg.begins_with("--capture-edge="): edge_capture=arg.trim_prefix("--capture-edge=")
		if arg.begins_with("--old-camp="): old_camp=load(arg.trim_prefix("--old-camp="))
	if not edge_capture.is_empty():
		var edges: Dictionary={}
		for kind in ["return_receipts","allied_recipient"]: edges[kind]=edge_case(kind,true)
		var out=FileAccess.open(edge_capture,FileAccess.WRITE); out.store_string(JSON.stringify({"output":edges,"sha256":digest(edges)},"\t",true,true)); out.close()
		Art._registry=previous; Character.clear_cache(); await process_frame
		print("CAMP_EDGE_ORACLE=",digest(edges)," CHECKS=",checks," FAILURES=",failures); quit(0 if failures==0 else 1); return
	var old: bool=not capture.is_empty() or not FileAccess.file_exists(PAGE_PATH)
	var output: Dictionary={}
	for kind in ["orders_initial","orders_provider_bad","orders_accepted","orders_disabled","orders_mustered","orders_gate","orders_target","orders_no_route","orders_returned","formation_empty","formation_accepted","formation_stale","formation_single","formation_pledged","formation_progress","supply_default","supply_empty","supply_foreign","supply_reserved","training_none","training_auto","training_manual","training_duplicate","training_profile_missing"]:
		output[kind]=camp_case(kind,old)
	if not capture.is_empty():
		var file=FileAccess.open(capture,FileAccess.WRITE); file.store_string(JSON.stringify({"output":output,"sha256":digest(output),"snapshots":camp_snapshots},"\t",true,true)+"\n"); file.close(); print("CAMP_ORACLE_SHA=",digest(output))
	else:
		if digest(output)!=CAMP_ORACLE_SHA:
			var mismatch=FileAccess.open("user://camp-mismatch.json",FileAccess.WRITE); mismatch.store_string(JSON.stringify(output,"\t",true,true)); mismatch.close()
		check(digest(output)==CAMP_ORACLE_SHA,"原四页控件/画像/报价/数量编辑oracle相同")
		check(not old,"有限Camp页存在")
		if not old:
			var edges: Dictionary={}
			for kind in ["return_receipts","allied_recipient"]: edges[kind]=edge_case(kind,false)
			check(digest(edges)==EDGE_ORACLE_SHA,"旧同人回执/他人友军完整控件与真实预览oracle")
			finite_checks(); training_compatibility(output); main_camp_routes(output)
	Art._registry=previous; Character.clear_cache()
	await process_frame
	print("CAMP_SNAPSHOTS=",JSON.stringify(camp_snapshots,"",true)); print("CAMP_VIEW_CHECKS=",checks," FAILURES=",failures)
	quit(0 if failures==0 else 1)
