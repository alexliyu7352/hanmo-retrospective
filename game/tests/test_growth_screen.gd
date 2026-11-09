## 用真实作者内容和 Main 命令验证成长页；截图来自实际 Godot GUI，不创建替代世界或免费知识。
extends SceneTree
const Host=preload("res://tests/growth_host_fixture.gd")
const Interaction=preload("res://core/interaction_rules.gd")
var checks: int=0
var failures: int=0
var main
var capture_directory: String=""

## 等待窗口和根节点就绪后开始场景验收；截图库由调用方放在独立验证目录。
func _init() -> void:
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--capture="): capture_directory=arg.trim_prefix("--capture=")
	call_deferred("run_checks")

## 记录失败并保留非零退出码，避免截图成功掩盖世界命令失败。
func check(value: bool, label: String) -> void:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)

## 等待真实递延重绘和短行动完成，不直接改变世界时间或存档。
func settle() -> void:
	for _frame in range(6): await process_frame

## 按专用名称查找当前可见成长控件，避免持有上一次渲染的旧节点。
func control(label: String) -> Control:
	return main.find_child(label,true,false)

## 图形模式用鼠标点击真实按钮，无图形模式只验按钮回调；禁用控件不能被测试强行提交。
func click(label: String) -> bool:
	var button: Button=control(label)
	check(button!=null and not button.disabled,"按钮可操作："+label)
	if button==null or button.disabled: return false
	if DisplayServer.get_name()=="headless":
		button.pressed.emit(); await settle(); return true
	var parent: Node=button.get_parent()
	while parent!=null:
		if parent is ScrollContainer: parent.ensure_control_visible(button)
		parent=parent.get_parent()
	await settle()
	var point: Vector2=button.get_global_rect().get_center()
	var motion=InputEventMouseMotion.new(); motion.position=point; motion.global_position=point
	Input.parse_input_event(motion)
	var down=InputEventMouseButton.new(); down.button_index=MOUSE_BUTTON_LEFT; down.pressed=true; down.position=point; down.global_position=point
	Input.parse_input_event(down)
	await process_frame
	var up=InputEventMouseButton.new(); up.button_index=MOUSE_BUTTON_LEFT; up.position=point; up.global_position=point
	Input.parse_input_event(up)
	await settle()
	return true

## 控件选择只触发其正常信号；世界快照用于证明换卡、选择时长和预览没有暗改人物。
func select_option(label: String, index: int) -> void:
	var choice: OptionButton=control(label)
	choice.select(index); choice.item_selected.emit(index)
	await settle()

## 以真实已学卡片列表选择功法，索引来自当前作者定义的稳定 ID 排序。
func select_skill(id: String) -> void:
	var ids: Array=main.kernel.snapshot().actors["actor.player"].combat.learned.keys(); ids.sort()
	var index: int=ids.find(id)
	check(index>=0,"已学目录包含："+id)
	if index<0: return
	var cards: ItemList=control("GrowthCards")
	cards.select(index); cards.item_selected.emit(index)
	await settle()

## 准备场所和当面受教时仍调用真实 Main；权限、收费、时间和学习事实都由正式规则决定。
func perform(kind: String, params: Dictionary={}) -> void:
	var preview: Dictionary=main.kernel.preview(kind,params)
	check(preview.ok,"准备命令合法："+kind+" "+str(preview))
	if not preview.ok: return
	main._perform(kind,params)
	await settle()
	check(main._message.is_empty(),"准备命令完成："+kind+" "+main._message)

## 查找实际当前行动，空字典表示时段已完整结束。
func action() -> Dictionary:
	var state: Dictionary=main.kernel.snapshot()
	return state.actions.get(state.actors["actor.player"].action_id,{})

## 从真实场景进入空目录，再当面学习、换装、逐半日修炼和向在场学员传授。
func run_checks() -> void:
	main=Host.new(); root.add_child(main); main.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	await settle()
	check(main.start_content("scenario.alpha_life","res://data/generated/alpha_life.hmcatalog.json").ok,"真实 Alpha 作者内容从 Main 入口装载")
	if not main._started: finish(); return
	main.open_growth_fixture(); await settle()
	var state: Dictionary=main.kernel.snapshot()
	check(control("GrowthCards").item_count==0 and control("GrowthPractice")==null,"初始未学功法不发放卡片或修炼按钮")
	var weapons: OptionButton=control("GrowthWeaponChoice")
	check(weapons.item_count==2 and weapons.get_item_metadata(1)=="item.sword","只提供实际私人物品与空手选项")
	await capture("01-empty")
	await select_option("GrowthWeaponChoice",0)
	check(main.kernel.snapshot()==state,"选择装备不改写世界")
	await click("GrowthEquip")
	check(main.kernel.snapshot().actors["actor.player"].combat.weapon_id=="" and main.kernel.snapshot().inventories==state.inventories,"卸下武器保留实际行囊")
	await select_option("GrowthWeaponChoice",1); await click("GrowthEquip")
	check(main.kernel.snapshot().actors["actor.player"].combat.weapon_id=="item.sword" and main.kernel.snapshot().inventories==state.inventories,"换装使用已有实物且不复制物品")
	await click("GrowthClose")
	check(control("GrowthScreen")==null and main.kernel.snapshot().tick==state.tick,"返回实际场所不推进时钟")
	await perform("move_local",{"place_id":"west.training"})
	for skill in ["skill.cleave","skill.root","skill.parry"]:
		if skill=="skill.cleave":
			await perform("talk",{"role_id":"role.wei"})
			await perform("dialogue_next")
			await perform("dialogue_choose",{"option_id":"learn0"})
			await perform("dialogue_confirm")
		else:
			await perform("learn_skill",{"skill_id":skill,"context":Interaction.for_role(main.kernel.snapshot(),main._data,"role.wei")})
		check(not action().is_empty() and action().remaining_ticks==1,"受教先消耗一个半日并保留未完成时段")
		if skill=="skill.cleave":
			check(main.kernel.interaction_view().phase=="waiting","当面受教保留真实等待中的对白")
			await click("DialogueAdvance")
			check(main.kernel.interaction_view().phase=="say" and action().is_empty(),"对白中的继续半日解除双方占用并显示授艺结果")
			await perform("end_interaction")
		else: await perform("advance")
	check(main.kernel.snapshot().actors["actor.player"].combat.learned.size()==3 and main.kernel.snapshot().wallets["wallet.player"].balance==695,"三项知识由真实在场教师收费取得")
	main.open_growth_fixture(); await settle(); await select_skill("skill.cleave")
	check(control("GrowthCards").item_count==3,"实际已学知识组成卡片目录")
	check(control("GrowthSkillSource").text.contains(main._name(main._data.actors["actor.wei"])) and control("GrowthSkillSource").text.contains(main._name(main._data.places["west.training"])),"来源引用实际教师和学习地点事实")
	check(not control("GrowthSkillDescription").text.is_empty() and control("GrowthMastery").value==0,"卡片显示作者说明与真实初学熟练")
	check(control("GrowthTeach").disabled,"未到八成熟练不可传授")
	await capture("02-learned")
	var knowledge: Dictionary=main.kernel.snapshot().actors["actor.player"].combat.learned.duplicate(true)
	await select_skill("skill.root"); await click("GrowthHeart")
	check(main.kernel.snapshot().actors["actor.player"].combat.heart_id=="skill.root","心法按钮实际配置所学心法")
	await capture("03-heart")
	await click("GrowthHeart")
	check(main.kernel.snapshot().actors["actor.player"].combat.heart_id=="" and main.kernel.snapshot().actors["actor.player"].combat.learned==knowledge,"撤下心法不会遗忘已学知识")
	await select_skill("skill.cleave")
	state=main.kernel.snapshot()
	var ticks: SpinBox=control("GrowthPracticeTicks")
	check(ticks.min_value==1 and ticks.max_value==8 and ticks.step==1,"修炼时长允许一至八个完整半日")
	ticks.value=8; await settle()
	check(main.kernel.snapshot()==state and control("GrowthPracticeStatus").text.contains("0 → 100"),"时长改变只调用正式预览而不提前增长")
	await click("GrowthPractice")
	check(not action().is_empty() and action().remaining_ticks==7 and main.kernel.snapshot().tick==state.tick+1,"八半日修炼开始后仅自动进行首个半日")
	check(control("GrowthActionStatus").text.contains("7") and control("GrowthPractice").disabled and control("GrowthTeach").disabled and control("GrowthEquip").disabled,"未完成修炼显示剩余时段并禁用忙碌操作")
	await capture("04-practicing")
	await click("GrowthClose")
	check(control("GrowthScreen")==null and action().remaining_ticks==7,"进行中的修炼允许返回场所且不取消时段")
	main._save(); await settle()
	check(main.last_storage_result.get("ok",false),"修炼中可通过正式存储保存："+str(main.last_storage_result)+" "+main._message)
	var saved: Dictionary=main.kernel.snapshot()
	main.open_growth_fixture(); await settle(); await click("GrowthAdvance")
	check(not action().is_empty() and action().remaining_ticks==6 and main.kernel.snapshot().tick==saved.tick+1,"继续半日按钮一次仅推进一个半日")
	main._load(); await settle()
	check(main.kernel.snapshot()==saved and control("GrowthActionStatus").text.contains("7"),"正式载入恢复修炼剩余时段与卡片状态："+str(main.last_storage_result)+" "+main._message)
	check(control("GrowthNotice")!=null and control("GrowthNotice").text==main._message,"成长页保留真实保存载入反馈")
	var guard: int=0
	while not action().is_empty() and guard<8:
		var before_tick: int=main.kernel.snapshot().tick
		await click("GrowthAdvance")
		check(main.kernel.snapshot().tick==before_tick+1,"每次继续均仅推进一个半日")
		guard+=1
	check(action().is_empty() and main.kernel.snapshot().actors["actor.player"].combat.learned["skill.cleave"]==100,"完整修炼后实际熟练达到一百")
	check(control("GrowthPractice").disabled and control("GrowthPracticeStatus").text.contains("融会贯通"),"满熟练卡片解释不能重复修炼")
	await click("GrowthClose")
	await perform("move_local",{"place_id":"west.street"}); await perform("move_local",{"place_id":"west.grain"})
	main.open_growth_fixture(); await settle(); await select_skill("skill.cleave")
	var people: OptionButton=control("GrowthStudentChoice")
	var student_index: int=-1
	for index in range(people.item_count):
		var context: Dictionary=people.get_item_metadata(index)
		check(context.place_id=="west.grain" and context.provider_actor_id in ["actor.apprentice","actor.zheng"],"受教者仅取当前在场真实岗位")
		if context.role_id=="role.clerk": student_index=index
	check(student_index>=0,"在场学员可从真实岗位中选择")
	if student_index>=0:
		await select_option("GrowthStudentChoice",student_index)
		check(not control("GrowthTeach").disabled,"本人掌握后可当面传授")
		await click("GrowthTeach")
		check(not action().is_empty() and action().remaining_ticks==1,"传授占用双方实际半日")
		await click("GrowthAdvance")
		check(main.kernel.snapshot().actors["actor.apprentice"].combat.learned.get("skill.cleave",0)==10 and main.kernel.snapshot().actors["actor.player"].combat.learned["skill.cleave"]==100,"传授增加真实学员知识且本人不遗忘")
		check(control("GrowthSkillSource").text.contains(main._name(main._data.actors["actor.apprentice"])),"卡片记下真实受教者与传授地点")
	await capture("05-transmitted")
	state=main.kernel.snapshot()
	root.size=Vector2i(1024,768); await settle()
	var viewport_extent: Vector2=root.get_visible_rect().size
	check(control("GrowthScreen").get_global_rect().end.x<=viewport_extent.x+1 and control("GrowthScreen").get_global_rect().end.y<=viewport_extent.y+1,"较窄窗口保持成长主面板在视口内")
	await capture("06-compact")
	check(main.kernel.snapshot()==state,"重绘和截图不改变世界、随机流或请求序号")
	finish()

## 可选保存真实 GUI 帧；无图形模式只跑行为验证，不声称完成视觉验收。
func capture(label: String) -> void:
	if capture_directory.is_empty(): return
	if DisplayServer.get_name()=="headless": check(false,"截图要求真实图形驱动"); return
	await create_timer(.20).timeout
	await RenderingServer.frame_post_draw
	var picture: Image=root.get_texture().get_image()
	var path: String=capture_directory.path_join(label+".png")
	check(picture!=null and not picture.is_empty() and picture.save_png(path)==OK,"真实 GUI 截图："+label)
	print("CAPTURE_PATH=",path)

## 先停止真实音频并等待音频线程释放播放，再清理测试窗口；存储证据留在隔离 profile 中。
func finish() -> void:
	main._ambience.music.stop(); main._ambience.effect.stop()
	main._ambience.music.stream=null; main._ambience.effect.stream=null
	await create_timer(.10).timeout
	main.queue_free()
	await process_frame
	print("GROWTH_SCENE_CHECKS=",checks," FAILURES=",failures)
	quit(0 if failures==0 else 1)
