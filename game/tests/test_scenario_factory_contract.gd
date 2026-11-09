## 真实Factory初态与活动集合合同；legacy预期只来自固定提交的原Factory，不冒充Kernel已接通。
extends SceneTree
const Factory=preload("res://core/world_factory.gd")
const LegacyFactory=preload("res://tests/fixtures/scenario_factory/legacy_factory_ffec8fc.gd")
const Snapshot=preload("res://core/scenario_snapshot.gd")
const Strict=preload("res://core/strict_json.gd")
const LEGACY_FACTORY_SHA256="c049967ca510773a0759bbf84dd763da0588bd59d625e927bf99b4c16fe92b9c"
var checks: int=0
var failures: int=0

## 引擎初始化后读取真实作者fixture，所有断言在同一个有界短runner内执行。
func _init() -> void:
	call_deferred("run")

## 保留独立断言及非零退出码，不把前面成功掩盖后续兼容性失败。
func check(ok: bool, label: String) -> void:
	checks+=1
	if not ok: failures+=1; printerr("FAIL: ",label)

## 从同仓真实HM-CONTENT建立目录；这里只测Factory，不声称经过完整新Schema或Kernel。
func fixture(relative_path: String="tests/fixtures/content_packs/second_scenario/two_eras.json") -> Dictionary:
	var root: String=ProjectSettings.globalize_path("res://").path_join("..")
	var text: String=FileAccess.get_file_as_string(root.path_join("schemas/content.schema.json"))
	var registered: Dictionary=Snapshot.register_schema(text)
	if not registered.ok: return registered
	var parsed_schema: Dictionary=Strict.parse_text(text)
	var parsed_pack: Dictionary=Strict.parse_text(FileAccess.get_file_as_string(root.path_join(relative_path)))
	if not parsed_schema.ok: return parsed_schema
	if not parsed_pack.ok: return parsed_pack
	var schema: Dictionary=parsed_schema.value
	var pack: Dictionary=parsed_pack.value
	var data: Dictionary={"texts":pack.texts,"flags":pack.flags,"required_capabilities":pack.required_capabilities}
	for table in schema["x-hanmo-tables"]:
		data[table]={}
		for row in pack.get(table,[]): data[table][row.id]=row
	return {"ok":true,"data":data,"content_id":pack.id,"content_version":pack.version,"content_digest":(JSON.stringify(data,"",true,true)+"\n").sha256_text(),"schema_sha256":registered.schema_sha256}

## 修改测试作者初态后重算目录身份，避免用过期摘要包装本次实际输入。
func refresh_digest(catalog: Dictionary) -> void:
	catalog.content_digest=(JSON.stringify(catalog.data,"",true,true)+"\n").sha256_text()

## 保留库存枚举及内部lot先后顺序，不能用排序后的总数量掩盖实例ID改变。
func lot_sequence(state: Dictionary) -> Array:
	var sequence: Array=[]
	for id in state.inventories:
		for lot in state.inventories[id].lots:
			sequence.append({"inventory_id":id,"lot":lot.duplicate(true)})
	return sequence

## 沿真实库存加总同一物资，发现基础initial_goods与显式库存重复发放。
func item_quantity(state: Dictionary, item_id: String) -> int:
	var quantity: int=0
	for inventory in state.inventories.values():
		for lot in inventory.lots:
			if lot.item_id==item_id: quantity+=lot.quantity
	return quantity

## 单个legacy案例由固定旧代码建立预期，完整state、顺序、随机流和实例身份分别断言。
func legacy_case(catalog: Dictionary, scenario_id: String, label: String) -> void:
	var before: Dictionary=catalog.duplicate(true)
	var expected: Dictionary=LegacyFactory.create(catalog,scenario_id)
	var actual: Dictionary=Factory.create(catalog,scenario_id)
	check(expected.ok and actual.ok,label+"：原Factory与新Factory都成功")
	if not expected.ok or not actual.ok: return
	check(actual.state==expected.state,label+"：完整state与旧Factory逐字段相同")
	check(actual.state.rng==expected.state.rng and actual.state.rng.size()==5,label+"：五条随机流种子与状态保持")
	check(actual.state.next_instance_id==expected.state.next_instance_id,label+"：下一实例计数保持")
	check(lot_sequence(actual.state)==lot_sequence(expected.state),label+"：每一lot的ID、先后顺序、归属和数量保持")
	for table in ["actors","wallets","inventories","itineraries","knowledge","request_sequences"]:
		check(actual.state[table].keys()==expected.state[table].keys(),label+"：旧枚举顺序保持 "+table)
	check(catalog==before,label+"：两种Factory均不改源目录")

## 核对原Factory文件的确切原字节，再跑五份真实旧内容和非字典序的多货物目录。
func check_legacy_oracle() -> void:
	var path: String="res://tests/fixtures/scenario_factory/legacy_factory_ffec8fc.gd"
	check(FileAccess.get_file_as_string(path).sha256_text()==LEGACY_FACTORY_SHA256,"legacy oracle确为ffec8fc原Factory字节，没有从新实现构造预期")
	for pair in [["procurement","scenario.procurement"],["two_stories","scenario.two_stories"],["lived_city","scenario.lived_city"],["alpha_life","scenario.alpha_life"],["journey_life","scenario.lived_city"]]:
		var catalog: Dictionary=fixture("content/foundation/"+pair[0]+".json")
		check(catalog.ok,"真实legacy源可读："+pair[0])
		if catalog.ok: legacy_case(catalog,pair[1],pair[0])
	var reordered: Dictionary=fixture("content/foundation/alpha_life.json")
	if not reordered.ok: return
	for table in ["actors","wallets","inventories"]:
		var ids: Array=reordered.data[table].keys(); ids.reverse()
		var reversed_table: Dictionary={}
		for id in ids: reversed_table[id]=reordered.data[table][id]
		reordered.data[table]=reversed_table
	for inventory in reordered.data.inventories.values(): inventory.goods.reverse()
	refresh_digest(reordered)
	legacy_case(reordered,"scenario.alpha_life","alpha_life逆序实际定义")
	var legacy: Dictionary=Factory.create(reordered,"scenario.alpha_life")
	if legacy.ok:
		check(not legacy.state.inventories["bag:actor.player"].lots.is_empty(),"legacy人物初始武器仍实际进入背包")
		check(legacy.state.inventories["bag:actor.player"].lots[0].item_id=="item.sword","legacy初始货物沿旧物品身份保留")

## 初态只实例化选中真人；两年代地点、年龄与生命状态不被基础定义覆盖。
func check_two_eras() -> void:
	var catalog: Dictionary=fixture()
	var before: Dictionary=catalog.duplicate(true)
	for row in [["scenario.west","west.square",24],["scenario.east","east.square",44]]:
		var built: Dictionary=Factory.create(catalog,row[0])
		check(built.ok,"Factory接受确切剧本 "+row[0])
		if not built.ok: continue
		check(built.state.actors.size()==3 and not built.state.actors.has("actor.prototype"),"未激活原型没有真人实例")
		check(built.state.actors["actor.player"].location.place_id==row[1],"选中年代的开局地点")
		check(built.state.actors["actor.player"].get("birth_tick")==-row[2]*720,"选中年代的有效出生tick")
		check(not built.state.actors["actor.deceased"].alive and not built.state.actors["actor.deceased"].available,"已故初态人物保持死亡且不可用")
		check(built.has("catalog") and built.catalog.data.active_static_actor_ids.size()==3,"单一有效视图交给后续初始化调用方")
	check(catalog==before,"双年代Factory不修改源目录")

## 选人参数和非空外貌草稿只影响指定真人；家族创始身份与当前主角沿同一引用。
func check_player_and_draft() -> void:
	var catalog: Dictionary=fixture()
	var initial: Dictionary=catalog.data.scenarios["scenario.west"].initial
	var selected: Dictionary={}
	for row in initial.actors:
		if row.actor_id=="actor.partner": selected=row
	selected.appearance_definition.candidates.face_shape.append("round")
	selected.attributes.martial=77; selected.skills.transport=123
	refresh_digest(catalog)
	var draft: Dictionary={"base_features":{},"attire_style":selected.appearance_definition.attire_style}
	for key in selected.appearance_definition.candidates: draft.base_features[key]=selected.appearance_definition.candidates[key][0]
	draft.base_features.face_shape="round"
	var before: Dictionary=catalog.duplicate(true); var draft_before: Dictionary=draft.duplicate(true)
	var baseline: Dictionary=Factory.create(catalog,"scenario.west")
	var built: Dictionary=Factory.create(catalog,"scenario.west",draft,"actor.partner")
	check(baseline.ok and built.ok,"第四参选人和合法非空外貌草稿被接受")
	if not baseline.ok or not built.ok: return
	check(built.state.player_actor_id=="actor.partner" and built.state.family.founder_actor_id=="actor.partner","选中人物与家族创始身份一致且ID不换名")
	check(built.state.actors["actor.partner"].appearance.source=="custom" and built.state.actors["actor.partner"].appearance.base_features==draft.base_features,"草稿只在选中人物生成自定义外貌")
	check(built.state.actors["actor.partner"].attributes.martial==77 and built.state.actors["actor.partner"].skills.transport==123,"选中人物真实初始能力来自有效初态")
	for id in baseline.state.actors:
		if id!="actor.partner": check(built.state.actors[id].appearance==baseline.state.actors[id].appearance,"其他人物外貌不受选中草稿影响："+id)
	check(built.state.rng==baseline.state.rng and built.state.next_instance_id==baseline.state.next_instance_id,"选人和外貌草稿不消费玩法随机流或实例ID")
	check(built.state.actors.keys()==baseline.state.actors.keys(),"角色选择不交换或重建作者人物身份")
	for person in ["actor.deceased","actor.prototype","actor.missing"]:
		var failed: Dictionary=Factory.create(catalog,"scenario.west",{},person)
		check(not failed.ok and failed.code=="InvalidPlayableActor","非法选人保留失败诊断："+person)
	var invalid_draft: Dictionary=draft.duplicate(true); invalid_draft.base_features.eyes="forbidden"
	var failed: Dictionary=Factory.create(catalog,"scenario.west",invalid_draft,"actor.partner")
	check(not failed.ok and failed.code=="InvalidAppearanceSelection","非法草稿拒绝而不退回默认外貌")
	check(catalog==before and draft==draft_before,"选人成功与失败均不改输入目录和草稿")

## 改派与禁用岗位严格取初态；新模式不继承基础初始货物或重复创建显式公产。
func check_role_and_goods() -> void:
	var catalog: Dictionary=fixture()
	var scenario: Dictionary=catalog.data.scenarios["scenario.west"]
	scenario.initial.roles[0].actor_id="actor.player"; scenario.initial.roles[0].enabled=false
	for row in scenario.initial.actors:
		if row.actor_id=="actor.player": row.place_id="west.office"
	for actor in catalog.data.actors.values(): actor.initial_goods=[{"item_id":"item.grain","quantity":7}]
	refresh_digest(catalog)
	var before: Dictionary=catalog.duplicate(true)
	var built: Dictionary=Factory.create(catalog,"scenario.west")
	check(built.ok,"显式改派/禁用岗位与基础货物并存的完整新初态可创建")
	if not built.ok: return
	check(built.state.roles["role.west"]=={"actor_id":"actor.player","enabled":false},"岗位人员覆盖且禁用值没有被硬填true")
	check(built.state.wallets["wallet.west"].balance==300,"公库金额取显式初态而非基础5000")
	check(built.state.inventories["inventory.west"].lots.size()==1 and built.state.inventories["inventory.west"].lots[0].quantity==25,"显式库存只有一次初始货物而非基础50")
	check(item_quantity(built.state,"item.grain")==25,"基础人物initial_goods没有在新模式重复发粮")
	for id in built.state.actors:
		check(built.state.inventories["bag:"+id].lots.is_empty(),"新模式默认个人包保持空且不偷绑公产："+id)
	check(not built.state.inventories.has("bag:actor.prototype") and not built.state.wallets.has("personal:actor.prototype"),"未激活原型没有自动私产容器")
	check(not built.state.inventories.has("inventory.east") and not built.state.wallets.has("wallet.east") and not built.state.inventories.has("store:org.east"),"另一剧本公产及接收仓没有被实例化")
	check(built.state.inventories.has("store:org.west") and built.state.inventories["store:org.west"].lots.is_empty(),"现有组织默认接收仓仍被保留且不重复发货")
	check(lot_sequence(built.state)==[{"inventory_id":"inventory.west","lot":{"id":"lot:1","item_id":"item.grain","quantity":25,"owner":{"kind":"organization","id":"org.west"},"reserved_by":""}}] and built.state.next_instance_id==2,"显式货物只分配一个真实lot身份")
	var repeated: Dictionary=Factory.create(catalog,"scenario.west")
	check(repeated.ok and repeated.state==built.state,"同源重复创建的完整初态一致")
	check(catalog==before,"岗位与物资投影不改原始定义或作者初态")

## 只运行Factory短合同；领域初始化、Kernel提交和存读完整门由中央另外负责。
func run() -> void:
	check_two_eras()
	check_legacy_oracle()
	check_player_and_draft()
	check_role_and_goods()
	print("SCENARIO_FACTORY_CHECKS=",checks," FAILURES=",failures)
	quit(0 if failures==0 else 1)
