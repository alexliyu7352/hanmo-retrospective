## 原始随行/随军位置不带settlement_id；地图仍显示真实位置类别，不凭空提供场所返回。
extends SceneTree
const Screen=preload("res://ui/world_map_screen.gd")
var checks: int=0
var failures: int=0

## 仅提供已验证的有限空闲侧栏，不制造军令、地点或世界命令。
class Kernel extends RefCounted:
	func preview(_kind: String,_params: Dictionary={}) -> Dictionary: return {"ok":false,"code":"NotLeader"}

## 页面会读取的原旅程选择状态；不拥有共同位置。
class Navigation extends RefCounted:
	var selected: String=""
	var selected_point: Array=[]
	var via: Array=[]

## 本门只核旅行形态，不发起自动继续。
class Continuation extends RefCounted:
	static func render(_host,_parent,_kind) -> void: pass

## 记录真实控件和错误后未能完成的渲染，用于复现缺失raw位置字段。
class Host extends Node:
	var ActivityContinuationControls=Continuation
	var kernel=Kernel.new()
	var journey_view=Navigation.new()
	var _resident_mode: bool=true
	var _advancing: bool=false
	var _message: String=""
	var _data: Dictionary={"settlements":{"town":{"name":"原作者城邑"}}}
	var status_label: Label
	var words: Array=[]
	var commands: Array=[]
	func _text(parent: Node,text: String,_pixels: int=18) -> Label:
		words.append(text);var label=Label.new();label.text=text;parent.add_child(label);return label
	func _button(parent: Node,text: String,kind: String,_target: String,_callback: Callable,_disabled: bool=false) -> Button:
		commands.append(kind);var button=Button.new();button.text=text;parent.add_child(button);return button
	func _name(value: Dictionary) -> String: return value.name
	func _perform(_kind: String,_params: Dictionary) -> void: pass
	func _close_map() -> void: pass

## 在真实节点树建立后读取生产Screen的控件结果。
func _init() -> void: call_deferred("_run")

## 失败留具体raw位置类别，脚本错误导致后续内容缺失也不能误报通过。
func check(value: bool,label: String) -> void:
	checks+=1
	if not value: failures+=1;printerr("FAIL: ",label)

## 同一正式populate路径覆盖两种依附位置与正常城内返回。
func _run() -> void:
	for kind in ["Following","InArmy","Settlement"]:
		var host=Host.new();root.add_child(host);var box=VBoxContainer.new();host.add_child(box)
		var location: Dictionary={"kind":kind}
		if kind=="Settlement": location.merge({"settlement_id":"town","place_id":"entry"})
		var sidebar: Dictionary={"location":location,"travel":{"active":false},"encounter":{},"party_present":false,"companions":[],"logistics_quote":{}}
		Screen.populate(host,box,sidebar,{})
		var expected: String={"Following":"所在 · 跟随同行","InArmy":"所在 · 随军同行","Settlement":"所在 · 原作者城邑"}[kind]
		check(expected in host.words,"有限raw位置可读："+kind)
		check("点选据点或原野，先查看通路与费用。" in host.words,"地图详情完整渲染："+kind)
		check(("map_close" in host.commands)==(kind=="Settlement"),"只有真实场所位置提供返回场景："+kind)
		host.queue_free()
	for kind in ["Following","InArmy"]:
		for terrain in [true,false]:
			var host=Host.new();root.add_child(host);var box=VBoxContainer.new();host.add_child(box)
			host._data.settlements["target"]={"name":"原目标城"};host._data["routes"]={"edge":{"origin":"town","destination":"target","duration_ticks":3}}
			var travel: Dictionary={"active":true,"paused":false}
			if terrain: travel.merge({"destination":"target","remaining_ticks":2})
			else: travel.merge({"edge_id":"edge","progress":100})
			var sidebar: Dictionary={"location":{"kind":kind},"travel":travel,"encounter":{},"party_present":false,"companions":[],"logistics_quote":{}}
			var summary: String=Screen._travel_summary(host,sidebar)
			check("原目标城" in summary,"附属行程摘要按真实旅行形态："+kind+str(terrain))
			Screen.populate(host,box,sidebar,{},false)
			check(("前往 · 原目标城" if terrain else "原作者城邑 → 原目标城") in host.words,"附属行程详情按真实旅行形态："+kind+str(terrain))
			host.queue_free()
	await process_frame
	print("WORLD_MAP_FOLLOWING_CONTEXT_CHECKS=",checks," FAILURES=",failures," SOURCE=FINITE_RAW_LOCATION GUI=NOT_TESTED WORLD_COMMANDS=0")
	quit(0 if failures==0 else 1)
