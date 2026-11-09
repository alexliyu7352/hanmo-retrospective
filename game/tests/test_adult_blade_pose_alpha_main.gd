## 标准Alpha真实Main控件的一次切磋结算；只验证已授权新表现，headless不冒充GUI截图。
extends SceneTree
const Art = preload("res://ui/art_catalog.gd")
const Playback = preload("res://ui/personal_battle_playback.gd")
const Binding = preload("res://ui/adult_blade_pose_binding.gd")
var main
var checks: Array = []
var failures: int = 0
var output: String = ""
var actual_clip: Dictionary = {}
var round_command_count: int = 0

## 只接收证据目录，不覆盖原剧本、目录、世界或用户银行。
func _init() -> void:
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--output="): output = arg.trim_prefix("--output=")
	call_deferred("run")

## 失败具名且退出非零；原入口不可达时立即停止，不另造世界成功。
func check(value: bool, label: String) -> bool:
	checks.append({"ok":value,"label":label})
	if not value: failures += 1; printerr("FAIL: ",label)
	return value

## 等生产节点异步短行动，不直接推进内核tick。
func settle() -> void:
	for _frame in 12: await process_frame

## 只寻找当前树上可见且有生产语义元数据的按钮。
func button(kind: String, target: String="", parent: Node=null) -> Button:
	if parent == null: parent = main
	if parent is Button and parent.get_meta("command_kind","") == kind and (target.is_empty() or parent.get_meta("target_id","") == target) and parent.is_visible_in_tree(): return parent
	for child in parent.get_children():
		var found: Button = button(kind,target,child)
		if found != null: return found
	return null

## 信号经过同一产品回调；不把内核状态注入产品或调用隐蔽捷径。
func click(kind: String, target: String="") -> bool:
	var found: Button = button(kind,target)
	if not check(found != null and not found.disabled,"生产控件可用："+kind+" "+target): return false
	found.pressed.emit()
	await settle()
	return true

## 证据仅落本验证目录，不触碰任何已有bank、保存槽或正式存档。
func write(name: String, value: Variant) -> void:
	FileAccess.open(output.path_join(name),FileAccess.WRITE).store_string(JSON.stringify(value,"\t",true,true)+"\n")

## 标准标题、真实岗位对白和一次原战术提交后，继续只读原回放。
func run() -> void:
	DirAccess.make_dir_recursive_absolute(output)
	if not check(DisplayServer.get_name() == "headless","明确headless控件证据，未启动GUI"): finish(); return
	check(ProjectSettings.get_setting("application/config/default_scenario","") == "scenario.alpha_life","仍为标准Alpha默认剧本")
	main = load("res://main.tscn").instantiate(); root.add_child(main)
	await settle()
	if not await click("start") or not await click("scenario_select","scenario.alpha_life"): finish(); return
	if not check(main._started and not main._resident_mode,"标准标题实际进入Alpha原legacy Kernel"): finish(); return
	if not check(main.kernel.content().content_digest == Binding.CATALOG_SHA,"实际内容字节属于唯一Alpha范围"): finish(); return
	if not await click("move_local","west.training") or not await click("talk","role.wei") or not await click("dialogue_next") or not await click("dialogue_choose","spar") or not await click("dialogue_confirm"): finish(); return
	var screen = main.page.find_child("PersonalBattleScreen",true,false)
	if not check(screen != null and screen.is_inside_tree(),"当面切磋真实进入原PersonalBattleScreen"): finish(); return
	var page: Dictionary = screen.page_input.duplicate(true)
	write("actual-planning-page.json",page)
	if not check(page.actor_id == "actor.player" and page.character_inputs["actor.player"].actor_view.age_years == 42 and page.battle.units["actor.player"].weapon_id == "item.sword","实际42岁actor.player持本人公开环首刀"): finish(); return
	var canvas = screen.canvas
	var body: Dictionary = canvas.battle_descriptors["actor.player"]
	var identity: Dictionary = canvas.identity_descriptors["actor.player"]
	check(body.has("battle_pose_binding") and canvas._blade_pose_consumers.has("actor.player"),"真实Screen入口命中新显式身体")
	check(identity.identity_key == body.identity_key and identity.actor_id == body.actor_id and identity.layers == body.layers and identity.display_name == body.display_name,"真实页头像、姓名、ID、identity_key保持同人")
	check(not canvas._blade_pose_consumers.has("actor.wei") and not page.battle.units["actor.wei"].has("weapon_id"),"未知敌装备与其他身份保留原身体")
	write("actual-descriptors.json",{"body":body,"identity":identity})
	canvas.destination_chosen.emit([350,360])
	await settle()
	var submit: Button = button("battle_submit",page.battle.id)
	if not check(submit != null and not submit.disabled,"原预览允许这一轮正常前进出手"): finish(); return
	var sequence_before: int = main.kernel.snapshot().request_sequences["actor.player"]
	submit.pressed.emit()
	screen = main.page.find_child("PersonalBattleScreen",true,false)
	var pause: Button = button("battle_playback_pause",page.battle.id)
	if pause != null and not pause.disabled: pause.pressed.emit()
	await settle()
	screen = main.page.find_child("PersonalBattleScreen",true,false)
	if not check(screen != null and screen.page_input.revision > page.revision,"原命令完成一次真实轮次"): finish(); return
	round_command_count = main.kernel.snapshot().request_sequences["actor.player"] - sequence_before
	check(round_command_count == 1 and screen.page_input.revision == page.revision + 1,"只提交一条原战术命令，未隐式多结算")
	actual_clip = screen._playback_clip.duplicate(true)
	write("actual-settled-page.json",screen.page_input)
	write("actual-playback-clip.json",actual_clip)
	if not check(actual_clip.get("playable",false) and actual_clip.actors.has("actor.player"),"真实前后公开页生成原Playback片段"): finish(); return
	var state_bytes: String = JSON.stringify(main.kernel.snapshot(),"",true,true)
	var frame: Dictionary = screen.playback_frame()
	check(frame.active and frame.paused,"同一真实页及时暂停原回放")
	var before: float = frame.elapsed
	await settle()
	check(screen.playback_frame().elapsed == before and JSON.stringify(main.kernel.snapshot(),"",true,true) == state_bytes,"暂停保持原时钟与整世界字节")
	if not await click("battle_playback_speed",page.battle.id): finish(); return
	check(screen.playback_frame().speed == 2.0 and JSON.stringify(main.kernel.snapshot(),"",true,true) == state_bytes,"原倍速控件只改表现时钟")
	capture_actual(screen)
	if not await click("battle_playback_skip",page.battle.id): finish(); return
	check(screen.playback_frame().complete and JSON.stringify(main.kernel.snapshot(),"",true,true) == state_bytes,"原跳过回终态，世界、RNG、序号全字节不变")
	write("actual-read-only-witness.json",{"world_sha256":state_bytes.sha256_text(),"after_playback_world_sha256":JSON.stringify(main.kernel.snapshot(),"",true,true).sha256_text(),"frame":screen.playback_frame(),"command_count_after_entry":round_command_count,"evidence":"HEADLESS_REAL_MAIN_CONTROLS_NO_GUI"})
	finish()

## 复播原clip同一原Player类的阶段中心，仅CPU取样真实完整像素，不重造事件或规则结果。
func capture_actual(screen: Control) -> void:
	var consumer = screen.canvas._blade_pose_consumers.get("actor.player")
	if not check(consumer != null,"实际画布保留同一个已校验消费叶"): return
	var elapsed: float = 0.0
	var rows: Array = []
	for phase in actual_clip.phases:
		var clock = Playback.new(); clock.configure(actual_clip)
		clock.advance(elapsed + phase.duration * 0.5)
		var frame: Dictionary = clock.frame()
		var packet: Dictionary = consumer.packet("actor.player",screen.battle.units["actor.player"],actual_clip.actors["actor.player"],frame,112)
		if check(packet.ok,"原实际片段有限取样："+phase.kind):
			consumer.image_copy(packet).save_png(output.path_join("actual-"+phase.kind+"-112.png"))
			rows.append({"phase":phase.kind,"pose":packet.pose_key,"body_position":[packet.body_position.x,packet.body_position.y],"labels":packet.labels,"damage":packet.damage,"alpha":packet.alpha})
		elapsed += phase.duration
	write("actual-pose-selection.json",rows)

## 报告明确区分真实Main控件/实际结算与仍未进行的GUI画面验收。
func finish() -> void:
	write("result.json",{"format":"HM-ADULT-BLADE-ALPHA-MAIN/1","ok":failures==0,"checks":checks,"failures":failures,"evidence":"HEADLESS_REAL_MAIN_CONTROLS_NO_GUI","gui_verified":false,"source_clip_is_fixture":false})
	print("ADULT_BLADE_ALPHA_MAIN_CHECKS=",checks.size()," FAILURES=",failures," GUI=NOT_RUN")
	if is_instance_valid(main): main.queue_free()
	await process_frame
	quit(0 if failures == 0 else 1)
