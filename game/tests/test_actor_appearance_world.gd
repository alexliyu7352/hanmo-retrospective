## 稳定外貌接入真实新局、收养候选、规则迁移与原生槽；只跑有限半日，不冒称出生/长档或视觉验收。
extends SceneTree
const Appearance=preload("res://core/domains/identity/actor_appearance.gd")
const Identity=preload("res://core/actor_identity.gd")
const Factory=preload("res://core/world_factory.gd")
const Kernel=preload("res://core/world_kernel.gd")
const Codec=preload("res://core/save_codec.gd")
const Rules=preload("res://core/rules_migration.gd")
const Strict=preload("res://core/strict_json.gd")
const Native=preload("res://storage/native_storage.gd")
const Registry=preload("res://core/runtime/module_registry.gd")
const Builtin=preload("res://core/runtime/builtin_modules.gd")
const F=preload("res://tests/companion_fixture.gd")
var checks: int=0
var failed: int=0
var folder: String=""
var fixture: Dictionary={}
var catalog_path: String=""

## 测试使用唯一临时目录，退出时只清理本进程创建的文件。
func _init() -> void:
	call_deferred("_run")

## 明确记录每条行为断言，不允许脚本提前失败却以零断言通过。
func check(value: bool, label: String) -> bool:
	checks+=1
	if not value: failed+=1; printerr("FAIL: ",label)
	return value

## 从真实已准备作者目录构造带新语义的隔离样本，更新摘要后仍经正式加载/Schema/能力检查。
func make_catalog() -> void:
	folder=OS.get_user_data_dir().path_join("appearance-test-"+str(Time.get_ticks_usec()))
	DirAccess.make_dir_recursive_absolute(folder)
	fixture=Strict.parse_text(FileAccess.get_file_as_string("res://../tests/fixtures/appearance/contract.json")).value
	var index: Dictionary=Strict.parse_text(FileAccess.get_file_as_string("res://data/generated/alpha_life.hmcatalog.json")).value
	index.catalog.actors["actor.player"].appearance_definition=fixture.generated.duplicate(true)
	index.catalog.actors["actor.apprentice"].appearance_definition=fixture.generated.duplicate(true)
	index.catalog.actors["actor.chen"].appearance_definition=fixture.fixed.duplicate(true)
	index.catalog.actors["actor.chen"].origin="historical"
	index.catalog_sha256=(JSON.stringify(index.catalog,"",true,true)+"\n").sha256_text()
	catalog_path=folder.path_join("appearance.hmcatalog.json")
	var file=FileAccess.open(catalog_path,FileAccess.WRITE); file.store_string(JSON.stringify(index,"",true,true)); file.close()

## 返回正式Kernel，失败立即报告并交调用方停止其依赖断言。
func fresh():
	var k=Kernel.new()
	check(k.new_game("scenario.alpha_life",catalog_path).ok,"实际加载带新外貌的作者目录并完成新局")
	return k

## 只删除本任务新增字段用于逐项比较旧权威，不排除任何既有玩法值。
func without_appearance(state: Dictionary) -> Dictionary:
	var result: Dictionary=state.duplicate(true)
	for actor in result.actors.values(): actor.erase("appearance")
	return result

## 外貌新增拥有权必须属于固定身份域信任锚；删声明与自报要求也不能降级完整世界合同。
func check_registry_binding() -> void:
	for mode in ["missing","different_owner"]:
		var rows: Array=Builtin.descriptors()
		var claim: Dictionary={}
		var target: Dictionary={}
		for row in rows:
			if row.id=="runtime": target=row
			if row.id=="identity":
				for index in range(row.owns.size()-1,-1,-1):
					if row.owns[index].container=="actors" and row.owns[index].key=="appearance": claim=row.owns.pop_at(index)
			if row.id=="builtin":
				for index in range(row.required_ownership.size()-1,-1,-1):
					if row.required_ownership[index].container=="actors" and row.required_ownership[index].key=="appearance": row.required_ownership.remove_at(index)
		if mode=="different_owner": claim.owner="runtime"; target.owns.append(claim)
		check(Registry.build(rows).get("code")==("MissingStateOwner" if mode=="missing" else "MismatchedStateOwner"),"新增appearance拥有权不可由传入描述自证："+mode)

## 生成档案在工厂中一次确定；不声明外貌仍显式legacy，草案只在完整新局成功后写入。
func check_new_game() -> void:
	var k=fresh(); var state: Dictionary=k.snapshot()
	if state.is_empty(): return
	var content: Dictionary=k.content()
	var generated: Dictionary=state.actors["actor.player"].appearance
	check(generated.source=="generated" and Appearance.validate(generated).ok,"原新局创建路径保存生成实际结果")
	check(state.actors["actor.chen"].appearance.base_features==fixture.fixed.base_features and state.actors["actor.chen"].appearance.source=="fixed","历史fixed不借新局随机换脸")
	check(state.actors["actor.shen"].appearance==Appearance.legacy(),"未声明新外貌的旧作者保留显式legacy")
	var baseline: Dictionary=content.duplicate(true)
	for actor in baseline.data.actors.values(): actor.erase("appearance_definition")
	var with_profiles: Dictionary=Factory.create(content,"scenario.alpha_life").state
	var without_profiles: Dictionary=Factory.create(baseline,"scenario.alpha_life").state
	check(without_appearance(with_profiles)==without_appearance(without_profiles),"新外貌只增加档案，旧字段与五条玩法RNG完全相同")
	var draft: Dictionary={"base_features":fixture.fixed.base_features.duplicate(true),"attire_style":"plain"}
	var original: Dictionary=draft.duplicate(true)
	check(k.new_game("scenario.alpha_life",catalog_path,draft).ok,"新局第三参数复用受限候选确认完整选择")
	var chosen: Dictionary=k.snapshot()
	check(chosen.actors["actor.player"].appearance.source=="custom" and chosen.actors["actor.player"].appearance.base_features==draft.base_features and draft==original,"草案保持原样，实际选择标custom并深复制")
	check(without_appearance(chosen)==without_appearance(state),"自建外貌不换属性、出生日期、资产、身份或玩法RNG")
	var customized: Dictionary=Codec.decode(Codec.encode(chosen,0,"custom-appearance",k.content()),k.content())
	check(customized.ok and customized.state==chosen,"主角实际自建选择与custom来源完整Codec存读")
	draft.base_features.eyes="not-in-pool"
	check(not k.new_game("scenario.alpha_life",catalog_path,draft).ok and k.snapshot()==chosen,"非法草案失败保留原完整世界")
	draft=original.duplicate(true); draft.attire_style="not-declared"
	check(not k.new_game("scenario.alpha_life",catalog_path,draft).ok and k.snapshot()==chosen,"衣服选择不能绕过本轮声明范围")
	var bad: Dictionary=content.duplicate(true); bad.data.actors["actor.player"].appearance_definition.candidates.eyes=[]
	var saved: Dictionary=bad.duplicate(true)
	check(not Factory.create(bad,"scenario.alpha_life").ok and bad==saved,"工厂坏配置不修改作者源或返回半个新世界")
	var fixed_actor: Dictionary=content.data.actors["actor.chen"]
	var changed_fixed: Dictionary=fixture.fixed.base_features.duplicate(true); changed_fixed.eyes="soft"
	check(not Appearance.prepare("actor.chen","seed",fixed_actor,{"base_features":changed_fixed,"attire_style":"plain"}).ok,"历史fixed草案不能改成候选脸")

## 领域动态创建在分配身份和私产前验证外貌；同来源重放、更名和年龄查询不会重生档案。
func check_dynamic() -> void:
	var k=fresh(); var state: Dictionary=k.snapshot()
	if state.is_empty(): return
	var data: Dictionary=k.content().data
	state.facts["appearance-source-a"]={"kind":"family_adoption"}
	state.facts["appearance-source-b"]={"kind":"family_adoption"}
	var rng: Dictionary=state.rng.duplicate(true)
	var first: Dictionary=Identity.instantiate(state,data,"actor.apprentice","甲",-15*720,"west.street","appearance-source-a")
	var second: Dictionary=Identity.instantiate(state,data,"actor.apprentice","乙",-15*720,"west.street","appearance-source-b")
	if not check(first.ok and second.ok,"同原型产生两个独立真人及外貌"): return
	check(first.actor_id!=second.actor_id and state.rng==rng,"永久actor_id区分来源且外貌不消费玩法RNG")
	var saved: Dictionary=state.duplicate(true)
	var profile: Dictionary=state.actors[first.actor_id].appearance.duplicate(true)
	check(Identity.instantiate(state,data,"actor.apprentice","甲",-15*720,"west.street","appearance-source-a").created==false and state==saved,"同来源幂等保留整个人物与外貌")
	state.actors[first.actor_id].display_name="改名甲"; state.facts[first.actor_id].display_name="改名甲"
	check(Identity.validate(state,data).ok and state.actors[first.actor_id].appearance==profile,"合法身份记录更名不改变任何外貌字段")
	var variants: Array=[{"id":"child","ages":[0,15]},{"id":"adult","ages":[16,100]}]
	state.tick=719
	check(Appearance.age_band(Identity.age_years(state,first.actor_id),variants).age_band=="child","周岁未满十六仍取儿童层")
	state.tick=720
	check(Appearance.age_band(Identity.age_years(state,first.actor_id),variants).age_band=="adult" and state.actors[first.actor_id].appearance==profile,"唯一TimeRules跨年龄不改基础特征或衣服")
	state=saved.duplicate(true)
	var sibling: Dictionary=state.actors[second.actor_id].appearance.duplicate(true)
	state.actors[first.actor_id].appearance.base_features.eyes="modified"
	check(state.actors[second.actor_id].appearance==sibling,"同原型档案不共享可变字典")
	state=saved.duplicate(true); state.facts["bad-appearance-source"]={"kind":"family_adoption"}
	var before: Dictionary=state.duplicate(true)
	data.actors["actor.apprentice"].appearance_definition.generator_version="unsupported"
	check(not Identity.instantiate(state,data,"actor.apprentice","失败候选",0,"west.street","bad-appearance-source").ok and state==before,"外貌失败在ID/钱包/背包/事实写入前全回滚")
	check(Identity.instantiate(state,data,"actor.apprentice","甲",-15*720,"west.street","appearance-source-a").created==false,"生成器更替不能重算已创建来源")
	data.actors["actor.apprentice"].appearance_definition=fixture.generated.duplicate(true)
	state.rng=[]; before=state.duplicate(true)
	check(not Identity.instantiate(state,data,"actor.apprentice","坏seed",0,"west.street","bad-appearance-source").ok and state==before,"坏随机来源形状拒绝而不是抛异常或分配ID")

## 合法继承夹具完整存读，并分别触及身份层的自指/悬空亲代拒绝；此处不创建真实出生或血缘事实。
func check_inherited_storage() -> void:
	var k=fresh(); var original: Dictionary=k.snapshot()
	if original.is_empty(): return
	var state: Dictionary=original.duplicate(true)
	var parents: Array=[{"actor_id":"actor.player","appearance":state.actors["actor.player"].appearance},{"actor_id":"actor.chen","appearance":state.actors["actor.chen"].appearance}]
	var inherited: Dictionary=Appearance.inherit_features(parents,"appearance-child-fixture",fixture.generated)
	if not check(inherited.ok,"已保存生物亲代档案生成合法纯继承夹具"): return
	state.actors["actor.apprentice"].appearance=inherited.profile
	var text: String=Codec.encode(state,0,"inherited-fixture",k.content())
	var decoded: Dictionary=Codec.decode(text,k.content())
	check(not text.is_empty() and decoded.ok and decoded.state==state,"合法inherited实际结果与出处完整Schema/Codec存读")
	for parent_id in ["actor.apprentice","actor.unknown_parent"]:
		var bad: Dictionary=state.duplicate(true)
		var replaced: String=bad.actors["actor.apprentice"].appearance.parent_sources[0].actor_id
		for parent in bad.actors["actor.apprentice"].appearance.parent_sources:
			if parent.actor_id==replaced: parent.actor_id=parent_id
		check(Appearance.validate(bad.actors["actor.apprentice"].appearance).ok,"亲代引用负例通过纯profile形状，确实到达身份层："+parent_id)
		check(Identity.validate(bad,k.content().data).get("code")=="InvalidAppearanceParents","身份层拒绝自指或不存在亲代："+parent_id)
		check(Codec.encode(bad,0,"invalid-parent",k.content()).is_empty() and Codec.decode(envelope(bad,k.content()),k.content()).get("code")=="InvalidAppearanceParents","完整存档边界不能保存或导入坏亲代引用："+parent_id)
		check(not k.restore(bad).ok and k.snapshot()==original,"坏亲代引用恢复不替换现有世界："+parent_id)

## 原收养事务完成后保存自己的生成档案；监护关系不把养父母特征复制给既有人物。
func check_adoption() -> void:
	var k=fresh()
	if k.snapshot().is_empty(): return
	check(F.send(k,"move_local",{"place_id":"west.estate"}).ok,"到场办理真实收养")
	var before: Dictionary=k.snapshot()
	var command: Dictionary={"kind":"family_adopt","params":{"context":F.context(k,"role.estate_steward"),"display_name":"外貌接续者"},"actor_id":before.player_actor_id,"request_seq":before.request_sequences[before.player_actor_id]+1,"expected_revision":before.revision}
	if not check(k.submit(command).ok and k.advance_one_tick().ok,"收养首半日中间态"): return
	var bytes: String=Codec.encode(k.snapshot(),1,"pending-appearance",k.content())
	var decoded: Dictionary=Codec.decode(bytes,k.content())
	check(decoded.ok and k.restore(decoded.state).ok,"待收养中间态完整存读")
	if not check(k.advance_one_tick().ok,"原家事候选完成第二半日"): return
	var state: Dictionary=k.snapshot(); var id: String=state.family.children.keys()[0]
	check(state.actors[id].appearance.source=="generated" and state.actors[id].appearance.parent_sources.is_empty(),"收养新人仅一次生成，不谎称遗传养父母")
	for actor_id in before.actors: check(state.actors[actor_id].appearance==before.actors[actor_id].appearance,"收养不改既有外貌："+actor_id)
	check(k.submit(command).ok and k.snapshot()==state,"重放收养请求不重抽、不重复生人")
	bytes=Codec.encode(state,2,"completed-appearance",k.content()); decoded=Codec.decode(bytes,k.content())
	check(decoded.ok and decoded.state==state,"收养后实际特征与永久创建来源完整保存")

## 重算合法封装摘要以检验语义拒绝，不能只证明损坏checksum可被发现。
func envelope(state: Dictionary, content: Dictionary, generation: int=2) -> String:
	var valid: Dictionary=state.duplicate(true); valid.ruleset_id=Rules.CURRENT
	for actor in valid.actors.values(): actor.appearance=Appearance.legacy()
	# Envelope元数据沿正式encode，替换的测试payload仍使用原严格字节合同。
	var outer: Dictionary=JSON.parse_string(Codec.encode(valid,generation,"manual-1",content))
	outer.ruleset_id=state.ruleset_id
	outer.payload_json=JSON.stringify(state,"",true,true)
	outer.payload_length=outer.payload_json.to_utf8_buffer().size()
	outer.payload_sha256=outer.payload_json.sha256_text()
	return JSON.stringify(outer,"",true,true)

## 当前档案强制完整，旧链仅追加legacy；原件不写入，HM-SAVE外壳仍为1。
func check_storage_and_migration() -> void:
	var k=fresh(); var state: Dictionary=k.snapshot()
	if state.is_empty(): return
	var content: Dictionary=k.content()
	var text: String=Codec.encode(state,1,"manual-1",content)
	var decoded: Dictionary=Codec.decode(text,content)
	check(decoded.ok and decoded.state==state and JSON.parse_string(text).format_version==1,"完整档案存读相同且HM-SAVE1不变")
	var store=Native.new(folder.path_join("saves"),content)
	check(store.put("manual-1",text,0).ok,"隔离原生槽保存真实首代")
	for mutation in ["missing","seed","version","features","format","parents"]:
		var bad: Dictionary=state.duplicate(true); var actor: Dictionary=bad.actors["actor.player"]
		match mutation:
			"missing": actor.erase("appearance")
			"seed": actor.appearance.generation_seed=123
			"version": actor.appearance.generator_version="appearance-v2"
			"features": actor.appearance.base_features.erase("eyes")
			"format": actor.appearance.format_version=2
			"parents": actor.appearance.parent_sources=[{"source":"parent"}]
		var tampered: String=envelope(bad,content)
		check(Codec.encode(bad,2,"manual-1",content).is_empty() and not Codec.decode(tampered,content).ok,"坏外貌不能编码或借合法摘要导入："+mutation)
		check(not k.restore(bad).ok and k.snapshot()==state,"坏外貌恢复保留原完整世界："+mutation)
		check(not store.put("manual-1",tampered,1).ok and store.get_slot("manual-1").text==text,"坏外貌不覆盖原槽原字节："+mutation)
	for version in [Rules.ARCHITECTURE,Rules.ARCHITECTURE_PREVIOUS,Rules.MILITARY]:
		var old: Dictionary=without_appearance(state); old.ruleset_id=version
		var original: Dictionary=old.duplicate(true)
		var result: Dictionary=Rules.upgrade(old)
		if not check(result.ok and old==original,"代表旧链只修改迁移候选："+version): continue
		var expected: Dictionary=old.duplicate(true); expected.ruleset_id=Rules.CURRENT
		for actor in expected.actors.values(): actor.appearance=Appearance.legacy()
		check(result.state==expected and result.state.rng==old.rng,"旧字段全比较，只补legacy和规则版本："+version)
		decoded=Codec.decode(envelope(old,content),content)
		check(decoded.ok and decoded.state==expected,"旧校验和封装迁入，不按新作者配置补脸："+version)
		check(Rules.upgrade(expected).state==expected and not Rules.upgrade(expected).migrated,"新规则迁移幂等："+version)
		old.actors["actor.player"].appearance=state.actors["actor.player"].appearance.duplicate(true)
		check(Rules.upgrade(old).get("code")=="AmbiguousLegacyAppearance","旧版本夹带新脸不能静默丢弃或洗白："+version)
	var upgraded_definition: Dictionary=content.duplicate(true)
	upgraded_definition.data.actors["actor.player"].appearance_definition.candidates.eyes=["future-only"]
	check(Codec.decode(text,upgraded_definition).state==state,"恢复按实际档案，候选池升级不改已存特征")
	for filename in DirAccess.get_files_at(folder.path_join("saves")): DirAccess.remove_absolute(folder.path_join("saves").path_join(filename))
	DirAccess.remove_absolute(folder.path_join("saves"))

## 按有限出口执行并清理本测试临时文件，所有失败汇总后非零退出。
func _run() -> void:
	make_catalog()
	check_registry_binding()
	check_new_game()
	check_dynamic()
	check_inherited_storage()
	check_adoption()
	check_storage_and_migration()
	DirAccess.remove_absolute(catalog_path); DirAccess.remove_absolute(folder)
	print("APPEARANCE_WORLD_CHECKS=",checks," FAILURES=",failed)
	quit(0 if failed==0 else 1)
