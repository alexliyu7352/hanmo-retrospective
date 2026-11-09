## 显式legacy Bridge的独立GD oracle与深转换门；只读冻结工件，不重新跑旧游戏链或冒称GUI。
extends SceneTree
const ROOT="res://native_actor_commitments_bridge/"
const EXTENSION=ROOT+"hanmo_world.gdextension"
const LIMIT: int=64*1024*1024
var checks: int=0
var failures: int=0
var compared: int=0
var builds: Array=[]

## ClassDB动态验证后才调用新方法，旧so缺API应正常红而不是脚本解析失败。
func _init() -> void:call_deferred("run")
## 每个失败写明边界并影响退出码。
func check(value: bool,label: String) -> bool:
	checks+=1
	if not value:failures+=1;printerr("FAIL: ",label)
	return value
## 正式输入直接传PackedByteArray，不经Godot字符串解码或重新排序JSON。
func bytes(path: String) -> PackedByteArray:return FileAccess.get_file_as_bytes(ROOT+path)
## oracle和测试清单属于独立预期，不传进Bridge。
func document(path: String) -> Dictionary:return JSON.parse_string(FileAccess.get_file_as_string(ROOT+path))
## 每次创建真实扩展RefCounted，没有Kernel或query替身。
func fresh():return ClassDB.instantiate("HanmoWorldBridge")
## 独立解码原GD生成的tag树；整数直接从规范String转int64，不走JSON浮点。
func untag(row: Dictionary):
	match row.get("t",""):
		"null":return null
		"bool":
			if typeof(row.get("v"))!=TYPE_BOOL:check(false,"oracle bool类型");return null
			return row.v
		"int":
			if typeof(row.get("v"))!=TYPE_STRING:check(false,"oracle int必须十进制String");return null
			var number: int=row.v.to_int()
			if str(number)!=row.v:check(false,"oracle int64精确规范往返 "+row.v)
			return number
		"string":
			if typeof(row.get("v"))!=TYPE_STRING:check(false,"oracle String类型");return null
			return row.v
		"array":
			var result: Array=[]
			for value in row.v:result.append(untag(value))
			return result
		"dict":
			var result: Dictionary={}
			for key in row.v:result[key]=untag(row.v[key])
			return result
	check(false,"oracle未知tag");return null
## 逐层比较类型和值；bool/int/float不能因Variant宽松相等互相通过。
func exact(a: Variant,b: Variant) -> bool:
	if typeof(a)!=typeof(b):return false
	if a is Array:
		if a.size()!=b.size():return false
		for index in a.size():
			if not exact(a[index],b[index]):return false
		return true
	if a is Dictionary:
		if a.size()!=b.size():return false
		for key in b:
			if not a.has(key) or not exact(a[key],b[key]):return false
		return true
	return a==b
## 成功DTO只允许冻结六种纯值，递归拒绝Object/Callable/float等外泄。
func pure(value: Variant) -> bool:
	if value is Array:
		for child in value:
			if not pure(child):return false
		return true
	if value is Dictionary:
		for key in value:
			if typeof(key)!=TYPE_STRING or not pure(value[key]):return false
		return true
	return typeof(value) in [TYPE_NIL,TYPE_BOOL,TYPE_INT,TYPE_STRING]
## 任何失败都只有原code/path，无半批results/counts或成功字段。
func error(result: Dictionary,code: String,path: String,label: String) -> void:
	check(result=={"ok":false,"code":code,"path":path},label+": "+str(result))
## 期望逻辑来自原oracle，Godot格式明确独立于CLI标签格式。
func expected(path: String) -> Dictionary:
	var old: Dictionary=document(path)
	return {"ok":true,"format":"HM-GODOT-ACTOR-COMMITMENTS-RESULT/1","input_sha256":old.input_sha256,"queries_sha256":old.queries_sha256,"results":untag(old.results)}
## 每个冻结真实输入实际安装一次并核全部query，深改返回值及释放不影响组件或保留值。
func original_case(row: Dictionary) -> void:
	var path: String=row.directory;var input: PackedByteArray=bytes(path+"/artifact.json");var query: PackedByteArray=bytes(path+"/queries.json")
	var want: Dictionary=expected(path+"/oracle.json");var bridge=fresh()
	if not check(bridge.install_legacy_artifact(input).get("ok",false),"真实legacy安装 "+row.id):return
	var got: Dictionary=bridge.query_actor_commitments(query)
	if check(got.get("ok",false),"真实legacy查询 "+row.id):
		check(exact(got,want) and pure(got),"全部原GD oracle纯值/类型一致 "+row.id)
		compared+=got.results.size()
	var audit: Dictionary=bridge.audit();var saved: Dictionary=audit.duplicate(true)
	check(audit.get("installed",false) and typeof(audit.get("install_count"))==TYPE_INT and audit.install_count==1 and audit.input_sha256==row.input_sha256,"唯一安装/精确输入身份 "+row.id)
	check(audit.has("profile") and audit.has("record_component_sha256") and audit.build.profile=="offline_actor_commitments","legacy审计限定扩展 "+row.id)
	builds.append(audit.build.duplicate(true))
	input[0]=0
	if got.get("results",[]).size()>0:got.results[0].actors.clear()
	for group in audit.get("record_component_sha256",{}):audit.record_component_sha256[group].clear();break
	audit.inventory_lot_ids.clear();audit.build.clear()
	check(exact(bridge.audit(),saved) and exact(bridge.query_actor_commitments(query),want),"结果及audit深改不回写store "+row.id)
	var retained: Dictionary=bridge.query_actor_commitments(query);var weak: WeakRef=weakref(bridge);bridge=null
	check(weak.get_ref()==null and exact(retained,want),"释放真实RefCounted后纯值独立存活 "+row.id)
## 新wire值仅做明确语义变换；期望由Python改原oracle生成，不从native倒推。
func deep_values() -> void:
	var path: String="semantics/deep_values";var bridge=fresh()
	if not check(bridge.install_legacy_artifact(bytes(path+"/artifact.json")).ok,"纯wire深值安装"):return
	var want: Dictionary=expected(path+"/oracle.json");var got: Dictionary=bridge.query_actor_commitments(bytes(path+"/queries.json"))
	check(exact(got,want) and pure(got),"int64两端/bool/null/深owner数组独立预期精确一致")
	if got.get("ok",false):
		var probe: Dictionary=got.results[0].actors[0].current_activity.params.bridge_probe
		var values: Array=probe["极值"]
		check(typeof(values[0])==TYPE_NIL and typeof(values[1])==TYPE_BOOL and typeof(values[2])==TYPE_BOOL and typeof(values[3])==TYPE_INT and typeof(values[4])==TYPE_INT and typeof(values[5])==TYPE_STRING,"额外类型门不把相等数字/String/bool混同")
		check(str(values[3])=="-9223372036854775808" and str(values[4])=="9223372036854775807","有符号int64两端未过double")
		probe["极值"].clear();probe["深"]["原序"].reverse()
		check(exact(bridge.query_actor_commitments(bytes(path+"/queries.json")),want),"实际深params/owner返回不别名组件")
	bridge=null
## 深字符串在迟条结果才遇到也全批拒绝；此前好查询和审计均保持。
func string_boundaries() -> void:
	for name in ["params_nul","owner_nul","owner_key_nul"]:
		var path: String="semantics/"+name;var bridge=fresh()
		if not check(bridge.install_legacy_artifact(bytes(path+"/artifact.json")).ok,"原native值域允许NUL纯wire "+name):continue
		var before: Dictionary=bridge.audit();var safe: Dictionary=expected(path+"/safe-oracle.json")
		check(exact(bridge.query_actor_commitments(bytes(path+"/safe-queries.json")),safe),"未读坏文本的好query "+name)
		error(bridge.query_actor_commitments(bytes(path+"/queries.json")),"BridgeUnrepresentableString","/bridge/string","迟条深转换全体拒绝 "+name)
		check(exact(bridge.audit(),before) and exact(bridge.query_actor_commitments(bytes(path+"/safe-queries.json")),safe),"深转换拒绝后来源/好query仍相同 "+name)
		bridge=null
	var bridge=fresh();var input: PackedByteArray=bytes("cases/actual_reporting/artifact.json");var query: PackedByteArray=bytes("cases/actual_reporting/queries.json")
	check(bridge.install_legacy_artifact(input).ok,"迟条query字符串边界安装")
	var before: Dictionary=bridge.audit();var want: Dictionary=expected("cases/actual_reporting/oracle.json")
	for name in ["late_query_nul","late_error_path_nul","late_utf8"]:
		error(bridge.query_actor_commitments(bytes("semantics/"+name+"/queries.json")),"InvalidJson" if name=="late_utf8" else "BridgeUnrepresentableString","/queries" if name=="late_utf8" else "/bridge/string","原字节迟条拒绝 "+name)
	check(exact(bridge.audit(),before) and exact(bridge.query_actor_commitments(query),want),"所有迟条字符串拒绝未污染原完整batch")
	bridge=null
## 两install的类型/唯一实例和两query的profile门严格按冻结顺序；旧v1的count仍String。
func boundaries() -> void:
	var input: PackedByteArray=bytes("cases/actual_reporting/artifact.json");var query: PackedByteArray=bytes("cases/actual_reporting/queries.json")
	var old_input: PackedByteArray=bytes("inventory_v1/artifact.json");var old_query: PackedByteArray=bytes("inventory_v1/queries.json")
	var bridge=fresh();var empty: Dictionary=bridge.audit()
	check(not empty.installed and not empty.has("profile") and not empty.has("record_component_sha256") and empty.build.profile=="offline_inventory_count","空audit保持原v1形状")
	for bad in ["{}",[],[123,125],{},null,17,true]:
		for method in ["install_legacy_artifact","query_actor_commitments","install_artifact","query_batch"]:
			error(bridge.call(method,bad),"ExpectedPackedByteArray","/arguments/bytes","原Variant严格类型门 "+method)
	for method in ["query_actor_commitments","query_batch"]:error(bridge.call(method,PackedByteArray()),"NotInstalled","/runtime","未安装先拒 "+method)
	error(bridge.install_legacy_artifact("{".to_utf8_buffer()),"InvalidJson","/artifact","坏legacy安装不发布")
	error(bridge.install_artifact(input),"UnsupportedProjectionProfile","/artifact","原install不默接新profile")
	error(bridge.install_legacy_artifact(old_input),"UnsupportedProjectionProfile","/artifact","新install只接明确legacy profile")
	check(exact(bridge.audit(),empty),"所有安装失败仍原未装对象")
	check(bridge.install_legacy_artifact(input).ok,"同对象原失败后legacy重试")
	var before: Dictionary=bridge.audit()
	for method in ["install_artifact","install_legacy_artifact"]:
		error(bridge.call(method,"wrong"),"ExpectedPackedByteArray","/arguments/bytes","已装仍类型门先行 "+method)
		error(bridge.call(method,PackedByteArray()),"AlreadyInstalled","/runtime","任意成功install排斥另一install "+method)
	error(bridge.query_batch(PackedByteArray()),"UnsupportedProjectionProfile","/runtime/profile","legacy对象旧query先profile")
	var large=PackedByteArray();large.resize(LIMIT+1)
	error(bridge.query_batch(large),"UnsupportedProjectionProfile","/runtime/profile","跨profile先于字节限额")
	error(bridge.query_actor_commitments(large),"WireSizeLimit","/queries","正确profile查询字节限额")
	error(bridge.install_artifact(large),"AlreadyInstalled","/runtime","已装先于安装字节限额")
	var unused=fresh();error(unused.query_actor_commitments(large),"NotInstalled","/runtime","未装先于query限额")
	error(unused.install_legacy_artifact(large),"WireSizeLimit","/artifact","未装legacy安装限额")
	large.clear();unused=null
	var wrong: Dictionary=document("cases/actual_reporting/queries.json");wrong.input_sha256="0".repeat(64)
	error(bridge.query_actor_commitments(JSON.stringify(wrong).to_utf8_buffer()),"SourceMismatch","/queries/input_sha256","旧SHA不能刷新到当前工件")
	wrong=document("cases/actual_reporting/queries.json");wrong.queries.append(wrong.queries[0].duplicate(true))
	error(bridge.query_actor_commitments(JSON.stringify(wrong).to_utf8_buffer()),"DuplicateQueryId","/queries/queries/3/query_id","坏batch不返回前三行")
	check(exact(bridge.audit(),before) and exact(bridge.query_actor_commitments(query),expected("cases/actual_reporting/oracle.json")),"坏batch/profile/限额后原query与store不变")
	bridge=null
	var old=fresh();check(old.install_artifact(old_input).ok,"原v1真实工件可装")
	var old_result: Dictionary=old.query_batch(old_query);var old_want: Dictionary=document("inventory_v1/oracle.json")
	check(old_result.format==old_want.format and exact(old_result.counts,old_want.counts) and typeof(old_result.counts[0].count)==TYPE_STRING,"原v1 count格式/十进制String不变")
	var old_audit: Dictionary=old.audit();check(not old_audit.has("profile") and not old_audit.has("record_component_sha256") and old_audit.build.profile=="offline_inventory_count","v1 audit不增加legacy键")
	error(old.query_actor_commitments(PackedByteArray()),"UnsupportedProjectionProfile","/runtime/profile","v1对象新query先profile")
	error(old.install_legacy_artifact(input),"AlreadyInstalled","/runtime","v1成功后新install不能另装一个世界")
	old=null
## 仅专用link-wrapped库执行；必须实际消费独立标记，否则正常库不能假绿。
func install_exception() -> void:
	var bridge=fresh();var input: PackedByteArray=bytes("cases/actual_reporting/artifact.json")
	OS.set_environment("HANMO_REVIEW_FAIL_NEXT_LEGACY_AUDIT","1")
	error(bridge.install_legacy_artifact(input),"NativeBridgeFailure","/bridge","legacy候选audit一次bad_alloc不发布")
	check(not OS.has_environment("HANMO_REVIEW_FAIL_NEXT_LEGACY_AUDIT"),"新独立异常hook真实触发且只消费一次")
	var empty: Dictionary=bridge.audit();check(not empty.installed and empty.install_count==0 and empty.input_sha256.is_empty(),"异常后仍未安装")
	error(bridge.query_actor_commitments(bytes("cases/actual_reporting/queries.json")),"NotInstalled","/runtime","异常后query仍未装")
	check(bridge.install_legacy_artifact(input).ok,"同对象异常后可原样重试")
	check(exact(bridge.query_actor_commitments(bytes("cases/actual_reporting/queries.json")),expected("cases/actual_reporting/oracle.json")),"重试后实际query对原oracle")
	error(bridge.install_artifact(bytes("inventory_v1/artifact.json")),"AlreadyInstalled","/runtime","重试只发布唯一profile")
	var weak: WeakRef=weakref(bridge);bridge=null;check(weak.get_ref()==null,"异常及最终实例释放")
## GUI可要求已经自动加载；该模式绝不主动load_extension，headless结论单独标记。
func run() -> void:
	var args: PackedStringArray=OS.get_cmdline_user_args()
	if not check(ProjectSettings.get_setting("application/config/native_actor_bridge_isolated",false),"新显式Bridge隔离标记"):quit(1);return
	if "--require-auto-loaded" in args:
		if not check(GDExtensionManager.is_extension_loaded(EXTENSION),"GUI原自动发现已加载扩展"):quit(1);return
	else:
		var status: int=GDExtensionManager.load_extension(EXTENSION)
		if not check(status in [GDExtensionManager.LOAD_STATUS_OK,GDExtensionManager.LOAD_STATUS_ALREADY_LOADED],"明确实际扩展加载"):quit(1);return
	if not check(ClassDB.class_exists("HanmoWorldBridge"),"真实ClassDB扩展类"):quit(1);return
	var probe=fresh();var available: bool=probe.has_method("install_legacy_artifact") and probe.has_method("query_actor_commitments")
	check(available,"正式新API已绑定，旧so在此正常红")
	probe=null
	if not available:print("NATIVE_ACTOR_BRIDGE_CHECKS=",checks," FAILURES=",failures," COMPARED=",compared);quit(1);return
	if "--install-exception-only" in args:
		install_exception();print("ACTOR_BRIDGE_INSTALL_EXCEPTION_CHECKS=",checks," FAILURES=",failures);quit(0 if failures==0 else 1);return
	var manifest: Dictionary=document("cases.json")
	for row in manifest.cases:original_case(row)
	check(compared==60,"20冻结工件共60条旧oracle查询")
	deep_values();string_boundaries();boundaries()
	print("ACTOR_BRIDGE_BUILD=",JSON.stringify(builds[0] if not builds.is_empty() else {}))
	print("ACTOR_BRIDGE_SURFACE=",DisplayServer.get_name())
	print("NATIVE_ACTOR_BRIDGE_CHECKS=",checks," FAILURES=",failures," COMPARED=",compared)
	quit(0 if failures==0 else 1)
