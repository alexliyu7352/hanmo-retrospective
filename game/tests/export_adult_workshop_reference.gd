## 在原独立工坊根运行的只读参照导出；正式测试从外部传入元组表，不修改任何工坊文件。
extends SceneTree
const Renderer = preload("res://src/portrait_renderer.gd")
const Fittings = preload("res://src/fitting_catalog.gd")
const Representatives = preload("res://src/representative_catalog.gd")

## 用原渲染器逐个合成192元组，保存真实RGBA摘要给正式根的同引擎测试比对。
func _initialize() -> void:
	var args: PackedStringArray = OS.get_cmdline_user_args()
	if args.size() != 2:
		printerr("需要正式组合JSON绝对路径与参照JSON输出绝对路径。")
		quit(2)
		return
	var runtime: Dictionary = Fittings.read_registry()
	if not runtime.get("ok", false): quit(1); return
	var representatives: Dictionary = Representatives.read(runtime.catalog)
	if not representatives.get("ok", false): quit(1); return
	var catalog: Dictionary = JSON.parse_string(FileAccess.get_file_as_string(args[0]))
	var definitions: Dictionary = {}
	var renderers: Dictionary = {}
	for fitting in runtime.catalog.fittings:
		definitions[fitting.id] = fitting.definition
		var renderer: RefCounted = Renderer.new()
		if not renderer.configure(fitting.definition).ok: quit(1); return
		renderers[fitting.id] = renderer
	var hashes: Dictionary = {}
	for row in catalog.records:
		var definition: Dictionary = definitions[row.fitting_id]
		var state: Dictionary = {}
		for field in definition.defaults: state[field] = int(definition.defaults[field])
		for face in catalog.faces:
			if face.id == row.base_features.face_shape: state.face = int(face.face_index)
		var tokens: Dictionary = {"brows": row.base_features.brows, "eyes": row.base_features.eyes, "nose": row.base_features.nose, "mouth": row.base_features.mouth, "hair": "bun", "beard": "none", "skin": "warm", "hair_color": "black", "outfit": "plain", "cloth_color": "original"}
		for field in tokens: state[field] = definition.choice_semantics[field].find(tokens[field])
		var renderer: RefCounted = renderers[row.fitting_id]
		var rendered: Dictionary = renderer.render(state)
		if not rendered.ok: quit(1); return
		var portrait: Image = rendered.image
		hashes[row.id] = {"half_body": _hash(portrait), "avatar96": _hash(renderer.avatar(portrait, 96))}
	var file: FileAccess = FileAccess.open(args[1], FileAccess.WRITE)
	if file == null: quit(1); return
	file.store_string(JSON.stringify({"engine": Engine.get_version_info().string, "count": hashes.size(), "records": hashes}, "\t") + "\n")
	print(JSON.stringify({"test": "adult_workshop_reference", "count": hashes.size(), "ok": hashes.size() == 192}))
	quit(0 if hashes.size() == 192 else 1)

## 摘要只由真实完整RGBA字节计算，不使用资源路径或文件名冒充像素证据。
func _hash(image: Image) -> String:
	var context: HashingContext = HashingContext.new()
	context.start(HashingContext.HASH_SHA256)
	context.update(image.get_data())
	return context.finish().hex_encode()
