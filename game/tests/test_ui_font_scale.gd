## 用真实Godot字号和生产界面检查偏好缩放，不以窗口或截图尺寸代替font_size断言。
extends SceneTree
const Scale = preload("res://ui/ui_font_scale.gd")
const Picker = preload("res://ui/appearance_picker.gd")
const Registry = preload("res://ui/presentation_registry.gd")
var checks: int = 0
var failures: int = 0
var _settings: String = "user://font-scale-test-%d.cfg" % Time.get_ticks_usec()

## 等待树就绪后才建立Control，避免未继承主题的假基线。
func _init() -> void:
	call_deferred("_run")

## 每个失败均进入退出码，结果仅声明本次局部字号合同。
func check(value: bool, label: String) -> void:
	checks += 1
	if not value: failures += 1; printerr("FAIL: ", label)

## 让新节点监听及容器尺寸完成两帧布局，不模拟窗口缩放。
func settle() -> void:
	for _frame in range(3): await process_frame

## 同时覆盖继承主题、局部字号、迟建窗口、档位恢复和独立偏好读回。
func _run() -> void:
	await check_control_sizes()
	await check_production_ui()
	DirAccess.remove_absolute(_settings)
	print("UI_FONT_SCALE_CHECKS=", checks, " FAILURES=", failures)
	quit(1 if failures else 0)

## 100%原始字号必须逐轮恢复；字体档位、节点缩放和viewport是互相独立的状态。
func check_control_sizes() -> void:
	var host = Control.new(); root.add_child(host)
	host.theme = Theme.new(); host.theme.default_font_size = 18
	host.theme.set_font_size("font_size", "LineEdit", 19)
	var inherited = Label.new(); inherited.text = "继承主题"; host.add_child(inherited)
	var explicit = Label.new(); explicit.add_theme_font_size_override("font_size", 23); host.add_child(explicit)
	var typed = LineEdit.new(); host.add_child(typed)
	var controller = Scale.new(); host.add_child(controller); controller.configure(host, _settings)
	var viewport_size: Vector2 = root.get_visible_rect().size
	for cycle in range(2):
		for value in [100, 125, 150, 100]:
			check(controller.set_percent(value, false).ok, "有效档位 %d/%d" % [cycle, value])
			await settle()
			check(inherited.get_theme_font_size("font_size") == roundi(18 * value / 100.0), "继承主题只乘一次 %d" % value)
			check(explicit.get_theme_font_size("font_size") == roundi(23 * value / 100.0), "局部override保留100%%基数 %d" % value)
			check(typed.get_theme_font_size("font_size") == roundi(19 * value / 100.0), "类型主题字号只乘一次 %d" % value)
			check(host.scale == Vector2.ONE and inherited.scale == Vector2.ONE and root.get_visible_rect().size == viewport_size, "字号未修改Canvas或窗口缩放")
	controller.set_percent(150, false)
	var late = Label.new(); host.add_child(late); late.add_theme_font_size_override("font_size", 21)
	var window = Window.new(); host.add_child(window)
	var popup_label = Label.new(); window.add_child(popup_label)
	await settle()
	check(late.get_theme_font_size("font_size") == 32, "迟建控件在建树后设置override仍使用原始21")
	check(popup_label.get_theme_font_size("font_size") == 27, "新窗口子控件继承真实150%主题字号")
	controller.apply_subtree(host); controller.apply_subtree(host)
	check(late.get_theme_font_size("font_size") == 32, "重复刷新不累计放大")
	controller.configure(host, _settings)
	check(inherited.get_theme_font_size("font_size") == 18, "同宿主重配不把缩放主题当基数")
	controller.set_percent(150, false)
	check(Scale.scaled_size(late, 15) == 23, "自绘字号从独立原值计算")
	check(not controller.set_percent(200).ok and controller.percent == 150, "非法档位不改变显示或写配置")
	var stale: OptionButton = controller.make_selector(host)
	stale.queue_free(); stale.item_selected.emit(0)
	check(controller.percent == 150 and not FileAccess.file_exists(_settings), "排队释放选择器的迟到信号不改字号或写偏好")
	await settle()
	check(controller.set_percent(125).ok, "有效档位保存到独立设置")
	var cfg = ConfigFile.new(); check(cfg.load(_settings) == OK and cfg.get_value("ui", "font_scale_percent") == 125, "实际ConfigFile保存125")
	var other = Control.new(); other.theme = Theme.new(); other.theme.default_font_size = 18; root.add_child(other)
	var restored = Scale.new(); other.add_child(restored); restored.configure(other, _settings)
	check(restored.percent == 125 and other.theme.default_font_size == 23, "重开偏好恢复且以新100%基数计算")
	cfg.set_value("ui", "font_scale_percent", "150"); cfg.save(_settings)
	var invalid = Control.new(); root.add_child(invalid)
	var fallback = Scale.new(); invalid.add_child(fallback); fallback.configure(invalid, _settings)
	check(fallback.percent == 100, "错误类型偏好回到100%")
	var listener_count: int = node_added.get_connections().size()
	host.queue_free(); other.queue_free(); invalid.queue_free(); await settle()
	check(node_added.get_connections().size() == listener_count - 3, "三个字号控制器离树均断开全树连接")

## 生产标题、游戏菜单和场景字号改变后，完整世界快照/RNG及页面重建保持同一结果。
func check_production_ui() -> void:
	var main = load("res://main.tscn").instantiate(); root.add_child(main)
	main._ui_font_scale._settings = _settings
	main._ui_font_scale.set_percent(100, false)
	await settle()
	var selector: OptionButton = main.find_child("FontScaleSelector", true, false)
	check(selector != null and selector.get_item_id(2) == 150, "标题存在100/125/150真实选项入口")
	var begun: Dictionary = main.start_content("scenario.alpha_life", "res://data/generated/alpha_life.hmcatalog.json")
	if check_started(begun):
		var before: Dictionary = main.kernel.snapshot()
		for value in [125, 150, 100]:
			main._ui_font_scale.set_percent(value, false); await settle()
			var status: Label = main.find_child("WorldStatus", true, false)
			var title: Label = main.find_child("PlaceTitle", true, false)
			check(status.get_theme_font_size("font_size") == roundi(18 * value / 100.0), "生产状态栏真实字号 %d" % value)
			check(title.get_theme_font_size("font_size") == roundi(23 * value / 100.0), "生产场景标题真实字号 %d" % value)
			check(main.kernel.snapshot() == before, "字号不改变完整世界或RNG %d" % value)
		main._ui_font_scale.set_percent(150, false); main._render(); await settle()
		check(main.find_child("WorldStatus", true, false).get_theme_font_size("font_size") == 27, "150%重建页面仍为真实27而非二次放大")
		selector = main.find_child("FontScaleSelector", true, false)
		check(selector != null and selector.selected == 2 and selector.get_theme_font_size("font_size") == 27, "系统菜单同一偏好/继承字号")
		check(main.find_child("SystemMenuScroll", true, false) is ScrollContainer, "长系统菜单使用真实滚动容器")
		selector.select(1); selector.item_selected.emit(1); await settle()
		check(main._ui_font_scale.percent == 125 and selector.get_theme_font_size("font_size") == 23, "真实菜单选项信号应用125%")
		var saved = ConfigFile.new()
		check(saved.load(_settings) == OK and saved.get_value("ui", "font_scale_percent") == 125, "菜单入口实际保存字号偏好")
		check(main.kernel.snapshot() == before, "字号与重建不改变世界/RNG/存档字段")
		await check_picker_layout(main)
		await check_menu_layout(main)
		check(main.kernel.snapshot() == before, "选件/窗口布局验收不改变完整世界")
	main.queue_free(); await settle()

## 新局失败仍报告明确错误，避免后续索引异常掩盖缺失断言。
func check_started(result: Dictionary) -> bool:
	check(result.get("ok", false), "生产Alpha新局：" + str(result))
	return result.get("ok", false)

## 独立Window内使用正式选件控件；大小只用于布局测量，不冒充原生实窗或修改生产窗口配置。
func check_picker_layout(main: Control) -> void:
	var spec: Dictionary = JSON.parse_string(FileAccess.get_file_as_string("res://../tests/fixtures/presentation/vertical_slice.json"))
	var manifest: Dictionary = JSON.parse_string(FileAccess.get_file_as_string("res://../tests/fixtures/character_presentation/manifest.json"))
	var registry = Registry.new(); check(registry.configure(manifest).ok, "选件字体夹具使用正式注册表")
	var actor: Dictionary = main._data.actors["actor.player"].duplicate(true)
	actor.presentation_profile_id = "fixture.person"
	var candidates: Dictionary = {}
	for feature in spec.base_features:
		candidates[feature] = spec.actors["actor.player"].generated_candidates.get(feature, [spec.base_features[feature]])
	actor.appearance_definition = {"kind": "generated", "generator_version": "appearance-v1", "attire_style": "plain", "candidates": candidates}
	var dialog = Window.new(); dialog.name = "FontScaleTestPickerWindow"; main.add_child(dialog)
	var margin = MarginContainer.new(); dialog.add_child(margin); margin.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	for edge in ["left", "right", "top", "bottom"]: margin.add_theme_constant_override("margin_" + edge, 12)
	var picker = Picker.new(); margin.add_child(picker)
	check(picker.configure("actor.player", actor, "font-scale-seed", 25, registry).ok, "真实选件控件可配置")
	var draft: Dictionary = picker.draft()
	for dimensions in [Vector2i(520,700), Vector2i(360,460)]:
		dialog.size = dimensions
		for value in [100, 125, 150, 100]:
			main._ui_font_scale.set_percent(value, false); await settle()
			var choice: OptionButton = picker._choices.face_shape
			check(choice.get_theme_font_size("font_size") == roundi(18 * value / 100.0), "选件真实字号 %s/%d" % [dimensions,value])
			check(choice.get_popup().get_theme_font_size("font_size") == roundi(18 * value / 100.0), "选件弹出名单字号 %s/%d" % [dimensions,value])
			check(picker.get_child(0).get_theme_font_size("font_size") == roundi(24 * value / 100.0), "选件标题override从24缩放")
			var scroll: ScrollContainer = picker.get_child(1)
			check(scroll.horizontal_scroll_mode == ScrollContainer.SCROLL_MODE_DISABLED and scroll.size.y > 0, "窄选件保留纵向滚动空间")
			check(picker._confirm.get_global_rect().end.y <= dimensions.y - 11, "选件确定按钮保留在窗口内 %s/%d: %s" % [dimensions,value,picker._confirm.get_global_rect()])
			check(picker.draft() == draft, "字号切换保留未提交草案")
	dialog.queue_free(); await settle()

## 固定逻辑尺寸下检查菜单边界与滚动可达性；此门只证明控件布局，不声明物理分辨率验收。
func check_menu_layout(main: Control) -> void:
	var original: Vector2 = main.size
	for dimensions in [Vector2(1280,720), Vector2(960,720)]:
		main.size = dimensions
		for value in [100, 125, 150, 100]:
			main._ui_font_scale.set_percent(value, false); await settle()
			var menu: PanelContainer = main.find_child("SystemMenu",true,false)
			menu.show(); await settle()
			var toolbar: HBoxContainer = main.find_child("HudActions",true,false)
			for action in toolbar.get_children():
				check(action.get_global_rect().end.x <= main.size.x + 1, "窄逻辑视口HUD操作入口不越界 %s/%d/%s" % [dimensions,value,action.text])
			var selector: OptionButton = main.find_child("FontScaleSelector",true,false)
			check(menu.get_global_rect().end.x <= main.size.x + 1 and menu.get_global_rect().end.y <= main.size.y + 1, "菜单不越过逻辑视口 %s/%d: %s" % [dimensions,value,menu.get_global_rect()])
			check(selector.get_theme_font_size("font_size") == roundi(18 * value / 100.0), "菜单真实字号 %s/%d" % [dimensions,value])
			var scroll: ScrollContainer = main.find_child("SystemMenuScroll",true,false)
			menu.size.y = 180; await settle()
			scroll.ensure_control_visible(selector); await settle()
			check(selector.get_global_rect().end.y <= scroll.get_global_rect().end.y + 1, "限高菜单可滚到底部字号选择器")
			menu.hide()
	main.size = original
