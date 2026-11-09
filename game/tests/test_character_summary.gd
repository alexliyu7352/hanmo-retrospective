## 成长用途保持性门：真实Main方法、Growth控件和Kernel报价；专用有效输入不冒充完整世界或GUI验收。
extends SceneTree
const Main=preload("res://ui/main.gd")
const Growth=preload("res://ui/growth_screen.gd")
const Art=preload("res://ui/art_catalog.gd")
const Character=preload("res://ui/character_presentation.gd")
const Registry=preload("res://core/runtime/module_registry.gd")
const Builtin=preload("res://core/runtime/builtin_modules.gd")
const Strict=preload("res://core/strict_json.gd")
const MANIFEST="res://../tests/fixtures/character_presentation/manifest.json"
const VIEW_PATH="res://core/views/character_summary.gd"
const ORACLE_SHA="650c4fc1ea67a75c7d95d6d1a98528f3fa61b4971c1cd2ae80db5ad5f307a5d6"
var checks: int=0
var failures: int=0
var registrations: Dictionary={}
var registry: RefCounted

## 仅记录实际预览和快照，全部命令仍调用正式Kernel，拒绝用替身报价证明保持性。
class ObservedKernel extends "res://core/world_kernel.gd":
	var calls: Array=[]
	var snapshots: int=0
	## 记录输入和真实完整报价，不改变原首错。
	func preview(kind: String, params: Dictionary={}, actor_id: String="") -> Dictionary:
		var value: Dictionary=super.preview(kind,params,actor_id)
		calls.append({"kind":kind,"params":params.duplicate(true),"quote":value.duplicate(true)})
		return value
	## 保留实际深复制，仅计数用途是否增加快照。
	func snapshot() -> Dictionary:
		snapshots+=1; return super.snapshot()

## 读取原表现清单、记录纹理请求并返回真实缺图；描述身份可核，不能声称美术验收。
class DescriptorRegistry extends RefCounted:
	var manifest: Dictionary={}
	var reads: Array=[]
	## 隔离清单输入引用。
	func _init(value: Dictionary) -> void: manifest=value.duplicate(true)
	## 公开描述副本，维持正式解析器输入。
	func describe() -> Dictionary: return manifest.duplicate(true)
	## 本门无图片，保留MissingRenderTexture出口。
	func texture(group: String, id: String) -> Texture2D:
		reads.append([group,id]); return null

## 引擎就绪后运行，不把Main放入树以免启动存储和音频。
func _init() -> void: call_deferred("_run")

## 所有断言计入最终退出码，脚本异常另审原日志。
func check(value: bool, label: String) -> bool:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)
	return value

## 故意逆序卡片、重复岗位和同tick事实，冻结实际旧排序、名单与来源选择。
func fixture() -> Dictionary:
	var state: Dictionary={"tick":40,"revision":7,"player_actor_id":"player","actors":{},"roles":{},"actions":{},"facts":{},"tasks":{},"inventories":{},"battles":{},"sieges":{},"encounters":{},"interactions":{},"rng":{"world":{"state":"unchanged"}}}
	var data: Dictionary={"actors":{},"texts":{},"places":{"room":{"id":"room","name_key":"room.name","settlement_id":"town"}},"roles":{},"items":{},"personal_skills":{},"offers":{"offer":{"id":"offer","name_key":"offer.name"}},"organizations":{"org":{"id":"org","name_key":"org.name"}},"required_capabilities":["cultivation"]}
	data.texts={"room.name":"授艺庭","offer.name":"运粮委托","org.name":"官署"}
	for id in ["player","student","teacher","other","absent","dead"]:
		state.actors[id]={"id":id,"alive":id!="dead","available":id!="student","action_id":"","birth_tick":-24*720,"location":{"kind":"Settlement","settlement_id":"town","place_id":"room"},"fatigue":12,"injury":0,"hp":100,"wallet_id":"wallet."+id,"inventory_id":"bag."+id,"combat":{"weapon_id":"z.sword","heart_id":"b.heart","learned":{}}}
		data.actors[id]={"id":id,"name_key":"name."+id,"portrait_kind":"adult.dedicated.a","presentation_profile_id":"fixture.person"}
		data.texts["name."+id]="姓名"+id
	state.actors.player.display_name="当前人物"
	state.actors.absent.location.place_id="away"
	state.actors.player.combat.learned={"z.unknown":50,"c.guard":40,"b.heart":60,"a.strike":85}
	for row in [["a.strike","strike"],["b.heart","heart"],["c.guard","guard"]]:
		data.personal_skills[row[0]]={"id":row[0],"name_key":row[0],"description_key":row[0]+".description","kind":row[1],"weapons":["sword"],"practice_ticks":8}
		data.texts[row[0]]="功法"+row[0]; data.texts[row[0]+".description"]="原作者说明"+row[0]
	for row in [["z.sword","sword"],["a.spear","spear"],["grain",""]]:
		data.items[row[0]]={"id":row[0],"name_key":row[0],"weight":2}
		data.texts[row[0]]="物品"+row[0]
		if not row[1].is_empty(): data.items[row[0]].weapon={"category":row[1],"minimum_range":1,"maximum_range":2}
	var lots: Array=[]
	for row in [["reserved","z.sword",1,"actor","player","hold"],["spear","a.spear",2,"actor","player",""],["public","grain",3,"task","task","report"],["org","grain",4,"organization","org",""],["held","grain",5,"actor","teacher","missing"],["unknown","grain",1,"legacy","missing.person",""]]:
		lots.append({"id":row[0],"item_id":row[1],"quantity":row[2],"owner":{"kind":row[3],"id":row[4]},"reserved_by":row[5]})
	state.inventories["bag.player"]={"capacity":100,"lots":lots}
	state.tasks.task={"offer_id":"offer"}; state.actions.report={"actor_id":"other","kind":"report_offer"}
	for row in [["z.student","student"],["a.student","student"],["b.teacher","teacher"],["c.self","player"],["d.absent","absent"],["e.dead","dead"],["f.disabled","other"]]:
		data.roles[row[0]]={"id":row[0],"place_id":"room","services":[]}
		state.roles[row[0]]={"actor_id":row[1],"enabled":row[0]!="f.disabled"}
	for row in [["later",20,"teacher","player","room"],["first",10,"teacher","player","room"],["tied",10,"other","player","missing.place"],["taught",30,"player","student","room"],["taught.tie",30,"player","other","missing.place"],["self",1,"","player","room"]]:
		state.facts[row[0]]={"kind":"training","phase":"completed","success":true,"completed_tick":row[1],"training":{"skill_id":"a.strike","teacher_id":row[2],"student_id":row[3],"place_id":row[4]}}
	state.facts.unknown={"kind":"other"}; state.facts.failed={"kind":"training","phase":"completed","success":false}
	return {"state":state,"data":data}

## Main只作真实方法宿主；完整受信注册表保留报价路由，构造节点附属后统一释放。
func host_for(f: Dictionary):
	var main=Main.new()
	for node in [main.storage,main.journey_view,main._ui_font_scale,main._ambience]: main.add_child(node)
	main._ambience.add_child(main._ambience.music); main._ambience.add_child(main._ambience.effect)
	main.kernel=ObservedKernel.new(); main.kernel._state=f.state; main.kernel._catalog={"data":f.data}; main.kernel._registry=registrations; main._data=f.data
	return main

## 原控件完整记录；仅跳过新增GrowthArmorControls独立子树，不重录旧oracle或忽略未知控件。
func controls(node: Node) -> Array:
	if node.name=="GrowthArmorControls": return []
	var result: Array=[]
	if node is Label: result.append({"label":node.text})
	if node is OptionButton:
		var rows: Array=[]
		for i in range(node.item_count): rows.append({"text":node.get_item_text(i),"metadata":node.get_item_metadata(i)})
		result.append({"option":String(node.name),"rows":rows,"selected":node.selected,"disabled":node.disabled})
	elif node is Button: result.append({"button":node.text,"disabled":node.disabled,"tooltip":node.tooltip_text,"kind":node.get_meta("command_kind",""),"target":node.get_meta("target_id","")})
	if node is ItemList:
		var rows: Array=[]
		for i in range(node.item_count): rows.append({"text":node.get_item_text(i),"tooltip":node.get_item_tooltip(i)})
		result.append({"cards":rows,"selected":Array(node.get_selected_items())})
	if node is SpinBox: result.append({"ticks":node.value,"min":node.min_value,"max":node.max_value})
	if node.has_meta("lot_id"): result.append({"lot_id":node.get_meta("lot_id"),"item_id":node.get_meta("item_id")})
	if node.name=="GrowthPortrait": result.append({"portrait_result":node.get_meta("render_result"),"tooltip":node.tooltip_text})
	for child in node.get_children(): result.append_array(controls(child))
	return result

## 原保持性oracle只剥离本次新增的护具报价，所有其他调用和先后顺序仍逐项比较。
func legacy_calls(calls: Array) -> Array:
	return calls.filter(func(row): return row.kind!="equip_armor").duplicate(true)

## 新护具专门测试完整覆盖；旧合同只去除明确新增三字段，未知扩展仍会使比较失败。
func legacy_equipment(equipment: Dictionary) -> Dictionary:
	var result: Dictionary=equipment.duplicate(true)
	for field in ["armor_id","armor_options","armor_effective"]: result.erase(field)
	return result

## 缓存中的完整真实描述冻结actor、分层、identity_key和cache_key，不能只比较新接口自身。
func descriptors() -> Array:
	var result: Array=[]; var ids: Array=Character._descriptors.keys(); ids.sort()
	for id in ids: result.append(Character._descriptors[id].duplicate(true))
	return result

## 正常门绑定改动前摘要；只有显式capture可写一次旧输出文件。
func digest(value: Variant) -> String: return JSON.stringify(value,"",true,true).sha256_text()

## 覆盖完整页分支和同帧选择闭包；选项只预览，绝不提交、授奖或推进半日。
func menu_case(kind: String, original: bool) -> Dictionary:
	var f: Dictionary=fixture(); var main=host_for(f)
	if kind=="empty": f.state.actors.player.combat.learned={}; f.state.roles={}; f.state.facts={}
	if kind=="inventory": main.set_meta("growth_page","inventory"); f.state.facts={}; f.state.roles={}
	if kind=="empty_inventory": main.set_meta("growth_page","inventory"); f.state.inventories["bag.player"].lots=[]
	if kind=="unknown_page": main.set_meta("growth_page","unknown"); main.set_meta("growth_selected_skill","missing")
	if kind=="heart": main.set_meta("growth_selected_skill","b.heart")
	if kind=="bad_appearance": f.state.actors.player.appearance=false
	if kind=="no_students": f.state.roles={}
	if kind=="no_inventory": f.state.inventories={}
	if kind=="advancing": main._advancing=true; main._message="原有反馈"
	if kind=="action":
		f.state.actors.player.action_id="training"; f.state.actions.training={"actor_id":"player","kind":"practice_skill","remaining_ticks":3,"params":{"training":{"skill_id":"a.strike"}}}
	var before: Dictionary=f.duplicate(true); var panel=Control.new(); root.add_child(panel)
	Character.clear_cache(); registry.reads=[]; seed(826); var random_next: int=randi(); seed(826)
	var opening: Dictionary=f.state.duplicate(true)
	var view: Dictionary=opening if original else load(VIEW_PATH).query(f.state,f.data,main.get_meta("growth_page","skills"))
	Growth.build(main,panel,view)
	check(f==before and randi()==random_next and main.kernel.snapshots==0,"成长绘表不改输入/RNG且专用snapshot仍0 "+kind)
	var output: Dictionary={"initial":controls(panel),"descriptors":descriptors(),"reads":registry.reads.duplicate(true)}
	if kind in ["skills","closure"]:
		if kind=="closure": f.state.actors.player.combat.learned["b.heart"]=99; f.state.actors.teacher.display_name="世界后来改名"; f.state.actors.player.action_id="later"
		var cards: ItemList=panel.find_child("GrowthCards",true,false); cards.select(1); cards.item_selected.emit(1)
		output.selected=controls(panel)
		var ticks: SpinBox=panel.find_child("GrowthPracticeTicks",true,false); ticks.value=8
		var weapons: OptionButton=panel.find_child("GrowthWeaponChoice",true,false); weapons.select(0); weapons.item_selected.emit(0)
		var students: OptionButton=panel.find_child("GrowthStudentChoice",true,false); students.select(1); students.item_selected.emit(1)
		output.changed=controls(panel)
	output.calls=legacy_calls(main.kernel.calls)
	output.metadata={"page":main.get_meta("growth_page","skills"),"selected":main.get_meta("growth_selected_skill",""),"ticks":main.get_meta("growth_practice_ticks",1)}
	if kind=="skills": check(output.calls[0].quote.get("code")=="WeaponNotOwned" and output.calls[2].quote.get("code")=="PersonAbsent","预留武器仍列出且不可用在场者仍首位，实际报价拒绝")
	if kind=="closure": check(panel.find_child("GrowthMastery",true,false).value==60 and output.calls[3].quote.get("code")=="ActorBusy","开窗时卡片保持60，选择后的真实报价读取后来忙碌世界")
	else: check(f==before,"全部真实选择回调不改变输入 "+kind)
	panel.free(); main.free()
	return output

## 只承诺两页实际字段，容器深隔离且姓名不因外观失败；不创建新的坏输入错误协议。
func query_contract(leaf) -> void:
	var f: Dictionary=fixture(); var before: Dictionary=f.duplicate(true)
	var view: Dictionary=leaf.query(f.state,f.data)
	var keys: Array=view.keys().map(func(key): return String(key)); keys.sort()
	check(keys==["action","actor_id","display_name","equipment","fatigue","injury","place_id","portrait_input","skills","students"],"skills页只公开十个有限字段 "+str(keys))
	check(view.actor_id=="player" and view.display_name=="当前人物" and view.action=={} and view.fatigue==12 and view.injury==0,"本人身份、状态和无行动分支")
	check(legacy_equipment(view.equipment)=={"weapon_id":"z.sword","heart_id":"b.heart","weapon_options":[{"item_id":"a.spear","quantity":2},{"item_id":"z.sword","quantity":1}]},"本人武器按ID排序且不暗删预留项")
	check(view.skills.map(func(row): return row.skill_id)==["a.strike","b.heart","c.guard"],"原已学ID过滤未知作者定义并排序")
	check(view.skills[0].provenance_text=="师承：姓名teacher · 授艺庭\n已传授：姓名student · 授艺庭" and view.skills[1].provenance_text=="来源：未见传承记录","最早/最近同tick先者与无记录文字保持")
	check(view.students==[{"display_name":"姓名student","context":{"place_id":"room","provider_actor_id":"student","role_id":"a.student"}},{"display_name":"姓名teacher","context":{"place_id":"room","provider_actor_id":"teacher","role_id":"b.teacher"}}],"岗位排序去重及不可用人物保留，不附带全角色")
	var appearance_keys: Array=view.portrait_input.appearance_view.keys(); appearance_keys.sort()
	check(view.portrait_input.ok and view.portrait_input.actor_view.actor_id=="player" and appearance_keys==["attire_style","base_features","format_version","generator_version","legacy"],"画像沿既有单人物公开五字段")
	view.equipment.weapon_options[0].quantity=999; view.skills[0].mastery=999; view.students[0].context.role_id="forged"; view.portrait_input.actor_view.display_name="forged"; view.portrait_input.appearance_view.base_features.forged=true
	check(f==before and leaf.query(f.state,f.data).equipment.weapon_options[0].quantity==2,"多层返回容器变更不污染权威数据和下次结果")
	var inventory: Dictionary=leaf.query(f.state,f.data,"inventory")
	keys=inventory.keys().map(func(key): return String(key)); keys.sort()
	check(keys==["action","actor_id","display_name","equipment","fatigue","injury","inventory","place_id","portrait_input"],"inventory页不附带skills/students或历史 "+str(keys))
	check(inventory.inventory.weight==32 and inventory.inventory.lots.map(func(row): return row.lot_id)==["reserved","spear","public","org","held","unknown"],"实际重量含公物代管预留且批次原序")
	check(inventory.inventory.lots[2].ownership_text=="委托公物 · 运粮委托" and inventory.inventory.lots[3].ownership_text=="机构公物 · 官署" and inventory.inventory.lots[4].reservation_text=="已预留 · 进行中的行动" and inventory.inventory.lots[5].ownership_text=="代管财物 · 姓名未记","原产权/未知人物/缺预留行动文字保持")
	inventory.inventory.lots[0].quantity=999
	check(f==before,"行囊返回批次不暴露原件引用")
	f.state.actors.player.appearance=false
	var bad: Dictionary=leaf.query(f.state,f.data,"inventory")
	check(bad.display_name=="当前人物" and not bad.portrait_input.ok and bad.portrait_input.code=="InvalidAppearanceProfile" and bad.inventory.weight==32,"坏appearance只影响画像，姓名和其它页数据仍保留")
	f=fixture(); f.state.facts=false; f.state.roles=false
	check(leaf.query(f.state,f.data,"inventory").inventory.lots.size()==6,"行囊不触碰技能事实和岗位，保持原早退")
	f.state.actors.player.combat.learned={}
	check(leaf.query(f.state,f.data).skills==[] and leaf.query(f.state,f.data).students==[],"空卡不触碰事实或岗位")
	f=fixture()
	check(leaf.query(f.state,f.data,"unknown")==leaf.query(f.state,f.data,"skills"),"未知页值仍按原skills分支，不重写UI metadata")
	var main=host_for(f)
	if main.kernel.has_method("character_summary"):
		check(main.kernel.call("character_summary")==leaf.query(f.state,f.data) and main.kernel.call("character_summary","inventory")==leaf.query(f.state,f.data,"inventory") and main.kernel.snapshots==0 and main.kernel.calls==[],"实际Kernel直接委托两页，不取快照、不做资格报价")
	main.free()

## 经真实Main打开和页签信号重绘，检查Composer确实传有限view；只核成长区域和快照归属。
func main_route_checks(expected: Dictionary, snapshot_count: int) -> void:
	var f: Dictionary=fixture(); f.state.wallets={"wallet.player":{"id":"wallet.player","owner":{"kind":"actor","id":"player"},"balance":123}}
	var main=host_for(f); main._started=true; main.page=VBoxContainer.new(); root.add_child(main.page)
	if not check(main.kernel.hud_view().ok,"真实HUD相邻输入具备玩家私人钱包必要字段"):
		main.page.free(); main.free(); return
	var before: Dictionary=f.duplicate(true)
	main._open_growth()
	var growth: Control=main.page.find_child("GrowthScreen",true,false)
	if check(growth!=null,"真实Main打开后经Composer进入Growth"):
		check(controls(growth)==expected.skills.initial,"真实Main成长区域保持原skills完整菜单文字")
		check(legacy_calls(main.kernel.calls)==expected.skills.calls.slice(0,3),"真实分派后的equip/practice/teach原预览序与报价")
		check(main.kernel.snapshots==snapshot_count,"正常Main/Composer及成长绘表均不取快照；实测 "+str(main.kernel.snapshots))
		main.kernel.calls=[]; main.kernel.snapshots=0
		var tab: Button=main.page.find_child("GrowthTabInventory",true,false); tab.pressed.emit()
		growth=main.page.find_child("GrowthScreen",true,false)
		check(controls(growth)==expected.inventory.initial and main.get_meta("growth_page")=="inventory","真实页签回调经Composer取inventory有限输入")
		check(legacy_calls(main.kernel.calls)==expected.inventory.calls and main.kernel.snapshots==snapshot_count,"行囊页保留原装备预览且正常Main/Composer快照仍0")
	check(f==before and not main.is_inside_tree() and not main.storage._busy,"两页实际Main回调不改世界或启动存储")
	main.page.free(); main.free()

## 先跑改动前输出，再由同一门验证新API，避免把新版自身输出录作期望。
func _run() -> void:
	registrations=Registry.build(Builtin.descriptors())
	if not check(registrations.get("ok",false),"受信完整注册表可用于真实报价"): quit(1); return
	registry=DescriptorRegistry.new(Strict.parse_text(FileAccess.get_file_as_string(MANIFEST)).value)
	var previous: RefCounted=Art._registry; Art._registry=registry
	var capture: String=""; var leaf_only: bool=false
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--capture-oracle="): capture=arg.trim_prefix("--capture-oracle=")
		if arg=="--leaf-only": leaf_only=true
	var exists: bool=FileAccess.file_exists(VIEW_PATH)
	var outputs: Dictionary={}
	for kind in ["skills","empty","heart","inventory","empty_inventory","unknown_page","bad_appearance","no_students","no_inventory","advancing","action","closure"]: outputs[kind]=menu_case(kind,not capture.is_empty() or not exists)
	if not capture.is_empty():
		var file=FileAccess.open(capture,FileAccess.WRITE); file.store_string(JSON.stringify({"outputs":outputs,"sha256":digest(outputs)},"\t",true,true)+"\n"); file.close(); print("CHARACTER_SUMMARY_ORACLE=",digest(outputs))
	else:
		check(digest(outputs)==ORACLE_SHA,"9e真实Growth完整文字/菜单/画像身份/真实preview oracle保持")
		check(exists,"有限CharacterSummary叶存在")
		var main=host_for(fixture()); var has_api: bool=main.kernel.has_method("character_summary")
		check(has_api,"正式Kernel人物成长门面存在"); main.free()
		if exists: query_contract(load(VIEW_PATH))
		if exists and has_api and not leaf_only: main_route_checks(outputs,0)
	Art._registry=previous; Character.clear_cache()
	await process_frame
	print("CHARACTER_SUMMARY_CHECKS=",checks," FAILURES=",failures)
	quit(0 if failures==0 else 1)
