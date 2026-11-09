## 本地字号偏好：只缩放主题与控件字号，保留100%基数，不接触世界、窗口缩放或存档。
extends Node
signal changed(percent: int)
const PERCENTS: Array[int] = [100, 125, 150]
const BASE_META: StringName = &"ui_font_scale_base_sizes"
const SCALE_META: StringName = &"ui_font_scale_percent"
var percent: int = 100
var _host: Control
var _base_theme: Theme
var _settings: String = ""
var _tree: SceneTree
var _selectors: Array[WeakRef] = []
var _refresh_queued: bool = false

## 绑定唯一表现根并读取独立配置；错误或缺失偏好回到100%，不会重写无关配置。
func configure(host: Control, settings_path: String = "") -> void:
	_disconnect_tree()
	if _host != host or _base_theme == null:
		_base_theme = host.theme.duplicate() if host.theme != null else Theme.new()
	_host = host
	_settings = settings_path if not settings_path.is_empty() else default_settings_path()
	var cfg = ConfigFile.new()
	var stored: Variant = 100
	if cfg.load(_settings) == OK: stored = cfg.get_value("ui", "font_scale_percent", 100)
	percent = stored if stored is int and stored in PERCENTS else 100
	_tree = host.get_tree()
	_tree.node_added.connect(_node_added)
	_apply()

## 与现有声音偏好保持相同开发隔离约定，另用文件避免旧声音保存擦掉字号。
static func default_settings_path() -> String:
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--profile=") and arg != "--profile=default": return "user://ui-font-development.cfg"
	return "user://ui-font.cfg"

## 自绘文字复用同一比例；每次都从调用者的原字号计算，图像、坐标和热点不缩放。
static func scaled_size(node: Node, base_size: int) -> int:
	var ancestor: Node = node
	while ancestor != null:
		if ancestor.has_meta(SCALE_META): return maxi(1, roundi(base_size * int(ancestor.get_meta(SCALE_META)) / 100.0))
		ancestor = ancestor.get_parent()
	return base_size

## 只接受三个明确档位；保存失败仍保留当次显示并返回错误供入口提示。
func set_percent(value: int, persist: bool = true) -> Dictionary:
	if value not in PERCENTS: return {"ok": false, "code": "InvalidFontScale"}
	percent = value
	_apply()
	changed.emit(percent)
	if persist:
		var cfg = ConfigFile.new()
		cfg.load(_settings)
		cfg.set_value("ui", "font_scale_percent", percent)
		var error: Error = cfg.save(_settings)
		if error != OK: return {"ok": false, "code": "FontScaleSaveFailed", "error": error}
	return {"ok": true, "percent": percent}

## 标题和系统菜单使用同一真实选项控件；弱引用不延长已退出页面生命周期。
func make_selector(parent: Node) -> OptionButton:
	var selector = OptionButton.new()
	selector.name = "FontScaleSelector"
	selector.set_meta("command_kind", "font_scale")
	selector.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	selector.custom_minimum_size.y = 40
	selector.fit_to_longest_item = false
	selector.clip_text = true
	for value in PERCENTS: selector.add_item("字号 · %d%%" % value, value)
	selector.select(PERCENTS.find(percent))
	selector.item_selected.connect(_selected.bind(selector))
	parent.add_child(selector)
	_sync_selectors()
	_selectors.append(weakref(selector))
	return selector

## 入口只改表现偏好；无效索引不被转换成其他有效档位。
func _selected(index: int, selector: OptionButton) -> void:
	if not is_instance_valid(selector) or not selector.is_inside_tree() or index < 0 or index >= PERCENTS.size(): return
	var ancestor: Node = selector
	while ancestor != null:
		if ancestor.is_queued_for_deletion(): return
		if ancestor == _host: break
		ancestor = ancestor.get_parent()
	if ancestor != _host: return
	var result: Dictionary = set_percent(PERCENTS[index])
	selector.tooltip_text = "字号已应用，但本地偏好未能保存" if not result.ok else "只调整文字大小，不推进游戏时间"

## 主题与局部override分别从各自100%基数计算；不把已缩放的继承值再次相乘。
func _apply() -> void:
	if not is_instance_valid(_host) or _base_theme == null: return
	_host.set_meta(SCALE_META, percent)
	var skin: Theme = _base_theme.duplicate()
	var base_default: int = _base_theme.default_font_size if _base_theme.default_font_size > 0 else ThemeDB.fallback_font_size
	skin.default_font_size = scaled_size(_host, base_default)
	for type in _base_theme.get_type_list():
		for key in _base_theme.get_font_size_list(type):
			skin.set_font_size(key, type, scaled_size(_host, _base_theme.get_font_size(key, type)))
	_host.theme = skin
	apply_subtree(_host)
	_sync_selectors()

## 已关闭页面的弱引用及时丢弃，反复切场景不累积失效选择器。
func _sync_selectors() -> void:
	var live: Array[WeakRef] = []
	for reference in _selectors:
		var selector: OptionButton = reference.get_ref()
		if is_instance_valid(selector) and selector.is_inside_tree():
			selector.select(PERCENTS.find(percent)); live.append(reference)
	_selectors = live

## 只记录显式字号override的原值；继承主题的控件保持继承，不制造双重缩放。
func apply_subtree(node: Node) -> void:
	if node is Control or node is Window:
		var base: Dictionary = node.get_meta(BASE_META, {})
		for property in node.get_property_list():
			var path: String = property.name
			if not path.begins_with("theme_override_font_sizes/"): continue
			var key: String = path.trim_prefix("theme_override_font_sizes/")
			if node.has_theme_font_size_override(key) and not base.has(key): base[key] = int(node.get(path))
		if not base.is_empty():
			node.set_meta(BASE_META, base)
			for key in base: node.add_theme_font_size_override(key, scaled_size(node, base[key]))
		if node is Control: node.queue_redraw()
	for child in node.get_children(): apply_subtree(child)

## 生产页面和独立窗口可延后建立控件；等本轮建树及字号override完成后再读取基数。
func _node_added(node: Node) -> void:
	if _refresh_queued or not is_instance_valid(_host): return
	if _host.is_ancestor_of(node) and (node is Control or node is Window):
		_refresh_queued = true
		_apply_added.call_deferred()

## 同帧建树合并为一次扫描；已经离树或释放的宿主不再处理。
func _apply_added() -> void:
	_refresh_queued = false
	if is_instance_valid(_host) and _host.is_inside_tree() and not _host.is_queued_for_deletion(): apply_subtree(_host)

## 重配和离树均断开全树监听，防止重开主场景留下旧字号控制器。
func _disconnect_tree() -> void:
	if is_instance_valid(_tree) and _tree.node_added.is_connected(_node_added): _tree.node_added.disconnect(_node_added)
	_tree = null
	_refresh_queued = false

## 表现宿主销毁不遗留全树回调或控件引用。
func _exit_tree() -> void:
	_disconnect_tree()
	_selectors.clear()
