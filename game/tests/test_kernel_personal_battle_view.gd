## 真实个人输入节点与固定旧oracle；领域预览用原Personal，独立桥不冒称完整世界或GUI。
extends SceneTree
const Screen=preload("res://ui/personal_battle_screen.gd")
const Canvas=preload("res://ui/personal_battle_canvas.gd")
const Kernel=preload("res://core/world_kernel.gd")
const Personal=preload("res://core/personal_combat.gd")
const Round=preload("res://core/personal_round.gd")
const Catalog=preload("res://core/content_catalog.gd")
const Art=preload("res://ui/art_catalog.gd")
const Composer=preload("res://ui/screen_composer.gd")
const AreaFixture=preload("res://tests/test_personal_area_strike.gd")
const StrikeFixture=preload("res://tests/test_personal_strike_conditions.gd")
const ORACLE="b704403401ff625dfd6757c2f34ba3f24d0f41c2ad819c24063532a1ad7c42c0"
const CASES=["initial","area","defend","wait","stationary","range","invalid_destination","observer","withdrawn","down_target","stale_revision","stale_actor","stale_round","stale_phase","stale_controller","stale_epoch","stale_generation","stale_kernel","duplicate_submit","result_close","reset"]
var checks: int=0
var failures: int=0
var allocated: Array=[]

## 计数仍委托同一个真实Backend；不替换预览、提交或世界阶段。
class ObservedKernel extends "res://core/world_kernel.gd":
	var snapshot_reads: int=0
	var page_reads: int=0
	var context_reads: int=0
	var geometry_reads: int=0
	var hud_ids: Array=[]
	## 外层Composer原快照独立保留，个人节点不能暗中再读取。
	func snapshot() -> Dictionary: snapshot_reads+=1; return super.snapshot()
	## 转发完整用途查询，不能在计数器中拼造页面成功结果。
	func personal_battle_view() -> Dictionary:
		page_reads+=1; var actual=Kernel.new(); actual._backend=_backend; return actual.call("personal_battle_view")
	## 只计公开身份调用；叶内来源核对不算UI额外请求。
	func personal_battle_context(id: String) -> Dictionary:
		context_reads+=1; var actual=Kernel.new(); actual._backend=_backend; return actual.call("personal_battle_context",id)
	## 画布候选仍从同一真实battle读取。
	func personal_battle_geometry(id: String,draft: Dictionary) -> Dictionary:
		geometry_reads+=1; var actual=Kernel.new(); actual._backend=_backend; return actual.call("personal_battle_geometry",id,draft)
	## 原HUD上下文同一battle_id，不重新选择或用世界HP顶替零体力。
	func hud_view(id: String="") -> Dictionary: hud_ids.append(id); return super.hud_view(id)

## 显式局部桥调用唯一真实个人预览；记录旧snapshot与新context为相同身份读点。
class Bridge extends RefCounted:
	var current: Dictionary={}
	var definitions: Dictionary={}
	var events: Array=[]
	var snapshots: int=0
	var geometry_reads: int=0
	## 旧页面身份读取；副本不能改写测试权威输入。
	func snapshot() -> Dictionary:
		snapshots+=1; events.append("context"); return current.duplicate(true)
	## 新身份口只委托候选叶，不伪造成功。
	func personal_battle_context(id: String) -> Dictionary:
		events.append("context")
		return load("res://core/views/personal_battle_view.gd").context(current,id)
	## 新画布口仍用完整原会话计算，不把公开DTO喂回Round。
	func personal_battle_geometry(id: String,draft: Dictionary) -> Dictionary:
		geometry_reads+=1
		return load("res://core/views/personal_battle_view.gd").geometry(current,definitions,id,draft)
	## 原个人预览和revision回传；无随机替身或新控制者规则。
	func preview(kind: String,params: Dictionary,actor_id: String="") -> Dictionary:
		events.append(["preview",kind,params.duplicate(true),actor_id])
		var result: Dictionary=Personal.preview(current,definitions,actor_id if not actor_id.is_empty() else current.player_actor_id,kind,params)
		result.revision=current.revision; return result

## 只提供原节点宿主协议；命令记录不替代世界执行，重绘也不另起场景或存储。
class Host extends Control:
	var kernel
	var _data: Dictionary={}
	var _scene_counter: int=7
	var _advancing: bool=false
	var commands: Array=[]
	var renders: int=0
	var _message: String=""
	var status_label: Label
	var real_submit: bool=false
	var last_result: Dictionary={}
	## 沿原作者文本降级，运行人物名字仍由真实Screen读取。
	func _name(record: Dictionary) -> String: return _data.texts.get(record.get("name_key",""),record.get("id","unknown"))
	## 只观察既有命令种类/载荷及重复回调锁。
	func _perform(kind: String,params: Dictionary) -> void:
		commands.append({"kind":kind,"params":params.duplicate(true)})
		if real_submit:
			var context: Dictionary=kernel.command_context()
			last_result=kernel.submit({"kind":kind,"params":params,"actor_id":context.actor_id,"request_seq":context.next_request_seq,"expected_revision":context.revision})
	## 延迟失效请求只计数；不触发GUI或第二次世界推进。
	func _render() -> void: renders+=1
	## 整页装配仅创建真实功能控件，不截图或安排操作系统输入。
	func _text(parent: Node,words: String,font_size: int) -> Label:
		var label=Label.new(); label.text=words; label.add_theme_font_size_override("font_size",font_size); parent.add_child(label); return label
	## 原命令元数据和回调保留，按钮本身不替代核心资格。
	func _button(parent: Node,words: String,kind: String,target: String,callback: Callable,disabled: bool=false) -> Button:
		var button=Button.new(); button.text=words; button.disabled=disabled; button.set_meta("command_kind",kind); button.set_meta("target_id",target); button.pressed.connect(callback); parent.add_child(button); return button

## 字号菜单只隔离本地偏好控件；不接触任何世界查询或保存服务。
class FontControl extends RefCounted:
	## 保持宿主菜单可装配，本门不验证字体偏好存储。
	func make_selector(parent: Node) -> void: parent.add_child(OptionButton.new())

## 完整Composer个人分支的有限宿主，系统菜单回调不执行，避免初始化Native存储。
class ComposerHost extends Host:
	const Landscape=preload("res://ui/scene_stage.gd")
	var page: VBoxContainer
	var _started: bool=true
	var _ambience: Dictionary={"enabled":false}
	var _ui_font_scale=FontControl.new()
	var _open_map: Callable=_inactive
	var _journal: Callable=_inactive
	var _save: Callable=_inactive
	var _load: Callable=_inactive
	var _choose_export: Callable=_inactive
	var _choose_import: Callable=_inactive
	var _toggle_audio: Callable=_inactive
	var _title_prompt: Callable=_inactive
	## 原系统按钮只保绑定，本测试不点击或声明其存读行为。
	func _inactive() -> void: pass

## 等消息队列可处理旧页deferred回调后运行。
func _init() -> void: call_deferred("run")

## 记录所有反例并令失败退出非零。
func check(value: bool,label: String) -> bool:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)
	return value

## 复用已存在真实个人规则夹具；本段不是完整WorldSchema输入。
func fixture(label: String) -> Host:
	var data: Dictionary=StrikeFixture.content() if label in ["stationary","range"] else AreaFixture.content()
	var battle: Dictionary=StrikeFixture.battle() if label in ["stationary","range"] else AreaFixture.battle()
	data.required_capabilities=["personal_combat"]; data.actors={}; data.texts={}; battle.id="battle.view"; battle.kind="personal"
	var actors: Dictionary={}
	for id in battle.units:
		actors[id]={"id":id,"alive":true}; data.actors[id]={"id":id}
	if label=="withdrawn": battle.round=2; battle.units["b.near"].withdrawn_round=1
	if label=="down_target": battle.units["b.near"].hp=0
	if label=="result_close": battle.phase="result"; battle.applied=true; battle.outcome="defeat"; battle.units[battle.controller_id].hp=0
	var bridge=Bridge.new(); bridge.definitions=data
	bridge.current={"player_actor_id":"b.near" if label=="observer" else battle.controller_id,"revision":5,"tick":7,"actors":actors,"battles":{battle.id:battle},"rng":{"duel":{"seed":"11","state":"23"}}}
	var host=Host.new(); host.kernel=bridge; host._data=data; allocated.append(host); return host

## 不调用_build/_ready资源装配，沿真实草案/标签/范围/输入方法；新旧都共用同一控件。
func view_for(host: Host,projection: Dictionary={}):
	var view=Screen.new(); allocated.append(view); view.host=host
	if view.has_method("configure_view"):
		view.configure_view(projection if not projection.is_empty() else load("res://core/views/personal_battle_view.gd").query(host.kernel.current,host._data),host._data)
	else:
		view.state=host.kernel.current.duplicate(true); view.data=host._data; view.battle=view.state.battles["battle.view"]
	view.canvas=Canvas.new(); allocated.append(view.canvas); view.canvas.size=Vector2(800,500); view.canvas.battle=view.battle; view.canvas.data=view.data
	view._preview=Label.new(); view.add_child(view._preview); view._target=Label.new(); view.add_child(view._target)
	view._heading_label=Label.new(); view.add_child(view._heading_label); view._heading_slider=HSlider.new(); view.add_child(view._heading_slider)
	view._submit=Button.new(); view.add_child(view._submit); view._skills=OptionButton.new(); view.add_child(view._skills)
	view._skill_ids=[""]; view._skills.add_item("none")
	for id in view.battle.units[view.battle.controller_id].learned:
		if view.data.personal_skills[id].kind in ["strike","guard"]: view._skill_ids.append(id); view._skills.add_item(id)
	view._restore_draft(); view._refresh(); return view

## 每个case执行真实目标方法，摘要含旧命令/读点与可见结果；实例ID仅在草案签名里归一。
func observe(label: String,checking: bool) -> Dictionary:
	var host: Host=fixture(label); var view=view_for(host)
	var state: Dictionary=host.kernel.current; var battle: Dictionary=state.battles["battle.view"]
	match label:
		"area": view._choose_actor("z.primary"); view._choose_skill(view._skill_ids.find("pierce"))
		"defend","wait": view._choose_action(label)
		"stationary": view.draft.destination=[260,200]; view._choose_skill(view._skill_ids.find("aim"))
		"range": view._choose_skill(view._skill_ids.find("long"))
		"invalid_destination": view._choose_destination([-99,-99])
		"observer": view._choose_heading(72); view._submit_plan()
		"withdrawn": view.selected_id="b.near"; view.draft.target_id="b.near"; view._refresh(); view._choose_actor("b.near")
		"stale_revision": state.revision+=1; view._submit_plan()
		"stale_actor": state.player_actor_id="b.near"; view._submit_plan()
		"stale_round": battle.round+=1; view._submit_plan()
		"stale_phase": battle.phase="result"; view._submit_plan()
		"stale_controller": battle.controller_id="b.near"; view._submit_plan()
		"stale_epoch": host._scene_counter+=1; view._submit_plan()
		"stale_generation": host.set_meta("personal_battle_generation",1); view._submit_plan()
		"stale_kernel":
			var next=Bridge.new(); next.current=state.duplicate(true); next.definitions=host._data; next.events=host.kernel.events; host.kernel=next; view._submit_plan()
		"duplicate_submit": view._submit_plan(); view._submit_plan()
		"result_close": view._close_result(); view._close_result()
		"reset": view._choose_destination([225,220]); view._choose_heading(90); view._reset_plan()
	await process_frame
	if checking: check(host.kernel.snapshots==0,"页面身份守卫不复制全世界 "+label)
	if checking and label=="initial": check(host.kernel.geometry_reads==0,"正式成功quote复用其战术几何，不再重算回退")
	if checking and label=="invalid_destination": check(host.kernel.geometry_reads==1,"非法落点只在旧Canvas回退读一次geometry")
	var meta: Dictionary=host.get_meta("personal_battle_draft",{}).duplicate(true)
	if meta.has("signature"): meta.signature=":".join(meta.signature.split(":").slice(1))
	# 旧_draw_draft原表达式作为移动圈来源oracle，新画布直接读取真正用于绘制的有限缓存。
	var distance: int=view.canvas.get("_movement_distance") if view.canvas.has_method("configure_view") else view.canvas.tactical_preview.get("movement_distance",Round.movement_distance(view.battle,view.data,view.battle.controller_id,view.draft.skill_id,view.draft.action))
	return {"draft":view.draft.duplicate(true),"selected":view.selected_id,"text":view._preview.text,"target":legacy_equipment_text(view._target.text,"PersonalBattleTarget"),"disabled":view._submit.disabled,"stale":view._stale,"events":host.kernel.events.duplicate(true),"commands":host.commands.duplicate(true),"renders":host.renders,"meta":meta,"range":view.canvas._range_polygons.duplicate(true),"movement_circle":distance,"arena":view.canvas.arena_rect(),"hit":view.canvas.actor_at(view.canvas.screen_point([300,200])),"state":host.kernel.current.duplicate(true)}

## 旧无甲oracle只去除本片新增的两个明确后缀；新护具文字/数值另由专测逐项验证，不重录整页hash。
func legacy_equipment_text(words: String,node_name: String) -> String:
	if node_name not in ["PersonalBattleTarget","PersonalBattleCondition"]: return words
	return words.replace(" · 甲防0" if node_name=="PersonalBattleCondition" else " · 护具防御 0","").replace(" · 护具未知","")

## 递归收集真实控件文本/按钮而不绘制像素，用固定旧结果核对完整装配字段。
func control_rows(node: Node) -> Array:
	var rows: Array=[]
	if node.name in ["PersonalBattleEquipmentToggle","PersonalBattleEquipmentDetails"]: return rows
	if node is Label: rows.append(["label",legacy_equipment_text(node.text,node.name)])
	elif node is Button: rows.append(["button",node.text,node.disabled,node.get_meta("command_kind","")])
	if node is OptionButton:
		for index in node.item_count: rows.append(["option",node.get_item_text(index)])
	for child in node.get_children(): rows.append_array(control_rows(child))
	return rows

## 同一真实场景装配入口用于非GUI功能oracle；桥仍只执行真实Personal.preview，不伪称Schema。
func assembled_observe(label: String,checking: bool) -> Dictionary:
	var host: Host=fixture(label); host.size=Vector2(1280,800); root.add_child(host)
	var state: Dictionary=host.kernel.current
	# 纯Round旧夹具省略页面标题的kind；完整装配显式补已存在的切磋类型，不改领域公式。
	host._data.personal_objectives[state.battles["battle.view"].objective_id].kind="sparring"
	var view=Screen.render(host,host,state,"battle.view")
	if checking: check(host.kernel.snapshots==0,"完整装配不为身份再复制世界 "+label)
	if checking: check(host.kernel.geometry_reads=={"initial":1,"observer":2,"result_close":0}[label],"整页只在原初始化/回退位置取geometry "+label)
	return {"controls":control_rows(view),"bodies":view.canvas.battle_descriptors.duplicate(true),"identities":view.canvas.identity_descriptors.duplicate(true),"range":view.canvas._range_polygons.duplicate(true),"events":host.kernel.events.duplicate(true),"draft":view.draft.duplicate(true),"target":view.selected_id,"state":state.duplicate(true)}

## 携带真实当前身份发送命令，测试不预扣资源或手写战斗会话。
func send_actual(kernel,kind: String,params: Dictionary) -> Dictionary:
	var value: Dictionary=kernel.command_context()
	return kernel.submit({"kind":kind,"params":params,"actor_id":value.actor_id,"request_seq":value.next_request_seq,"expected_revision":value.revision})

## 真实正式Alpha、实际开战与一次节点提交；没有Native/Codec/GUI流程。
func actual_kernel_checks() -> void:
	if not Kernel.new().has_method("personal_battle_view"): return
	var kernel=ObservedKernel.new()
	var installed: Dictionary=Catalog.installed_catalogs()
	if not check(installed.ok and installed.catalogs.size()==1,"真实薄fixture只装一个正式Alpha组合"): return
	if not check(kernel.new_game("draft::scenario.alpha_life",installed.catalogs[0].catalog_path).ok,"真实Kernel启动Alpha"): return
	if not check(send_actual(kernel,"move_local",{"place_id":"draft::west.training"}).ok,"真实人物抵达练武场"): return
	var state: Dictionary=kernel.snapshot(); var role: String="draft::role.wei"
	var entered: Dictionary=send_actual(kernel,"duel_start",{"context":{"place_id":"draft::west.training","provider_actor_id":state.roles[role].actor_id,"role_id":role}})
	if not check(entered.ok,"真实注册命令建立个人战 "+str(entered)): return
	state=kernel.snapshot(); var projection: Dictionary=kernel.personal_battle_view()
	check(projection.ok and projection.battle_id==Screen.active(state),"实际Backend门面选择原会话")
	var before: Dictionary=state.duplicate(true); seed(919); var expected_random: int=randi(); seed(919)
	var stamp: Dictionary=kernel.personal_battle_context(projection.battle_id)
	var unit: Dictionary=projection.battle.units[projection.battle.controller_id]
	var candidate: Dictionary=kernel.personal_battle_geometry(projection.battle_id,{"destination":unit.position,"heading":unit.heading,"action":"wait","target_id":"","skill_id":""})
	check(stamp.ok and candidate.ok and kernel.snapshot()==before and randi()==expected_random,"真实三按需口不改变世界或随机")
	var prepared: Dictionary=Art.prepare_content(kernel.content())
	if check(prepared.ok,"正式tiny表现注册可用于完整个人分支"):
		Art.commit_content(prepared)
		var composed=ComposerHost.new(); allocated.append(composed); composed.kernel=kernel; composed._data=kernel.content().data; composed.size=Vector2(1280,800)
		composed.page=VBoxContainer.new(); composed.add_child(composed.page); root.add_child(composed)
		kernel.snapshot_reads=0; kernel.page_reads=0; kernel.context_reads=0; kernel.geometry_reads=0; kernel.hud_ids=[]
		Composer.render(composed)
		var screen=composed.page.get_child(0).get_node_or_null("PersonalBattleScreen")
		check(screen!=null and screen.page_input.battle_id==projection.battle_id and not screen.battle.has("ai_plans"),"实际Composer只把选中有限页面交给Screen")
		check(kernel.snapshot_reads==0 and kernel.page_reads==1 and kernel.context_reads==2 and kernel.geometry_reads==1,"现有有限外壳不取整世界快照，仍为1页面、2旧身份读点、1初始画布回退")
		check(kernel.hud_ids==[projection.battle_id] and kernel._state==before,"实际个人分支HUD同会话且完整装配不写世界")
	var host=Host.new(); allocated.append(host); host.kernel=kernel; host._data=kernel.content().data; host.real_submit=true
	var view=view_for(host,projection); view._submit_plan(); view._submit_plan()
	check(host.commands.size()==1 and host.last_result.ok and kernel.snapshot().revision==before.revision+1,"真实节点只提交一次原battle_plan")
	var other: String=state.battles[projection.battle_id].units.keys().filter(func(id): return id!=state.player_actor_id)[0]
	check(kernel.preview("battle_close",{"battle_id":projection.battle_id},other).get("code")=="NotBattleController","只读画布不改变控制者优先的实际首错")

## 稳定同引擎摘要，不把新输出自动写成旧基准。
func digest(value: Variant) -> String: return JSON.stringify(value,"",true,true).sha256_text()

## 只针对有限输出、按需读取和源隔离，不借UI真假返回证明行动权限。
func projection_checks() -> void:
	if not check(FileAccess.file_exists("res://core/views/personal_battle_view.gd"),"有限个人战源叶存在"): return
	var source=load("res://core/views/personal_battle_view.gd")
	check(source.query({},{})=={"ok":false,"code":"NoWorld"},"空世界明确失败")
	var host: Host=fixture("initial"); var state: Dictionary=host.kernel.current; var data: Dictionary=host._data
	var before: Dictionary=state.duplicate(true); var value: Dictionary=source.query(state,data)
	if not check(value.ok and value.battle_id=="battle.view" and value.has("battle"),"整页绑定当前个人会话 "+str(value)): return
	check(not value.battle.has("ai_plans") and not value.battle.has("source") and not value.has("state"),"整页仅当前会话公开字段")
	check(not value.battle.units["b.near"].has("learned") and not value.battle.units["b.near"].has("martial") and value.battle.units.defender.has("learned"),"不透传未消费NPC武学或冻结伤害输入")
	var early: Dictionary=state.duplicate(true); early.battles["unread_after_controller"]=false
	check(source.query(early,data).get("battle_id")=="battle.view","本人控制者早返后不读无关战场")
	early.battles={"military":{"kind":"military","phase":false},"closed":{"kind":"personal","phase":"closed","units":false},"other":{"kind":"personal","phase":"planning","units":{},"controller_id":false},"battle.view":state.battles["battle.view"],"unread":false}
	check(source.query(early,data).get("battle_id")=="battle.view","选择器按原kind/closed/参与者短路后才读控制者")
	value.battle.units.defender.position[0]+=100
	check(state==before,"整页嵌套位置与权威状态隔离")
	state.facts=false; state.wallets=false; state.actors=false; state.battles["battle.view"].ai_plans={"private":false}
	check(source.context(state,"battle.view").ok,"身份戳不触碰人物/外观/AI计划值/账目")
	state=before.duplicate(true); var battle: Dictionary=state.battles["battle.view"]
	var draft: Dictionary={"destination":battle.units.defender.position.duplicate(),"heading":0,"action":"wait","target_id":"","skill_id":""}
	seed(57); var expected: int=randi(); seed(57); var geometry: Dictionary=source.geometry(state,data,"battle.view",draft)
	check(geometry.ok and geometry.movement_distance>0 and geometry.fallback_tactical_preview.ok and randi()==expected and state==before,"真实会话同源候选纯查询")
	check(not source.context(state,"absent").ok and not source.geometry(state,data,"absent",draft).ok,"未知会话不借另一战场回退")
	battle.last_events=[{"action":"private_extension","target_id":"b.near","damage":7,"private":"hidden"}]
	value=source.query(state,data)
	check(value.battle.damage_feedback==[{"target_id":"b.near","damage":7}] and value.events.is_empty(),"飘字原无动作过滤，文本仍原白名单")
	state.actors.defender.appearance=false; value=source.query(state,data)
	check(value.ok and not value.character_inputs.defender.ok,"外观失败子结果不改变可用战场或制造替代人物")
	state=before.duplicate(true); state.battles["battle.view"].units.defender.hp=0
	check(source.query(state,data).battle.units.defender.hp==0,"公开冻结战场HP零不回填世界体力")
	state.battles["battle.view"].units.defender.position=false
	check(source.query(state,data).get("code")=="InvalidPersonalBattleViewSource","坏必要公开字段结构化拒绝")
	state=before.duplicate(true); state.battles["battle.view"].units["b.near"].withdrawn_round=true
	check(source.query(state,data).get("code")=="InvalidPersonalBattleViewSource","目标池读取的退出轮类型先检查")
	var invalid_data: Dictionary=data.duplicate(true); invalid_data.personal_objectives[before.battles["battle.view"].objective_id]=false
	check(source.query(before,invalid_data).get("code")=="InvalidPersonalBattleViewSource","所选目标定义坏引用结构化拒绝")
	state=before.duplicate(true); var first: Dictionary=state.battles["battle.view"].duplicate(true); first.id="first"; first.controller_id="b.near"
	var second: Dictionary=first.duplicate(true); second.id="second"
	state.battles={"first":first,"second":second}
	check(source.query(state,data).battle_id=="second","无本人控制会话保持最后fallback")
	state.battles.first.controller_id="defender"; check(source.query(state,data).battle_id=="first","本人控制者优先")
	state.battles.first.phase="closed"; state.battles.second.phase="closed"
	check(source.query(state,data).battle_id=="" and source.context(state,"first").phase=="closed","关闭不再路由，旧身份核仍能观察phase变化")

## 原源码显式捕获后只对比固定oracle；缺门面为正常行为红而非编译失败。
func run() -> void:
	if "--stamina-source-only" in OS.get_cmdline_user_args():
		stamina_source_checks()
		for object in allocated:
			if is_instance_valid(object): object.free()
		print("PERSONAL_VIEW_STAMINA_CHECKS=",checks," FAILURES=",failures); quit(0 if failures==0 else 1); return
	var capture: String=""
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--capture-oracle="): capture=arg.trim_prefix("--capture-oracle=")
	var outputs: Dictionary={}
	for label in CASES: outputs[label]=await observe(label,capture.is_empty())
	for label in ["initial","observer","result_close"]: outputs["assembled_"+label]=assembled_observe(label,capture.is_empty())
	if not capture.is_empty():
		var file=FileAccess.open(capture,FileAccess.WRITE); file.store_string(JSON.stringify({"screen_sha256":FileAccess.get_sha256("res://ui/personal_battle_screen.gd"),"canvas_sha256":FileAccess.get_sha256("res://ui/personal_battle_canvas.gd"),"sha256":digest(outputs),"outputs":outputs},"\t",true,true)+"\n"); file.close(); print("OLD_PERSONAL_VIEW_ORACLE=",digest(outputs))
	else:
		check(digest(outputs)==ORACLE,"原节点选择/文字/几何/草案/命令与守卫时点保持")
		for name in ["personal_battle_view","personal_battle_context","personal_battle_geometry"]: check(Kernel.new().has_method(name),"完整用途按需入口 "+name)
		projection_checks()
		stamina_source_checks()
		actual_kernel_checks()
	for object in allocated:
		if is_instance_valid(object): object.free()
	print("KERNEL_PERSONAL_BATTLE_VIEW_CHECKS=",checks," FAILURES=",failures); quit(0 if failures==0 else 1)

## 可选初始气势仍服从正式整数0..100；坏源容器不能经目标投影形成目录可写别名。
func stamina_source_checks() -> void:
	var source=load("res://core/views/personal_battle_view.gd"); var host: Host=fixture("initial")
	var state: Dictionary=host.kernel.current; var data: Dictionary=host._data; var id: String=state.battles["battle.view"].objective_id
	check(source.query(state,data).objective.initial_stamina==50,"缺省初始气势仍为50")
	for valid in [0,50,100]:
		data.personal_objectives[id].initial_stamina=valid
		var value: Dictionary=source.query(state,data)
		check(value.ok and value.objective.initial_stamina==valid,"合法初始气势原值保留 "+str(valid))
	for invalid in [{"private":["hidden"]},["hidden"],true,"50",null,50.0,-1,101]:
		data.personal_objectives[id].initial_stamina=invalid; var before: Dictionary=data.duplicate(true)
		check(source.query(state,data).get("code")=="InvalidPersonalBattleViewSource" and data==before,"初始气势坏源结构化拒绝且不改写 "+JSON.stringify(invalid))
