## 通过真实Main与独立X11输入验证中文文件框；原生取消、覆盖确认和存档往返均不代发信号。
extends "res://tests/test_family_screen.gd"
const SaveFiles=preload("res://storage/save_files.gd")
const LocalizedDialog=preload("res://ui/localized_file_dialog.gd")

## 没有独立图形输入时明确跳过；真实运行使用本轮目录中的新文件，不读写玩家存档。
func run_checks() -> void:
	if external_input_directory.is_empty() or capture_directory.is_empty() or DisplayServer.get_name()=="headless":
		print("LOCALIZED_FILE_DIALOG_SKIPPED: 需要独立X11输入与验证目录")
		quit(); return
	var locale: String=TranslationServer.get_locale()
	var main_open: String=TranslationServer.translate("Open")
	main=Host.new(); root.add_child(main); main.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	await settle()
	await click("import")
	var dialog: FileDialog=current_dialog()
	if dialog==null: await finish(); return
	check(dialog is LocalizedDialog,"Main导入入口使用局部中文FileDialog")
	check(dialog.tr(dialog.title)=="导入存档" and dialog.get_ok_button().tr(dialog.get_ok_button().text)=="打开","标题和原生确认按钮使用中文")
	check(dialog.get_cancel_button().tr(dialog.get_cancel_button().text)=="取消","原生取消按钮继承独立翻译域")
	await external_input({"kind":"capture","name":"01-import-chinese"})
	await dialog_button(dialog,dialog.get_cancel_button(),"取消导入")
	check(not main._started and current_dialog(false)==null,"取消关闭真实文件框并保留标题界面")
	check(TranslationServer.get_locale()==locale and TranslationServer.translate("Open")==main_open,"局部中文没有改变全局语言或其他翻译")
	check(main.start_content("scenario.alpha_life","res://data/generated/alpha_life.hmcatalog.json").ok,"正式Main建立同工程世界用于实际导出往返")
	if not main._started: await finish(); return
	var source: Dictionary=main.kernel.snapshot()
	var path: String=capture_directory.path_join("dialog-roundtrip.hmsave")
	check(not FileAccess.file_exists(path),"本次导出目标是独立验证目录的新文件")
	await click("export")
	dialog=current_dialog()
	if dialog==null: await finish(); return
	check(dialog.tr(dialog.title)=="导出存档" and dialog.get_ok_button().tr(dialog.get_ok_button().text)=="保存","导出标题与确认操作中文可辨")
	await enter_path(dialog,path)
	await external_input({"kind":"capture","name":"02-export-chinese"})
	await external_input({"kind":"keys","keys":["Return"]})
	await settle()
	var exported: Dictionary=SaveFiles.read_world(path,main.kernel.content())
	check(exported.ok and exported.get("state",{})==source,"原生保存确认写出正式Codec文件并完整保留世界")
	if not exported.ok: await finish(); return
	var before: String=FileAccess.get_file_as_string(path)
	await click("export")
	dialog=current_dialog()
	if dialog==null: await finish(); return
	await enter_path(dialog,path)
	await external_input({"kind":"keys","keys":["Return"]})
	await settle()
	var confirmation: ConfirmationDialog=null
	for child in dialog.find_children("*","ConfirmationDialog",true,false):
		if child.visible: confirmation=child; break
	check(confirmation!=null,"选择已有文件出现真实覆盖确认")
	if confirmation!=null:
		check(confirmation.dialog_text=="文件“%s”已存在。\n是否覆盖？"%path,"覆盖提示采用中文")
		await external_input({"kind":"capture","name":"03-overwrite-chinese"})
		await dialog_button(confirmation,confirmation.get_cancel_button(),"取消覆盖")
		check(FileAccess.get_file_as_string(path)==before,"取消覆盖后既有导出文件字节不变")
	await dialog_button(dialog,dialog.get_cancel_button(),"取消导出")
	await click("import")
	dialog=current_dialog()
	if dialog==null: await finish(); return
	await enter_path(dialog,path)
	await external_input({"kind":"keys","keys":["Return"]})
	await settle()
	check(main._message.begins_with("已导入旅途") and main.kernel.snapshot()==source,"中文原生文件框真实导入导出文件，完整世界往返一致")
	await capture("04-imported-world")
	observed.locale=locale; observed.file=path; observed.tick=source.tick; observed.content_digest=source.content_digest
	await finish()

## 从Main读取实际存在的文件窗口；缺失时记录失败，不能另建替代控件冒充入口。
func current_dialog(required: bool=true) -> FileDialog:
	var dialogs: Array=main.find_children("*","FileDialog",true,false)
	if required: check(dialogs.size()==1,"Main的原生文件窗口唯一存在")
	return dialogs[0] if dialogs.size()==1 else null

## 将文件路径逐键输入原生文件名栏；不设置文本属性或直接调用导入回调。
func enter_path(dialog: FileDialog, path: String) -> void:
	await mouse(dialog.get_line_edit().get_global_rect().get_center()+Vector2(dialog.position),"文件路径输入框")
	await external_input({"kind":"chord","keys":["Control_L","a"]})
	await external_input({"kind":"type","text":path})

## 按窗口实际屏幕偏移点击原生按钮；子窗口也经同一XTest输入验证。
func dialog_button(dialog: Window, control: Button, label: String) -> void:
	await mouse(control.get_global_rect().get_center()+Vector2(dialog.position),label)
	await settle()

## 输出本次实际图形验证结果；截图和输入日志保留在独立目录中。
func finish() -> void:
	observed.checks=checks; observed.failures=failures; observed.native_input=not external_input_directory.is_empty(); observed.display=DisplayServer.get_name()
	if not capture_directory.is_empty():
		var file: FileAccess=FileAccess.open(capture_directory.path_join("observed.json"),FileAccess.WRITE)
		file.store_string(JSON.stringify(observed,"  ")); file.close()
	print("LOCALIZED_FILE_DIALOG_CHECKS=%d FAILURES=%d"%[checks,failures])
	if is_instance_valid(main): main.queue_free()
	for _frame in range(8): await process_frame
	quit(0 if failures==0 else 1)
