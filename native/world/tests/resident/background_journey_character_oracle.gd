## 原character_input只读白名单字段基准；仅取明确四个人物，不枚举世界、不改身份或生成外貌。
extends SceneTree
const Strict=preload("res://core/strict_json.gd")
const CharacterInput=preload("res://core/views/character_input.gd")
var manifest: String=""
var output: String=""
## 来源与新输出显式给定，原档与已经存在的证据不覆盖。
func _init() -> void:
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--manifest="):manifest=arg.substr(11)
		if arg.begins_with("--output="):output=arg.substr(9)
	call_deferred("run")
## 每次投影前后完整字节一致；人物公开输入包含原空名、年龄和legacy外观，不借Renderer资源兜底。
func run() -> void:
	if manifest.is_empty() or output.is_empty() or FileAccess.file_exists(output):quit(1);return
	var rows: Array=[]
	for input in Strict.parse_text(FileAccess.get_file_as_string(manifest)).value:
		var bytes: String=FileAccess.get_file_as_string(input.state)
		var state: Dictionary=Strict.parse_text(bytes).value
		var catalog: Dictionary=Strict.parse_text(FileAccess.get_file_as_string(input.catalog)).value
		var data: Dictionary=catalog.get("data",catalog.get("catalog",{}))
		var characters: Dictionary={}
		for id in ["actor.player","actor.walker","actor.bandit","actor.blackwater.commander"]:
			characters[id]=CharacterInput.query(state,data,id)
		rows.append({"state":input.state,"catalog":input.catalog,"state_sha256":bytes.sha256_text(),"characters":characters})
		if JSON.stringify(state,"",false,true)!=bytes:quit(1);return
	var file=FileAccess.open(output,FileAccess.WRITE)
	if file==null:quit(1);return
	file.store_string(JSON.stringify(rows,"",false,true));file.close();print("background_journey_character_oracle: PASS rows=",rows.size());quit(0)
