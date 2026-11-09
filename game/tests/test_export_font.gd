## 在固定 Godot 引擎检查准备后的字体面及实际中文覆盖；系统字体回退不能掩盖内嵌字体缺失。
extends SceneTree

## 只读取构建字体并逐字检查当前中文界面及作者正文，缺字会记录码点并失败退出。
func _init() -> void:
	var checks: int=0; var failures: int=0
	if not ResourceLoader.exists("res://assets/fonts/cjk.tres"):
		printerr("FAIL: 尚未准备实际 CJK 字体资源")
		print("EXPORT_FONT_CHECKS=1 FAILURES=1"); quit(1); return
	var font: Font=load("res://assets/fonts/cjk.tres")
	font.base_font.allow_system_fallback=false
	var text: String="汉末行记旅途保存继续导入导出粮草武学门派庄园坞堡收养传承"
	var sources: Array=["res://ui/main.gd","res://ui/screen_composer.gd"]
	for file in DirAccess.get_files_at("res://data/generated"):
		if file.ends_with(".hmcatalog.json"): sources.append("res://data/generated/"+file)
	for source in sources: text+=FileAccess.get_file_as_string(source)
	var seen: Dictionary={}
	for index in range(text.length()):
		var code: int=text.unicode_at(index)
		if code<0x3400 or code>0x9fff or seen.has(code): continue
		seen[code]=true; checks+=1
		if not font.has_char(code): failures+=1; printerr("FAIL: 内嵌字体缺字 ",String.chr(code)," U+%04X"%code)
	var server: TextServer=TextServerManager.get_primary_interface()
	print("EXPORT_FONT_NAME=",server.font_get_name(font.get_rids()[0])," FACE_INDEX=",server.font_get_face_index(font.get_rids()[0]))
	print("EXPORT_FONT_CHECKS=",checks," FAILURES=",failures)
	quit(1 if failures else 0)
