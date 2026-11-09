## 实际S3 codec样本的有限地图消费；没有原生World或业务命令。
extends SceneTree
const Fixture=preload("res://tests/test_world_map_presence.gd")
const Obligations=preload("res://ui/world_map_obligations.gd")
const Journey=preload("res://ui/journey_view.gd")
const NATIVE_FIXTURE: Dictionary={"ok":true,"version":1,"identity":{"world_id":"0b000000000000000000000000000000","execution_epoch":3,"revision":4,"tick":7},"observer_actor_id":"player","destination_settlement_id":"city","cursor":0,"scanned":6,"rows":[{"task_id":"purchase","display_name":"委托:purchase","template_id":"procurement","task_status":"active","place_id":"office","place_name":"office","target_kind":"report","deadline_scope":"delivery_completion","deadline_tick":12,"deadline_inclusive":true,"minimum_handover_ticks":1,"requires_prior_objective":false},{"task_id":"escort","display_name":"委托:escort","template_id":"escort","task_status":"active","place_id":"school","place_name":"县学","target_kind":"escort_arrival","deadline_scope":"report_completion","deadline_tick":12,"deadline_inclusive":true,"minimum_handover_ticks":1,"requires_prior_objective":true}]}
var checks: int=0
var failures: int=0
class Host extends Node:
 var kernel=Fixture.Kernel.new()
 var _resident_mode: bool=true
 var _scene_counter: int=1
 var journey_view=Journey.new()
 var renders: int=0
 var _data: Dictionary={"settlements":{"city":{},"other":{}}}
 ## 只记录重绘请求，不模拟任何任务或旅行规则。
 func _render() -> void:renders+=1
 ## 返回原拒绝码供控件元数据诊断。
 func _reason(code: String) -> String:return code
 ## 使用真实Label检查有限回执的玩家文案。
 func _text(parent: Node,text: String,pixels: int=18) -> Label:
  var label=Label.new();label.text=text;label.add_theme_font_size_override("font_size",pixels);parent.add_child(label);return label
 ## 使用真实Button和原语义元数据检查旧回调。
 func _button(parent: Node,text: String,kind: String,_target: String,callback: Callable) -> Button:
  var button=Button.new();button.text=text;button.set_meta("command_kind",kind);button.pressed.connect(callback);parent.add_child(button);return button
## 等控件环境就绪，再检查原生样本与旧按钮的消费边界。
func _init() -> void:call_deferred("_run")
## 失败保留明确的合同语义或来源边界。
func check(value: bool,label: String) -> void:
 checks+=1
 if not value:failures+=1;printerr("FAIL: ",label)
## 按原语义标记查找真实按钮，不依赖文字匹配或节点排列。
func button(node: Node,kind: String) -> Button:
 if node is Button and node.get_meta("command_kind","")==kind:return node
 for child in node.get_children():
  var found: Button=button(child,kind)
  if found!=null:return found
 return null
## 只验证本人目的城、截止含义、完整游标及实际释放，不构造任务世界。
func _run() -> void:
 var host=Host.new();root.add_child(host);host.add_child(host.journey_view);host.kernel.source=NATIVE_FIXTURE.identity.duplicate(true)
 host.journey_view.opened=true;host.journey_view.selected="city";host.journey_view.selected_point=[]
 var page: Dictionary=NATIVE_FIXTURE.duplicate(true);page.observer_actor_id="hero";host.kernel.result=page
 host.kernel.pending=true;check(not Obligations.prepare(host),"等待原异步屏障")
 host.kernel.pending=false;check(Obligations.prepare(host),"实际codec两种目标语义可读")
 var source: Dictionary=host.get_meta(Obligations.META).source
 check(host.kernel.requests[-1].args=={"destination_settlement_id":"city"},"首读仅目的城不带远方人物")
 var bad: Dictionary=page.duplicate(true);bad.destination_settlement_id="other";check(not Obligations._validate(bad,source).ok,"拒绝另一目的城页")
 bad=page.duplicate(true);bad.identity.execution_epoch+=1;check(not Obligations._validate(bad,source).ok,"拒绝旧epoch")
 bad=page.duplicate(true);bad.rows[1].deadline_scope="delivery_completion";check(not Obligations._validate(bad,source).ok,"护送到达不能冒充完整交付截止")
 bad=page.duplicate(true);bad.rows[0].deadline_tick=12.5;check(not Obligations._validate(bad,source).ok,"拒绝非原生整数期限")
 var quote: Dictionary={"ok":true,"revision":source.identity.revision,"duration":5}
 var purchase: String="\n".join(Obligations.lines(page.rows[0],source,quote))
 var escort: String="\n".join(Obligations.lines(page.rows[1],source,quote))
 check(purchase.contains("余时不足最低办理耗时") and purchase.contains("交付完成截止"),"到城恰好截止仍提示办理耗时不足")
 check(escort.contains("到达后仍须回报") and escort.contains("回报完成截止") and not escort.contains("余时不足"),"护送行只借原回报截止不承诺抵达履约")
 var cursor: Dictionary={"identity":source.identity.duplicate(true),"observer_actor_id":"hero","destination_settlement_id":"city","offset":6}
 page.next_cursor=cursor;host.kernel.result=page;Obligations.prepare(host)
 var parent=VBoxContainer.new();host.add_child(parent);var guard: Callable=func():return true
 Obligations.append(host,parent,quote,guard)
 check(parent.get_child_count()>2,"实际侧栏控件显示有限行")
 var next: Button=button(parent,"map_obligations_next")
 var old_callback: Callable=next.pressed.get_connections()[0].callable
 next.pressed.emit();check(host.get_meta(Obligations.META).cursor==cursor and host.renders==1,"翻页原样交回完整游标")
 host.journey_view.selected="other";host.kernel.result=page.duplicate(true);host.kernel.result.destination_settlement_id="other";host.kernel.result.erase("next_cursor")
 Obligations.prepare(host);check(not host.kernel.requests[-1].args.has("cursor"),"换城清游标")
 old_callback.call();check(host.renders==1,"旧城回调不改新草稿")
 host.journey_view.selected_point=[1,2];Obligations.prepare(host);check(not host.has_meta(Obligations.META),"原野不查询或保留合同目的地")
 host.queue_free();await process_frame;old_callback.call()
 check(not is_instance_valid(host),"真实释放后迟到回调安全终止")
 print("WORLD_MAP_OBLIGATIONS_CHECKS=",checks," FAILURES=",failures," SOURCE=S3_CODEC_FIXTURE WORLD_COMMANDS=0 GUI=NOT_TESTED")
 quit(0 if failures==0 else 1)
