## 本人页复用生产resident队列的合同测试；transport为显式夹具，不冒充原生领域验收。
extends SceneTree
const Adapter=preload("res://ui/self_profile_adapter.gd")
const ProtocolFixture=preload("res://tests/test_native_resident_backend.gd")
const ProfileFixture=preload("res://tests/test_self_profile_adapter.gd")
var failures: int=0
var checks: int=0

## 同一原有wire新增本人投影，来源与基础帧始终由同一传输对象持有。
class ProfileWire extends ProtocolFixture.Wire:
	var profile_actor: String="actor.player"
	## 只扩展纯query回执，不为页面提供命令或世界写入路径。
	func answer(id: String) -> Dictionary:
		if id=="self_profile":
			var kernel=ProfileFixture.ProfileKernel.new(); kernel.source=source.duplicate(true)
			var profile: Dictionary=ProfileFixture.fixture(kernel)
			profile.actor_id=profile_actor
			return profile
		return super.answer(id)

## 保持生产本人getter、读取、缓存和失效逻辑，仅替换外部传输。
class ProfileBackend extends "res://ui/native_resident_backend.gd":
	var wire=ProfileWire.new()
	## 明示wire不替代或修改实际backend的纯读队列。
	func _create_bridge() -> Object: return wire

## SceneTree初始化后手工轮询，避免任何真实计时推进。
func _init() -> void: call_deferred("_run")

## 检查失败进入退出码，成功仅覆盖此处声明的协议行为。
func check(value: bool, label: String) -> void:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)

## 有限基础帧只由生产poll装配，不直接写私有缓存。
func refresh(backend: ProfileBackend, epoch: int) -> void:
	check(backend.refresh(epoch).get("ok",false),"基础帧接纳纯读取")
	for index in range(40):
		if not backend.is_busy(): return
		backend.poll()
	check(false,"基础帧必须有限完成")

## 浏览及载入来源切换沿同一真实缓存，读请求不得污染命令序号。
func _run() -> void:
	var backend=ProfileBackend.new(); root.add_child(backend); backend.set_process(false)
	var catalog: Dictionary={"ok":true,"content_id":"foundation.alpha_life","data":{"texts":{},"scenarios":{"scenario.alpha_life":{"world_map_id":"map"}},"world_maps":{"map":{}}}}
	check(backend.install(catalog,{"player_actor_id":"actor.player","scenario_id":"scenario.alpha_life"}).get("ok",false),"安装单一resident来源")
	var unframed: Dictionary=Adapter.prepare(backend)
	check(unframed.get("code")=="ResidentFrameUnavailable" and not unframed.has("profile"),"基础帧未发布前不显示profile")
	check(not backend.has_pending_reads() and backend.wire.calls.is_empty(),"基础帧未准备好时不登记本人页或命令")
	refresh(backend,4)
	var original_context: Dictionary=backend.command_context()
	var original_source: Dictionary=backend.source_identity()
	var count: int=backend.wire.calls.size()
	var pending: Dictionary=Adapter.prepare(backend)
	check(pending.get("code")=="NativeAsyncRequired" and not pending.has("profile"),"首开只登记读取需求且不发布正文")
	Adapter.prepare(backend)
	check(backend.wire.calls.size()==count and backend._pending_reads.size()==1,"重复浏览只去重同一队列签名")
	check(backend.begin_pending_read(4).get("ok",false),"页面读取使用原单槽")
	check(backend.wire.calls[-1].payload=={"query_id":"self_profile","args":{}},"默认仅当前本人且不传其他人物ID")
	backend.poll()
	var result: Dictionary=Adapter.prepare(backend)
	check(result.get("ready",false),"同源投影完成后可显示")
	count=backend.wire.calls.size(); Adapter.prepare(backend)
	check(backend.wire.calls.size()==count and not backend.has_pending_reads(),"再次浏览复用原缓存")
	check(backend.command_context()==original_context and backend.source_identity()==original_source,"浏览保持命令序号及世界时钟")
	var loaded_source: Dictionary=original_source.duplicate(true); loaded_source.execution_epoch+=1
	backend.wire.source=loaded_source.duplicate(true)
	check(backend.adopt_source(loaded_source).get("ok",false),"同revision载入采用新epoch")
	check(not Adapter.prepare(backend).has("profile"),"载入后旧基础帧和profile不能显示")
	refresh(backend,5)
	check(Adapter.prepare(backend).get("code")=="NativeAsyncRequired","新epoch必须重读本人页")
	check(backend.begin_pending_read(5).get("ok",false),"新epoch本人查询接纳")
	backend.poll(); result=Adapter.prepare(backend)
	check(result.get("ready",false) and result.source.execution_epoch==loaded_source.execution_epoch,"新epoch完整值恢复页面")
	var newer_source: Dictionary=loaded_source.duplicate(true); newer_source.revision+=1
	backend.wire.source=newer_source.duplicate(true); backend.adopt_source(newer_source)
	refresh(backend,6); Adapter.prepare(backend); backend.begin_pending_read(6)
	backend.invalidate(); backend.poll()
	check(Adapter.prepare(backend).get("code")=="NativeAsyncRequired","失效的在途本人回执不进入缓存")
	check(backend.begin_pending_read(6).get("ok",false),"读取间切人夹具使用相同生产query槽")
	backend.wire.profile_actor="actor.other"
	backend.poll()
	var foreign: Dictionary=Adapter.prepare(backend)
	check(foreign.get("code")=="InvalidSelfProfile" and not foreign.has("profile"),"在途回执变为他人时完整正文拒绝")
	for packet in backend.wire.calls: check(packet.kind=="query","浏览仅提交纯query数据包")
	check(backend.command_context().next_request_seq==original_context.next_request_seq,"跨来源读取仍不消费命令request_seq")
	backend.queue_free()
	print("SELF_PROFILE_RESIDENT_READS_CHECKS=",checks," FAILURES=",failures)
	print("self profile resident reads: ","PASS" if failures==0 else "FAIL"," (",checks," checks)")
	quit(1 if failures else 0)
