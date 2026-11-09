## 围城有限页面的真实来源与存活页时序；先证Main收尾前提，不运行Native存读或GUI输入。
extends SceneTree
const Kernel=preload("res://core/world_kernel.gd")
const F=preload("res://tests/siege_page_fixture.gd")
const Schema=preload("res://core/state_schema.gd")
const World=preload("res://core/siege_world.gd")
const Rest=preload("res://core/long_rest.gd")
const Leaf=preload("res://core/views/siege_page.gd")
const Rules=preload("res://core/siege_rules.gd")
const Screen=preload("res://ui/siege_screen.gd")
const Art=preload("res://ui/art_catalog.gd")
var checks: int=0
var failures: int=0
var allocated: Array=[]

## 只保configure回调形状，不启动文件/槽服务；世界恢复仍走真实Kernel。
class StorageBoundary extends Node:
	var configurations: int=0
	## 收录Main的已验证上下文配置，不执行任何物理存储。
	func configure(_catalog: Dictionary,_registry: Dictionary) -> void: configurations+=1

## 保真实Main命令/advance/render；仅隔离启动资源服务与文本解码入口。
class LifeHost extends "res://ui/main.gd":
	var restore_state: Dictionary={}
	## 测试显式准备page和表现，不连接Native或启动音频。
	func _ready() -> void: pass
	## 已记录的真实合法状态交正式restore；不伪造成功，不把此桥标作Codec/存读验收。
	func _restore_text(_text: String) -> Dictionary:
		var result: Dictionary=kernel.restore_with_catalog(restore_state,kernel.content(),kernel._registry)
		if result.ok:
			set_meta("personal_battle_generation",int(get_meta("personal_battle_generation",0))+1)
			_data=kernel.content().data
		return result

## 节点oracle只替Kernel外壳，不改原围城/战略预览；边界变体不会写回正式世界。
class RuleKernel extends RefCounted:
	var source: Dictionary
	var data: Dictionary
	var draft_calls: Array=[]
	## 原单部之外的整套军议与交涉继续走真实World.preview。
	func preview(kind: String,params: Dictionary={}) -> Dictionary: return World.preview(source,data,source.player_actor_id,kind,params)
	## 新页的按需读调用真实候选叶；旧oracle不会触发此方法。
	func siege_draft(identity: Dictionary,requests: Array,first_ok: bool=false) -> Dictionary:
		draft_calls.append({"identity":identity.duplicate(true),"requests":requests.duplicate(true),"first_ok":first_ok})
		return load("res://core/views/siege_page.gd").draft(source,data,identity,requests,first_ok)

## 历史投影仅排除本轮新增的攻具选择、其紧邻提示和轮后反馈；其余旧控件仍逐字段精确比较。
func node_rows(node: Node,rows: Array,historical: bool=false) -> void:
	if historical and node.name in ["SiegeEngine","SiegeRoundFeedback"]: return
	if historical and node is Label and node.get_parent()!=null and node.get_index()>0:
		var previous: Node=node.get_parent().get_child(node.get_index()-1)
		if previous is OptionButton and previous.name=="SiegeEngine": return
	if node is Label: rows.append(["text",node.text])
	if node is Button and not node is OptionButton: rows.append(["button",node.text,node.disabled,node.tooltip_text,node.get_meta("command_kind",""),node.get_meta("target_id","")])
	if node is OptionButton:
		var options: Array=[]
		for i in range(node.item_count): options.append([node.get_item_text(i),node.get_item_metadata(i),node.is_item_disabled(i),node.get_popup().get_item_tooltip(i)])
		rows.append(["options",options,node.selected])
	for child in node.get_children(): node_rows(child,rows,historical)

## 默认仍记录全部新旧控件用于重绘一致性；只有固定旧oracle显式投影历史行，草案与几何不裁剪。
func view_record(view,historical: bool=false) -> Dictionary:
	var rows: Array=[]; node_rows(view,rows,historical)
	view.canvas.size=Vector2(900,700); view.canvas.layout()
	var points: Dictionary={}
	for id in view.canvas.positions: points[id]=[view.canvas.positions[id].x,view.canvas.positions[id].y]
	var extent: Vector2=view.canvas.minimum_extent()
	return {"rows":rows,"orders":view.orders.duplicate(true),"selected":view.selected_id,"inspected":view.inspected_id,"node":view.node_id,"points":points,"extent":[extent.x,extent.y]}

## 原真实Screen的黄金JSON不重录；只将候选新增行留给真实攻具流程检查，旧控件/草案/地图仍保持精确比较。
func oracle(state: Dictionary,data: Dictionary,siege_id: String) -> void:
	var records: Dictionary={}
	var screen_script=load("res://tests/oracle_siege_screen.gd") if "--record-oracle" in OS.get_cmdline_user_args() else Screen
	for variant in ["active","no_food","work_pending","pending_node","damaged_gate","result"]:
		var source: Dictionary=state.duplicate(true); var siege: Dictionary=source.sieges[siege_id]
		var own: String=siege.unit_nodes.keys()[0]
		if variant=="no_food":
			for inventory in source.inventories.values():
				for lot in inventory.lots: lot.quantity=0
		if variant=="work_pending": siege.engine_work[own]={"node_id":siege.unit_nodes[own],"remaining":1}
		if variant=="pending_node":
			var id: String=siege.unit_nodes[own]
			siege.pending_nodes[id]={"source_id":"probe","node_id":id,"unit_ids":[own],"origin_nodes":{own:id}}
		if variant=="damaged_gate":
			for node in data.siege_topologies[siege.topology_id].nodes:
				if node.kind=="gate": siege.node_hp[node.id]=1
		if variant=="result": siege.phase="result"; siege.result={"kind":"truce","tick":source.tick,"winner_side":-1}
		var host=LifeHost.new(); allocated.append(host); host._data=data
		var api=RuleKernel.new(); api.source=source; api.data=data; host.kernel=api
		var stage=Control.new(); host.add_child(stage)
		var view=screen_script.render(host,stage,source,siege_id)
		records[variant+":initial"]=view_record(view,true)
		for id in siege.unit_nodes:
			view._choose_unit(id); records[variant+":unit:"+id]=view_record(view,true)
		for node in data.siege_topologies[siege.topology_id].nodes:
			view._choose_node(node.id); records[variant+":node:"+node.id]=view_record(view,true)
		var cached: Dictionary=view_record(view)
		stage.remove_child(view); view.free(); view=screen_script.render(host,stage,source,siege_id)
		check(view_record(view)==cached,"同修订草案与节点重建相同 "+variant)
	var encoded: String=JSON.stringify(records,"",true,true)
	var path: String=ProjectSettings.globalize_path("res://").get_base_dir().path_join("siege-old-oracle.json")
	if "--record-oracle" in OS.get_cmdline_user_args():
		var file=FileAccess.open(path,FileAccess.WRITE); file.store_string(encoded+"\n"); file.close(); print("OLD_SIEGE_ORACLE_CASES=",records.size()," SHA256=",encoded.sha256_text())
	else:
		check(FileAccess.file_exists(path) and FileAccess.get_file_as_string(path).strip_edges()==encoded,"固定旧Screen实际节点/候选/选择/布局oracle "+str(records.size())+"组")

## 等真实deferred队列就绪，旧页销毁与短行动收尾均由原逻辑处理。
func _init() -> void: call_deferred("run")

## 记录失败并影响退出码；引擎诊断另由原日志核查。
func check(value: bool,label: String) -> bool:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)
	return value

## 正式tiny内容先经真实表现校验，再把原Main装到树中；不替换_render。
func host_for(kernel):
	var prepared: Dictionary=Art.prepare_content(kernel.content())
	if not check(prepared.ok,"真实围城作者的tiny表现可装配 "+str(prepared.get("code",""))): return null
	Art.commit_content(prepared)
	var host=LifeHost.new(); allocated.append(host)
	host.storage.free(); host.storage=StorageBoundary.new()
	host.kernel=kernel; host._data=kernel.content().data; host._started=true
	host.page=VBoxContainer.new(); host.add_child(host.page); host.size=Vector2(1280,800)
	root.add_child(host); host._render(); return host

## 实际提交/半日/新局/恢复均交原Main收尾，旧节点在输入树中必须已经移除。
func lifecycle(kernel,entered: Dictionary) -> void:
	var host=host_for(kernel)
	if host==null: return
	var old=host.find_child("SiegeScreen",true,false)
	if not check(old!=null and old.is_inside_tree(),"真实Main已创建正常存活围城页"): return
	var before: Dictionary=kernel.snapshot(); var restore_state: Dictionary=before.duplicate(true)
	old._submit_orders()
	var committed: Dictionary=kernel.snapshot(); old._submit_orders()
	check(kernel.snapshot()==committed,"原一次提交锁阻止迟来重复确认，不新增身份授权")
	check(kernel.snapshot().tick==before.tick+1 and kernel.snapshot().revision>before.revision,"实际军议提交及唯一半日均发生")
	check(not old.is_inside_tree() and host._scene_counter>=2,"真实_perform收尾同步_render移除旧围城页")
	old=host.page.get_child(0); before=kernel.snapshot(); host._finish_short()
	check(not old.is_inside_tree() and kernel.snapshot()==before,"无当前短行动仍按原_finish_short重绘，不擅自推进")
	old=host.page.get_child(0); host.restore_state=restore_state; host._storage_intent="load"
	host._stored("get" if OS.has_feature("web") else "inspect",{"ok":true,"text":"recorded legal candidate","generation":1})
	check(kernel.snapshot()==restore_state and not old.is_inside_tree(),"真实Kernel恢复经原_stored收尾移除旧页；解码/存储出口隔离")
	old=host.page.get_child(0); host.restore_state=before
	var restored: Dictionary=host._restore_text("recorded legal candidate")
	host._apply_import(restored,true)
	check(restored.ok and kernel.snapshot()==before and not old.is_inside_tree(),"真实Kernel恢复经原_apply_import同步重绘")
	old=host.page.get_child(0)
	var started: Dictionary=host.start_content("scenario.alpha_life",F.content_path())
	check(started.ok and not old.is_inside_tree(),"真实new_game在start_content成功返回前重绘旧页")
	var actor: String=host.kernel.snapshot().player_actor_id
	var context: Dictionary=preload("res://core/interaction_rules.gd").for_role(host.kernel.snapshot(),host._data,"role.military.a")
	var tick: int=host.kernel.snapshot().tick
	host._perform("office_accept",{"office_id":"office.military.a","context":context})
	var short_page=host.page.get_child(0)
	await process_frame
	check(host.kernel.snapshot().tick==tick+1 and host.kernel.snapshot().actors[actor].action_id.is_empty() and not is_instance_valid(short_page),"真实短行动deferred推进后原Main重绘并销毁前页")

## 隔离领域前置条件以直达原army互斥分支；边界副本不提交/恢复，不冒称完整WorldSchema。
func rest_exclusion(state: Dictionary,data: Dictionary,siege_id: String) -> void:
	var boundary: Dictionary=state.duplicate(true); var actor: String=state.player_actor_id
	var id: String=data.estates.keys()[0]; var definition: Dictionary=data.estates[id]
	boundary.estates[id].owner_actor_id=actor
	boundary.actors[actor].location={"kind":"Settlement","settlement_id":data.places[definition.place_id].settlement_id,"place_id":definition.place_id}
	boundary.actors[actor].action_id=""; boundary.interactions.erase(actor); boundary.parties={}
	check(World.for_actor(boundary,actor)==siege_id,"同一活动army谓词仍能定位本人的围城")
	check(Rest.preview(boundary,data,actor,"long_rest",{"estate_id":id,"duration":1}).get("code")=="RestNeedsQuiet","实际LongRest资格在同一army成员条件下拒绝，不能进入节流休整推进")

## 领域输入由实际开局链给出；新增输出只核本用途，不以人工边界变体冒充正式存档。
func leaf_checks(state: Dictionary,data: Dictionary,siege_id: String) -> void:
	var before: Dictionary=state.duplicate(true); var original_data: Dictionary=data.duplicate(true)
	var page: Dictionary=Leaf.query(state,data)
	if not check(page.ok and page.siege_id==siege_id,"当前有限页选中原真实围城"): return
	check(page.keys().size()==15 and page.identity.size()==5 and page.siege.size()==12,"有限页精确外层/身份/城防字段")
	check(not page.has("state") and not page.has("observation") and not page.siege.has("ai_plans") and not page.has("facts"),"不输出全态、全军观察或AI")
	var observation: Dictionary=World.observe(state,data,state.sieges[siege_id]); var requests: Array=[]; var expected: Array=[]
	for id in state.sieges[siege_id].unit_nodes:
		for node in data.siege_topologies[state.sieges[siege_id].topology_id].nodes:
			for action in Screen.ACTION_NAMES:
				var order: Dictionary={"action":action,"node_id":node.id}
				requests.append({"unit_id":id,"order":order})
				expected.append({"index":expected.size(),"unit_id":id,"quote":Rules.preview(state.sieges[siege_id],data,observation,state.player_actor_id,id,order)})
	var all_quotes: Dictionary=Leaf.draft(state,data,page.identity,requests)
	check(all_quotes.ok and all_quotes.quotes==expected,"原单部领域报价按原序逐项相同 "+str(expected.size()))
	var first: Dictionary=Leaf.draft(state,data,page.identity,requests,true); var expected_first: Array=[]
	for row in expected:
		expected_first.append(row)
		if row.quote.ok: break
	check(first.ok and first.quotes==expected_first,"首成功短路保原候选顺序")
	var own: String=page.units.keys()[0]; var wait_request: Dictionary={"unit_id":own,"order":{"action":"wait","node_id":page.siege.unit_nodes[own]}}
	var short: Dictionary=Leaf.draft(state,data,page.identity,[wait_request,false],true)
	check(short.ok and short.quotes.size()==1,"首成功后不读后续坏请求")
	check(Leaf.draft(state,data,page.identity,[false]).get("code")=="InvalidSiegeViewSource","遇到的坏请求显式拒绝")
	var repeated: Array=[{"unit_id":own,"order":{"action":"move","node_id":"missing"}},{"unit_id":own,"order":{"action":"move","node_id":"missing"}}]
	check(Leaf.draft(state,data,page.identity,repeated,true).quotes.size()==2,"全部失败保重复候选，不排序或去重")
	var changed: Dictionary=page.identity.duplicate(); changed.revision+=1
	var poison: Dictionary=state.duplicate(true); poison.facts=false
	check(Leaf.draft(poison,data,changed,[wait_request]).get("code")=="SiegeViewChanged","身份改变先拒绝，不提前observe无关坏源")
	for key in ["actor_id","revision","siege_id","round","phase"]:
		changed=page.identity.duplicate(); changed[key]=[]
		check(Leaf.draft(state,data,changed,[]).get("code")=="InvalidSiegeViewSource","身份明确类型 "+key)
	var invalid: Dictionary=state.duplicate(true); invalid.sieges[siege_id].round=false
	check(Leaf.query(invalid,data).get("code")=="InvalidSiegeViewSource","命中页的必要round类型明确拒绝")
	var selected: Dictionary=state.duplicate(true); var army: String=preload("res://core/world_location.gd").army_id(state,state.player_actor_id)
	selected.sieges[siege_id].withdrawn_army_ids.append(army)
	check(Leaf.query(selected,{}).siege_id==World.for_actor(selected,state.player_actor_id),"原撤军但非本人withdrawn终局不重开页面")
	selected.sieges[siege_id].phase="result"; selected.sieges[siege_id].result={"kind":"withdrawn","tick":state.tick,"winner_side":-1}; selected.sieges[siege_id].last_events=[{"kind":"withdrawal_request","army_ids":[army]}]
	check(Leaf.query(selected,data).siege_id==World.for_actor(selected,state.player_actor_id),"本人此次withdrawal_request终局保原确认例外")
	check(Leaf.query({},data).get("code")=="NoWorld","空世界新只读失败码")
	var idle: Dictionary=state.duplicate(true); idle.armies={}; idle.facts=false
	check(Leaf.query(idle,{} )=={"ok":true,"actor_id":state.player_actor_id,"revision":state.revision,"siege_id":""},"无活动围城只读原选择，未读观察/目录")
	page.siege.node_hp[page.siege.node_hp.keys()[0]]=999; page.siege.pending_node_ids.append("private"); page.siege.unit_nodes.clear(); page.units.clear(); page.other_supply_rows.clear(); page.own_supply_quote.clear(); all_quotes.quotes[0].quote.clear()
	check(state==before and data==original_data,"页面与报价深复制且不推进RNG/资产/世界")

## 只测原选择/observe实际入口消费的坏形状，不把本薄口提升为全世界验收。
func source_guards(state: Dictionary,data: Dictionary,siege_id: String) -> void:
	var identity: Dictionary=Leaf.query(state,data).identity
	for bad in [false,{}, {"phase":false}]:
		var source: Dictionary=state.duplicate(true); source.sieges[siege_id]=bad
		check(Leaf.query(source,data).get("code")=="InvalidSiegeViewSource","选择器必要封套先拒绝 "+str(bad))
	var source: Dictionary=state.duplicate(true); source.facts=false
	check(Leaf.draft(source,data,identity,[{"unit_id":"unit.military.a","order":{"action":"wait","node_id":"missing"}}]).get("code")=="InvalidSiegeViewSource","draft与页面共用observe必要集合守卫")
	var tail: Dictionary=state.duplicate(true); tail.sieges["unread"]=false
	check(Leaf.query(tail,data).ok,"原首个命中之后无关siege尾部不读")

## 新按需调用只打包旧调用点的请求，不把所有单位/节点/动作矩阵预计算成资格。
func batch_checks(state: Dictionary,data: Dictionary,siege_id: String) -> void:
	var host=LifeHost.new(); allocated.append(host); host._data=data
	var api=RuleKernel.new(); api.source=state; api.data=data; host.kernel=api
	var stage=Control.new(); host.add_child(stage)
	var view=Screen.render_view(host,stage,Leaf.query(state,data))
	check(api.draft_calls.size()==3 and api.draft_calls[0].requests.size()==state.sieges[siege_id].unit_nodes.size() and api.draft_calls[1].requests.size()==1 and api.draft_calls[2].requests.size()==8,"新草案依序批量待命/观察可控/七动作加当前令")
	var expected: Array=Screen.ACTION_NAMES.keys(); var observed: Array=[]
	for request in api.draft_calls[2].requests: observed.append(request.order.action)
	expected.append(view.orders[view.selected_id].action)
	check(observed==expected,"动作菜单原插入序及末尾当前令不去重")
	api.draft_calls.clear(); stage.remove_child(view); view.free()
	view=Screen.render_view(host,stage,Leaf.query(state,data))
	check(api.draft_calls.size()==2 and api.draft_calls[0].requests.size()==1,"同revision草案复原不重求全单位待命")
	api.draft_calls.clear(); view._choose_node(view.topology.inner_id)
	check(api.draft_calls[0].first_ok and api.draft_calls[0].requests.size()==4 and api.draft_calls[0].requests[0].order.action=="wait" and api.draft_calls[0].requests[3].order.action=="wait","选点保持重复wait及原四候选first_ok")
	var result: Dictionary=state.duplicate(true); result.sieges[siege_id].phase="result"; result.sieges[siege_id].result={"kind":"truce","tick":state.tick,"winner_side":-1}
	api.source=result; api.draft_calls.clear(); stage.remove_child(view); view.free()
	view=Screen.render_view(host,stage,Leaf.query(result,data))
	check(api.draft_calls.is_empty(),"终局原早返不请求单部资格或菜单")

## 可选导出仅保存真实短链结果，供外壳路由窄门复用；不写玩家档案或运行完整围城流程。
func run() -> void:
	var kernel=Kernel.new(); var entered: Dictionary=F.enter(kernel)
	if check(entered.ok,"正式短链实际建立活动围城 "+str(entered.get("code",""))):
		var state: Dictionary=kernel.snapshot(); var data: Dictionary=kernel.content().data
		check(Schema.validate(state,kernel.content(),kernel._registry).ok,"导出来源通过完整已注册WorldSchema")
		for arg in OS.get_cmdline_user_args():
			if arg.begins_with("--export-source="):
				var file=FileAccess.open(arg.trim_prefix("--export-source="),FileAccess.WRITE)
				file.store_string(JSON.stringify({"format":"HM-TEST-SIEGE-SOURCE/1","state":state,"catalog":kernel.content(),"siege_id":entered.siege_id},"",true,true)+"\n"); file.close()
		var art: Dictionary=Art.prepare_content(kernel.content()); if art.ok: Art.commit_content(art)
		oracle(state,data,entered.siege_id)
		leaf_checks(state,data,entered.siege_id)
		if not "--record-oracle" in OS.get_cmdline_user_args():
			batch_checks(state,data,entered.siege_id)
			source_guards(state,data,entered.siege_id)
		if not "--oracle-only" in OS.get_cmdline_user_args():
			rest_exclusion(state,data,entered.siege_id)
			if check(kernel.has_method("siege_page") and kernel.has_method("siege_draft"),"真实Kernel已接两个有限围城入口"):
				check(kernel.siege_page()==Leaf.query(state,data),"真实Kernel/Backend页与唯一叶相同")
				var input: Dictionary=kernel.siege_page()
				var id: String=input.siege.unit_nodes.keys()[0]
				var request: Array=[{"unit_id":id,"order":{"action":"wait","node_id":input.siege.unit_nodes[id]}}]
				check(kernel.siege_draft(input.identity,request)==Leaf.draft(state,data,input.identity,request),"真实Kernel/Backend单部报价与唯一叶相同")
				await lifecycle(kernel,entered)
	await process_frame
	for host in allocated:
		if not is_instance_valid(host): continue
		if host.get_parent()!=null: host.get_parent().remove_child(host)
		for node in [host.storage,host.journey_view,host._ui_font_scale,host._ambience]:
			if node.get_parent()==null: host.add_child(node)
		for node in [host._ambience.music,host._ambience.effect]:
			if node.get_parent()==null: host._ambience.add_child(node)
		host.free()
	print("KERNEL_SIEGE_PAGE_CHECKS=",checks," FAILURES=",failures)
	quit(0 if failures==0 else 1)
