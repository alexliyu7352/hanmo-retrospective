## 可见外观五字段与既有描述同态门；只读真实manifest，不解码PNG，不宣称资源或绘图验收。
extends SceneTree
const Appearance=preload("res://core/domains/identity/actor_appearance.gd")
const Character=preload("res://ui/character_presentation.gd")
const Art=preload("res://ui/art_catalog.gd")
const Strict=preload("res://core/strict_json.gd")
const MANIFEST="res://../tests/fixtures/character_presentation/manifest.json"
## 2c5eebd原resolver在修改前生成的22份完整描述摘要，原始输出保存在独立本地证据中。
const ORACLE_MANIFEST_SHA="9b6e4c6603eb5fd442ff67db4d34cf56ca0b22beceaddf0e690eda4e9b68ee4b"
const ORACLE_HASHES={
	"actor_error": "3af4b4c9e1a2b64ec7c50f823cb15de8342a5d45b4ec2e7f04f7898f1a541a37",
	"actor_ref_error": "3af4b4c9e1a2b64ec7c50f823cb15de8342a5d45b4ec2e7f04f7898f1a541a37",
	"age_error": "ceb00cf909d6003d6e54e3061a0795297e1b10c29d972708e448acce13d9669d",
	"avatar": "98416b43f737a3afad815b41a925f29ea243c038e227952d7afceb91086fbfba",
	"battle": "54041e58e03c1429d7a8c87a283d1d7acf6833449f51833e402a88e81de02edb",
	"child": "43c4027f13b124580b9672213d07c673cd14880f49f28ace08df1d431f2e478b",
	"context_error": "a6d84a48df8ef69c118c2915db19921d643d32ea8b30e6095503316b4703dc34",
	"custom": "0aaf1caf4f6e4ae490801fb860b324f4713f59a00dd7f68158bf68afe3676285",
	"expression": "8bdf0e8bf1805c8f8f72160f3b28c82f2437b4fa1566a39fa6d556d133eeac4d",
	"fixed": "0aaf1caf4f6e4ae490801fb860b324f4713f59a00dd7f68158bf68afe3676285",
	"generated": "0aaf1caf4f6e4ae490801fb860b324f4713f59a00dd7f68158bf68afe3676285",
	"inherited": "0aaf1caf4f6e4ae490801fb860b324f4713f59a00dd7f68158bf68afe3676285",
	"legacy": "496163fdcf42248a53d48803b8c6d499e1c4807adcc5bc0c8d9814f96af6b471",
	"missing_binding": "bc8f02053b7094a218b651739fd5bec57340f378456054646dc693e17b321e76",
	"parents_error": "3666115fd2f1d1d8ba028d4b8b231bb44f53f6419180d2009f6fee441fe7673c",
	"profile_error": "a13dfb4bf95b56156e18c0f474f608144f61c0260115b52c3e31c02796ee968c",
	"resolution": "3c28efc9b5cf6bb1ab4d5eea98df919935fe97747baf3ac603a606863d497abe",
	"resolution_error": "4740c008d1ba83ad4a789577b25f3384941eea684f2f81a4a6a3a970cfb37592",
	"seed_error": "67a9d4d1dcbd5d8affa9811a9875306b12936cc4ff9c14203426844446868064",
	"unknown_age": "888c2fda7941b97ebb47aba5766a6e0868d9030ac14da6e0ce72d1d05b54f527",
	"unmapped_feature": "6f64506fe3b24c6828a55d1f5793c7775c182e1fea95be14d1a5bd5fe0f20a64",
	"view_error": "b4b46fcd1585ef35553427f0710aabd2be4d6f4abfc0cf8cc51123403d0bddcd"
}
var appearance_api=Appearance.new()
var character_api=Character.new()
var art_api=Art.new()
var checks: int=0
var failures: int=0

## 只提供真实manifest的描述接口；若解析器开始读取图片立即计数，不伪造纹理成功。
class DescriptorRegistry extends RefCounted:
	var manifest: Dictionary={}
	var texture_reads: int=0
	## 测试显式保存只读清单副本，不冒称已执行Registry资源验收。
	func _init(value: Dictionary) -> void: manifest=value.duplicate(true)
	## 与正式注册表一致返回深副本，解析器不能污染fixture。
	func describe() -> Dictionary: return manifest.duplicate(true)
	## 本门不具备图片，不返回假的Texture；任何调用都可被断言识别。
	func texture(_group: String, _id: String) -> Texture2D:
		texture_reads+=1
		return null

## 固定引擎就绪后执行局部纯查询，不初始化世界或SceneHost。
func _init() -> void: call_deferred("_run")

## 每个失败进入退出码；确切旧oracle的摘要同时绑定全部描述字段与cache_key。
func check(value: bool, label: String) -> void:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)

## 固定真实语义档案覆盖五个既有来源；亲代与种子仅属于输入，绝不进入公开结果。
func profiles() -> Dictionary:
	var fixed: Dictionary=Appearance.create("person.a","unused",{"kind":"fixed","generator_version":"appearance-v1","attire_style":"plain","base_features":{"face_shape":"round","eyes":"calm","brows":"straight","nose":"plain","mouth":"plain","hair":"short","beard":"none","skin_palette":"warm","build":"slender"}}).profile
	var custom: Dictionary=fixed.duplicate(true); custom.source="custom"
	var generated: Dictionary=fixed.duplicate(true); generated.source="generated"; generated.generation_seed="private-generated-seed"
	var inherited: Dictionary=generated.duplicate(true); inherited.source="inherited"
	inherited.parent_sources=[{"feature":"face_shape","source":"parent","actor_id":"private.parent","value":"round","generator_version":"appearance-v1","generation_seed":"private-parent-seed"}]
	return {"legacy":Appearance.legacy(),"fixed":fixed,"custom":custom,"generated":generated,"inherited":inherited}

## 限定输入只含稳定公开显示信息；和原Character接口使用同一实际actor身份。
func actor_view() -> Dictionary:
	return {"actor_id":"person.a","display_name":"甲","age_years":24,"presentation_profile_id":"fixture.person","portrait_kind":"adult.dedicated.a","visibility_version":3}

## 明确列举原解析器的成功、降级和首错输入，避免只让两个新入口互相比对。
func cases() -> Array:
	var result: Array=[]
	for kind in profiles(): result.append({"label":kind,"id":"person.a","actor":actor_view(),"profile":profiles()[kind],"view":"half_body","context":{}})
	for variant in ["avatar","battle","child","unknown_age","resolution","expression","missing_binding","unmapped_feature","actor_error","actor_ref_error","view_error","profile_error","age_error","resolution_error","context_error","seed_error","parents_error"]:
		var row: Dictionary={"label":variant,"id":"person.a","actor":actor_view(),"profile":profiles().fixed,"view":"half_body","context":{}}
		match variant:
			"avatar","battle": row.view=variant
			"child": row.actor.age_years=8
			"unknown_age": row.actor.age_years=-1
			"resolution": row.context.resolution=[128,160]
			"expression": row.context.expression="smile"
			"missing_binding": row.actor.presentation_profile_id="absent"
			"unmapped_feature": row.profile.base_features.face_shape="unavailable"
			"actor_error": row.id=""; row.profile={}; row.view="invalid"
			"actor_ref_error": row.actor.actor_id="other"; row.profile={}
			"view_error": row.view="invalid"; row.profile={}
			"profile_error": row.profile={}; row.actor.age_years=24.0
			"age_error": row.actor.age_years=24.0
			"resolution_error": row.context.resolution=[0,80]
			"context_error": row.context.pose=true
			"seed_error": row.profile=profiles().generated; row.profile.generation_seed=false
			"parents_error": row.profile=profiles().inherited; row.profile.parent_sources=[]
		result.append(row)
	return result

## 规范摘要只比较完整已返回描述，不替代实际旧输入来源与字段白名单断言。
func digest(value: Variant) -> String: return JSON.stringify(value,"",true,true).sha256_text()

## 只在显式oracle捕获参数下写独立证据文件；常规门不会重录、覆盖或接受当前实现作旧基准。
func capture_oracle(path: String, rows: Array, registry) -> void:
	var outputs: Dictionary={}
	for row in rows:
		Character.clear_cache()
		var value: Dictionary=Character.resolve(row.id,row.actor,row.profile,row.view,row.context,registry)
		outputs[row.label]={"result":value,"sha256":digest(value)}
	var receipt: Dictionary={"resolver_sha256":FileAccess.get_sha256("res://ui/character_presentation.gd"),"appearance_sha256":FileAccess.get_sha256("res://core/domains/identity/actor_appearance.gd"),"manifest_sha256":FileAccess.get_sha256(MANIFEST),"outputs":outputs}
	var file=FileAccess.open(path,FileAccess.WRITE)
	if file==null: printerr("Cannot preserve oracle"); quit(1); return
	file.store_string(JSON.stringify(receipt,"\t",true,true)+"\n"); file.close()
	print("CHARACTER_ORACLE_CASES=",outputs.size()," TEXTURE_READS=",registry.texture_reads)
	quit(0 if registry.texture_reads==0 else 1)

## 新方法缺席时明确反例失败，不能因GDScript静态方法尚未存在而把编译错误当行为红门。
func available() -> bool:
	var ready: bool=true
	for spec in [[appearance_api,"project_visible"],[appearance_api,"validate_visible"],[character_api,"resolve_visible"],[art_api,"character_visible"]]:
		var exists: bool=spec[0].has_method(spec[1]); check(exists,"公开输入接口存在 "+spec[1]); ready=ready and exists
	return ready

## 公开字段深副本与来源私密信息彻底隔离；可修改的返回值不变成第二份权威外貌。
func projection_contract() -> void:
	var expected: Array=["attire_style","base_features","format_version","generator_version","legacy"]
	for kind in profiles():
		var profile: Dictionary=profiles()[kind]; var before: Dictionary=profile.duplicate(true)
		var projected: Dictionary=appearance_api.call("project_visible",profile)
		check(projected.ok,"完整合法来源可投影 "+kind)
		if not projected.ok: continue
		var keys: Array=projected.appearance_view.keys(); keys.sort()
		check(keys==expected and typeof(projected.appearance_view.legacy)==TYPE_BOOL and projected.appearance_view.legacy==(kind=="legacy"),"只给五字段并折叠legacy类别 "+kind)
		var serialized: String=JSON.stringify(projected)
		check(not "generation_seed" in serialized and not "parent_sources" in serialized and not "private" in serialized and not "source" in projected.appearance_view,"不泄露原始来源/亲代/seed "+kind)
		check(appearance_api.call("validate_visible",projected.appearance_view).ok and profile==before,"公开视图合法且输入不变 "+kind)
		projected.appearance_view.base_features.changed="mutated"
		check(profile==before and not appearance_api.call("project_visible",profile).appearance_view.base_features.has("changed"),"嵌套视图与原件/下次结果隔离 "+kind)
	for bad in [null,true,1,[],{},"profile"]:
		check(not appearance_api.call("project_visible",bad).ok and not appearance_api.call("validate_visible",bad).ok,"非档案/缺字段值拒绝")
	var valid: Dictionary=appearance_api.call("project_visible",profiles().fixed).appearance_view
	for field in ["generation_seed","parent_sources","source","wallet_id","skills"]:
		var invalid: Dictionary=valid.duplicate(true); invalid[field]="private"
		check(not appearance_api.call("validate_visible",invalid).ok,"额外字段不能进入可见合同 "+field)
	for pair in [["format_version",true],["format_version",1.0],["format_version",2],["legacy",0],["legacy","false"],["legacy",null],["generator_version",false],["generator_version","unknown"],["base_features",[]],["base_features",{}],["attire_style",false],["legacy",true]]:
		var invalid: Dictionary=valid.duplicate(true); invalid[pair[0]]=pair[1]
		check(not appearance_api.call("validate_visible",invalid).ok,"公开字段类型/版本/legacy一致性拒绝 "+str(pair))
	var invalid_legacy: Dictionary=appearance_api.call("project_visible",profiles().legacy).appearance_view
	invalid_legacy.legacy=false
	check(not appearance_api.call("validate_visible",invalid_legacy).ok,"legacy版本不能冒充普通外貌")

## 新投影只加两类已证实异常的类型前检；原结构/legacy首错与actor/view优先级仍保持。
func source_type_guards(registry) -> void:
	for field in ["source","generator_version"]:
		var profile: Dictionary=profiles().fixed; profile[field]=false
		var expected: String="InvalidAppearanceProfile" if field=="source" else "UnsupportedAppearanceGenerator"
		var before: Dictionary=profile.duplicate(true)
		check(appearance_api.call("project_visible",profile)=={"ok":false,"code":expected},"已证实来源比较异常改为有限结构化拒绝 "+field)
		check(Character.resolve("person.a",actor_view(),profile,"half_body",{},registry)=={"ok":false,"code":expected} and profile==before,"旧完整入口复用新类型前检且不改原件 "+field)
		check(Character.resolve("",actor_view(),profile,"invalid",{},registry).get("code")=="InvalidPresentationActor","来源错类型不能抢actor首错 "+field)
		check(Character.resolve("person.a",actor_view(),profile,"invalid",{},registry).get("code")=="InvalidCharacterView","来源错类型不能抢view首错 "+field)
		profile.erase("parent_sources")
		check(appearance_api.call("project_visible",profile).get("code")=="InvalidAppearanceProfile","缺字段原结构首错不被来源前检替代 "+field)
	var legacy: Dictionary=profiles().legacy; legacy.generator_version=false
	check(appearance_api.call("project_visible",legacy).get("code")=="InvalidLegacyAppearance","legacy坏canonical仍沿原专用拒绝")

## 旧入口与冻结旧oracle一致后再检查公开入口；同一缓存主体不能因调用先后或返回修改串状态。
func resolver_contract(rows: Array, registry) -> void:
	var old_registry=Art._registry; Art._registry=registry
	for row in rows:
		var before: String=JSON.stringify(row,"",true,true)
		Character.clear_cache()
		var old: Dictionary=Character.resolve(row.id,row.actor,row.profile,row.view,row.context,registry)
		check(ORACLE_HASHES.has(row.label) and digest(old)==ORACLE_HASHES.get(row.label,""),"原完整描述与修改前oracle逐字段同态 "+row.label)
		var projected: Dictionary=appearance_api.call("project_visible",row.profile)
		var full_valid: Dictionary=Appearance.validate(row.profile)
		check(projected.ok==full_valid.ok and (projected.ok or projected==full_valid),"完整来源仍先走原validate "+row.label)
		if projected.ok:
			var view: Dictionary=projected.appearance_view
			Character.clear_cache()
			var visible: Dictionary=character_api.call("resolve_visible",row.id,row.actor,view,row.view,row.context,registry)
			check(visible==old and art_api.call("character_visible",row.id,row.actor,view,row.view,row.context)==old,"公开resolver与Art薄入口精确保持原描述/cache_key "+row.label)
			if visible.get("layers") is Array:
				if not visible.layers.is_empty(): visible.layers[0].asset_id="mutated"
				visible.layers.append({"mutated":true})
			visible.clear()
			check(Character.resolve(row.id,row.actor,row.profile,row.view,row.context,registry)==old,"公开先调用及返回修改不污染旧入口缓存 "+row.label)
		check(JSON.stringify(row,"",true,true)==before,"所有输入原字节语义不变 "+row.label)
	Art._registry=old_registry
	for spec in [["",actor_view(),{},"invalid","InvalidPresentationActor"],["person.a",actor_view(),{},"invalid","InvalidCharacterView"],["person.a",actor_view(),{},"half_body","InvalidAppearanceProfile"]]:
		check(character_api.call("resolve_visible",spec[0],spec[1],spec[2],spec[3],{},registry).get("code")==spec[4],"公开入口保留actor/view/profile错误先后")

## 显式捕获必须先在旧源码运行；常规执行只读取已冻结摘要，不自动更新oracle。
func _run() -> void:
	var parsed: Dictionary=Strict.parse_text(FileAccess.get_file_as_string(MANIFEST))
	if not parsed.ok: printerr(parsed); quit(1); return
	var registry=DescriptorRegistry.new(parsed.value)
	var rows: Array=cases(); var args: PackedStringArray=OS.get_cmdline_user_args()
	if args.size()==2 and args[0]=="--capture-oracle": capture_oracle(args[1],rows,registry); return
	if not available(): print("CHARACTER_VISIBLE_CHECKS=%d FAILURES=%d"%[checks,failures]); quit(1); return
	check(FileAccess.get_sha256(MANIFEST)==ORACLE_MANIFEST_SHA,"fixture原字节保持旧oracle输入身份")
	projection_contract(); source_type_guards(registry); resolver_contract(rows,registry)
	check(registry.texture_reads==0,"本门没有伪造或解码图片结果")
	print("CHARACTER_VISIBLE_CHECKS=%d FAILURES=%d"%[checks,failures])
	quit(0 if failures==0 else 1)
