## 旧行动注册只开放明确启动桥；真实旧validator、同owner与原拥有权缺一不可，门位保持独立。
extends SceneTree
const Registry=preload("res://core/runtime/module_registry.gd")
const Builtin=preload("res://core/runtime/builtin_modules.gd")
const Family=preload("res://core/family_rules.gd")
const Kernel=preload("res://core/world_kernel.gd")
var checks: int=0
var failures: int=0

## 延迟至内建资源就绪后验证；只运行共享ABI的有限负例。
func _init() -> void:
	call_deferred("run_checks")

## 失败保留完整标签并影响退出码。
func check(value: bool, label: String) -> void:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)

## 从真实组合根取得旧family_adopt validator与拥有权，唯一增量是可执行command描述。
func descriptors() -> Array:
	var rows: Array=Builtin.descriptors()
	for row in rows:
		if row.id=="family":
			row.commands=[{"kind":"family_adopt","mode":"action","compat_action":"legacy_begin_action","gate_id":"legacy_kernel","preview":Family.preview,"apply":null}]
	return rows

## 同一组真实旧描述按owner查找，不复制Schema和阶段生命周期。
func family_row(rows: Array) -> Dictionary:
	for row in rows:
		if row.id=="family": return row
	return {}

## 每个负例都使用真实注册器，失败不能返回部分可运行commands。
func rejected(rows: Array, code: String, label: String) -> void:
	var result: Dictionary=Registry.build(rows)
	check(not result.ok and result.get("code")==code and not result.has("commands"),label+" "+str(result))

## 生产Kernel仍按既有守卫顺序执行，兼容标记本身不允许域命令进入Party专用早门。
func guard_independence() -> void:
	var k=Kernel.new()
	check(k.new_game().ok,"真实Kernel启动")
	var state: Dictionary=k.snapshot(); var actor: String=state.player_actor_id
	var command: Dictionary={"kind":"compat_probe","owner":"family","mode":"action","compat_action":"legacy_begin_action","gate_id":"party_before_actor","preview":Family.preview,"apply":null}
	k._registry.commands.compat_probe=command
	state.actors[actor].available=false
	check(k._preview(state,"compat_probe",{},actor).get("code")=="ActorUnavailable","兼容标记与借用Party门不能绕不可用守卫")
	state.actors[actor].available=true; state.actors[actor].action_id="busy"
	check(k._preview(state,"compat_probe",{},actor).get("code")=="ActorBusy","兼容标记不能绕忙碌守卫")
	state.encounters.pending={"status":"pending","participants":[actor]}
	check(k._preview(state,"compat_probe",{},actor).get("code")=="EncounterPending","兼容标记不能绕遭遇守卫")

## 只有真实旧validator及其拥有权允许兼容，正式action描述仍必须有完整生命周期。
func run_checks() -> void:
	var rows: Array=descriptors(); var before: Array=rows.duplicate(true)
	var built: Dictionary=Registry.build(rows)
	check(built.ok and rows==before,"纯注册成功且输入只读")
	check(built.get("commands",{}).get("family_adopt",{}).get("compat_action")=="legacy_begin_action" and not built.get("actions",{}).has("family_adopt"),"明确兼容命令不伪造正式action生命周期")
	check(built.get("legacy_action_validators",{}).get("family_adopt",{}).get("validate")==Family.validate_action,"保持真实旧领域validator绑定")
	for value in ["", "unknown", true, 1, {}, []]:
		rows=descriptors(); family_row(rows).commands[0].compat_action=value
		rejected(rows,"InvalidCompatActionDescriptor","拒绝未知或错误类型的兼容标记 "+str(value))
	rows=descriptors(); family_row(rows).commands[0].erase("compat_action")
	rejected(rows,"UnknownCommandAction","旧validator不能默认豁免完整action")
	rows=descriptors(); family_row(rows).commands[0].mode="legacy_action"
	rejected(rows,"InvalidCommandDescriptor","不得新增平行命令mode")
	rows=descriptors(); family_row(rows).commands[0].mode="instant"; family_row(rows).commands[0].apply=Family.apply
	rejected(rows,"InvalidCompatActionDescriptor","即时命令不得携带兼容行动")
	rows=descriptors(); family_row(rows).commands[0].apply=Family.apply
	rejected(rows,"InvalidCompatActionDescriptor","兼容行动不得伪造apply回调")
	rows=descriptors(); family_row(rows).legacy_action_validators=[]
	rejected(rows,"UnknownCompatAction","缺真实旧validator拒绝")
	rows=descriptors(); var borrowed: Dictionary=family_row(rows).commands.pop_back()
	for row in rows:
		if row.id=="cultivation": row.commands.append(borrowed)
	rejected(rows,"UnknownCompatAction","别域不能借family旧validator")
	rows=descriptors(); var claims: Array=[]
	for claim in family_row(rows).owns:
		if not (claim.container=="actions" and claim.scope=="kind" and claim.key=="family_adopt"): claims.append(claim)
	family_row(rows).owns=claims
	rejected(rows,"MissingStateOwner","缺原行动拥有权拒绝")
	rows=descriptors()
	for row in rows:
		if row.id=="tasks": row.commands[0].compat_action="legacy_begin_action"
	rejected(rows,"UnknownCompatAction","已正式迁移action不能降级为兼容桥")
	guard_independence()
	print("COMPAT_ACTION_CHECKS=",checks," FAILURES=",failures)
	quit(1 if failures else 0)
