## 本人只读适配合同的孤立测试；显式DTO夹具不冒充真实世界或图形验收。
extends SceneTree
const Adapter=preload("res://ui/self_profile_adapter.gd")
var checks: int=0
var failures: int=0

## 夹具仅模拟原生读取的有限输入，记录命令序号与换源时机。
class ProfileKernel extends RefCounted:
	var source: Dictionary={"world_id":"0123456789abcdef0123456789abcdef","execution_epoch":2,"revision":7,"tick":3}
	var context: Dictionary={"ok":true,"actor_id":"actor.player","revision":7,"next_request_seq":23}
	var value: Variant={}
	var reads: int=0
	var change_source: bool=false
	var change_actor: bool=false
	## 身份复制对应生产只读getter，不让调用者改动当前来源。
	func source_identity() -> Dictionary: return source.duplicate(true)
	## 此getter不递增请求序号，也不创建命令。
	func command_context() -> Dictionary: return context.duplicate(true)
	## 模拟纯读及读取间换源，返回DTO不运行任何领域逻辑。
	func self_profile() -> Variant:
		reads+=1
		if change_source: source.execution_epoch+=1
		if change_actor: context.actor_id="actor.other"
		return value.duplicate(true) if value is Dictionary else value

## 延后运行断言，让所有失败都进入进程退出码。
func _init() -> void: call_deferred("_run")

## 明示当前人物测试数据包含五维、十项及一个启用岗位。
static func fixture(kernel: ProfileKernel) -> Dictionary:
	var attributes: Array=[]
	for id in Adapter.ATTRIBUTE_IDS: attributes.append({"attribute_id":id,"value":57})
	var skills: Array=[]
	for id in Adapter.SKILL_IDS: skills.append({"skill_id":id,"xp":345,"level":2})
	return {"kind":"self_profile","actor_id":"actor.player","display_name":"测试本人","age_years":27,"attributes":attributes,"skills":skills,"roles":[{"role_id":"role.servant","place_id":"place.hall","organization_id":""}],"status":{"alive":true,"available":true,"hp":96,"fatigue":4,"injury":0,"action_kind":"study_skill"},"source":{"world_id":kernel.source.world_id,"execution_epoch":kernel.source.execution_epoch,"revision":kernel.source.revision}}

## 断言计数只报告此适配层的合同范围。
func check(value: bool, label: String) -> void:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)

## 拒绝必须无任何profile正文，禁止旧值或他人数据沿错误分支回显。
func check_refusal(result: Dictionary, code: String, label: String) -> void:
	check(result.get("ok")==false and result.get("ready")==false and result.get("code")==code and not result.has("profile") and not result.has("source"),label)

## 覆盖类型、部分数据、他人、载入epoch及读取间换源，校验重复浏览无写入。
func _run() -> void:
	var kernel=ProfileKernel.new(); kernel.value=fixture(kernel)
	var initial_source: Dictionary=kernel.source.duplicate(true)
	var initial_context: Dictionary=kernel.context.duplicate(true)
	var result: Dictionary=Adapter.prepare(kernel)
	check(result.get("ok")==true and result.get("ready")==true,"完整本人DTO可显示")
	check(result.get("profile")==kernel.value and result.get("source")==kernel.source,"保持当前实际值与来源")
	result.profile.attributes[0].value=999
	result.source.revision=999
	check(kernel.value.attributes[0].value==57 and kernel.source.revision==7,"返回值深拥有且不能改底层")
	Adapter.prepare(kernel)
	check(kernel.reads==2 and kernel.source==initial_source and kernel.context==initial_context,"反复浏览不改时钟或命令序号")
	kernel.value={"ok":false,"code":"NativeAsyncRequired","secret":"不得回显"}
	check_refusal(Adapter.prepare(kernel),"NativeAsyncRequired","等待不发布空或旧profile")
	kernel.value={"ok":false,"code":"UnsupportedLiveQuery","actor_id":"actor.other"}
	check_refusal(Adapter.prepare(kernel),"UnsupportedLiveQuery","原生拒绝保留诊断但剥除正文")
	kernel.value=fixture(kernel); kernel.value.actor_id="actor.other"
	check_refusal(Adapter.prepare(kernel),"InvalidSelfProfile","他人DTO不能进入本人页")
	kernel.value=fixture(kernel); kernel.value.source.execution_epoch-=1
	check_refusal(Adapter.prepare(kernel),"InvalidSelfProfile","同revision的读档旧epoch拒绝")
	kernel.value=fixture(kernel); kernel.value.source.world_id="abcdef0123456789abcdef0123456789"
	check_refusal(Adapter.prepare(kernel),"InvalidSelfProfile","另一世界DTO拒绝")
	kernel.value=fixture(kernel); kernel.value.source.revision-=1
	check_refusal(Adapter.prepare(kernel),"InvalidSelfProfile","旧revision的完整DTO拒绝")
	kernel.value=fixture(kernel); kernel.value.erase("status")
	check_refusal(Adapter.prepare(kernel),"InvalidSelfProfile","部分页不能当成功")
	kernel.value=fixture(kernel); kernel.value["relationships"]={"actor.other":99}
	check_refusal(Adapter.prepare(kernel),"InvalidSelfProfile","额外隐私字段拒绝")
	kernel.value=fixture(kernel); kernel.value.attributes[0].value=57.0
	check_refusal(Adapter.prepare(kernel),"InvalidSelfProfile","浮点五维不能转换为真整数")
	kernel.value=fixture(kernel); kernel.value.attributes[0].attribute_id=kernel.value.attributes[1].attribute_id
	check_refusal(Adapter.prepare(kernel),"InvalidSelfProfile","重复维度不能掩盖缺项")
	kernel.value=fixture(kernel); kernel.value.skills.pop_back()
	check_refusal(Adapter.prepare(kernel),"InvalidSelfProfile","部分九项技能拒绝")
	kernel.value=fixture(kernel); kernel.value.skills[0]["talent"]="不存在"
	check_refusal(Adapter.prepare(kernel),"InvalidSelfProfile","技能额外天赋字段拒绝")
	kernel.value=fixture(kernel); kernel.value.skills=[]; kernel.value.age_years=-1
	check(Adapter.prepare(kernel).get("ready")==true,"原生未启用技艺及未知年龄保持真实空值")
	kernel.value=fixture(kernel); kernel.value.status.alive=1
	check_refusal(Adapter.prepare(kernel),"InvalidSelfProfile","状态不接受整数冒充布尔")
	kernel.value=fixture(kernel); kernel.value.roles[0].place_id=null
	check_refusal(Adapter.prepare(kernel),"InvalidSelfProfile","岗位部分类型拒绝")
	kernel.value=fixture(kernel); kernel.value.display_name="汉".repeat(171)
	check_refusal(Adapter.prepare(kernel),"InvalidSelfProfile","姓名按UTF8字节上限拒绝超长")
	kernel.value=fixture(kernel); kernel.value.roles[0].organization_id="x".repeat(257)
	check_refusal(Adapter.prepare(kernel),"InvalidSelfProfile","岗位ID按原生上限拒绝超长")
	kernel.value=fixture(kernel)
	for index in range(64): kernel.value.roles.append(kernel.value.roles[0].duplicate(true))
	check_refusal(Adapter.prepare(kernel),"InvalidSelfProfile","超过64行任职拒绝")
	kernel.value=fixture(kernel); kernel.value.attributes[0].value=111
	check_refusal(Adapter.prepare(kernel),"InvalidSelfProfile","超出原生五维范围拒绝")
	kernel.value=fixture(kernel); kernel.value.skills[0].xp=-1
	check_refusal(Adapter.prepare(kernel),"InvalidSelfProfile","负技能经验拒绝")
	kernel.value=fixture(kernel); kernel.change_source=true
	check_refusal(Adapter.prepare(kernel),"StaleSelfProfile","读取中载入换源不发布旧页")
	kernel.change_source=false; kernel.value=fixture(kernel); kernel.change_actor=true
	check_refusal(Adapter.prepare(kernel),"StaleSelfProfile","读取中更换玩家不发布旧本人")
	kernel.change_actor=false; kernel.context=initial_context.duplicate(true); kernel.value=fixture(kernel)
	check(Adapter.prepare(kernel).get("ready")==true,"新epoch重新读取后可恢复显示")
	kernel.context.revision-=1
	var reads_before: int=kernel.reads
	check_refusal(Adapter.prepare(kernel),"StaleSelfProfile","陈旧基础帧先拒绝")
	check(kernel.reads==reads_before,"基础帧未准备好时不发额外页需求")
	check_refusal(Adapter.prepare(null),"NoWorld","空backend明确拒绝")
	print("SELF_PROFILE_ADAPTER_CHECKS=",checks," FAILURES=",failures)
	print("self profile adapter: ","PASS" if failures==0 else "FAIL"," (",checks," checks)")
	quit(1 if failures else 0)
