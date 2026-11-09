## 用实际地图侧栏核同源预计到达与地形报价；有限投影不模拟旅行提交或GUI验收。
extends "res://tests/test_world_map_scene.gd"

## HUD只给本次原半日及本人版本，测试可明确替换为过期来源。
class ClockKernel extends MapKernel:
	var hud: Dictionary={"ok":true,"actor_id":"hero","revision":2,"day_index":0,"half_day":1}
	## 返回有限原HUD形状，不提供时间推进方法。
	func hud_view(_battle: String="") -> Dictionary:return hud.duplicate(true)

## 报价保持原字段；只用于侧栏显示，不新增业务命令。
class QuoteHost extends MapHost:
	var quote: Dictionary={"ok":true,"duration":3,"cost":24,"path":["r"]}
	## 保原报价值，界面不能改写它。
	func _map_quote() -> Dictionary:return quote.duplicate(true)

## 每次创建生产侧栏控件，取可见文本后释放，避免测试只镜像格式化实现。
func route_text(host, context: Dictionary) -> String:
	var box=VBoxContainer.new();host.add_child(box)
	Screen.populate(host,box,context,host._map_quote(),false)
	var words: String=""
	for node in box.get_children():
		if node is Label: words+=node.text+"\n"
	box.free();return words

## 下午出发跨日、旧HUD拒绝、地形专属字段和无副作用分别保留原接口边界。
func _run() -> void:
	var host=QuoteHost.new();host.kernel=ClockKernel.new();root.add_child(host);host.add_child(host.journey_view);host.journey_view.selected="b"
	var context: Dictionary=sidebar(false);context.actor_id="hero";context.revision=2
	var before: Dictionary=host.quote.duplicate(true)
	var text: String=route_text(host,context)
	check(text.contains("预计到达 第3日上半日"),"第1日下半日加3个半日沿原时钟跨到第3日上半日")
	check(text.contains("余程费用 24钱") and not text.contains("负重"),"普通道路保原费用，不伪造地形负重或运力")
	host.kernel.hud.revision=1
	check(not route_text(host,context).contains("预计到达"),"旧版本HUD不能与新路线拼到达日期")
	host.kernel.hud.revision=2;host.kernel.hud.actor_id="other"
	check(not route_text(host,context).contains("预计到达"),"别人的HUD不用于当前本人路线")
	host.kernel.hud.actor_id="hero";host.kernel.hud.half_day=2
	check(not route_text(host,context).contains("预计到达"),"非法半日不补造成合法日期")
	host.kernel.hud.half_day=0;host.kernel.hud.day_index=3
	host.quote={"ok":true,"duration":1,"cost":8,"terrain":true,"mode":"army","starting_load":7,"fatigue_per_tick":3}
	text=route_text(host,context)
	check(text.contains("预计到达 第4日下半日") and text.contains("军旅行进 · 启程负重 7") and text.contains("每半日疲劳 +3"),"地形行旅显示原模式负重和疲劳，不自行推算运力")
	host.quote={"ok":false,"code":"RouteBlocked"}
	text=route_text(host,context)
	check(text.contains("RouteBlocked") and not text.contains("预计到达"),"拒绝报价只保原拒绝，不留下旧估计")
	check(host.actions.is_empty() and before=={"ok":true,"duration":3,"cost":24,"path":["r"]},"查看路线不提交命令或改写原报价")
	host.queue_free();await process_frame
	print("WORLD_MAP_ROUTE_CONTEXT_CHECKS=",checks," FAILURES=",failures," SOURCE=FINITE_HUD_AND_QUOTE GUI=NOT_TESTED WORLD_COMMANDS=0")
	quit(0 if failures==0 else 1)
