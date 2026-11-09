## 独立Godot进程只读取测试专属存档并执行明确下一命令，证明恢复并非内存内复制。
extends SceneTree
const Kernel=preload("res://core/world_kernel.gd")
const Codec=preload("res://core/save_codec.gd")
const Strict=preload("res://core/strict_json.gd")

## 引擎准备完成后才开始子进程读取。
func _init() -> void:
	call_deferred("run_resume")

## 保存恢复前后完整快照、首次回执和重复请求结果，父测试负责逐字段比较。
func run_resume() -> void:
	var args: Dictionary={}
	for argument in OS.get_cmdline_user_args():
		var parts: PackedStringArray=argument.trim_prefix("--").split("=",true,1)
		if parts.size()==2: args[parts[0]]=parts[1]
	var k=Kernel.new(); var result: Dictionary=k.new_game()
	if not result.ok: quit(1); return
	var decoded: Dictionary=Codec.decode(FileAccess.get_file_as_string(args.input),k.content())
	if not decoded.ok or not k.restore(decoded.state).ok: quit(1); return
	var before: Dictionary=k.snapshot(); var command: Dictionary=Strict.parse_text(FileAccess.get_file_as_string(args.command)).value
	var reply: Dictionary=k.submit(command); var after: Dictionary=k.snapshot()
	var duplicate: Dictionary=k.submit(command)
	var file=FileAccess.open(args.output,FileAccess.WRITE)
	if file==null: quit(1); return
	file.store_string(JSON.stringify({"before":before,"reply":reply,"after":after,"duplicate":duplicate,"after_duplicate":k.snapshot()},"",true,true)); file.close()
	quit(0 if reply.ok else 1)
