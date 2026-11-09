## 真Main侧栏布局短门：量测Rect与滚动可达，真实第二岗位pressed对照同Kernel命令；不声称OS输入。
extends SceneTree
const Main=preload("res://ui/main.gd")
const Kernel=preload("res://core/world_kernel.gd")
var checks: int=0
var failures: int=0
var observations: Array=[]

## 引擎和资源就绪后执行唯一限定节点门。
func _init() -> void:
	call_deferred("_run")

## 每项真断言写入退出码，原版布局失败不得伪作通过。
func check(value: bool, label: String) -> bool:
	checks+=1
	if not value:
		failures+=1
		printerr("FAIL: ",label)
	return value

## 只让真实容器排版和旧节点释放，不推进世界时间。
func settle() -> void:
	await process_frame
	await process_frame
	await process_frame

## 按生产语义标记找真实按钮，身份不依赖中文或坐标。
func buttons(main: Control, kind: String) -> Array:
	var result: Array=[]
	for node in main.find_children("*","Button",true,false):
		if node.get_meta("command_kind","")==kind: result.append(node)
	return result

## 沿真实父链找滚动容器，测试不创建替代布局或覆盖回调。
func scroll_parent(node: Node) -> ScrollContainer:
	var parent: Node=node.get_parent()
	while parent!=null:
		if parent is ScrollContainer: return parent
		parent=parent.get_parent()
	return null

## 同一真实场所底栏的窄断言，供当前场景或已验证返程快照复用；只改滚动与焦点，不点击世界命令。
static func navigation_footer_access(tree: SceneTree, main: Control) -> Dictionary:
	var rows: Array=[]
	var scroll: ScrollContainer=main.find_child("SceneNavigationScroll",true,false)
	if scroll==null: return {"ok":false,"rows":[{"ok":false,"label":"当前场所存在实际底栏滚动容器"}]}
	var viewport: Rect2=main.get_viewport_rect()
	rows.append({"ok":viewport.grow(1).encloses(scroll.get_global_rect()) and scroll.horizontal_scroll_mode==ScrollContainer.SCROLL_MODE_DISABLED and scroll.follow_focus,"label":"底栏在实际视口内，只纵向滚动并跟随焦点","rect":str(scroll.get_global_rect()),"viewport":str(viewport)})
	var actions: Node=scroll.get_node("SceneNavigationActions")
	var order: Array=[]
	var first_focus: Button=null; var last_focus: Button=null
	for target in actions.get_children():
		if not target is Button: continue
		order.append({"kind":target.get_meta("command_kind",""),"target":target.get_meta("target_id",""),"text":target.text})
		if not target.disabled:
			if first_focus==null: first_focus=target
			last_focus=target
		scroll.ensure_control_visible(target)
		for _frame in range(3): await tree.process_frame
		rows.append({"ok":scroll.get_global_rect().grow(1).encloses(target.get_global_rect()) and viewport.grow(1).encloses(target.get_global_rect()),"label":"底栏完整可达 "+str(target.get_meta("command_kind","")),"rect":str(target.get_global_rect())})
	if first_focus!=null:
		first_focus.grab_focus()
		for _frame in range(3): await tree.process_frame
		scroll.scroll_vertical=0; last_focus.release_focus(); last_focus.grab_focus()
		for _frame in range(3): await tree.process_frame
		rows.append({"ok":last_focus.has_focus() and scroll.get_global_rect().grow(1).encloses(last_focus.get_global_rect()),"label":"末行按钮仅凭焦点变化即可完整进入底栏"})
	var ok: bool=not order.is_empty()
	for row in rows: ok=ok and row.ok
	return {"ok":ok,"rows":rows,"action_order":order,"scroll_max":scroll.get_v_scroll_bar().max_value,"scroll_page":scroll.get_v_scroll_bar().page,"commands_invoked":false}

## 比较全部侧栏按钮的实际矩形，记录原版同起点盖住NPC的可复核几何反例。
func non_overlapping(targets: Array, label: String) -> void:
	var overlaps: Array=[]
	for first in range(targets.size()):
		for second in range(first+1,targets.size()):
			if targets[first].get_global_rect().intersects(targets[second].get_global_rect()):
				overlaps.append([targets[first].get_meta("target_id"),targets[second].get_meta("target_id")])
	check(overlaps.is_empty(),label+" 人物事务和通道按钮不互盖："+str(overlaps))
	observations.append({"case":label,"overlaps":overlaps})

## 每个尺寸字号使用真实新世界，浏览全程与同源目录/RNG保持精确相等，点击只产生真实talk。
func layout_case(width: int, percent: int, red_only: bool) -> void:
	root.content_scale_size=Vector2i.ZERO
	root.size=Vector2i(width,720)
	var main=Main.new()
	main.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	root.add_child(main)
	await settle()
	var path: String=ProjectSettings.get_setting("application/config/default_catalog")
	var opened: Dictionary=main.start_content("side::scenario.two_stories",path)
	if not check(opened.ok,"真实Main/Kernel及受信Art启程："+str(opened)):
		main.queue_free(); await settle(); return
	check(main._ui_font_scale.set_percent(percent,false).ok,"正式字号控制器应用 %d%%"%percent)
	await settle()
	var before: Dictionary=main.kernel.snapshot()
	var catalog: Dictionary=main.kernel.content()
	var talks: Array=buttons(main,"talk")
	var paths: Array=buttons(main,"move_local")
	check(main._data.places["side::west.grain"].parent_place==null and main._data.places["side::west.street"].parent_place==null,"正式作者含两个顶层互通场所")
	check(talks.size()==7 and paths.size()==6,"实际Screen含六NPC七岗位与六通道")
	var targets: Array=talks+paths
	var label: String="%dx720/%d%%"%[width,percent]
	check(main.size==Vector2(width,720) and root.get_visible_rect().size==Vector2(width,720),label+" 实际Main与视口尺寸匹配测试输入")
	check(main.get_meta("ui_font_scale_percent")==percent,label+" 实际表现根采用指定字号档位")
	non_overlapping(targets,label)
	if red_only:
		main.queue_free(); await settle(); return
	if not check(not talks.is_empty() and not paths.is_empty(),label+" 存在两类真实入口"):
		main.queue_free(); await settle(); return
	var scroll: ScrollContainer=scroll_parent(talks[0])
	if not check(scroll!=null and scroll==scroll_parent(paths[0]),label+" 人物事务与通道共用一个有界滚动容器"):
		main.queue_free(); await settle(); return
	var viewport: Rect2=Rect2(Vector2.ZERO,Vector2(width,720))
	check(viewport.encloses(scroll.get_global_rect()) and scroll.size.y<720 and scroll.horizontal_scroll_mode==ScrollContainer.SCROLL_MODE_DISABLED,label+" 侧栏在视口内且横向不滚动")
	var people: Control=scroll.get_node_or_null("Groups/PeopleActions")
	var places: Control=scroll.get_node_or_null("Groups/PlaceActions")
	check(people!=null and places!=null and people.get_global_rect().end.y<=places.get_global_rect().position.y,label+" 人物事务与通道为自然排列的两个独立小节")
	check(scroll.get_v_scroll_bar().max_value>scroll.get_v_scroll_bar().page,label+" 有限长名单确实触发纵向滚动")
	observations[-1].merge({"main_size":str(main.size),"viewport_size":str(root.get_visible_rect().size),"font_percent":main.get_meta("ui_font_scale_percent"),"sidebar_rect":str(scroll.get_global_rect()),"scroll_max":scroll.get_v_scroll_bar().max_value,"scroll_page":scroll.get_v_scroll_bar().page})
	seed(486)
	var expected_random: int=randi()
	seed(486)
	for target in targets:
		check(scroll_parent(target)==scroll,label+" 每个真实入口属于同一滚动容器")
		scroll.ensure_control_visible(target)
		await settle()
		var rect: Rect2=target.get_global_rect()
		var bounds: Rect2=scroll.get_global_rect()
		check(rect.size.y>0 and bounds.grow(1).encloses(rect),label+" 滚动可完整到达 "+str(target.get_meta("target_id")))
	check(randi()==expected_random,label+" 滚动不消费全局RNG")
	check(main.kernel.snapshot()==before and main.kernel.content()==catalog,label+" 字号/排版/滚动保持完整世界、RNG、ID与catalog")
	var footer: Dictionary=await navigation_footer_access(self,main)
	for row in footer.rows: check(row.ok,label+" "+row.label)
	observations.append({"case":label+" navigation","footer":footer})
	check(main.kernel.snapshot()==before and main.kernel.content()==catalog,label+" 底栏滚动和焦点不改变世界或作者目录")
	var second: Button=null
	for target in talks:
		if target.get_meta("target_id")=="side::role.merchant": second=target
	if check(second!=null and not second.disabled,label+" 同NPC第二岗位真实按钮可用"):
		scroll.ensure_control_visible(second); await settle()
		var control=Kernel.new()
		if check(control.new_game("side::scenario.two_stories",path).ok and control.snapshot()==before,label+" 独立真实Kernel建立同一完整初态"):
			var actor: String=before.player_actor_id
			var command: Dictionary={"kind":"talk","params":{"role_id":"side::role.merchant"},"actor_id":actor,"request_seq":before.request_sequences[actor]+1,"expected_revision":before.revision}
			check(control.submit(command).ok,label+" 对照真实talk合法")
			second.pressed.emit(); await settle()
			var after: Dictionary=main.kernel.snapshot()
			check(after==control.snapshot() and main.kernel.content()==catalog,label+" 真实第二岗位pressed仅产生同一正式命令整态")
			var session: Dictionary=after.interactions.get(actor,{})
			check(session.get("dialogue_id")=="side::dialog.shop" and session.get("context",{}).get("role_id")=="side::role.merchant" and session.get("context",{}).get("provider_actor_id")=="side::actor.zheng",label+" 第二岗位进入原对白与正确人/岗context")
	main.queue_free(); await settle()

## 原版仅一反例；修后只四个受影响组合，证据仅本地，不打开GUI或重跑其他门。
func _run() -> void:
	var args: PackedStringArray=OS.get_cmdline_user_args()
	var red_only: bool="--red-only" in args
	var evidence: String=""
	for arg in args:
		if arg.begins_with("--evidence="): evidence=arg.trim_prefix("--evidence=")
	for width in ([1280] if red_only else [1280,960]):
		for percent in ([100] if red_only else [100,150]):
			await layout_case(width,percent,red_only)
	if not evidence.is_empty():
		var file=FileAccess.open(evidence.path_join("red-result.json" if red_only else "green-result.json"),FileAccess.WRITE)
		file.store_string(JSON.stringify({"checks":checks,"failures":failures,"observations":observations,"os_input":false}))
	print("SCENE_SIDEBAR_CHECKS=",checks," FAILURES=",failures)
	quit(0 if failures==0 else 1)
