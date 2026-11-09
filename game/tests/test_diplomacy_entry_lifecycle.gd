## 实际外交控件的局部生命周期门；只给有限展示DTO，不创建玩法世界或声称GUI视觉验收。
extends SceneTree
const DiplomacyEntry=preload("res://ui/diplomacy_entry.gd")
var checks: int=0
var failures: int=0

## 来源变化模拟原Backend身份；本夹具没有提交或领域写接口。
class Kernel extends RefCounted:
	var identity: Dictionary={"world_id":"fixture","execution_epoch":1,"revision":1,"tick":0}
	func source_identity() -> Dictionary: return identity.duplicate(true)
	func diplomacy_enabled() -> bool: return true
	func presentation_location() -> Dictionary: return {"place_id":"here"}
	func diplomacy_entries(_place: String) -> Dictionary: return {"ok":true,"rows":[{"id":"local","source_fact_id":"known","display_name":"通行商议","text":"提议已呈递","terms_text":"尚待明确答复","actions":[]}]}

## 使用真实Button信号并保留回调，重现宿主已释放后的延迟调用。
class Host extends Control:
	var kernel=Kernel.new()
	var _resident_mode: bool=true
	var _started: bool=true
	var _scene_counter: int=1
	var _advancing: bool=false
	var rendered: int=0
	var delayed: Callable
	func _text(parent: Node, text: String, _size: int) -> void:
		var label=Label.new(); label.text=text; parent.add_child(label)
	func _button(parent: Node, text: String, _kind: String, _id: String, callback: Callable, disabled: bool=false) -> Button:
		var button=Button.new();button.text=text;button.disabled=disabled;button.pressed.connect(callback);parent.add_child(button);delayed=callback;return button
	func _regional_button(_parent: Node, _text: String, _kind: String, _id: String, _params: Dictionary, _dialogue: Dictionary, _disabled: bool) -> Button:
		return null
	func _render() -> void: rendered+=1

## 失败累计进真实退出码；引擎SCRIPT ERROR另由调用端作为失败核验。
func check(value: bool, label: String) -> void:
	checks+=1
	if not value: failures+=1;printerr("FAIL: ",label)

func _initialize() -> void: call_deferred("run")

## 同源可展开，忙碌/新场景/新来源失效；已释放宿主回调必须安全返回。
func run() -> void:
	var host=Host.new();root.add_child(host)
	var panel=VBoxContainer.new();host.add_child(panel)
	DiplomacyEntry.local_affairs(host,panel)
	var button: Button=panel.find_children("*","Button",true,false)[0]
	button.pressed.emit();check(host.rendered==1 and host.get_meta("diplomacy_expanded").get("local:known"),"same-source control expands actual terms")
	host._advancing=true;button.pressed.emit();check(host.rendered==1,"busy host rejects old toggle")
	host._advancing=false;host._scene_counter=2;button.pressed.emit();check(host.rendered==1,"new scene rejects old toggle")
	host._scene_counter=1;host.kernel.identity.revision=2;button.pressed.emit();check(host.rendered==1,"new source rejects old toggle")
	var delayed: Callable=host.delayed
	host.free();check(not is_instance_valid(host),"host is actually freed")
	delayed.call();check(true,"retained control callback returns after host release")
	print("DIPLOMACY_ENTRY_LIFECYCLE checks=%d failures=%d headless real-control fixture"%[checks,failures])
	quit(1 if failures else 0)
