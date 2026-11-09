## 实际ResidentPage候选的有限读屏障门；只替换transport返回，不提交命令或建立玩法状态。
extends SceneTree
const Requests=preload("res://ui/resident_page_requests.gd")
var checks: int=0
var failures: int=0

## 两张原页和报价分别可等待/失败；记录原参数以核无改写。
class Kernel extends RefCounted:
	var fiscal: bool=true
	var governance: bool=false
	var responsibility: Dictionary={"ok":true,"rows":[]}
	var political: Dictionary={"ok":true,"rows":[{"actions":[{"command":"political_arrive","params":{"appointment_id":"known"}}]}]}
	var taxes: Dictionary={"ok":true,"rows":[{"actions":[{"command":"political_set_tax","params":{"mode":"normal"}}]}]}
	var quote: Dictionary={"ok":true}
	var calls: Array=[]
	func political_fiscal_enabled() -> bool: return fiscal
	func politics_governance_enabled() -> bool: return governance
	func politics_governance(place: String) -> Dictionary: calls.append(["governance",place]);return responsibility
	func politics_entries(place: String) -> Dictionary: calls.append(["politics",place]);return political
	func political_fiscal_entries(place: String) -> Dictionary: calls.append(["fiscal",place]);return taxes
	func preview(kind: String, params: Dictionary, actor: String="") -> Dictionary: calls.append(["preview",kind,params.duplicate(true),actor]);return quote

class Host extends RefCounted:
	var kernel=Kernel.new()
	var _resident_page_error: Dictionary={}

## 每种等待和失败使用同一正式helper，不把缺页当空成功。
func check(value: bool, label: String) -> void:
	checks+=1
	if not value: failures+=1;printerr("FAIL: ",label)

func _initialize() -> void:
	var host=Host.new()
	host.kernel.political={"ok":false,"code":"NativeAsyncRequired"}
	check(not Requests._politics_ready(host,"seat"),"political page pending blocks publish")
	host.kernel.political={"ok":true,"rows":[{"actions":[{"command":"political_arrive","params":{"appointment_id":"known"}}]}]}
	host.kernel.taxes={"ok":false,"code":"NativeAsyncRequired"}
	check(not Requests._politics_ready(host,"seat"),"tax page pending blocks publish")
	host.kernel.taxes={"ok":true,"rows":[]};host.kernel.quote={"ok":false,"code":"NativeAsyncRequired"}
	check(not Requests._politics_ready(host,"seat"),"original quote pending blocks publish")
	host.kernel.quote={"ok":false,"kind":"rule","code":"PoliticalCandidateNotAtSeat"}
	check(Requests._politics_ready(host,"seat"),"ordinary rule rejection still publishes original disabled control")
	host.kernel.quote={"ok":false,"kind":"internal","code":"SourceMismatch"}
	check(not Requests._politics_ready(host,"seat") and host._resident_page_error.get("code")=="SourceMismatch","internal quote rejection remains visible and blocks publish")
	host.kernel.quote={"ok":true};host.kernel.fiscal=false;host.kernel.calls=[];host.kernel.taxes={"ok":false,"kind":"unsupported","code":"PoliticalFiscalNotEnabled"}
	check(Requests._politics_ready(host,"seat"),"politics-only scene does not request disabled fiscal module")
	check(host.kernel.calls==[["politics","seat"],["preview","political_arrive",{"appointment_id":"known"},""]],"same original source arguments only; no fiscal request or command")
	# 新治理页与其中原受领报价也必须完成，旧情景仍不请求未开启的域。
	host.kernel.governance=true;host.kernel.political={"ok":true,"rows":[]}
	host.kernel.responsibility={"ok":false,"code":"NativeAsyncRequired"}
	check(not Requests._politics_ready(host,"seat"),"governance page pending blocks publish")
	host.kernel.responsibility={"ok":true,"rows":[{"actions":[{"command":"governance_accept","params":{"authority_fact_id":"known-authority","expected_world_revision":7}}]}]}
	host.kernel.quote={"ok":false,"code":"NativeAsyncRequired"}
	check(not Requests._politics_ready(host,"seat"),"governance original quote pending blocks publish")
	host.kernel.quote={"ok":false,"kind":"rule","code":"InsufficientReliefFunds"}
	check(Requests._politics_ready(host,"seat"),"actual insufficient public resources remains an explainable disabled control")
	host.kernel.quote={"ok":false,"kind":"internal","code":"StaleGovernanceViewIndex"}
	check(not Requests._politics_ready(host,"seat") and host._resident_page_error.get("code")=="StaleGovernanceViewIndex","stale governance source blocks publish")
	host.kernel.quote={"ok":true};host.kernel.calls=[]
	check(Requests._politics_ready(host,"seat"),"complete governance original page is publishable")
	check(host.kernel.calls==[["politics","seat"],["governance","seat"],["preview","governance_accept",{"authority_fact_id":"known-authority","expected_world_revision":7},""]],"governance preview preserves the original issued ID and revision")
	print("POLITICS_PAGE_READINESS checks=%d failures=%d finite-transport fixture"%[checks,failures])
	quit(1 if failures else 0)
