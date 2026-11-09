## 只复核真实完成态的有限技艺页布局；不安装世界、不推进课程，也不替代已完成的Main存读节点。
extends SceneTree
const MainScene=preload("res://main.tscn")
const Strict=preload("res://core/strict_json.gd")
const Catalog=preload("res://core/content_catalog.gd")
const View=preload("res://core/views/ordinary_skills_view.gd")
const Screen=preload("res://ui/ordinary_skills_screen.gd")
const Art=preload("res://ui/art_catalog.gd")

## 推迟到场景树可用后执行有限渲染，不通过动画产生任何领域效果。
func _initialize() -> void: call_deferred("run")

## 显式使用已保存的实际完成态与同代目录，原状态仅供纯查询且保持逐字节不变。
func run() -> void:
	var snapshot: String=""; var catalog_path: String=""; var output: String=""
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--snapshot="): snapshot=arg.trim_prefix("--snapshot=")
		elif arg.begins_with("--catalog="): catalog_path=arg.trim_prefix("--catalog=")
		elif arg.begins_with("--output="): output=arg.trim_prefix("--output=")
	if not snapshot.is_absolute_path() or not output.is_absolute_path() or DisplayServer.get_name()=="headless": quit(2); return
	var raw: String=FileAccess.get_file_as_string(snapshot)
	var decoded: Dictionary=Strict.parse_text(raw)
	var catalog: Dictionary=Catalog.read_prepared(catalog_path)
	if not decoded.get("ok",false) or not decoded.get("value") is Dictionary or not catalog.get("ok",false): quit(2); return
	var prepared: Dictionary=Art.prepare_content(catalog)
	if not prepared.get("ok",false): quit(2); return
	Art.commit_content(prepared)
	var state: Dictionary=decoded.value
	var before: String=JSON.stringify(state,"",false,true)
	var view: Dictionary=View.query(state,catalog.data,"rites")
	root.size=Vector2i(1280,900)
	var main=MainScene.instantiate(); root.add_child(main); await process_frame
	for child in main.page.get_children(): main.page.remove_child(child); child.queue_free()
	var stage=main.Landscape.new(); stage.visual_tag="city"; stage.size_flags_horizontal=Control.SIZE_EXPAND_FILL; stage.size_flags_vertical=Control.SIZE_EXPAND_FILL; stage.custom_minimum_size=Vector2(0,680); main.page.add_child(stage)
	Screen.build(main,stage,view)
	for frame in range(4): await process_frame
	await RenderingServer.frame_post_draw
	var heading: Label=stage.find_child("OrdinarySkillsHeading",true,false)
	var close: Button=stage.find_child("OrdinarySkillsClose",true,false)
	var ok: bool=heading!=null and close!=null and heading.get_line_count()==1 and heading.get_parent().size.y<90 and close.get_global_rect().size.y<90 and before==JSON.stringify(state,"",false,true)
	var pixels: Image=root.get_texture().get_image()
	ok=ok and pixels.save_png(output.path_join("history-fixed.png"))==OK
	var record: Dictionary={"ok":ok,"scope":"actual completed snapshot finite-view rendering only; no gameplay or save replay","state_sha256":raw.sha256_text(),"query_nonmutation":before==JSON.stringify(state,"",false,true),"heading_lines":heading.get_line_count() if heading!=null else -1,"header_height":heading.get_parent().size.y if heading!=null else -1,"viewport_size":[pixels.get_width(),pixels.get_height()],"view":view}
	var file=FileAccess.open(output.path_join("result.json"),FileAccess.WRITE)
	if file==null: quit(2); return
	file.store_string(JSON.stringify(record,"",false,true)+"\n"); file.close()
	main.queue_free(); await process_frame
	print("STUDY_HISTORY_VIEW_OK=",ok); quit(0 if ok else 1)
