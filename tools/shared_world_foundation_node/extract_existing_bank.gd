## 旧物理银行仅经原产品封装校验器取出同一payload；原银行、generation和存槽均不改写。
extends SceneTree
const Codec=preload("res://storage/resident_slot_codec.gd")

## 两条显式路径分别是只读旧原件及本门临时输出，不创建或安装任何世界。
func _initialize() -> void:
	var args: PackedStringArray=OS.get_cmdline_user_args()
	if args.size()!=2: quit(2); return
	var original: String=FileAccess.get_file_as_string(args[0])
	var result: Dictionary=Codec.decode_payload(original)
	if not result.get("ok",false): printerr(JSON.stringify(result)); quit(1); return
	var target=FileAccess.open(args[1],FileAccess.WRITE)
	if target==null: quit(2); return
	target.store_buffer(result.payload); target.close()
	print(JSON.stringify({"ok":true,"original_sha256":FileAccess.get_sha256(args[0]),"payload_sha256":FileAccess.get_sha256(args[1]),"original_path":args[0],"payload_path":args[1]}))
	quit(0)
