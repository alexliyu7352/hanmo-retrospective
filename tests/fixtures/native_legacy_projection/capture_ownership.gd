## 独立读取旧Schema原拥有权声明，供Python审计oracle使用；不读取新codec覆盖账本。
extends SceneTree
## 只写明示证据文件并退出；没有输出参数或文件失败时拒绝。
func _init() -> void:
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--output="):
			var file=FileAccess.open(arg.trim_prefix("--output="),FileAccess.WRITE)
			if file==null: quit(1); return
			file.store_string(JSON.stringify(preload("res://core/runtime/world_validation.gd").required_ownership(),"",true,true)+"\n"); file.close(); quit(); return
	quit(2)
