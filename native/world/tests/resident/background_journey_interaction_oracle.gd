## 只读已冻结真实输入，采原GD行旅/对白有限视图和原军粮报价；不改来源状态或推进世界。
extends SceneTree
const Strict=preload("res://core/strict_json.gd")
const Sidebar=preload("res://core/views/scene_journey_sidebar.gd")
const Dialogue=preload("res://core/dialogue_runner.gd")
const ActionInput=preload("res://core/views/action_context.gd")
const Assets=preload("res://core/military_assets.gd")
var manifest: String=""
var output: String=""
## 明确输入清单与新输出文件，禁止覆盖已有证据。
func _init() -> void:
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--manifest="):manifest=arg.substr(11)
		if arg.begins_with("--output="):output=arg.substr(9)
	call_deferred("run")
## 原严格JSON整数/字段序往返必须保持，任何读取副作用立即失败。
func run() -> void:
	if manifest.is_empty() or output.is_empty() or FileAccess.file_exists(output):quit(1);return
	var rows: Array=[]
	for input in Strict.parse_text(FileAccess.get_file_as_string(manifest)).value:
		var bytes: String=FileAccess.get_file_as_string(input.state)
		var state: Dictionary=Strict.parse_text(bytes).value
		var catalog: Dictionary=Strict.parse_text(FileAccess.get_file_as_string(input.catalog)).value
		var data: Dictionary=catalog.get("data",catalog.get("catalog",{}))
		var supplies: Dictionary={}
		for id in state.armies:supplies[id]=Assets.supply_preview(state,data,id)
		rows.append({"state":input.state,"catalog":input.catalog,"state_sha256":bytes.sha256_text(),"sidebar":Sidebar.journey(state,data),"dialogue":Dialogue.view(state,data,state.player_actor_id),"needs_input":ActionInput.needs_input(state),"supplies":supplies})
		if JSON.stringify(state,"",false,true)!=bytes:quit(1);return
	var file=FileAccess.open(output,FileAccess.WRITE)
	if file==null:quit(1);return
	file.store_string(JSON.stringify(rows,"",false,true));file.close();print("background_journey_interaction_oracle: PASS rows=",rows.size());quit(0)
