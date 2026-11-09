## 新页面异步读的开发期接口门；只用明示wire验证单槽、来源和冻结参数，不冒充玩法验收。
extends SceneTree
const Fixtures=preload("res://tests/test_native_resident_backend.gd")
var checks: int=0
var failures: int=0

## 页面wire沿现有协议夹具，仅增加原页面DTO与可观测请求序号。
class PageWire extends Fixtures.Wire:
	## 固定家庭页仅是协议夹具，不代表真实领域结果。
	func answer(id: String) -> Dictionary:
		if id=="family_page": return {"actor_id":"actor.player","children":[],"read_marker":sequence}
		if id=="action_manual_window": return {"ok":true,"manual":true}
		return super.answer(id)

## 生产backend所有需求、缓存和poll逻辑保持原样，仅替换外部传输。
class PageHarness extends "res://ui/native_resident_backend.gd":
	var wire=PageWire.new()
	## 明示测试传输替换不修改生产排队与读取代码。
	func _create_bridge() -> Object: return wire

## 延后执行Node装配，所有断言都进入进程失败码。
func _init() -> void: call_deferred("_run")
## 逐次记录实际检查与失败，不将协议夹具冒充完整游戏检查。
func check(value: bool, label: String) -> void:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)

## 检查有限需求不提交命令；陈旧代次、重复签名和普通拒绝均不能触发隐式重放。
func _run() -> void:
	var backend=PageHarness.new(); root.add_child(backend); backend.set_process(false)
	var messages: Array=[]
	backend.page_reads_ready.connect(func(source,epoch): messages.append({"source":source,"epoch":epoch}))
	check(backend.install({"ok":true,"content_id":"foundation.alpha_life","data":{"texts":{},"scenarios":{"scenario.alpha_life":{"world_map_id":"map"}},"world_maps":{"map":{}}}},{"player_actor_id":"actor.player","scenario_id":"scenario.alpha_life"}).ok,"安装明示wire")
	var context: Dictionary={"place_id":"hall","provider_actor_id":"person","role_id":"adopter"}
	check(backend.family_page("family",context).code=="NativeAsyncRequired","首次读取明确pending")
	context.role_id="changed"
	backend.family_page("family",{"place_id":"hall","provider_actor_id":"person","role_id":"adopter"})
	check(backend.wire.calls.is_empty() and backend._pending_reads.size()==1,"getter只记录并去重需求")
	check(backend.begin_pending_read(4).ok,"唯一槽接纳页面读取")
	check(backend.wire.calls[0].payload.args.provider_context.role_id=="adopter","读取参数深冻结")
	backend.poll()
	check(messages.size()==1 and messages[0].epoch==4,"当前代次读取只发布一次")
	check(backend.family_page("family",{"place_id":"hall","provider_actor_id":"person","role_id":"adopter"}).actor_id=="actor.player" and not backend.has_pending_reads(),"同源getter复用实际缓存")
	check(backend.preview("family_retire",{}).code=="NativeAsyncRequired","动态preview进入纯读需求")
	check(backend.begin_pending_read(4).ok,"preview不走command")
	backend.poll()
	check(backend.preview("family_retire",{}).ok,"preview来源缓存可供原按钮")
	backend.demand_view("unknown_leaf")
	backend.begin_pending_read(4); backend.poll()
	check(backend.demand_view("unknown_leaf").code=="FixtureUnknownQuery" and not backend.has_pending_reads(),"负查询真实缓存，不空成功或无限重试")
	backend.demand_view("family_page",{"tab":"rest"}); backend.begin_pending_read(4)
	backend.invalidate(); backend.poll()
	check(messages.size()==3,"已撤销代次不发布旧页面")
	var source: Dictionary=backend.source_identity(); source.revision+=1; source.tick+=1
	check(backend.adopt_source(source).ok,"接纳新来源")
	check(backend.family_page("family",{}).code=="NativeAsyncRequired","新revision不能复用旧页面")
	for call in backend.wire.calls: check(call.kind in ["query","preview"],"读取从未提交命令")
	backend.queue_free()
	print("NATIVE_RESIDENT_PAGE_READS_CHECKS=",checks," FAILURES=",failures)
	print("native resident page read interface: ","PASS" if failures==0 else "FAIL")
	quit(1 if failures else 0)
