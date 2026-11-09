## 同一输入直接调用既有GD character_input；核对值/类型/字段序，不复制另一套表现规则。
extends SceneTree
const CharacterInput=preload("res://core/views/character_input.gd")
const Strict=preload("res://core/strict_json.gd")

## 递归比较有限输出；字典键序和数值类型都是验收内容，不作正规化或截断。
func exact(a: Variant, b: Variant) -> bool:
	if typeof(a)!=typeof(b): return false
	if a is Dictionary:
		if a.keys()!=b.keys(): return false
		for key in a:
			if not exact(a[key],b[key]): return false
		return true
	if a is Array:
		if a.size()!=b.size(): return false
		for index in range(a.size()):
			if not exact(a[index],b[index]): return false
		return true
	return a==b

## 只跑传入的有限projector向量，输入/目录必须原样保留；不创建世界或推进半日。
func _initialize() -> void:
	var args=OS.get_cmdline_user_args()
	if args.size()!=1: printerr("NeedCharacterVectorPath"); quit(2); return
	var loaded: Dictionary=Strict.parse_text(FileAccess.get_file_as_string(args[0]),1024*1024)
	if not loaded.ok: printerr(loaded); quit(2); return
	var failed: int=0; var count: int=0
	for row in loaded.value.cases:
		var state: Dictionary=row.state.duplicate(true); var data: Dictionary=row.data.duplicate(true)
		var actual: Dictionary=CharacterInput.query(state,data,row.actor_id)
		if not exact(actual,row.expected) or not exact(state,row.state) or not exact(data,row.data):
			failed+=1; printerr("CharacterProjectionMismatch: ",row.case," actual=",actual," expected=",row.expected)
		count+=1
	print("CHARACTER_GD_ORACLE_CASES=",count," FAILURES=",failed," SCOPE=existing selected projector only")
	quit(0 if failed==0 else 1)
