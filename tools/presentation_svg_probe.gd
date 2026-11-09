## 开发期旧SVG资源探针：只解码本地图片，不执行场景、回调或作者脚本。
extends SceneTree
## 从固定调用者传入的单一本地SVG读取像素，输出小型机器可读结果。
func _init() -> void:
	var args: PackedStringArray = OS.get_cmdline_user_args()
	if args.size() != 1 or not args[0].ends_with(".svg"):
		print("HM_SVG={\"ok\":false,\"code\":\"AssetPath\"}"); quit(2); return
	var file = FileAccess.open(args[0], FileAccess.READ)
	if file == null or file.get_length() > 16777216:
		print("HM_SVG={\"ok\":false,\"code\":\"AssetDecode\"}"); quit(2); return
	var raw: PackedByteArray = file.get_buffer(file.get_length()); file.close()
	var image = Image.new()
	if image.load_svg_from_buffer(raw) != OK or image.is_empty():
		print("HM_SVG={\"ok\":false,\"code\":\"AssetDecode\"}"); quit(2); return
	if image.get_width() * image.get_height() > 16777216:
		print("HM_SVG={\"ok\":false,\"code\":\"AssetTooLarge\"}"); quit(2); return
	print("HM_SVG=", JSON.stringify({"ok": true, "size": [image.get_width(), image.get_height()]}))
	quit(0)
