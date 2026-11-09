## 用真实prepare函数核附随位置分流；只提供有限DTO和读口，不安装World或授予行动。
extends SceneTree
var checks: int=0
var failures: int=0

## 当前页读口记录请求，world投影尚未到达时保原异步等待。
class Kernel extends RefCounted:
	var location: Dictionary={}
	var scene_requests: Array=[]
	var place_reads: int=0
	## 无个人战抢占，仍经过原页面优先级判断。
	func personal_battle_view() -> Dictionary:return {"ok":true,"battle_id":""}
	## 无野战活动，不短路被测的地图分流。
	func military_battle_page() -> Dictionary:return {"ok":true,"active":false}
	## 无围城活动，不提供任何军事世界。
	func siege_page() -> Dictionary:return {"ok":true,"siege_id":""}
	## 此夹具的首页手动窗口已齐，不请求或提交时间推进。
	func demand_view(_query: String,_args: Dictionary={}) -> Dictionary:return {"ok":true,"manual":true}
	## 未启用计划页，地图分支不依赖自动活动。
	func player_plan_enabled() -> bool:return false
	## 没有本人长行动，附随位置与行动占用分别表达。
	func action_context() -> Dictionary:return {"ok":true,"action_kind":""}
	## 无学习活动，不借其它页面隐藏无place输入。
	func study_activity() -> Dictionary:return {"ok":true,"active":false}
	## 原始互斥位置保留kind，不补造place_id。
	func presentation_location() -> Dictionary:return location.duplicate(true)
	## 只收空place的world请求，原生结果未就绪不能发布半页。
	func scene_view(place: String,kind: String) -> Dictionary:
		scene_requests.append({"place_id":place,"view_kind":kind})
		return {"ok":false,"code":"NativeAsyncRequired"}
	## 附随态若错误走本地页，以有限明确首错终止，不补造place。
	func place_sidebar() -> Dictionary:
		place_reads+=1
		return {"ok":false,"code":"FixtureLocalPageReached"}

## 与Main同名的只读路由字段；没有submit或世界更新方法。
class Host extends Node:
	## 夹具没有需要活动条接续的本人行动类别。
	class ActivityModes extends RefCounted:
		const KINDS: Array=[]
	const ActivityContinuationControls=ActivityModes
	var kernel=Kernel.new()
	var _resident_page_error: Dictionary={}
	var _resident_trade_receipt_pending: String=""
	var _cards_open: bool=false
	var _self_profile_open: bool=false
	var _ordinary_skills_open: bool=false
	var _courses_open: bool=false
	var _dynamic_open: bool=false
	var _military_open: bool=false
	var _family_open: bool=false
	var _estate_id: String=""
	var _growth_open: bool=false
	var journey_view: Dictionary={"opened":false}
	## 无地域话题扩展，原地方页仍用于对照。
	func _regional_life_enabled() -> bool:return false
	## 未启政治页，不授予其它域读取能力。
	func _politics_enabled() -> bool:return false

## 保实际旧宿主输入状态，等待任何新页面时都必须禁用。
class Scene extends Control:
	var enabled: bool=true
	## 记录原prepare给旧宿主的输入禁用指令。
	func set_input_enabled(value: bool) -> void:enabled=value

## 根树就绪后再执行与生产相同的节点遍历。
func _init() -> void:call_deferred("_run")

## 保留具体分流、首错或旧输入问题，不能仅以返回false判通过。
func check(value: bool,label: String) -> void:
	checks+=1
	if not value:failures+=1;printerr("FAIL: ",label)

## 地图分支与原城内分支分别检查，保证没有为附随者制造本地读权限。
func _run() -> void:
	var path: String="res://ui/resident_page_requests.gd"
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--page-script="):path=arg.trim_prefix("--page-script=")
	var pages: Script=load(path)
	for kind in ["Following","InArmy"]:
		var host=Host.new();root.add_child(host);host.kernel.location={"kind":kind}
		var scene=Scene.new();scene.name="SceneHost";host.add_child(scene)
		check(not pages.prepare(host) and host.kernel.scene_requests==[{"place_id":"","view_kind":"world"}],kind+"在未主动开图时也先等原world投影")
		check(host.kernel.place_reads==0 and host._resident_page_error.is_empty(),kind+"不访问缺失的place或伪造本地资料")
		check(not scene.enabled,kind+"等待期间旧宿主仍禁用")
		host.queue_free();await process_frame
	var city=Host.new();root.add_child(city);city.kernel.location={"kind":"Settlement","place_id":"entry"}
	check(not pages.prepare(city) and city.kernel.scene_requests.is_empty() and city.kernel.place_reads==1 and city._resident_page_error.get("code")=="FixtureLocalPageReached","原城内关闭地图时仍走原本地页")
	city.queue_free();await process_frame
	print("ATTACHED_MAP_PAGE_CHECKS=",checks," FAILURES=",failures," SOURCE=FINITE_SCREEN_DTOS WORLD_INSTANCES=0 GUI=NOT_TESTED")
	quit(0 if failures==0 else 1)
