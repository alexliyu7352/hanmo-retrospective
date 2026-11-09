## 隔离Linux真实扩展门：复用冻结旧oracle，验证字节边界、唯一实例、独立返回值和释放。
extends SceneTree
const Kernel=preload("res://core/world_kernel.gd")
const ROOT="res://native_inventory_bridge/"
const LIMIT: int=64*1024*1024
var checks: int=0
var failures: int=0
var compared: int=0
var identities: Array=[]

## 延后加载扩展，缺类时以明确断言失败，避免把脚本解析错误冒充产品反例。
func _init() -> void: call_deferred("run")

## 每条观察保留实际失败原因，最终退出码不依赖日志关键词推断。
func check(value: bool,label: String) -> bool:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)
	return value

## 原始字节直接传桥；独立旧oracle只在本测试读取，绝不传入原生对象。
func bytes(path: String) -> PackedByteArray: return FileAccess.get_file_as_bytes(ROOT+path)

## JSON仅用于读取测试预期和构造明确坏输入，正式artifact传递不经此解码。
func document(path: String) -> Dictionary: return JSON.parse_string(FileAccess.get_file_as_string(ROOT+path))

## 只有ClassDB已注册后才实例化，确保执行的是实际加载的扩展类。
func fresh(): return ClassDB.instantiate("HanmoWorldBridge")

## 失败不得带半批counts，code/path均保留原库或明确桥类型门。
func error(result: Dictionary,code: String,path: String,label: String) -> void:
	check(result.get("ok")==false and result.get("code")==code and result.get("path")==path and not result.has("counts"),label+": "+str(result))

## 每个冻结工件实际安装一次，对照旧count结果、原SHA和同CLI组件摘要。
func oracle_case(row: Dictionary) -> void:
	var bridge=fresh()
	var input: PackedByteArray=bytes(row.artifact)
	var query: PackedByteArray=bytes(row.queries)
	var expected: Dictionary=document(row.oracle)
	var installed: Dictionary=bridge.install_artifact(input)
	if not check(installed.get("ok",false),row.id+"真实安装"): return
	check(installed.input_sha256==row.input_sha256,row.id+"原artifact字节SHA")
	var got: Dictionary=bridge.query_batch(query)
	if check(got.get("ok",false),row.id+"真实查询"):
		check(got.format==expected.format and got.input_sha256==expected.input_sha256 and got.queries_sha256==expected.queries_sha256,row.id+"原输入/查询身份")
		check(got.counts.size()==expected.counts.size(),row.id+"全批数量")
		for index in range(min(got.counts.size(),expected.counts.size())):
			check(got.counts[index]==expected.counts[index],row.id+"旧Inventory.count："+expected.counts[index].query_id)
			compared+=1
	var audit: Dictionary=bridge.audit()
	check(audit.installed and audit.install_count==1,row.id+"唯一成功安装")
	for inventory in row.lot_orders:
		check(audit.inventory_lot_ids.get(inventory.inventory_id)==inventory.lot_ids,row.id+"lot原序："+inventory.inventory_id)
	var cli: Dictionary=document("cli_audits/"+row.id+".json")
	check(audit.inventory_component_sha256==cli.inventory_component_sha256,row.id+"完整owner/lot实际Flecs组件与已审CLI相同")
	check(audit.payload_sha256==cli.payload_sha256,row.id+"payload SHA不变")
	identities.append(audit.build.duplicate(true))
	var saved_audit: Dictionary=audit.duplicate(true)
	input[0]=0
	check(bridge.query_batch(query)==got,row.id+"调用方改输入字节不污染安装实例")
	if not got.get("counts",[]).is_empty(): got.counts[0].count="caller-mutated"
	var first_id: String=audit.inventory_lot_ids.keys()[0]
	audit.inventory_lot_ids[first_id].append("caller-mutated")
	audit.inventory_component_sha256.clear(); audit.build.clear()
	check(bridge.audit()==saved_audit,row.id+"深层audit返回值无别名")
	check(bridge.query_batch(query).counts==expected.counts,row.id+"counts返回值无别名")
	var weak: WeakRef=weakref(bridge)
	bridge=null
	check(weak.get_ref()==null,row.id+"最后引用释放")

## 类型门、原失败优先级、一次安装和失败后重试均在同一个实际扩展对象上观察。
func boundaries(row: Dictionary) -> void:
	var input: PackedByteArray=bytes(row.artifact)
	var query: PackedByteArray=bytes(row.queries)
	var bridge=fresh()
	for wrong in ["{}",[],[123,125],{},null,17]:
		error(bridge.install_artifact(wrong),"ExpectedPackedByteArray","/arguments/bytes","拒绝非PackedByteArray安装")
		error(bridge.query_batch(wrong),"ExpectedPackedByteArray","/arguments/bytes","拒绝非PackedByteArray查询")
	error(bridge.query_batch(PackedByteArray()),"NotInstalled","/runtime","未装查询原首错")
	error(bridge.install_artifact("{".to_utf8_buffer()),"InvalidJson","/artifact","坏安装拒绝")
	check(not bridge.audit().installed and bridge.audit().install_count==0,"坏安装不产生半实例")
	check(bridge.install_artifact(input).ok,"同实例坏安装后合法重试")
	var before: Dictionary=bridge.audit()
	error(bridge.install_artifact(PackedByteArray()),"AlreadyInstalled","/runtime","二次安装保原首错")
	error(bridge.query_batch("{".to_utf8_buffer()),"InvalidJson","/queries","坏查询原失败")
	error(bridge.query_batch('{"a":1,"\\u0061":2}'.to_utf8_buffer()),"DuplicateJsonKey","/queries","转义重复键不被桥清洗")
	error(bridge.query_batch(PackedByteArray([123,34,255,34,58,48,125])),"InvalidJson","/queries","非法UTF8不先替换")
	error(bridge.query_batch(PackedByteArray([123,34,97,0,98,34,58,48,125])),"InvalidJson","/queries","原始嵌入NUL不被截断")
	error(bridge.query_batch(("[".repeat(64)+"0"+"]".repeat(64)).to_utf8_buffer()),"InvalidProjectionValue","/queries","深64到原结构门")
	error(bridge.query_batch(("[".repeat(65)+"0"+"]".repeat(65)).to_utf8_buffer()),"WireDepthLimit","/queries","深65拒绝")
	var changed: Dictionary=JSON.parse_string(query.get_string_from_utf8())
	changed.input_sha256="0".repeat(64)
	error(bridge.query_batch(JSON.stringify(changed).to_utf8_buffer()),"SourceMismatch","/queries/input_sha256","错来源摘要不改绑定")
	check(bridge.audit()==before and bridge.query_batch(query).ok,"所有坏查询与好查询均不重装/改组件")
	var padded: PackedByteArray=query.duplicate()
	padded.resize(LIMIT); padded.fill(32)
	for index in range(query.size()): padded[index]=query[index]
	check(bridge.query_batch(padded).ok,"精确64MiB原字节查询通过")
	padded.append(32)
	error(bridge.query_batch(padded),"WireSizeLimit","/queries","64MiB加1查询拒绝")
	error(bridge.install_artifact(padded),"AlreadyInstalled","/runtime","已装超限安装仍先AlreadyInstalled")
	var empty=fresh()
	error(empty.query_batch(padded),"NotInstalled","/runtime","未装超限查询仍先NotInstalled")
	error(empty.install_artifact(padded),"WireSizeLimit","/artifact","未装超限安装拒绝且无大复制")
	check(empty.install_artifact(input).ok,"超限失败后原实例可重试")
	check(bridge.audit()==before,"另一实例安装不影响原实例")
	empty=null; padded.clear(); bridge=null
	for index in range(12):
		var instance=fresh()
		check(not instance.audit().installed and instance.install_artifact(input).ok,"循环新实例从未装开始："+str(index))
		var weak: WeakRef=weakref(instance)
		instance=null
		check(weak.get_ref()==null,"循环释放最后引用："+str(index))

## 原ASCII转义直接传入；不用String.chr(0)先把NUL换成替代字符而掩盖真缺陷。
func string_boundaries(row: Dictionary) -> void:
	var bridge=fresh()
	var query: PackedByteArray=bytes(row.queries)
	check(bridge.install_artifact(bytes(row.artifact)).ok,"字符串边界实例真实安装")
	var before: Dictionary=bridge.audit()
	var changed: String=query.get_string_from_utf8().replace('"task13"','"a\\u0000b"')
	error(bridge.query_batch(changed.to_utf8_buffer()),"BridgeUnrepresentableString","/bridge/string","native接受的NUL query_id不能截断后报成功")
	var missing: Dictionary=JSON.parse_string(query.get_string_from_utf8())
	missing.queries[0].inventory_id="nul-sentinel"
	changed=JSON.stringify(missing).replace("nul-sentinel","a\\u0000b")
	error(bridge.query_batch(changed.to_utf8_buffer()),"BridgeUnrepresentableString","/bridge/string","原错误路径含NUL也不得截断")
	check(bridge.audit()==before and bridge.query_batch(query).ok,"返回类型拒绝不改已安装状态")
	bridge=null
	for target in ["inventory","lot"]:
		var envelope: Dictionary=JSON.parse_string(bytes(row.artifact).get_string_from_utf8())
		var payload: Dictionary=JSON.parse_string(envelope.payload_json)
		var inventories: Array=payload.v.inventories.v
		if target=="inventory": inventories[0].v.id.v="nul-sentinel"
		else:
			for inventory in inventories:
				if not inventory.v.lots.v.is_empty(): inventory.v.lots.v[0].v.id.v="nul-sentinel"; break
		var raw: String=JSON.stringify(payload).replace("nul-sentinel","a\\u0000b")
		envelope.payload_json=raw; envelope.payload_sha256=raw.sha256_text()
		var unusual=fresh()
		check(unusual.install_artifact(JSON.stringify(envelope).to_utf8_buffer()).ok,"native原输入域仍接受NUL "+target+" ID")
		var rejected: Dictionary=unusual.audit()
		error(rejected,"BridgeUnrepresentableString","/bridge/string","audit "+target+" ID无法表示则全体拒绝")
		check(rejected.keys().size()==3,"audit类型拒绝没有半成品")
		error(unusual.install_artifact(bytes(row.artifact)),"AlreadyInstalled","/runtime","audit类型拒绝不更换原安装")
		unusual=null

## 专用审计异常注入必须真实触发，拒绝后应保持未安装并可重试；无hook时此门必须失败。
func install_exception_boundary(row: Dictionary) -> void:
	var bridge=fresh()
	var input: PackedByteArray=bytes(row.artifact)
	var query: PackedByteArray=bytes(row.queries)
	check(not bridge.audit().installed,"注入前实例未安装")
	OS.set_environment("HANMO_REVIEW_FAIL_NEXT_AUDIT","1")
	error(bridge.install_artifact(input),"NativeBridgeFailure","/bridge","审计构造bad_alloc拒绝整个安装")
	check(not OS.has_environment("HANMO_REVIEW_FAIL_NEXT_AUDIT"),"确定性hook已实际触发并只触发一次")
	var after: Dictionary=bridge.audit()
	check(not after.installed and after.install_count==0 and after.input_sha256.is_empty(),"错误返回不发布候选实例")
	error(bridge.query_batch(query),"NotInstalled","/runtime","失败后查询仍未安装")
	check(bridge.install_artifact(input).ok,"提交前异常之后可在同对象重试")
	check(bridge.audit().installed and bridge.audit().install_count==1,"重试只发布一次成功安装")
	check(bridge.query_batch(query).counts==document(row.oracle).counts,"重试后原查询与旧oracle相同")
	error(bridge.install_artifact(input),"AlreadyInstalled","/runtime","成功之后仍拒绝第二次安装")
	var weak: WeakRef=weakref(bridge); bridge=null
	check(weak.get_ref()==null,"异常候选及最后已发布实例按生命周期释放")

## 保持真实旧Kernel存活并验证state/RNG/revision，Bridge全过程没有收到该活世界引用。
func run() -> void:
	if not check(ProjectSettings.get_setting("application/config/native_inventory_bridge_isolated",false),"专用隔离工程标记"): quit(1); return
	if FileAccess.file_exists(ROOT+"hanmo_world.gdextension"):
		var loaded: int=GDExtensionManager.load_extension(ROOT+"hanmo_world.gdextension")
		check(loaded==GDExtensionManager.LOAD_STATUS_OK or loaded==GDExtensionManager.LOAD_STATUS_ALREADY_LOADED,"真实扩展加载状态")
	if not check(ClassDB.class_exists("HanmoWorldBridge"),"实际ClassDB存在HanmoWorldBridge"):
		print("NATIVE_BRIDGE checks=",checks," failures=",failures," oracle=",compared); quit(1); return
	check(ClassDB.is_parent_class("HanmoWorldBridge","RefCounted"),"桥为真实RefCounted")
	var manifest: Dictionary=document("cases.json")
	if "--string-boundaries-only" in OS.get_cmdline_user_args():
		string_boundaries(manifest.cases[1])
		print("BRIDGE_STRING checks=",checks," failures=",failures); quit(0 if failures==0 else 1); return
	if "--install-exception-only" in OS.get_cmdline_user_args():
		install_exception_boundary(manifest.cases[1])
		print("BRIDGE_INSTALL_EXCEPTION checks=",checks," failures=",failures); quit(0 if failures==0 else 1); return
	var kernel=Kernel.new()
	if not check(kernel.new_game("scenario.procurement","res://data/generated/procurement.hmcatalog.json").ok,"真实旧Kernel正式tiny开局"): quit(1); return
	var state: Dictionary=kernel.snapshot()
	var bundle: Dictionary=kernel.save_context()
	var state_bytes: PackedByteArray=var_to_bytes(state)
	for row in manifest.cases: oracle_case(row)
	boundaries(manifest.cases[1])
	string_boundaries(manifest.cases[1])
	check(compared==23,"复用全部冻结23旧oracle")
	check(kernel.snapshot()==state and var_to_bytes(kernel.snapshot())==state_bytes,"真实旧state/RNG/revision逐值逐字未变")
	check(kernel.save_context()==bundle,"真实旧三元组未被Bridge改写")
	print("BRIDGE_BUILD=",JSON.stringify(identities[0] if not identities.is_empty() else {}))
	print("NATIVE_BRIDGE checks=",checks," failures=",failures," oracle=",compared)
	quit(0 if failures==0 else 1)
