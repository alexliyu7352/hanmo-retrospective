## 原Backend的当前通路异步入口窄门；有限缓存夹具不安装World或借地图读取远方人物。
extends SceneTree
var checks: int=0
var failures: int=0

## 测试只替换脚本来源以核中央候选，默认使用当前工程的生产Backend。
func _init() -> void:call_deferred("_run")

## 每项断言保留明确的缓存、来源或场所边界。
func check(value: bool,label: String) -> void:
	checks+=1
	if not value:failures+=1;printerr("FAIL: ",label)

## 创建未挂树、无Bridge的有限已发布页，复用真实Backend排队与旧来源检查。
func backend(script: Script,location: Dictionary,legacy: bool=true) -> Node:
	var value=script.new()
	value._installed=true
	value._source={"world_id":"00112233445566778899aabbccddeeff","execution_epoch":1,"revision":2,"tick":3}
	value._published={"source":value._source.duplicate(true),"views":{"screen_context":{"ok":true,"world_scene_legacy":legacy,"presentation_location":location}}}
	return value

## 当前城内转交真实有限查询，旧原野/在途回退、异步等待与过期拒绝分别保持。
func _run() -> void:
	var path: String="res://ui/native_resident_backend.gd"
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--backend-script="):path=arg.trim_prefix("--backend-script=")
	var script: Script=load(path)
	var current=backend(script,{"kind":"Settlement","place_id":"courtyard"})
	var waiting: Dictionary=current.scene_view("courtyard","world")
	check(waiting.get("code")=="NativeAsyncRequired" and current._pending_reads.size()==1,"当前城内无模板仍等待真实有限投影")
	if not current._pending_reads.is_empty():
		check(current._pending_reads[0].payload=={"query_id":"scene_view","args":{"place_id":"courtyard","view_kind":"world"}},"仅转交原当前场所与world参数")
	current.scene_view("courtyard","world")
	check(current._pending_reads.size()==1,"同源重复渲染不会重复排队")
	check(current.scene_view("remote","world").get("code")=="UnsupportedNativeCapability","无模板快路仍拒绝非当前场所")
	current.free()
	for kind in ["Wilderness","OnRoute","Following","InArmy"]:
		var value=backend(script,{"kind":kind})
		check(value.scene_view("","world")=={"ok":true,"visibility":"current"} and value._pending_reads.is_empty(),kind+"无场所保持合法地图回退")
		if kind in ["Following","InArmy"]:
			check(value.scene_view("remote","world").get("code")=="UnsupportedNativeCapability" and value._pending_reads.is_empty(),kind+"不借附随身份读取任意场所")
		value.free()
	var mapped=backend(script,{"kind":"Settlement","place_id":"courtyard"},false)
	check(mapped.scene_view("courtyard","world").get("code")=="NativeAsyncRequired" and mapped._pending_reads.size()==1,"已有模板继续原投影队列")
	mapped.free()
	var stale=backend(script,{"kind":"Settlement","place_id":"courtyard"})
	stale._source.revision+=1
	check(stale.scene_view("courtyard","world").get("code")=="StalePreview" and stale._pending_reads.is_empty(),"旧发布来源不能发起新通路读取")
	stale.free()
	print("MAP_SCENE_BACKEND_CHECKS=",checks," FAILURES=",failures," SOURCE=FINITE_BACKEND_CACHE WORLD_INSTANCES=0 GUI=NOT_TESTED")
	quit(0 if failures==0 else 1)
