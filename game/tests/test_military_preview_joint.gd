## 同一合法军旅来源同时交给正式GD页、C++查询及真实Main有限队列，不替换规则或Bridge。
extends SceneTree
const Page=preload("res://core/views/military_battle_page.gd")
const Round=preload("res://core/military_round.gd")
const Combat=preload("res://core/military_combat.gd")
const Kernel=preload("res://core/world_kernel.gd")
const Main=preload("res://ui/main.gd")
const Art=preload("res://ui/art_catalog.gd")
const Strict=preload("res://core/strict_json.gd")
const Fixture=preload("res://tests/military_presentation_fixture.gd")
var checks: int=0
var failures: Array=[]
var output: String=""
var read_events: Array=[]
var command_events: Array=[]
var job_kinds: Array=[]

## 延后初始化，真正读完或超时才退出，不将待运行门记为通过。
func _initialize() -> void: call_deferred("run")
## 断言保留标签并计入报告，输入准入失败立即停止依赖步骤。
func check(value: bool,label: String) -> bool:
	checks+=1
	if not value: failures.append(label); printerr("FAIL: ",label)
	return value
## JSON保留int64和字典字段，所有报告写进调用者指定新目录。
func write_json(path: String,value: Variant) -> void:
	var file=FileAccess.open(path,FileAccess.WRITE)
	if check(file!=null,"报告可写 "+path): file.store_string(JSON.stringify(value,"",false,true)+"\n"); file.close()
## 只从公开当前阵势生成默认防御，所有可控部均保留在完整草案内。
func defaults(page: Dictionary) -> Dictionary:
	var orders: Dictionary={}
	for id in page.header.command_unit_ids:
		if page.units[id].controllable: orders[id]={"action":"defend","destination":page.units[id].position.duplicate(),"heading":page.units[id].heading,"target_id":"","tactic_id":""}
	return orders
## 有限共同样本来自当前兵种实际战法、目标、朝向与完整旁部草案，不增添运行军队或学习记录。
func cases_for(state: Dictionary,data: Dictionary,battle_id: String) -> Array:
	var page: Dictionary=Page.query(state,data,battle_id); var cases: Array=[]
	if not check(page.get("active",false),"作者真实接敌公开页存在"): return cases
	var original: Dictionary=defaults(page)
	for id in original:
		for heading in [0,3]:
			var orders: Dictionary=original.duplicate(true); orders[id].heading=heading
			cases.append({"label":id+":defend:"+str(heading),"args":{"battle_id":battle_id,"selected_id":id,"orders":orders.duplicate(true)}})
			for tactic in page.units[id].tactics:
				orders[id].action="tactic"; orders[id].tactic_id=tactic.id
				var target: String=""
				for other in page.units:
					if page.units[other].side!=page.units[id].side: target=other; break
				orders[id].target_id=target if tactic.kind in ["charge","volley","suppress","flank"] else ""
				cases.append({"label":id+":"+tactic.kind+":"+str(heading),"args":{"battle_id":battle_id,"selected_id":id,"orders":orders.duplicate(true)}})
				var changed: Dictionary=orders.duplicate(true); changed[id].destination=[999,999]
				cases.append({"label":id+":"+tactic.kind+":invalid_destination:"+str(heading),"args":{"battle_id":battle_id,"selected_id":id,"orders":changed}})
	return cases
## 同错保护只检查跨边界暴露的落点：每个格子以真实来源和当前完整单部命令复核。
func legal_movement(input: Dictionary,row: Dictionary,draft: Dictionary) -> void:
	if not draft.get("ok",false): return
	var args: Dictionary=row.args; var battle: Dictionary=input.state.battles[args.battle_id]
	var order: Dictionary=args.orders[args.selected_id]
	if order.action!="tactic": return
	var sources: Dictionary=Combat._damage_sources(input.state,input.catalog.data,battle)
	if not check(sources.get("ok",false),row.label+":真实伤害来源可读"): return
	var expected: Array=[]
	for cell in input.catalog.data.battlefields[battle.battlefield_id].cells:
		var candidate: Dictionary=order.duplicate(true); candidate.destination=[cell.q,cell.r]
		if Round.preview(battle,input.catalog.data,args.selected_id,candidate,sources.factors).get("ok",false): expected.append(candidate.destination)
	var actual: Array=[]
	for move in draft.movement: actual.append(move.position)
	check(actual.size()==expected.size(),row.label+":战法圈合法数量一致")
	for position in actual: check(position in expected,row.label+":公开落点通过真实当前军令 "+str(position))
## 正式Main与backend的原poll调度；记录实际已接纳操作类型，不注入完成或私改cache。
func pump(main: Node) -> void:
	if not main.kernel._job.is_empty(): job_kinds.append(main.kernel._job.kind)
	main.kernel.poll(); main._resident_adapter.poll(); main._resident_process()
	if not main.kernel._job.is_empty(): job_kinds.append(main.kernel._job.kind)
## 包含所有有限页面需求，防止误把没有外层frame的pending当空闲。
func idle(main: Node) -> bool:
	return not main._advancing and not main._resident_frame_pending and not main.kernel.is_busy() and not main.kernel.has_pending_reads() and not main._resident_adapter.is_busy() and main._resident_command.is_empty() and main._resident_quote.is_empty()
## deadline只防进程悬挂，未终态清楚失败。
func settle(main: Node) -> bool:
	var deadline: int=Time.get_ticks_usec()+30000000
	while Time.get_ticks_usec()<deadline:
		pump(main)
		if idle(main): return true
		await process_frame
	return false
## 唯一冷安装后实际Main渲染同一世界；军令换向、旧回调撤销和新读必须零提交零半日。
func main_boundary(input: Dictionary) -> Dictionary:
	if not check(ClassDB.class_exists("HanmoResidentBridge"),"真实冻结扩展已加载"): return {}
	var candidate=Kernel.new(); candidate._catalog=input.catalog.duplicate(true); candidate._state=input.state.duplicate(true)
	var art: Dictionary=Art.prepare_content(candidate.content())
	if not check(art.get("ok",false),"作者目录通过正式美术清单 "+str(art)): return {}
	ProjectSettings.set_setting("application/config/storage_profile","military-preview-joint-"+str(Time.get_ticks_usec()))
	var main=Main.new(); root.add_child(main); main.set_process(false)
	var installed: Dictionary=main._start_resident(candidate,art)
	if not check(installed.get("ok",false),"原Main真实Bridge完整准入 "+str(installed)): main.queue_free(); await process_frame; return {}
	main.kernel.set_process(false); main._resident_adapter.set_process(false)
	main.kernel.page_reads_ready.connect(func(source,epoch): read_events.append({"source":source.duplicate(true),"epoch":epoch}))
	main.kernel.command_completed.connect(func(kind,result,source,current): command_events.append({"kind":kind,"result":result,"source":source,"current":current}))
	if not check(await settle(main),"真实Main初始有限军令读链完成"): main.queue_free(); await process_frame; return {}
	var before: Dictionary=main.kernel.transport().debug_export_state(); var source: Dictionary=main.kernel.source_identity(); var context: Dictionary=main.kernel.command_context()
	var view=main.find_child("MilitaryBattleScreen",true,false)
	if not check(before.get("ok",false) and view!=null,"真实会战控件与完整只读基线存在"): main.queue_free(); await process_frame; return {}
	var selected: String=view.selected_id; var changed_heading: int=posmod(view.orders[selected].heading+1,6)
	view._choose_heading(changed_heading)
	check(main.kernel.has_pending_reads() and view._submit.disabled,"真实改向只排纯读并禁用未核军令确认")
	main._resident_process()
	check(main.kernel._job.get("page_read",false) and main.kernel._job.get("query_id")=="military_battle_draft","唯一worker接纳实际军令草案")
	var accepted: int=read_events.size(); var prior_generation: int=main.kernel._generation
	main._stop_journey_auto("military_preview_navigation")
	check(main.kernel._generation>prior_generation,"正常Main导航撤销旧读取代次")
	var deadline: int=Time.get_ticks_usec()+30000000
	while main.kernel.is_busy() and Time.get_ticks_usec()<deadline: main.kernel.poll(); await process_frame
	check(not main.kernel.is_busy() and read_events.size()==accepted,"已接纳旧回调排空但不发布为当前军令")
	check(await settle(main),"正常Main重读当前来源后完成新军令")
	view=main.find_child("MilitaryBattleScreen",true,false)
	check(view!=null and view.orders[selected].heading==changed_heading,"重读保留完整当前草案朝向")
	if view!=null:
		var expected: Dictionary=Page.draft(input.state,input.catalog.data,view.identity.battle_id,view.selected_id,view.orders)
		check(view._draft_result==expected,"当前Canvas收到同输入GD/C++正式草案结果")
	var after: Dictionary=main.kernel.transport().debug_export_state()
	check(after.get("ok",false) and before.json==after.json and source==main.kernel.source_identity() and context==main.kernel.command_context(),"完整世界/RNG/时钟/ID及请求序号不变")
	check(command_events.is_empty() and not "command" in job_kinds and not main._resident_adapter.auto_running,"预览全过程没有提交或自动半日许可")
	var report: Dictionary={"source":source,"before_sha256":str(before.json).sha256_text(),"after_sha256":str(after.get("json","")).sha256_text(),"command_count":command_events.size(),"read_publications":read_events.size(),"job_kinds":job_kinds,"gui_pixels_accepted":false}
	main.queue_free(); await process_frame
	return report
## 可先捕获真实军旅世界，或重用精确输入逐端查询；准入与规则失败都原样落报告。
func run() -> void:
	var input_path: String=""; var use_main: bool=false
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--input="): input_path=arg.trim_prefix("--input=")
		elif arg.begins_with("--output="): output=arg.trim_prefix("--output=")
		elif arg=="--main": use_main=true
	if not check(output.is_absolute_path(),"显式绝对报告目录"): quit(2); return
	DirAccess.make_dir_recursive_absolute(output)
	var input: Dictionary={}
	if input_path.is_empty():
		var kernel=Kernel.new(); var opened: Dictionary=Fixture.open_battle(kernel)
		if not check(opened.get("ok",false),"既有作者正式任官/编组/行旅/接敌 "+str(opened)): quit(1); return
		input={"catalog":kernel.content(),"state":kernel.snapshot()}
		input.cases=cases_for(input.state,input.catalog.data,opened.battle_id)
		write_json(output.path_join("input.json"),input)
	else:
		var loaded: Dictionary=Strict.parse_text(FileAccess.get_file_as_string(input_path))
		if not check(loaded.get("ok",false),"同源完整输入可读"): quit(2); return
		input=loaded.value
	var before: Dictionary=input.duplicate(true); var rows: Array=[]
	for row in input.cases:
		var args: Dictionary=row.args
		var page: Dictionary=Page.query(input.state,input.catalog.data,args.battle_id)
		var draft: Dictionary=Page.draft(input.state,input.catalog.data,args.battle_id,args.selected_id,args.orders)
		rows.append({"label":row.label,"page":page,"draft":draft})
		legal_movement(input,row,draft)
	check(input==before,"GD全部正式查询保持完整输入与RNG不变")
	var main_report: Dictionary=await main_boundary(input) if use_main else {}
	var report: Dictionary={"ok":failures.is_empty(),"checks":checks,"failures":failures,"cases":rows,"zero_write":input==before,"main":main_report}
	write_json(output.path_join("gd.json"),report)
	print("MILITARY_PREVIEW_JOINT_GD_CASES=",rows.size()," CHECKS=",checks," FAILURES=",failures.size()," MAIN=",use_main)
	quit(0 if failures.is_empty() else 1)
