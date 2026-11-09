## 真实首次图形导入之后，只依靠自动扩展发现验证查询、引用释放及进程退出。
extends SceneTree
var checks: int = 0
var failures: int = 0
var oracle_queries: int = 0
var source_identity: Dictionary = {}

## 正常SceneTree初始化后查询ClassDB，不显式加载或重新安装扩展。
func _init() -> void:
	call_deferred("run_checks")

## 保留每项断言结果，失败时设置最终非零退出码。
func check(condition: bool, label: String) -> bool:
	checks += 1
	if not condition:
		failures += 1
		printerr("FAIL: ", label)
	return condition

## 冻结工件按原字节传入，不改变字符串编码和摘要。
func raw(path: String) -> PackedByteArray:
	return FileAccess.get_file_as_bytes("res://fixtures/" + path)

## 预期仅在测试端读取，不送进原生对象。
func json(path: String) -> Dictionary:
	return JSON.parse_string(raw(path).get_string_from_utf8())

## 每组旧oracle使用新的真实RefCounted对象，并校验最后引用释放。
func verify_case(row: Dictionary) -> void:
	var bridge = ClassDB.instantiate("HanmoWorldBridge")
	check(not bridge.audit().installed, row.id + " fresh instance")
	var installed: Dictionary = bridge.install_artifact(raw(row.artifact))
	if not check(installed.get("ok", false), row.id + " install"):
		return
	check(installed.input_sha256 == row.input_sha256, row.id + " original input SHA")
	var expected: Dictionary = json(row.oracle)
	var query: PackedByteArray = raw(row.queries)
	var got: Dictionary = bridge.query_batch(query)
	if check(got.get("ok", false), row.id + " query"):
		check(got.input_sha256 == expected.input_sha256 and got.queries_sha256 == expected.queries_sha256 and got.format == expected.format, row.id + " query identity")
		check(got.counts.size() == expected.counts.size(), row.id + " query count")
		for i in range(min(got.counts.size(), expected.counts.size())):
			check(got.counts[i] == expected.counts[i], row.id + " frozen oracle " + str(i))
			oracle_queries += 1
	var audit: Dictionary = bridge.audit()
	check(audit.installed and audit.install_count == 1, row.id + " single install")
	var cli: Dictionary = json("cli_audits/" + row.id + ".json")
	check(audit.inventory_component_sha256 == cli.inventory_component_sha256 and audit.payload_sha256 == cli.payload_sha256, row.id + " original component digest")
	for inventory in row.lot_orders:
		check(audit.inventory_lot_ids.get(inventory.inventory_id) == inventory.lot_ids, row.id + " lot order " + inventory.inventory_id)
	source_identity = audit.build.duplicate(true)
	check(source_identity.source_commit == json("expected_build.json").source_commit, row.id + " frozen source identity")
	check(bridge.install_artifact(PackedByteArray()).get("code") == "AlreadyInstalled", row.id + " second install rejected")
	check(bridge.query_batch("{".to_utf8_buffer()).get("code") == "InvalidJson", row.id + " malformed query rejected")
	check(bridge.audit() == audit and bridge.query_batch(query).counts == expected.counts, row.id + " recovery keeps state")
	var weak: WeakRef = weakref(bridge)
	bridge = null
	check(weak.get_ref() == null, row.id + " last reference released")

## 确认自动加载，再执行所有五组旧oracle及无安装实例的释放控制。
func run_checks() -> void:
	check(GDExtensionManager.is_extension_loaded("res://hanmo.gdextension"), "extension was automatically loaded")
	if not check(ClassDB.class_exists("HanmoWorldBridge"), "registered ClassDB entry"):
		finish(); return
	check(ClassDB.is_parent_class("HanmoWorldBridge", "RefCounted"), "real RefCounted parent")
	for row in json("cases.json").cases:
		verify_case(row)
	for i in range(12):
		var unused = ClassDB.instantiate("HanmoWorldBridge")
		check(not unused.audit().installed, "new independent object " + str(i))
		var weak: WeakRef = weakref(unused)
		unused = null
		check(weak.get_ref() == null, "unused object released " + str(i))
	check(oracle_queries == 23, "all 23 frozen oracle rows")
	finish()

## 把结构化结果落在隔离工程，再使用正常quit进入引擎析构流程。
func finish() -> void:
	var result: Dictionary = {"checks": checks, "failures": failures, "oracle_queries": oracle_queries, "source_identity": source_identity, "automatic_load_only": true, "display_server": DisplayServer.get_name()}
	var file = FileAccess.open("res://query-result-" + DisplayServer.get_name() + ".json", FileAccess.WRITE)
	file.store_string(JSON.stringify(result, "  ") + "\n")
	file.close()
	print("GUI_IMPORT_QUERY checks=", checks, " failures=", failures, " oracle=", oracle_queries, " display=", DisplayServer.get_name())
	quit(0 if failures == 0 else 1)
