## 真实flat旧入口只做存档列表兼容分类；候选可用不等于已完成decode、写入或CAS确认。
extends SceneTree
const Host=preload("res://ui/main.gd")
const Kernel=preload("res://core/world_kernel.gd")
const Catalog=preload("res://core/content_catalog.gd")
const Codec=preload("res://core/save_codec.gd")
var checks: int=0
var failures: int=0
var host

## 引擎就绪后运行纯分类，不建立UI窗口或触发存储请求。
func _init() -> void:
	call_deferred("run")

## 断言失败导致非零退出，诊断只由调用者留在本地。
func check(ok: bool, label: String) -> void:
	checks+=1
	if not ok: failures+=1; printerr("FAIL: ",label)

## 从真实Kernel保存上下文编码并物理inspect，不手造可信目录、锁或元信息。
func row_for(kernel) -> Dictionary:
	var context: Dictionary=kernel.save_context()
	var text: String=Codec.encode(context.state,1,"manual-1",context.catalog,context.registry)
	check(not text.is_empty(),"真实Kernel完整状态通过正式Codec编码")
	return Codec.inspect_metadata(text)

## 对P六canonical索引与真实flat采购世界验证保存候选，另身份与无锁当前世界不能放宽v2。
func run() -> void:
	if not ProjectSettings.get_setting("application/config/restore_menu_isolated",false):
		printerr("FAIL: requires isolated six-canonical fixture"); quit(1); return
	var installed: Dictionary=Catalog.installed_catalogs()
	check(installed.get("ok",false) and installed.get("catalogs",[]).size()==6,"P六canonical安装产物由真实索引完整校验")
	if not installed.get("ok",false) or installed.get("catalogs",[]).size()!=6: finish(); return
	var flat_path: String="res://data/generated/procurement.hmcatalog.json"
	var flat: Dictionary=Catalog.read_prepared(flat_path)
	check(flat.get("ok",false) and not flat.has("pack_set") and not flat.has("pack_set_digest"),"真实flat采购目录保留旧入口无sidecar身份")
	if not flat.get("ok",false): printerr(flat); finish(); return
	var kernel=Kernel.new()
	var started: Dictionary=kernel.new_game(flat.data.scenarios.keys()[0],flat_path)
	check(started.get("ok",false),"真实Kernel从flat采购入口新局，不手造state")
	if not started.get("ok",false): printerr(started); finish(); return
	var context: Dictionary=kernel.save_context()
	var current: Dictionary=context.state.duplicate(true)
	var row: Dictionary=row_for(kernel)
	check(row.get("ok",false) and row.get("metadata",{}).get("format_version")==1,"无锁旧入口真实生成v1并通过物理inspect")
	if not row.get("ok",false): printerr(row); finish(); return
	var canonical: Dictionary={}; var other: Dictionary={}
	for candidate in installed.catalogs:
		if candidate.content_id==flat.content_id and candidate.content_version==flat.content_version and candidate.content_digest==flat.content_digest: canonical=candidate
		if candidate.content_id=="foundation.two_stories": other=candidate
	check(not canonical.is_empty() and canonical.has("pack_set_digest"),"P canonical采购侧锁与当前flat三元组精确相同")
	check(not other.is_empty() and other.content_id!=flat.content_id,"反例来自另一真实已安装目录")
	if canonical.is_empty() or other.is_empty(): finish(); return
	host=Host.new(); host.kernel=kernel
	var status: String=host._slot_status(row,installed,context.registry,"save")
	print("FLAT_V1_SAVE_STATUS=",status)
	check(status.is_empty(),"flat真v1在已安装canonical全集解析后可作为保存候选")
	var locked=Kernel.new()
	started=locked.new_game(current.scenario_id,canonical.catalog_path,{},current.player_actor_id)
	check(started.get("ok",false),"匹配canonical目录真实新局构造带锁v2反例")
	if not started.get("ok",false): printerr(started); finish(); return
	var locked_row: Dictionary=row_for(locked)
	check(locked_row.get("ok",false) and locked_row.get("metadata",{}).get("format_version")==2,"匹配内容v2锁来自正式Kernel目录，不合成trusted包装")
	status=host._slot_status(locked_row,installed,context.registry,"save")
	check(status=="PackDigestMismatch","当前flat无锁不能因同三元组冒通过带锁v2候选")
	var foreign=Kernel.new()
	started=foreign.new_game(other.data.scenarios.keys()[0],other.catalog_path)
	check(started.get("ok",false),"另一真实目录可创建完整世界")
	if not started.get("ok",false): printerr(started); finish(); return
	var foreign_row: Dictionary=row_for(foreign)
	check(foreign_row.get("ok",false),"不同内容档通过真实Codec物理封装检查")
	status=host._slot_status(foreign_row,installed,context.registry,"save")
	check(status=="IncompatibleContent","flat当前世界与真正另一内容身份明确不兼容")
	check(kernel.snapshot()==current and kernel.content()==context.catalog and kernel.save_context().registry==context.registry,"纯列表分类保留完整世界、目录、registry与RNG")
	finish()

## 不加入窗口树的真实Main只释放内存；此微门不代表完整恢复、写槽或CAS确认。
func finish() -> void:
	if is_instance_valid(host):
		# 未调用_ready时这些预分配Node没有父节点；本测试必须逐个释放，不能靠宿主free回收。
		for node in [host._ambience.music,host._ambience.effect,host._ambience,host.journey_view,host.storage,host._ui_font_scale]:
			if is_instance_valid(node): node.free()
		host.free()
	print("FLAT_SLOT_COMPATIBILITY_CHECKS=",checks," FAILURES=",failures,"; FULL_DECODE_CAS=NOT_RUN; OS_INPUT=NOT_RUN")
	quit(0 if failures==0 else 1)
