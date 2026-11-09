## 检验同行与路遇的新规则版本、旧旅行的无损迁移和混合格式拒绝。
extends SceneTree
const Kernel=preload("res://core/world_kernel.gd")
const Codec=preload("res://core/save_codec.gd")
const Rules=preload("res://core/rules_migration.gd")
var checks: int=0
var failures: int=0
## 延后运行，确保真实内容目录与引擎元数据可读取。
func _init() -> void:
	call_deferred("run_checks")
## 累计可观察断言，不能用异常退出或缺少输出视为成功。
func check(value: bool, label: String) -> void:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)
## 新集合只从已知旧格式初始化；有真实同行事实的混合旧标识必须拒绝。
func run_checks() -> void:
	var k=Kernel.new(); check(k.new_game().ok,"实际工厂创建世界")
	var current: Dictionary=k.snapshot()
	check(current.ruleset_id==Rules.CURRENT and current.ruleset_id!=Rules.TRAVEL,"当前规则显式区别于缺少同行与战斗的旧行旅版本")
	var old: Dictionary=current.duplicate(true); old.ruleset_id="f03-travel-1"
	for actor in old.actors.values(): actor.erase("appearance")
	for key in ["parties","encounters","itineraries"]: old.erase(key)
	var original: Dictionary=old.duplicate(true)
	check(k.restore(old).ok,"旧行旅快照显式迁移")
	check(old==original,"原始来源快照保持不变")
	var migrated: Dictionary=k.snapshot()
	check(migrated.get("parties",{}).is_empty() and migrated.get("encounters",{}).is_empty(),"迁移不补造同伴或事件")
	for key in ["parties","encounters","itineraries"]: migrated.erase(key)
	migrated.ruleset_id=old.ruleset_id
	for actor in migrated.actors.values(): actor.erase("appearance")
	check(migrated==old,"旧位置、资金、任务、时钟和随机流不变")
	var mixed: Dictionary=old.duplicate(true); mixed.parties={"forged":{"id":"forged"}}
	check(not k.restore(mixed).ok,"不把新队伍伪装成旧格式后丢弃")
	var encoded: String=Codec.encode(k.snapshot(),1,"m01b",k.content())
	check(not encoded.is_empty() and Codec.decode(encoded,k.content()).ok,"新规则封装可读回")
	var bad: Dictionary=k.snapshot(); bad.parties=[]
	check(not k.restore(bad).ok,"新规则不修补损坏的新集合")
	print("MOBILITY_MIGRATION_CHECKS=",checks," FAILURES=",failures)
	quit(0 if failures==0 else 1)
