@tool
## Register the dedicated author workbench as a Godot main-screen plugin; never ship it in player builds.
extends EditorPlugin
var panel: Control
## Install the content workbench using the shared author-process model.
func _enter_tree() -> void:
	panel=preload("res://addons/hanmo_author/panel.gd").new()
	get_editor_interface().get_editor_main_screen().add_child(panel)
	panel.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	panel.hide()
## Identify the plugin's main-screen tab.
func _get_plugin_name() -> String:
	return "内容工坊"
## Expose a main-screen editor rather than an unrelated web administration interface.
func _has_main_screen() -> bool:
	return true
## Make the author panel follow the Godot selected editor tab.
func _make_visible(visible: bool) -> void:
	if is_instance_valid(panel): panel.visible=visible
## Release the workbench and its child process on disable or editor shutdown.
func _exit_tree() -> void:
	if is_instance_valid(panel): panel.queue_free()
