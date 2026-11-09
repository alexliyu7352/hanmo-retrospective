## 真实Main入口的有界回归：草案取消/晚信号、模板接线、完整请求与离场失效，不改生产清单。
extends SceneTree
const Main=preload("res://ui/main.gd")
const Kernel=preload("res://core/world_kernel.gd")
const Art=preload("res://ui/art_catalog.gd")
const Strict=preload("res://core/strict_json.gd")
const F=preload("res://tests/companion_fixture.gd")

## 保留真实核心行为，只记录边界参数来核对Main没有替换画面版本或丢失目标。
class RecordingKernel extends Kernel:
	var submitted: Array=[]
	var previewed: Array=[]
	## 实际执行同一核心提交，输入记录独立深复制。
	func submit(command: Dictionary) -> Dictionary:
		submitted.append(command.duplicate(true))
		return super.submit(command)
	## 实际执行同一核心预览，不伪造权限、报价或版本。
	func preview(kind: String, params: Dictionary={}, actor_id: String="") -> Dictionary:
		previewed.append({"kind":kind,"params":params.duplicate(true),"actor_id":actor_id})
		return super.preview(kind,params,actor_id)

var main: Control
var checks: int=0
var failures: int=0
var original_manifest: Dictionary
var original_settings: Dictionary={}
var catalog_path: String
var fixed_path: String
var folder: String
var features: Dictionary={"face_shape":"round","eyes":"calm","brows":"straight","nose":"plain","mouth":"plain","hair":"short","beard":"none","skin_palette":"warm","build":"slender"}

## 资源系统可用后执行，避免导入时测试副作用。
func _init() -> void:
	call_deferred("_run")

## 每项失败影响退出码，并保留可定位标签。
func check(value: bool, label: String) -> void:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)

## 等待布局、短行动和旧树释放完成，不长时间推进游戏。
func settle() -> void:
	for frame in range(5): await process_frame

## 用语义标记查当前真实控件，禁止靠静态屏幕坐标猜按钮。
func find_button(key: String, value: String, node: Node=null) -> Button:
	if node==null: node=main
	if node is Button and node.get_meta(key,"")==value: return node
	for child in node.get_children():
		var found: Button=find_button(key,value,child)
		if found!=null: return found
	return null

## 通过按钮所属Viewport派发鼠标输入，窗口与主页面使用各自实际坐标。
func click(target: Button) -> void:
	check(target!=null and target.is_visible_in_tree() and not target.disabled,"真实输入目标可见且可用")
	if target==null or target.disabled: return
	var viewport: Viewport=target.get_viewport()
	var point: Vector2=target.get_global_rect().get_center()
	for pressed in [true,false]:
		var event=InputEventMouseButton.new(); event.button_index=MOUSE_BUTTON_LEFT
		event.position=point; event.global_position=point; event.pressed=pressed
		if viewport is Window and viewport!=root:
			event.position+=Vector2(viewport.position); event.global_position=event.position
			root.push_input(event,true)
		else: viewport.push_input(event,true)
		await process_frame
	await settle()

## 生成本测试专用完整目录，仍经正式摘要、Schema与Kernel校验。
func write_catalog(index: Dictionary, path: String) -> void:
	index.catalog_sha256=(JSON.stringify(index.catalog,"",true,true)+"\n").sha256_text()
	var file=FileAccess.open(path,FileAccess.WRITE); file.store_string(JSON.stringify(index,"",true,true)); file.close()

## 只在进程registry装载既有技术小样；作者文件、生产清单与正式存档保持原样。
func prepare() -> void:
	original_manifest=Art.catalog().describe()
	var fixture_root: String=ProjectSettings.globalize_path("res://").path_join("../tests/fixtures/")
	var fixture: Dictionary=Strict.parse_text(FileAccess.get_file_as_string(fixture_root+"character_presentation/manifest.json")).value
	var scene_fixture: Dictionary=Strict.parse_text(FileAccess.get_file_as_string(fixture_root+"presentation/layered-v2.json")).value
	var candidate: Dictionary=original_manifest.duplicate(true); candidate.version=2
	for group in ["backgrounds","portraits","components","character_profiles","scene_templates"]:
		if not candidate.has(group): candidate[group]={}
		candidate[group].merge(fixture.get(group,{}),true)
	candidate.backgrounds.merge(scene_fixture.backgrounds,true)
	for kind in ["city","interior"]:
		var scene: Dictionary=scene_fixture.scene_templates["fixture.room"].duplicate(true)
		scene.id="entry.scene."+kind; scene.view_kind=kind; scene.layers=scene.layers.slice(0,1); scene.ui_safe_areas=[]
		var destination: String="west.grain" if kind=="city" else "west.street"
		var door: Dictionary=scene.layers[0].duplicate(true)
		door.id="door"; door.kind="building"; door.target_ref={"kind":"place","id":destination}; door.after=["back"]
		scene.layers.append(door)
		scene.layers.append({"id":"door.hit","kind":"hotspot","rect":[45,35,15,15],"target_layer_id":"door","after":["door"],"candidate_selector":{"command_kind":"move_local","params":{}}})
		candidate.scene_templates[scene.id]=scene
	check(Art.catalog().configure(candidate).ok,"测试人物/场景小样通过同一registry")
	folder=OS.get_user_data_dir().path_join("visual-entry-"+str(Time.get_ticks_usec()))
	DirAccess.make_dir_recursive_absolute(folder)
	catalog_path=folder.path_join("generated.hmcatalog.json"); fixed_path=folder.path_join("fixed.hmcatalog.json")
	var index: Dictionary=Strict.parse_text(FileAccess.get_file_as_string("res://data/generated/alpha_life.hmcatalog.json")).value
	var definition: Dictionary={"kind":"generated","generator_version":"appearance-v1","attire_style":"plain","candidates":{}}
	for feature in features: definition.candidates[feature]=[features[feature]]
	definition.candidates.face_shape.append("angular"); definition.candidates.hair.append("bun")
	index.catalog.actors["actor.player"].appearance_definition=definition
	index.catalog.actors["actor.player"].presentation_profile_id="fixture.person"
	write_catalog(index,catalog_path)
	index.catalog.actors["actor.player"].appearance_definition={"kind":"fixed","generator_version":"appearance-v1","attire_style":"plain","base_features":features.duplicate(true)}
	index.catalog.actors["actor.player"].origin="historical"
	write_catalog(index,fixed_path)
	for setting in ["default_catalog","default_scenario","storage_profile"]:
		var key: String="application/config/"+setting; original_settings[key]=ProjectSettings.get_setting(key)
	ProjectSettings.set_setting("application/config/default_scenario","scenario.alpha_life")
	ProjectSettings.set_setting("application/config/default_catalog",catalog_path)
	ProjectSettings.set_setting("application/config/storage_profile","visual-entry-"+str(Time.get_ticks_usec()))

## 标题真实启程、取消、关闭及故意迟到信号均不得创建或替换已有世界。
func check_picker() -> void:
	var before: Dictionary=main.kernel.snapshot()
	await click(find_button("command_kind","start"))
	var picker: Control=main._appearance_picker
	check(picker!=null and main.kernel.snapshot()==before and not main._started,"标题选件先出现，未实例化世界")
	if picker==null: return
	await click(find_button("appearance_action","cancel",picker))
	check(main._appearance_picker==null and main.kernel.snapshot()==before,"鼠标取消只撤销草案")
	check(main._begin_appearance("scenario.alpha_life",catalog_path).ok,"可以再次打开草案")
	picker=main._appearance_picker
	var draft: Dictionary=picker.draft()
	picker.cancel_selection(); picker.confirmed.emit(draft)
	check(main.kernel.snapshot()==before and not main._started,"取消后的同帧晚confirmed不能创建世界")
	await settle()
	main._begin_appearance("scenario.alpha_life",catalog_path)
	picker=main._appearance_picker
	main._appearance_dialog.close_requested.emit(); picker.confirmed.emit(draft)
	check(main.kernel.snapshot()==before and main._appearance_picker==null,"关闭窗口后晚confirmed不创建世界")
	await settle()
	main._begin_appearance("scenario.alpha_life",catalog_path)
	var stale_picker: Control=main._appearance_picker
	main._begin_appearance("scenario.alpha_life",catalog_path)
	picker=main._appearance_picker
	stale_picker.confirmed.emit(draft)
	check(main._appearance_picker==picker and main.kernel.snapshot()==before,"重开后旧代次confirmed不能确认新窗口")
	draft=picker.draft()
	await settle()
	await click(find_button("appearance_action","confirm",picker))
	check(main._started and main._appearance_picker==null,"标题草案鼠标确认进入真实新局一次")
	check(main.start_content("scenario.alpha_life",catalog_path,draft).ok,"Main第三参数进入真实新局")
	check(main.kernel.snapshot().actors["actor.player"].appearance.base_features==draft.base_features and main.kernel.snapshot().actors["actor.player"].appearance.source=="custom","实际保存完整确认选择与custom来源")
	before=main.kernel.snapshot()
	var bad: Dictionary=draft.duplicate(true); bad.attire_style="formal"
	check(not main.start_content("scenario.alpha_life",catalog_path,bad).ok and main.kernel.snapshot()==before,"现有第二衣服素材不放宽作者衣着合同")
	main._started=false; main._render(); await settle()
	main._begin_appearance("scenario.alpha_life",catalog_path); picker=main._appearance_picker
	picker.cancel_selection(); picker.confirmed.emit(draft)
	check(main.kernel.snapshot()==before and not main._started,"标题保留旧world时取消也不替换world")
	await settle()
	main._begin_appearance("scenario.alpha_life",fixed_path); picker=main._appearance_picker
	check(picker!=null,"历史固定人物进入只读预览")
	if picker!=null:
		for option in picker.find_children("*","OptionButton",true,false): check(option.disabled,"历史固定选项不可编辑")
		picker.confirm_selection()
		check(main._started and main.kernel.snapshot().actors["actor.player"].appearance.source=="fixed","固定预览确认不把历史人物改成custom")
	main._started=false; main._render(); await settle()
	main._continue(); await settle()
	check(main._appearance_picker==null and main.last_storage_result.get("code")=="NotFound" and not main._started,"继续旅途绕过picker，空测试槽仍回标题")

## 返回当前Host的完整绑定请求，唯一候选与代次来自实际控件自身。
func request_for(host: Control, kind: String, target: String="") -> Dictionary:
	for candidate in host.describe_projection().hotspot_candidates:
		if candidate.command_kind==kind and (target.is_empty() or candidate.target_ref.id==target):
			return host._request(candidate)
	return {}

## 真实Main加载场景映射，保留旧无模板路径，并覆盖新旧按钮与版本化事务边界。
func check_scenes() -> void:
	check(main.start_content("scenario.alpha_life","res://data/generated/alpha_life.hmcatalog.json").ok,"旧两参入口保持可用")
	await settle()
	check(main.find_child("SceneHost",true,false)==null,"旧无模板内容不挂新Host")
	main._data.places["west.street"].scene_template_ids={"city":"entry.scene.city"}
	main._data.places["west.grain"].scene_template_ids={"interior":"entry.scene.interior"}
	var before: Dictionary=main.kernel.snapshot()
	main._render(); await settle()
	var host: Control=main.find_child("SceneHost",true,false)
	check(host!=null and host.get_meta("render_result",{}).get("mode")=="layered","真实城市场景由Composer挂分层Host")
	if host==null: return
	check(main.kernel.snapshot()==before,"挂载分层、人物与名单完全只读")
	var candidate_count: int=host.describe_projection().hotspot_candidates.size()
	var actual: Array=[]; collect_scene_buttons(main,actual)
	check(actual.size()==candidate_count,"旧通路/人物/物件列表不再重复同一候选")
	var request: Dictionary=request_for(host,"move_local","west.grain")
	check(not request.is_empty(),"场景入口保留真实通路候选")
	if request.is_empty(): return
	var altered: Dictionary=request.duplicate(true); altered.target_ref.id="west.estate"
	check(not main._scene_request(altered,host).ok and main.kernel.snapshot()==before,"更换目标引用不能与原参数混用")
	altered=request.duplicate(true); altered.actor_id="actor.chen"
	check(not main._scene_request(altered,host).ok and main.kernel.snapshot()==before,"请求不能替换观察者为NPC")
	main.kernel.submitted.clear(); main.kernel.previewed.clear()
	var accepted: Dictionary=main._scene_request(request,host)
	check(accepted.ok,"完整候选经真实Main复验并提交")
	var command: Dictionary=main.kernel.submitted[-1].duplicate(true); command.erase("request_seq")
	check(command==request,"submit原值保留actor/revision/target/epoch及完整params")
	check(main.kernel.previewed[-1]=={"kind":request.kind,"params":request.params,"actor_id":request.actor_id},"preview原值保留命令/参数/actor")
	var after: Dictionary=main.kernel.snapshot()
	check(not main._scene_request(request,host).ok and main.kernel.snapshot()==after,"成功后旧Host重复信号不再次提交")
	await settle()
	host=main.find_child("SceneHost",true,false)
	check(host!=null and host.describe().view_kind=="interior","进入真实商铺后挂内景模板")
	if host==null: return
	request=request_for(host,"talk")
	check(not request.is_empty(),"内景人物交谈与名单来自同一候选")
	if request.is_empty(): return
	var npc: String=request.target_ref.id
	check(F.send(main.kernel,"move_local",{"place_id":"west.street"},npc).ok,"同一Kernel真实执行人物离场")
	after=main.kernel.snapshot()
	check(main._scene_request(request,host).get("code")=="StalePreview" and main.kernel.snapshot()==after,"旧画面revision不能沿最新版本提交离场人物交谈")
	altered=request.duplicate(true); altered.expected_revision=after.revision
	check(not main._scene_request(altered,host).ok and main.kernel.snapshot()==after,"把旧请求revision换新也不能越过Host绑定")
	main._render(); await settle()
	host=main.find_child("SceneHost",true,false)
	check(request_for(host,"talk",npc).is_empty(),"新画面不再列离场人物候选")
	request=request_for(host,"move_local","west.street")
	main._open_map()
	check(not main._scene_request(request,host).ok and main.kernel.snapshot()==after,"切换地图后旧场景请求不能生效")
	await settle(); main._close_map(); await settle()
	host=main.find_child("SceneHost",true,false)
	check(not main._scene_request(request,host).ok and main.kernel.snapshot()==after,"返回同地点同revision仍拒绝旧epoch")
	main._data.places["west.grain"].scene_template_ids.interior="missing.scene"
	main._render(); await settle()
	host=main.find_child("SceneHost",true,false)
	check(host!=null and host.get_meta("render_result",{}).get("mode")=="error" and main.find_child("SceneRenderError",true,false)!=null,"明确坏模板显示错误，不静默显示旧图")
	check(main.find_child("ScenePersonPortrait",true,false)==null,"失败列表不叠加旧人物入口")
	var path_button: Button=find_button("command_kind","move_local",host)
	await click(path_button)
	check(main.kernel.snapshot().actors["actor.player"].location.place_id=="west.street","显错场景合法名单鼠标点击仍经Main完整请求实际返回")

## 统计仅属于Host四类动作的按钮，工具栏和地图入口不计入候选重复数。
func collect_scene_buttons(node: Node, output: Array) -> void:
	if node is Button and node.get_meta("command_kind","") in ["move_local","talk","inspect","estate_build"]: output.append(node)
	for child in node.get_children(): collect_scene_buttons(child,output)

## 汇总本切片证据，清理本测试文件并恢复进程设置；不声称全量或完整视觉矩阵通过。
func _run() -> void:
	root.size=Vector2i(1280,800); prepare()
	main=Main.new(); main.kernel=RecordingKernel.new(); root.add_child(main)
	main.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT); await settle()
	await check_picker()
	await check_scenes()
	main.queue_free(); await settle()
	for key in original_settings: ProjectSettings.set_setting(key,original_settings[key])
	Art.catalog().configure(original_manifest)
	for path in [catalog_path,fixed_path]: DirAccess.remove_absolute(path)
	DirAccess.remove_absolute(folder)
	print("VISUAL_ENTRYPOINTS_CHECKS=",checks," FAILURES=",failures)
	quit(0 if failures==0 else 1)
