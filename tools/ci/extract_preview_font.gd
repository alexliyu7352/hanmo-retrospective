## 只读挂载已发布PCK并取原字体；不加载旧场景、脚本或原生库。
extends SceneTree
func _init() -> void:
	var args=OS.get_cmdline_user_args()
	if args.size()!=2 or not ProjectSettings.load_resource_pack(args[0],true):
		printerr("Published font package could not be mounted"); quit(1); return
	var path="res://assets/fonts/cjk.ttc"
	var font=load(path)
	if not font is FontFile:
		printerr("Published font resource missing"); quit(1); return
	var bytes: PackedByteArray=font.data
	var hash=HashingContext.new(); hash.start(HashingContext.HASH_SHA256); hash.update(bytes)
	if hash.finish().hex_encode()!="b76b0433203017ca80401b2ee0dd69350349871c4b19d504c34dbdd80541690a":
		printerr("Published font digest differs"); quit(1); return
	var file=FileAccess.open(args[1],FileAccess.WRITE)
	if file==null: quit(1); return
	file.store_buffer(bytes); file.close(); quit(0)
