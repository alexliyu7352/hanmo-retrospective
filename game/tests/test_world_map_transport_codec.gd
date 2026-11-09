## 直接消费S3实际原生codec输出，核对两域行形状；该来源仍明确为局部fixture。
extends SceneTree
const Fixture=preload("res://tests/test_world_map_presence.gd")
const Transport=preload("res://ui/world_map_transport.gd")
const StrictJSON=preload("res://core/strict_json.gd")
var checks: int=0
var failures: int=0

## 保留producer发来的本人身份，不把测试固定名字当运行身份。
class Kernel extends Fixture.Kernel:
	var observer: String=""
	func command_context() -> Dictionary:return {"actor_id":observer}

## 初始化真实节点树后加载明确传入的原codec文件。
func _init() -> void:call_deferred("_run")

## 每项断言注明生产/消费界面而非虚构全游戏验收。
func check(value: bool,label: String) -> bool:
	checks+=1
	if not value:failures+=1;printerr("FAIL: ",label)
	return value

## 只收集实际渲染标签附带的有限原行。
func rows(node: Node) -> Array:
	var result: Array=[]
	if node.has_meta("map_transport_row"):result.append(node.get_meta("map_transport_row"))
	for child in node.get_children():result.append_array(rows(child))
	return result

## 对玩家实际看见的文字核时态，不只比较标签附带的机器元数据。
func visible_words(node: Node) -> String:
	var result: String=node.text+"\n" if node is Label else ""
	for child in node.get_children():result+=visible_words(child)
	return result

## 原任务codec→现DemandView消费→实际列表逐值一致，不做映射修补或数量合计。
func _run() -> void:
	var input: String=""
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--input="):input=arg.trim_prefix("--input=")
	if not check(FileAccess.file_exists(input),"原生codec样本实际存在"):quit(1);return
	var parsed: Dictionary=StrictJSON.parse_text(FileAccess.get_file_as_string(input))
	if not check(parsed.get("ok",false) and parsed.get("value") is Dictionary,"原严格JSON读取保整数语义"):quit(1);return
	var page: Dictionary=parsed.value
	var host=Fixture.Host.new();host.kernel=Kernel.new();root.add_child(host)
	host.kernel.observer=page.observer_actor_id;host.kernel.source=page.identity.duplicate(true);host.kernel.result=page.duplicate(true)
	check(Transport.prepare(host) and host.get_meta(Transport.META).page==page,"实际producer完整DTO无需改写即可通过消费验证")
	var stage=Control.new();host.add_child(stage);stage.size=Vector2(960,720)
	var map_script=load("res://tests/test_world_map_transport.gd");var canvas=map_script.Map.new();stage.add_child(canvas)
	var panel=Transport.attach(host,stage,canvas,func():return true)
	if check(panel!=null,"正式控件接住同一codec样本"):
		canvas.transport_requested.emit();await process_frame
		check(rows(panel)==page.rows,"货批身份/数量/预留与护送尚未启程逐值保持")
		var words: String=visible_words(panel)
		check("正在履行" in words and "尚未启程" in words and "已预留" in words,"玩家文字保留真实委托状态、货批预留与护送时态")
		check(panel.visible and stage.get_global_rect().encloses(panel.get_global_rect()),"真实清单可展开且保持视口内")
	host.queue_free();await process_frame
	print("MAP_TRANSPORT_CODEC_CHECKS=",checks," FAILURES=",failures," SOURCE=ACTUAL_NATIVE_CODEC_FIXTURE GUI=NOT_TESTED FORMAL_NATIVE_WORLD=NOT_RUN")
	quit(0 if failures==0 else 1)
