## 原地图控件的有限模式提示；只验证文字与可点击状态，不创建世界或代替真实Main验收。
extends SceneTree
const Screen=preload("res://ui/world_map_screen.gd")
const Journey=preload("res://ui/journey_view.gd")
var checks: int=0
var failures: int=0

## 没有resident字段的旧消费者仍能绘制原地图；夹具不提供任何世界写入接口。
class LegacyHost extends Node:
	var journey_view: Node=Journey.new()
	var _data: Dictionary={"settlements":{"town":{"display_name":"城镇"},"other":{"display_name":"远村"}}}
	var _message: String=""
	var _advancing: bool=false
	var status_label: Label
	var closed: int=0
	var confirmed: int=0
	var reasons: Array=[]
	## 原实际Label和Button承载显示及禁用状态。
	func _text(parent: Node,value: String,_size: int=20) -> Label:
		var label=Label.new(); label.text=value; parent.add_child(label); return label
	## 保留既有语义标记与真实pressed连接，便于检查入口。
	func _button(parent: Node,label: String,kind: String,_target: String,callback: Callable,disabled: bool=false) -> Button:
		var button=Button.new(); button.text=label; button.disabled=disabled; button.set_meta("command_kind",kind)
		button.pressed.connect(callback); parent.add_child(button); return button
	## 名称只来自传入的有限目录记录。
	func _name(record: Dictionary) -> String: return record.display_name
	## 真实规则拒绝必须仍通过既有文案入口，未知码保留原值。
	func _reason(code: String) -> String:
		reasons.append(code); return {"NotAtCityExit":"请先走到城门，再安排出行","NoRoute":"当前没有可通行的路线"}.get(code,code)
	## 只记录控件是否仍连接旧关闭入口。
	func _close_map() -> void: closed+=1
	## 只记录确认入口，不提供真实命令执行。
	func _map_confirm() -> void: confirmed+=1
	## 本门不触发途经点编辑；保留原Callable形状。
	func _map_add_via() -> void: pass
	## 本门不触发途经点编辑；保留原Callable形状。
	func _map_clear_via() -> void: pass

## 显式resident模式只改变当前获准的两处提示。
class ResidentHost extends LegacyHost:
	var _resident_mode: bool=true

## 延迟到SceneTree初始化后挂载实际控件。
func _initialize() -> void: call_deferred("_run")

## 每个失败保留具体合同说明，脚本错误另由运行日志检查。
func check(value: bool,label: String) -> void:
	checks+=1
	if not value: failures+=1; push_error(label)

## 构造只有地图populate需要的同源显示字段。
func draw(host: Node,quote: Dictionary) -> VBoxContainer:
	var box=VBoxContainer.new(); host.add_child(box)
	Screen.populate(host,box,{"location":{"kind":"Settlement","settlement_id":"town"},"travel":{"active":false},"encounter":{},"party_present":false,"companions":[],"logistics_quote":{}},quote)
	return box

## 按原语义标记查实际Button，不依赖布局位置。
func button(parent: Node,kind: String) -> Button:
	for child in parent.get_children():
		if child is Button and child.get_meta("command_kind","")==kind: return child
		var found: Button=button(child,kind)
		if found!=null: return found
	return null

## 文案只读取控件树，不检查实现源码。
func labels(parent: Node) -> PackedStringArray:
	var found: PackedStringArray=[]
	for child in parent.get_children():
		if child is Label: found.append(child.text)
		found.append_array(labels(child))
	return found

## 依次验证旧宿主、原生等待、真实拒绝与成功报价，不重复旅程或性能门。
func _run() -> void:
	var legacy=LegacyHost.new(); root.add_child(legacy); legacy.add_child(legacy.journey_view)
	var old_box: VBoxContainer=draw(legacy,{"ok":false,"code":"InvalidDestination"})
	var close: Button=button(old_box,"map_close")
	check(close!=null and not close.disabled and close.text=="返回所在场景","无新属性的legacy仍保留原关闭入口")
	close.pressed.emit(); check(legacy.closed==1,"legacy关闭Callable仍生效")
	legacy.journey_view.selected="other"
	var old_error: VBoxContainer=draw(legacy,{"ok":false,"code":"NativeAsyncRequired"})
	check("NativeAsyncRequired" in labels(old_error),"legacy未知错误原样保留，不被resident等待文案覆盖")
	var resident=ResidentHost.new(); root.add_child(resident); resident.add_child(resident.journey_view)
	var resident_box: VBoxContainer=draw(resident,{"ok":false,"code":"InvalidDestination"})
	close=button(resident_box,"map_close")
	check(close!=null and close.disabled and close.text=="城内场景暂不可用","resident城内入口在点击前明确不可用")
	check(button(resident_box,"map_confirm")==null,"没有目标时不出现确认按钮")
	resident.journey_view.selected="other"
	var pending: VBoxContainer=draw(resident,{"ok":false,"code":"NativeAsyncRequired"})
	check("正在查看通路与费用…" in labels(pending) and not "NativeAsyncRequired" in labels(pending),"等候真实预览显示状态文案")
	check(button(pending,"map_confirm").disabled and resident.reasons.is_empty(),"等待期间不能确认，也不误当规则拒绝")
	for code in ["NotAtCityExit","NoRoute","UnexpectedRuleCode"]:
		var rejected: VBoxContainer=draw(resident,{"ok":false,"code":code})
		var expected: String={"NotAtCityExit":"请先走到城门，再安排出行","NoRoute":"当前没有可通行的路线"}.get(code,code)
		check(expected in labels(rejected) and resident.reasons[-1]==code and button(rejected,"map_confirm").disabled,"保留真实拒绝与诊断码："+code)
	var ready: VBoxContainer=draw(resident,{"ok":true,"duration":3,"cost":27})
	check("预计 3个半日\n余程费用 27钱" in labels(ready) and not button(ready,"map_confirm").disabled,"成功quote保持原费用与可确认状态")
	button(ready,"map_confirm").pressed.emit(); check(resident.confirmed==1,"成功quote仍连接唯一原确认入口")
	check(resident.journey_view.selected=="other" and resident.closed==0,"纯绘制不修改选择也不关闭当前地图")
	legacy.queue_free(); resident.queue_free(); await process_frame
	print("RESIDENT_MAP_AFFORDANCE_CHECKS=",checks," FAILURES=",failures," WORLD_OPERATIONS=0")
	quit(0 if failures==0 else 1)
