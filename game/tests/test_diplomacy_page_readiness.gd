## 只替换transport结果，使用真实ResidentPage外交预读函数，不创建World或提交命令。
extends SceneTree
const Requests=preload("res://ui/resident_page_requests.gd")
var checks: int=0
var failures: int=0
class Kernel extends RefCounted:
	var enabled: bool=true
	var page: Dictionary={"ok":true,"rows":[]}
	var quote: Dictionary={"ok":true}
	var calls: Array=[]
	func diplomacy_enabled() -> bool: return enabled
	func diplomacy_entries(place: String) -> Dictionary: calls.append(["page",place]);return page
	func preview(kind: String, params: Dictionary, actor: String="") -> Dictionary: calls.append(["preview",kind,params.duplicate(true),actor]);return quote
class Host extends RefCounted:
	var kernel=Kernel.new()
	var _resident_page_error: Dictionary={}
func check(value: bool, label: String) -> void:
	checks+=1
	if not value: failures+=1;printerr("FAIL: ",label)
func _initialize() -> void:
	var host=Host.new()
	host.kernel.enabled=false
	check(Requests._diplomacy_ready(host,"seat") and host.kernel.calls.is_empty(),"old scene issues no diplomacy read")
	host.kernel.enabled=true;host.kernel.page={"ok":false,"code":"NativeAsyncRequired"}
	check(not Requests._diplomacy_ready(host,"seat"),"page pending blocks publication")
	host.kernel.page={"ok":true,"rows":[{"actions":[{"command":"envoy_deliver","params":{"task_id":"known","provider_actor_id":"present","expected_world_revision":7}},{"command":"advance","params":{}}]}]}
	host.kernel.quote={"ok":false,"code":"NativeAsyncRequired"}
	check(not Requests._diplomacy_ready(host,"seat"),"original preview pending blocks publication")
	host.kernel.quote={"ok":false,"kind":"rule","code":"DiplomaticRepresentativeChanged"}
	check(Requests._diplomacy_ready(host,"seat"),"ordinary original refusal stays visible")
	host.kernel.quote={"ok":false,"kind":"internal","code":"StaleDiplomacyViewIndex"}
	check(not Requests._diplomacy_ready(host,"seat") and host._resident_page_error.get("code")=="StaleDiplomacyViewIndex","source corruption blocks publication")
	host.kernel.quote={"ok":true};host.kernel.calls=[]
	check(Requests._diplomacy_ready(host,"seat"),"complete actual command choices can publish")
	check(host.kernel.calls==[["page","seat"],["preview","envoy_deliver",{"task_id":"known","provider_actor_id":"present","expected_world_revision":7},""],["preview","advance",{},""]],"original provider/task/revision and advance params are unchanged")
	print("DIPLOMACY_PAGE_READINESS checks=%d failures=%d no World or commands"%[checks,failures])
	quit(1 if failures else 0)
