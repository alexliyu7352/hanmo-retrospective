## 本人军团摘要的地图图例/详情入口；夹具不创建军令或模拟行军结算。
extends SceneTree
const Fixture=preload("res://tests/test_world_map_presence.gd")
const Company=preload("res://ui/world_map_company.gd")
var checks: int=0
var failures: int=0

## 只计进入原军旅页的导航，不提供军事业务命令。
class Host extends Fixture.Host:
	var camp_opens: int=0
	func _open_military_camp() -> void:camp_opens+=1

## 地图夹具仅容纳当前位置图例。
class Map extends Control:
	var observer_label: String="所在位置"

## 等控件树正常初始化。
func _init() -> void:call_deferred("_run")

## 失败保留权限或场景来源说明。
func check(value: bool,label: String) -> void:
	checks+=1
	if not value:failures+=1;printerr("FAIL: ",label)

## 使用本人已存在的同一军团来源，禁止由数量或政治身份推导军令。
func page(host,commander: bool) -> Dictionary:
	return {"ok":true,"version":1,"identity":host.kernel.source_identity(),"observer_actor_id":"hero","army":{"army_id":"own","commander_actor_id":"hero" if commander else "leader","commander_name":"本军统帅","commanded_by_observer":commander,"observed_tick":3}}

## 实际控件核统领/随军区别、陈旧输入拒绝及正常空值。
func _run() -> void:
	var host=Host.new();root.add_child(host);host.kernel.result=page(host,true)
	check(Company.prepare(host) and host.kernel.requests[-1]=={"kind":"map_company","args":{}},"军团查询只读当前本人无任意军团参数")
	var stage=Control.new();host.add_child(stage);var canvas=Map.new();stage.add_child(canvas);var box=VBoxContainer.new();stage.add_child(box)
	Company.bind(host,stage,canvas)
	var allowed: Dictionary={"value":true};Company.append(host,box,func():return allowed.value)
	check(canvas.observer_label=="所领军团","本人统领时原地图锚点标明军团身份")
	var manage: Button=box.get_child(box.get_child_count()-1)
	var saved_manage: Callable=manage.get_signal_connection_list("pressed")[0].callable
	check(manage.get_meta("command_kind")=="map_company_manage","原军旅入口具名可查")
	manage.pressed.emit();allowed.value=false;manage.pressed.emit();check(host.camp_opens==1,"导航走原入口且旧输入守卫拒绝")
	var invalid: Dictionary=page(host,true);invalid.army.commander_actor_id="leader"
	check(not Company._validate(invalid,host.get_meta(Company.META).source).ok,"本人统领标记不可与真实统帅身份冲突")
	host.kernel.result=page(host,false);Company.prepare(host);var member_box=VBoxContainer.new();stage.add_child(member_box);Company.bind(host,stage,canvas);Company.append(host,member_box,func():return true)
	check(stage.get_signal_connection_list("tree_exiting").size()==1,"同代场景重绑只留一份军团清理回调")
	var saved_exit: Callable=stage.get_signal_connection_list("tree_exiting")[0].callable
	check(canvas.observer_label=="所随军团" and member_box.get_child_count()==2 and member_box.get_child(0) is Label,"随军者仍见真实统帅和独立查看入口")
	var member_open: Button=member_box.get_child(1)
	check(member_open.text=="查看军旅" and member_open.get_meta("command_kind")=="map_company_manage","部将入口只表明查看军旅，不宣称统领权")
	member_open.pressed.emit();check(host.camp_opens==2,"部将只进入同一原营务页，不执行军令")
	host.kernel.source.revision+=1;var stale_box=VBoxContainer.new();stage.add_child(stale_box);Company.append(host,stale_box,func():return true)
	check(stale_box.get_child_count()==0,"旧来源军团摘要不挂到新世界")
	host.kernel.result=page(host,false);host.kernel.result.army={};Company.prepare(host);var empty_box=VBoxContainer.new();stage.add_child(empty_box);Company.append(host,empty_box,func():return true)
	check(empty_box.get_child_count()==0,"独行来源不制造军团详情")
	host.queue_free();await process_frame
	saved_manage.call();saved_exit.call();check(not is_instance_valid(host),"真正释放宿主后军旅旧导航及清理安静失效")
	print("MAP_COMPANY_UI_CHECKS=",checks," FAILURES=",failures," SOURCE=FINITE_DTO_FIXTURE GUI=NOT_TESTED COMMANDS=0")
	quit(0 if failures==0 else 1)
