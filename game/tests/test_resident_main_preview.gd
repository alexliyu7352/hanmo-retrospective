## 原Main与真实Bridge的路线读取消门；只改候选选择，世界字节与半日必须完全不变。
extends SceneTree
const Main=preload("res://ui/main.gd")
var checks: int=0
var failures: int=0
var completions: Array=[]
var errors: Array=[]

## 初始化后才挂载实际Main，测试不抢占图形窗口或创建替代规则世界。
func _initialize() -> void: call_deferred("_run")

## 失败计入退出码；等待终态失败时不继续冒称后续分支已经执行。
func check(value: bool,label: String) -> bool:
	checks+=1
	if not value: failures+=1; push_error(label)
	return value

## 使用原生产poll与Main调度，只关闭自动_process以固定未消费回执的切换边界。
func pump(main: Node) -> void:
	main.kernel.poll(); main._resident_adapter.poll(); main._resident_process()

## 真实worker可以异步工作，页面空闲必须等所有票据和有限读链终态。
func idle(main: Node) -> bool:
	return not main._advancing and not main._resident_frame_pending and not main.kernel.is_busy() and not main._resident_adapter.is_busy() and main._resident_command.is_empty() and main._resident_quote.is_empty()

## 有限截止仅防止测试悬挂，未完成会返回明确失败。
func settle(main: Node) -> bool:
	var deadline: int=Time.get_ticks_usec()+30000000
	while Time.get_ticks_usec()<deadline:
		pump(main)
		if idle(main): return true
		await process_frame
	return false

## 停在真实动态preview已接纳、但backend尚未消费其完成的边界。
func begin_pending_quote(main: Node,destination: Variant) -> bool:
	if destination is String: main._map_select(destination)
	else: main._map_select_point(destination)
	var deadline: int=Time.get_ticks_usec()+30000000
	while Time.get_ticks_usec()<deadline:
		pump(main)
		var job: Dictionary=main.kernel.get("_job")
		if job.get("kind")=="preview" and not job.get("fixed_quote",false):
			return job.get("params",{}).get("destination")==destination
		await process_frame
	return false

## 只观察生产信号实际发布的目标，旧ticket完成不得借新选择重新标记来源。
func preview_finished(kind: String,params: Dictionary,result: Dictionary,source: Dictionary,scene: int) -> void:
	completions.append({"kind":kind,"params":params.duplicate(true),"result":result.duplicate(true),"source":source.duplicate(true),"scene":scene})

## 真实Label文字与按钮状态由原Main/WorldMapScreen产生，不从实现源码推导通过。
func labels(node: Node) -> PackedStringArray:
	var values: PackedStringArray=[]
	for child in node.get_children():
		if child is Label: values.append(child.text)
		values.append_array(labels(child))
	return values

## 按既有语义标记查找当前页面按钮，已脱离树的旧页面不能充当结果。
func button(node: Node,kind: String) -> Button:
	for child in node.get_children():
		if child is Button and child.get_meta("command_kind","")==kind: return child
		var result: Button=button(child,kind)
		if result!=null: return result
	return null

## 实际选点期间没有提交命令，所有案例共用一次安装与同一原生世界。
func _run() -> void:
	ProjectSettings.set_setting("development/native_resident_world_enabled",true)
	ProjectSettings.set_setting("application/config/storage_profile","resident-preview-test-"+str(Time.get_ticks_usec()))
	if not check(ClassDB.class_exists("HanmoResidentBridge"),"已加载冻结的真实resident扩展"): quit(1); return
	var main=Main.new(); root.add_child(main); main.set_process(false)
	var installed: Dictionary=main.start_content("scenario.alpha_life","res://data/generated/alpha_life.hmcatalog.json")
	if not check(installed.get("ok",false),"原Main真实Alpha仅冷安装一次"): await finish(main); return
	main.kernel.set_process(false); main._resident_adapter.set_process(false)
	main.kernel.preview_ready.connect(preview_finished)
	main.kernel.failed.connect(func(error): errors.append(error.duplicate(true)))
	if not check(await settle(main),"首个有限frame完整发布"): await finish(main); return
	var initial_source: Dictionary=main.kernel.source_identity()
	var initial: Dictionary=main.kernel.transport().debug_export_state()
	if not check(initial.get("ok",false),"读取初始完整字节只用于测试对照"): await finish(main); return
	if not check(await begin_pending_quote(main,"village.east"),"真实A报价接纳后停在未消费边界"): await finish(main); return
	var accepted_before: int=completions.size()
	main._map_select_point([190,390])
	if not check(await settle(main),"选择B后旧读排空且B报价完成"): await finish(main); return
	check(completions.size()==accepted_before+1 and completions[-1].params.destination==[190,390],"晚到A成功没有发布，只有当前B完成")
	check(main.journey_view.selected.is_empty() and main.journey_view.selected_point==[190,390],"完成没有覆盖当前整数候选选择")
	var quote: Dictionary=main._map_quote()
	var canvas: Node=main.find_child("WorldMapCanvas",true,false)
	check(quote.get("ok",false) and canvas!=null and canvas.terrain_points==quote.get("points"),"原Canvas只绘制当前B的真实报价路径")
	check(main.kernel.source_identity()==initial_source,"取消A与读取B均不改世界身份")
	if not check(await begin_pending_quote(main,[500,430]),"实际水面拒绝报价在完成发布前被换选"): await finish(main); return
	accepted_before=completions.size()
	main._map_select("village.east")
	if not check(await settle(main),"过期拒绝排空后新目的地报价完成"): await finish(main); return
	check(completions.size()==accepted_before+1 and completions[-1].params.destination=="village.east" and completions[-1].result.get("ok",false),"晚到旧拒绝不能覆盖新成功报价")
	check(main._message.is_empty() and errors.is_empty(),"旧报价失效不冒充当前页面失败")
	main._map_select_point([500,430])
	if not check(await settle(main),"当前水面目标的真实规则拒绝完成"): await finish(main); return
	quote=main._map_quote()
	check(quote.get("code")=="InvalidDestination" and not quote.get("ok",false),"当前失败保留core原始InvalidDestination")
	var confirm: Button=button(main.page,"map_confirm")
	check("请选择可通行的地点" in labels(main.page) and confirm!=null and confirm.disabled,"原界面显示实际拒绝原因且禁止确认")
	canvas=main.find_child("WorldMapCanvas",true,false)
	check(canvas!=null and canvas.terrain_points.is_empty(),"失败目标不残留前一目的地的路径")
	main._map_select("village.east")
	if not check(await settle(main),"失败后重新选择仍可完成纯预览"): await finish(main); return
	confirm=button(main.page,"map_confirm")
	check(main._map_quote().get("ok",false) and confirm!=null and not confirm.disabled,"成功新报价恢复原确认入口")
	var final_state: Dictionary=main.kernel.transport().debug_export_state()
	check(final_state.get("ok",false) and final_state.json==initial.json and main.kernel.source_identity()==initial_source,"所有换选与失败只读，完整世界字节和时钟不变")
	print("RESIDENT_PREVIEW_INITIAL_SHA=",str(initial.json).sha256_text()," FINAL_SHA=",str(final_state.get("json","")).sha256_text())
	await finish(main)

## 清理唯一页面并报告实际执行范围，禁止把本门当作完整GUI验收。
func finish(main: Node) -> void:
	main.queue_free(); await process_frame
	print("RESIDENT_MAIN_PREVIEW_CHECKS=",checks," FAILURES=",failures," HALF_DAYS=0 GUI_PIXELS_ACCEPTED=false")
	quit(0 if failures==0 else 1)
