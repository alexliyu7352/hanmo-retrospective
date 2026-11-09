## 原Main实际读取的HUD/地图/空活动页基准；只读既有三个真实状态，不生成新世界或提前半日。
extends SceneTree
const Strict=preload("res://core/strict_json.gd")
const Hud=preload("res://core/views/hud_view.gd")
const Sidebar=preload("res://core/views/scene_journey_sidebar.gd")
const MapView=preload("res://core/views/map_view.gd")
const SceneView=preload("res://core/views/scene_projection.gd")
const Personal=preload("res://core/views/personal_battle_view.gd")
const Military=preload("res://core/views/military_battle_page.gd")
const Siege=preload("res://core/views/siege_page.gd")
const Study=preload("res://core/views/study_activity_input.gd")
var manifest: String=""
var output: String=""
## 原输入清单显式给定；只写新证据文件，不覆盖原世界或既有回执。
func _init() -> void:
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--manifest="):manifest=arg.substr(11)
		if arg.begins_with("--output="):output=arg.substr(9)
	call_deferred("run")
## 无模板结论必须来自原SceneProjection成功且确实没有template_id，不能从空测试变量推断。
func run() -> void:
	if manifest.is_empty() or output.is_empty() or FileAccess.file_exists(output):quit(1);return
	var inputs: Array=Strict.parse_text(FileAccess.get_file_as_string(manifest)).value
	var rows: Array=[]
	for index in [0,2,5]:
		var input: Dictionary=inputs[index]
		var bytes: String=FileAccess.get_file_as_string(input.state)
		var state: Dictionary=Strict.parse_text(bytes).value
		var catalog: Dictionary=Strict.parse_text(FileAccess.get_file_as_string(input.catalog)).value
		var data: Dictionary=catalog.data
		var actor: Dictionary=state.actors[state.player_actor_id]
		var place: String=actor.location.get("place_id","") if actor.location.kind=="Settlement" else ""
		var scene: Dictionary=SceneView.query(state,data,place,"world",actor.id)
		if not scene.get("ok",false) or scene.has("template_id"):quit(1);return
		var screen: Dictionary={"ok":true,"personal_battle_view":Personal.query(state,data),"military_battle_page":Military.query(state,data),"siege_page":Siege.query(state,data),"study_activity":Study.active(state),"presentation_location":Sidebar.raw_location(state),"world_scene_legacy":true}
		rows.append({"state":input.state,"catalog":input.catalog,"state_sha256":bytes.sha256_text(),"hud":Hud.query(state,data),"screen":screen,"sidebar":Sidebar.journey(state,data),"map":MapView.query(state,data)})
		if JSON.stringify(state,"",false,true)!=bytes:quit(1);return
	var file=FileAccess.open(output,FileAccess.WRITE)
	if file==null:quit(1);return
	file.store_string(JSON.stringify(rows,"",false,true));file.close();print("background_live_projection_oracle: PASS rows=",rows.size());quit(0)
