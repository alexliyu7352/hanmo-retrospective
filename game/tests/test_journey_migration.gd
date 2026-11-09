## 新行程语义必须有显式规则版本，旧档可迁移，新档不能被旧版误读后继续移动。
extends SceneTree
const Kernel = preload("res://core/world_kernel.gd")
const Codec = preload("res://core/save_codec.gd")
var checks: int = 0
var failures: int = 0
## 引擎初始化后检验真实工厂和存档编解码。
func _init() -> void:
	call_deferred("_run")
## 累积校验结果并保留失败退出码。
func check(ok: bool, label: String) -> void:
	checks += 1
	if not ok: failures += 1; printerr("FAIL: ",label)
## 只构造旧规则元数据和旧行程字段，不绕过当前解码与状态检查。
func _run() -> void:
	var k=Kernel.new(); check(k.new_game().ok,"新版本工厂可创建世界")
	var s: Dictionary=k.snapshot()
	check(s.ruleset_id!="f02-1","带暂停语义的局使用新的规则版本")
	var original: Dictionary=s.duplicate(true); original.ruleset_id="f02-1"
	for actor in original.actors.values(): actor.erase("appearance")
	var copy: Dictionary=original.duplicate(true)
	check(k.restore(original).ok,"旧民间生活局经显式迁移恢复")
	check(original==copy,"迁移不修改来源快照")
	var migrated: Dictionary=k.snapshot()
	check(migrated.ruleset_id==s.ruleset_id,"恢复后的局写入当前规则标识")
	migrated.ruleset_id="f02-1"
	for actor in migrated.actors.values(): actor.erase("appearance")
	check(migrated==original,"除规则标识外的旧人物、时刻、财物和随机状态不变")
	var text: String=Codec.encode(k.snapshot(),1,"migration-check",k.content())
	check(not text.is_empty() and JSON.parse_string(text).ruleset_id!="f02-1","新档封装也使用新规则标识")
	var envelope: Dictionary=JSON.parse_string(text)
	var payload: String=JSON.stringify(original,"",true,true)
	envelope.ruleset_id="f02-1"; envelope.payload_json=payload
	envelope.payload_length=payload.to_utf8_buffer().size(); envelope.payload_sha256=payload.sha256_text()
	var decoded: Dictionary=Codec.decode(JSON.stringify(envelope,"",true,true),k.content())
	check(decoded.ok and decoded.state==k.snapshot(),"旧封装先核验原字节再迁移，不用新规则误判校验摘要")
	var future: Dictionary=k.snapshot(); future.ruleset_id="future-rule"
	var before: Dictionary=k.snapshot()
	check(not k.restore(future).ok and k.snapshot()==before,"未知规则不猜测迁移，失败保留当前局")
	print("JOURNEY_MIGRATION_CHECKS=",checks," FAILURES=",failures)
	quit(0 if failures==0 else 1)
