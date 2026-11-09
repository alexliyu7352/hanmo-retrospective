## 隔离小工程内验证真实同Kernel读档边界；仅检查输入节点，不替代图形、网页或全量验收。
extends SceneTree
const Main=preload("res://ui/main.gd")
const Kernel=preload("res://core/world_kernel.gd")
const Catalog=preload("res://core/content_catalog.gd")
const Codec=preload("res://core/save_codec.gd")
const SaveFiles=preload("res://storage/save_files.gd")
const Screen=preload("res://ui/personal_battle_screen.gd")
const Canvas=preload("res://ui/personal_battle_canvas.gd")
const BattleView=preload("res://core/views/personal_battle_view.gd")
const F=preload("res://tests/companion_fixture.gd")

## 仅在指定反例中破坏已解码候选，再委托真实Kernel终验，绝不伪造成功或失败返回。
class CheckedKernel extends Kernel:
	var reject_candidate: bool=false
	var restore_calls: int=0
	## 正常路径原样恢复；晚期反例只改候选副本，失败后仍由原实现保留活世界。
	func restore_with_catalog(saved: Dictionary, catalog: Dictionary, registry: Variant=null) -> Dictionary:
		restore_calls+=1
		if reject_candidate:
			saved=saved.duplicate(true); saved.revision=-1
		return super.restore_with_catalog(saved,catalog,registry)

## 保留Main全部读档/导入实现，只隔离画面构建并记录个人命令出口。
class RestoreHost extends Main:
	var commands: Array=[]
	## 安装真实异步StorageService，不加载声音或图形资源。
	func _ready() -> void:
		add_child(storage); storage.completed.connect(_stored)
	## 只推进原页面票据；重绘不改变读档代数或任何世界状态。
	func _render() -> void:
		_scene_counter+=1
	## 观察旧页是否越过正式命令边界；本测试不让错误回调改变后续反例。
	func _perform(kind: String, params: Dictionary={}) -> void:
		commands.append({"kind":kind,"params":params.duplicate(true)})

var checks: int=0
var failures: int=0
var host: RestoreHost
var views: Array=[]

## 等场景树就绪后才创建真实存储节点和本次独立存档目录。
func _init() -> void:
	call_deferred("run")

## 所有断言保留计数与明确失败，最终退出码不能掩盖失败。
func check(value: bool, label: String) -> bool:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)
	return value

## 复用生产草案、预览与按钮回调；不走图片装配或另造个人战规则。
func view_for():
	var view=Screen.new(); view.host=host; view.configure_view(BattleView.query(host.kernel.snapshot(),host._data),host._data)
	view.canvas=Canvas.new(); view.add_child(view.canvas); view.canvas.battle=view.battle; view.canvas.data=view.data
	view._submit=Button.new(); view.add_child(view._submit)
	view._restore_draft(); view._refresh(); views.append(view)
	return view

## 编码整份真实世界，包含RNG；相同封装参数使字节相等可证明UI没有偷偷推进或写入草案。
func save_text() -> String:
	var context: Dictionary=host.kernel.save_context()
	return Codec.encode(context.state,1,"manual-1",context.catalog,context.registry)

## 只读取本次隔离银行的原字节摘要，不修改玩家目录或以生成数量代替原档一致性。
func bank_hashes() -> Dictionary:
	var result: Dictionary={}
	for name in DirAccess.get_files_at(host.storage.native.directory):
		result[name]=FileAccess.get_sha256(host.storage.native.directory.path_join(name))
	return result

## 有界等待真实StorageService的deferred完成；超时单独失败，不手工模拟完成信号。
func settle() -> void:
	for unused in range(30):
		if host._storage_intent.is_empty() and not host.storage._busy: return
		await process_frame
	check(false,"native slot restore completes within 30 frames")

## 同身份成功恢复、早晚失败、重绘与旧回调集中覆盖；固定成功次数以免隐含重复恢复。
func run() -> void:
	if not check(ProjectSettings.get_setting("application/config/personal_load_isolated",false),"requires explicitly prepared personal-load isolation"):
		finish(); return
	var installed: Dictionary=Catalog.installed_catalogs()
	if not check(installed.get("ok",false) and installed.get("catalogs",[]).size()==1,"prepared tiny presentation and installed catalog validate"):
		printerr(installed); finish(); return
	var catalog: Dictionary=installed.catalogs[0]
	ProjectSettings.set_setting("application/config/storage_profile","personal-load-"+str(OS.get_process_id())+"-"+str(Time.get_ticks_usec()))
	host=RestoreHost.new(); host.kernel=CheckedKernel.new(); root.add_child(host)
	var started: Dictionary=host.kernel.new_game("draft::scenario.alpha_life",catalog.catalog_path)
	if not check(started.ok,"real registered Kernel starts composed alpha source"):
		printerr(started); finish(); return
	check(F.send(host.kernel,"move_local",{"place_id":"draft::west.training"}).ok,"real actor reaches training place")
	var entered: Dictionary=F.send(host.kernel,"duel_start",{"context":F.context(host.kernel,"draft::role.wei")})
	if not check(entered.ok,"real command creates personal battle"):
		printerr(entered); finish(); return
	host._data=host.kernel.content().data; host._started=true
	var context: Dictionary=host.kernel.save_context(); host.storage.configure(context.catalog,context.registry)
	var before: Dictionary=host.kernel.snapshot(); var kernel_id: int=host.kernel.get_instance_id()
	var original_text: String=save_text()
	check(not original_text.is_empty() and host.storage.native.put("manual-1",original_text,0).ok,"real native bank saves unchanged battle")
	var hashes: Dictionary=bank_hashes()
	var path: String="user://personal-load-"+str(OS.get_process_id())+".hmsave"
	check(SaveFiles.write_world(context.state,path,context.catalog,context.registry).ok,"real bounded export creates import file")
	var raw: Dictionary=SaveFiles.read_text(path); var file_hash: String=FileAccess.get_sha256(path)
	if not check(raw.ok,"native read returns actual saved bytes"): finish(); return
	var view=view_for(); var initial_heading: int=view.draft.heading
	view._choose_heading(posmod(initial_heading+90,360)); var chosen: Dictionary=view.draft.duplicate(true)
	check(chosen.heading!=initial_heading and host.kernel.snapshot()==before and save_text()==original_text,"unsubmitted UI choice leaves all world/RNG/save bytes unchanged")
	check(int(host.get_meta("personal_battle_generation",0))==0,"existing host without metadata starts generation zero")
	host._render(); var redraw=view_for()
	check(redraw.draft==chosen and int(host.get_meta("personal_battle_generation",0))==0,"same-world redraw keeps valid draft and generation")
	var current_meta: Dictionary=host.get_meta("personal_battle_draft").duplicate(true)
	view._submit_plan()
	check(host.commands.is_empty() and host.get_meta("personal_battle_draft")==current_meta,"old redraw callback cannot submit or erase newer view draft")
	await process_frame
	redraw=view_for(); current_meta=host.get_meta("personal_battle_draft").duplicate(true)
	var failed: Dictionary=host._restore_text("invalid save")
	check(not failed.ok and host.kernel.restore_calls==0,"malformed save fails before Kernel adoption")
	check(host.get_meta("personal_battle_draft")==current_meta and redraw._matches_live_context() and int(host.get_meta("personal_battle_generation",0))==0,"early failure keeps live draft and generation")
	host.kernel.reject_candidate=true
	failed=host._restore_text(raw.text)
	check(not failed.ok and host.kernel.restore_calls==1,"decoded save reaches actual Kernel final validation and is rejected")
	check(host.kernel.snapshot()==before and save_text()==original_text and host.get_meta("personal_battle_draft")==current_meta and redraw._matches_live_context() and int(host.get_meta("personal_battle_generation",0))==0,"late Kernel failure preserves all live state, draft and generation")
	host.kernel.reject_candidate=false
	var restored: Dictionary=host._restore_text(raw.text)
	check(restored.ok and host.kernel.get_instance_id()==kernel_id and host.kernel.snapshot()==before,"real successful restore retains same Kernel and equal battle/revision identity")
	check(int(host.get_meta("personal_battle_generation",0))==1,"successful world adoption increments host generation exactly once")
	check(not redraw._matches_live_context(),"successful restore invalidates old input before any redraw")
	var fresh=view_for()
	check(fresh.draft.heading==initial_heading and fresh.draft!=chosen,"same-identity successful restore cannot adopt pre-load draft")
	fresh._choose_heading(posmod(initial_heading+180,360)); current_meta=host.get_meta("personal_battle_draft").duplicate(true)
	redraw._submit_plan()
	check(host.commands.is_empty() and host.get_meta("personal_battle_draft")==current_meta,"old load callback cannot submit or erase new-generation draft even before scene epoch changes")
	await process_frame
	restored=host._restore_text(raw.text)
	check(restored.ok and int(host.get_meta("personal_battle_generation",0))==2,"repeated equal successful restore advances exactly one generation per adoption")
	view=view_for(); check(view.draft.heading==initial_heading,"second equal load also starts from authoritative heading")
	view._choose_heading(posmod(initial_heading+90,360)); chosen=view.draft.duplicate(true)
	failed=host.import_snapshot(path+".missing")
	check(not failed.ok and int(host.get_meta("personal_battle_generation",0))==2,"failed file import and its redraw do not advance generation")
	view=view_for(); check(view.draft==chosen,"failed file import preserves draft across actual Main error redraw")
	restored=host.import_snapshot(path)
	check(restored.ok and int(host.get_meta("personal_battle_generation",0))==3,"actual native file import reaches shared adoption boundary once")
	view=view_for(); check(view.draft.heading==initial_heading,"actual file import does not recover pre-import draft")
	view._choose_heading(posmod(initial_heading+90,360))
	host._pending_slot={"mode":"load","slot":"manual-1","generation":1,"new":false,"epoch":host._slot_epoch,"identity":host._slot_world_identity()}
	var request: Dictionary=host._request_storage("load_check","inspect",{"slot":"manual-1"})
	check(request.ok,"real native slot inspection request accepted")
	await settle()
	check(host.last_storage_result.ok and int(host.get_meta("personal_battle_generation",0))==4,"actual deferred slot read and Main checked-load adopt once")
	view=view_for(); check(view.draft.heading==initial_heading,"native slot load discards pre-load draft with equal revision")
	check(host.kernel.restore_calls==5,"one rejected candidate plus four successful adoptions, without duplicate restore")
	check(host.kernel.snapshot()==before and save_text()==original_text and bank_hashes()==hashes and FileAccess.get_sha256(path)==file_hash,"all UI cleanup preserves complete world/RNG, serialized save and native source bytes")
	await process_frame
	finish()

## 释放未挂到主画面的节点，测试产生的文件只留本次隔离profile用于诊断。
func finish() -> void:
	for view in views: view.free()
	if is_instance_valid(host):
		host.journey_view.free(); host._ui_font_scale.free()
		host._ambience.music.free(); host._ambience.effect.free(); host._ambience.free(); host.free()
	print("PERSONAL_LOAD_DRAFT checks=",checks," failures=",failures)
	quit(1 if failures else 0)
