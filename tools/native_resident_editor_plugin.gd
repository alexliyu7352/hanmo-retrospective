## 只在隔离stage注册原生内容导出补充器，不运行游戏、创建世界或改存档。
@tool
extends EditorPlugin
const Exporter = preload("res://addons/hanmo_resident_export/export_plugin.gd")
var _exporter: EditorExportPlugin

## 沿Godot标准插件接口注册本次导出规则。
func _enter_tree() -> void:
	_exporter = Exporter.new()
	add_export_plugin(_exporter)

## 编辑器关闭时移除当前实例，不留下全局插件状态。
func _exit_tree() -> void:
	if _exporter != null:
		remove_export_plugin(_exporter)
		_exporter = null
