## 家事庄务用途门：原Main方法、真实页面和Kernel预览，不把小领域输入称为完整世界或GUI验收。
extends SceneTree
const Main=preload("res://ui/main.gd")
const FamilyScreen=preload("res://ui/family_screen.gd")
const EstateScreen=preload("res://ui/estate_screen.gd")
const Art=preload("res://ui/art_catalog.gd")
const Character=preload("res://ui/character_presentation.gd")
const Strict=preload("res://core/strict_json.gd")
const Registry=preload("res://core/runtime/module_registry.gd")
const Builtin=preload("res://core/runtime/builtin_modules.gd")
const F=preload("res://tests/family_estate_view_fixture.gd")
const FAMILY_PATH="res://core/views/family_page.gd"
const ESTATE_PATH="res://core/views/estate_page.gd"
const ORACLE_SHA="f68a071268c41bacbd30b01097311201aa7c71aea2dda9c017f76ac531afd07f"
var checks: int=0
var failures: int=0
var registrations: Dictionary={}
var registry: RefCounted
var snapshot_counts: Dictionary={}

## 原Kernel预览完整记录但不替换规则，快照仍执行真实深副本。
class ObservedKernel extends "res://core/world_kernel.gd":
	var calls: Array=[]
	var snapshots: int=0
	## 保留原首错、训练/资产计划和revision。
	func preview(kind: String, params: Dictionary={}, actor_id: String="") -> Dictionary:
		var quote: Dictionary=super.preview(kind,params,actor_id)
		calls.append({"kind":kind,"params":params.duplicate(true),"quote":quote.duplicate(true)})
		return quote
	## 观察Main._actor和入口是否仍拷贝整态。
	func snapshot() -> Dictionary:
		snapshots+=1; return super.snapshot()

## 描述取既有真实清单，纹理明确缺失，保身份/cache oracle而不声称画面通过。
class DescriptorRegistry extends RefCounted:
	var manifest: Dictionary={}
	var reads: Array=[]
	## 复制清单避免测试共享可写引用。
	func _init(value: Dictionary) -> void: manifest=value.duplicate(true)
	## 返回原注册表同形独立描述。
	func describe() -> Dictionary: return manifest.duplicate(true)
	## 无图片时保原MissingRenderTexture。
	func texture(group: String, id: String) -> Texture2D:
		reads.append([group,id]); return null

## 引擎就绪后运行，Main本体始终离树，不初始化Native/音频。
func _init() -> void: call_deferred("_run")

## 每个准备或保持性失败进入退出码，日志脚本错误另行核验。
func check(value: bool, label: String) -> bool:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)
	return value

## 保存真实构造对象供统一释放，所有页面仍调用Main原方法。
func host_for(f: Dictionary):
	var main=Main.new()
	for node in [main.storage,main.journey_view,main._ui_font_scale,main._ambience]: main.add_child(node)
	main._ambience.add_child(main._ambience.music); main._ambience.add_child(main._ambience.effect)
	main.kernel=ObservedKernel.new(); main.kernel._state=f.state; main.kernel._catalog={"data":f.data}; main.kernel._registry=registrations; main._data=f.data
	return main

## 只冻结可读控件和命令元数据，不把自动节点编号、动画时刻放入oracle。
func controls(node: Node) -> Array:
	var output: Array=[]
	if node is Label: output.append({"text":node.text})
	if node is OptionButton:
		var rows: Array=[]
		for index in range(node.item_count): rows.append({"text":node.get_item_text(index),"metadata":node.get_item_metadata(index),"disabled":node.is_item_disabled(index)})
		output.append({"options":rows,"selected":node.selected,"disabled":node.disabled,"resident_actor_id":node.get_meta("resident_actor_id","")})
	elif node is Button: output.append({"button":node.text,"kind":node.get_meta("command_kind",""),"target":node.get_meta("target_id",""),"disabled":node.disabled,"pressed":node.button_pressed,"tooltip":node.tooltip_text,"heir_actor_id":node.get_meta("heir_actor_id",""),"rest_duration":node.get_meta("rest_duration",0)})
	if node.has_meta("family_actor_id"): output.append({"family_actor_id":node.get_meta("family_actor_id")})
	if node.has_meta("render_result"): output.append({"portrait_result":node.get_meta("render_result"),"tooltip":node.tooltip_text})
	for child in node.get_children(): output.append_array(controls(child))
	return output

## 完整真实渲染描述包括actor和缓存身份，不用新入口互相对照。
func descriptors() -> Array:
	var values: Array=[]; var keys: Array=Character._descriptors.keys(); keys.sort()
	for key in keys: values.append(Character._descriptors[key].duplicate(true))
	return values

## 固定完整输出摘要，正常运行不录新期望。
func digest(value: Variant) -> String: return JSON.stringify(value,"",true,true).sha256_text()

## 只为指定页构造所需状态；真实完成来源走既有owner，个别显示错误边界单独标明。
func fixture(kind: String) -> Dictionary:
	var f: Dictionary=F.create()
	if not check(f.ok,"原Identity/Family/Estate初始领域输入 "+kind): return {}
	var status: Dictionary={"ok":true}
	if kind in ["family_child","family_adult","family_child_away","family_successor","rest_active","rest_completed","rest_tied"]: status=F.adopt(f)
	if status.ok and kind in ["family_adult","family_successor"]: status=F.designate(f)
	if status.ok and kind=="family_successor":
		var quote: Dictionary=F.Family.preview(f.state,f.data,"player","family_retire",{})
		status=F.Family.apply(f.state,f.data,"player","family_retire",quote) if quote.ok else quote
	if status.ok and kind=="family_pending": status=F.adopt(f,true)
	if status.ok and kind in ["rest_active","rest_completed","rest_tied"]: status=F.rest(f,kind!="rest_active")
	if status.ok and kind in ["estate_build_active","estate_build_done","estate_build_failed"]: status=F.build(f,kind!="estate_build_active",kind=="estate_build_failed")
	if not check(status.ok,"原owner准备所选显示阶段 "+kind+" "+str(status)): return {}
	if kind=="family_child_away": f.state.actors[f.child].location.place_id="away"; f.state.actors[f.child].alive=false
	if kind=="family_unknown_age": f.state.actors.player.erase("birth_tick")
	if kind=="family_bad_portrait": f.state.actors.player.appearance=false
	if kind=="family_bad_provider": f.state.roles["role.z.family"].enabled=false
	if kind=="rest_tied":
		for fact in f.state.facts.values():
			if fact.get("kind")=="long_rest":
				var later: Dictionary=fact.duplicate(true); later.stop_reason="duration"; later.consumed=999; f.state.facts["rest.tie"]=later; break
	if kind=="rest_empty": f.state.estates={}; f.data.estates={}
	if kind=="estate_no_manager": f.state.estates["estate.z"].manager_actor_id=""
	if kind=="estate_nonowner":
		f.state.estates["estate.z"].owner_actor_id="provider"; f.data.estates["estate.z"].owner_actor_id="provider"
		f.state.inventories["store.estate.z"].owner={"kind":"actor","id":"provider"}; f.state.wallets["cash.estate.z"].owner={"kind":"actor","id":"provider"}
	if kind=="estate_away": f.state.actors.player.location.place_id="away"
	if kind=="estate_detached":
		f.state.facts.muster={"kind":"estate_muster","estate_id":"estate.z"}; f.state.estates["estate.z"].detached.absent="muster"
	if kind=="estate_storage":
		f.state.wallets["cash.estate.z"].reserved={"held":150}; f.state.inventories["store.estate.z"].lots[0].reserved_by="held"
		f.state.inventories["store.estate.z"].lots.append({"id":"foreign","item_id":"grain","quantity":9,"owner":{"kind":"actor","id":"provider"},"reserved_by":""})
	if kind=="estate_same_item": f.data.estates["estate.z"].construction.material_item_id="grain"
	if kind=="estate_unfed":
		f.state.inventories["store.estate.z"].lots.pop_front()
		check(F.Estate.step(f.state,f.data,f.state.tick+1).ok,"原庄务生成缺粮产消事实"); f.state.tick+=1
	return f

## 调真实旧页或有限输入新页，全部真实preview和画像身份以旧输出为准。
func page_case(kind: String, original: bool, capturing: bool) -> Dictionary:
	var f: Dictionary=fixture(kind)
	if f.is_empty(): return {}
	var main=host_for(f); var stage=Control.new(); root.add_child(stage)
	var family: bool=kind.begins_with("family") or kind.begins_with("rest")
	var tab: String="rest" if kind.begins_with("rest") else "family"
	if not family: tab="build" if kind=="estate_build" else "storage" if kind in ["estate_storage","estate_same_item"] else "people"
	if kind.ends_with("unknown_tab"): tab="unknown"
	main.set_meta("family_tab" if family else "estate_tab",tab)
	main._message="原有反馈"; main._rest_auto=kind=="rest_active"
	Character.clear_cache(); registry.reads=[]
	var before: Dictionary=f.duplicate(true); seed(439); var next_random: int=randi(); seed(439)
	var id: String="missing" if kind=="estate_missing" else "estate.z"
	if family:
		if original: FamilyScreen.new().call("render",main,stage,f.state.duplicate(true),f.context)
		else: FamilyScreen.new().call("render",main,stage,load(FAMILY_PATH).query(f.state,f.data,tab,f.context))
	else:
		if original: EstateScreen.new().call("render",main,stage,f.state.duplicate(true),id)
		else: EstateScreen.new().call("render",main,stage,load(ESTATE_PATH).query(f.state,f.data,id,tab))
	var count: int=0 if family or kind in ["estate_away","estate_missing"] else 1+(0 if kind=="estate_no_manager" else 1)+(4 if tab not in ["build","storage"] else 0)
	snapshot_counts[kind]=main.kernel.snapshots
	check(main.kernel.snapshots==(count if capturing else 0),"该页专用snapshot "+kind+"="+str(main.kernel.snapshots))
	check(f==before and main.kernel._state==before.state and randi()==next_random,"原真实页面不改输入/全局及世界RNG "+kind)
	var output: Dictionary={"controls":controls(stage),"calls":main.kernel.calls.duplicate(true),"descriptors":descriptors(),"reads":registry.reads.duplicate(true),"tab":main.get_meta("family_tab" if family else "estate_tab")}
	if kind=="estate_nonowner": check(not output.controls.is_empty() and output.calls[-1].quote.get("code")=="EstateAuthorityRequired","同地非庄主仍可看，真实命令拒绝权限")
	if kind=="family_bad_provider": check(output.calls[0].quote.get("code")=="RoleNotAuthorized","原首提供人失效仍拒绝，不选后续可用岗位")
	stage.free(); main.free()
	return output

## 字段仅服务选中页面；错误外观、已知资格失败和未选择分支都不被新整页守卫改写。
func query_contract() -> void:
	var family=load(FAMILY_PATH); var estate=load(ESTATE_PATH)
	var f: Dictionary=F.create(); var before: Dictionary=f.duplicate(true)
	check(family.provider_context(f.state,f.data)==f.context,"开窗取作者首插入岗位而非按ID排序")
	f.state.roles["role.z.family"].enabled=false
	check(family.provider_context(f.state,f.data)==f.context,"首岗位无效仍不换第二人")
	f.state.roles.erase("role.z.family")
	check(family.provider_context(f.state,f.data)=={},"首作者岗位缺实际任职仍break，不择后项")
	f=before.duplicate(true)
	var view: Dictionary=family.query(f.state,f.data,"family",f.context)
	var keys: Array=view.keys().map(func(key): return String(key)); keys.sort()
	check(keys==["action","actor_id","adoption","age_years","children","display_name","fatigue","guardian","heir","hp","place_id","portrait_input"],"family页有限12字段不含原family/actors/facts")
	check(view.display_name=="本局庄主" and view.age_years==42 and view.children==[] and view.adoption.context==f.context,"姓名/年龄与原办理context保持")
	view.adoption.context.provider_actor_id="forged"; view.portrait_input.appearance_view.base_features.forged=true
	check(f==before,"家事返回context与外观深隔离")
	f.state.actors.player.appearance=false
	view=family.query(f.state,f.data)
	check(view.display_name=="本局庄主" and not view.portrait_input.ok,"坏画像不让已有姓名或整页失效")
	f=F.create(); f.state.actors.player.prototype_id="missing"
	check(family.query(f.state,f.data).display_name=="","未知身份沿家事空名回退，不借成长姓名未记")
	f=F.create(); check(F.adopt(f).ok,"子女返回隔离使用真实收养来源")
	view=family.query(f.state,f.data)
	check(view.children.size()==1 and view.children[0].actor_id==f.child and view.children[0].age_years==16 and view.children[0].heir_min_age==18 and view.children[0].together,"真实子女有限关系与年龄门槛")
	before=f.duplicate(true); view.children[0].display_name="forged"; view.children[0].portrait_input.actor_view.actor_id="forged"
	check(f==before,"子女行不泄漏身份或外观原件")
	f.data.roles=false; f.state.facts[f.state.family.children[f.child]].terms.policy=false
	view=family.query(f.state,f.data,"rest")
	check(not view.has("children") and not view.has("adoption") and view.estates[0].estate_id=="estate.a","rest不读取子女门槛或收养岗位，私庄按原ID序")
	f=F.create(); f.state.actors.player.location.place_id="away"; f.state.estates["estate.z"]=false
	check(not estate.available(f.state,f.data,"estate.z") and estate.query(f.state,f.data,"estate.z")=={"available":false},"不在场先短路，不读取损坏庄园内部")
	f=F.create(); view=estate.query(f.state,f.data,"estate.z")
	keys=view.keys().map(func(key): return String(key)); keys.sort()
	check(keys==["activity","available","completion","estate_id","last_production","manager","owner","residents","workforce"],"people页有限九字段，不返回Estate.view整对象")
	check(view.residents.map(func(row): return row.actor_id)==["worker","manager","absent","guard"] and view.workforce=={"workers_count":1,"guards_count":1,"defense":11},"居民原序不筛缺席，劳力守备复用原汇总")
	before=f.duplicate(true); view.residents[0].duty="forged"; view.manager.portrait_input.actor_view.actor_id="forged"
	check(f==before,"庄务居民和管事表现输入深隔离")
	view=estate.query(f.state,f.data,"estate.z","build")
	check(not view.has("residents") and not view.has("storage") and view.facilities==[{"facility_id":"farm","level":1},{"facility_id":"granary","level":0},{"facility_id":"palisade","level":1}],"build只投影原序三设施等级，不预先报价")
	f.state.wallets["cash.estate.z"]=false
	check(estate.query(f.state,f.data,"estate.z","build").facilities.size()==3,"build不提前读storage钱柜")
	f=fixture("estate_storage"); view=estate.query(f.state,f.data,"estate.z","storage")
	check(view.storage.wallet_balance==170 and view.storage.items[0]=={"item_id":"grain","available_quantity":0} and view.storage.weight==149,"钱柜仍总额、物品仍本人未预留、重量仍含代管")
	before=f.duplicate(true); view.storage.items[0].available_quantity=999
	check(f==before,"仓储行隔离原库存批次")
	f=F.create(); f.state.actors.player.action_id="busy"; f.state.actions.busy={"kind":"wait","remaining_ticks":1}; f.state.facts.bad={"kind":"estate_construction","phase":"completed","terms":false}
	check(estate.query(f.state,f.data,"estate.z","people").completion=={},"任何当前行动均不读取最近建设分支")
	f=F.create(); var main=host_for(f)
	if main.kernel.has_method("family_page"):
		check(main.kernel.family_provider_context()==family.provider_context(f.state,f.data) and main.kernel.family_page("family",f.context)==family.query(f.state,f.data,"family",f.context),"真实Kernel/Backend家事双口保持叶结果")
		check(main.kernel.estate_available("estate.z") and main.kernel.estate_page("estate.z","storage")==estate.query(f.state,f.data,"estate.z","storage") and main.kernel.snapshots==0 and main.kernel.calls==[],"真实庄务双口不借snapshot或preview")
	main.free()

## 查找真实页签按钮，避免依赖自动节点名称或绕过原回调。
func button(node: Node, kind: String, target: String) -> Button:
	if node is Button and node.get_meta("command_kind","")==kind and node.get_meta("target_id","")==target: return node
	for child in node.get_children():
		var found: Button=button(child,kind,target)
		if found!=null: return found
	return null

## 真实两个打开入口与页签串过Composer；开窗context不因之后任职改变被静默重选。
func main_route_checks(expected: Dictionary) -> void:
	var f: Dictionary=fixture("family_bad_provider"); var main=host_for(f)
	main._started=true; main._message="原有反馈"; main.page=VBoxContainer.new(); root.add_child(main.page)
	main._open_family()
	var frame: Control=main.page.find_child("FamilyScreen",true,false)
	if check(frame!=null,"真实Main家事打开经Composer有限输入"):
		check(main._family_context==f.context and controls(frame)==expected.family_bad_provider.controls and main.kernel.calls==expected.family_bad_provider.calls,"原首提供人、完整家事文字/报价均未漂移")
		check(main.kernel.snapshots==0,"家事入口及当前Main/Composer均不snapshot")
		main.kernel.snapshots=0; main.kernel.calls=[]
		button(main.page,"family_tab","rest").pressed.emit()
		check(controls(main.page.find_child("FamilyScreen",true,false))==expected.rest_initial.controls and main.kernel.calls==expected.rest_initial.calls and main.kernel.snapshots==0,"真实rest页签只读有限页、零snapshot，原报价/外地庄园保留")
		f.state.roles["role.z.family"].actor_id="manager"
		button(main.page,"family_tab","family").pressed.emit()
		check(main._family_context==f.context and controls(main.page.find_child("FamilyScreen",true,false))==expected.family_bad_provider.controls,"任职变动后重绘仍保原开窗context，不换可办理者")
	main.page.free(); main.free()
	f=fixture("estate_storage"); main=host_for(f)
	main._started=true; main._message="原有反馈"; main.page=VBoxContainer.new(); root.add_child(main.page)
	main._open_estate("estate.z")
	frame=main.page.find_child("EstateScreen",true,false)
	if check(frame!=null,"真实Main庄务打开经有限探针和Composer"):
		check(controls(frame)==expected.estate_people.controls and main.kernel.calls==expected.estate_people.calls and main.kernel.snapshots==0,"庄务people零snapshot，姓名与原办理报价保持")
		main.kernel.snapshots=0; main.kernel.calls=[]
		button(main.page,"estate_tab","storage").pressed.emit()
		check(controls(main.page.find_child("EstateScreen",true,false))==expected.estate_storage.controls and main.kernel.calls==expected.estate_storage.calls and main.kernel.snapshots==0,"实际storage页签零snapshot，保总余额、预留过滤与原报价序")
		main.kernel.snapshots=0; var epoch: int=main._scene_counter
		main._open_estate("missing")
		check(main._scene_counter==epoch and main._estate_id=="estate.z" and main.kernel.snapshots==0,"不存在庄园在轻探针原位置返回，不重绘或改页面")
	main.page.free(); main.free()

## 先录改动前真实页面，再绑定同一摘要；没有有限叶时明确以缺接口和额外快照形成红门。
func _run() -> void:
	registrations=Registry.build(Builtin.descriptors())
	if not check(registrations.get("ok",false),"完整受信注册表用于真实页面报价"): quit(1); return
	registry=DescriptorRegistry.new(Strict.parse_text(FileAccess.get_file_as_string("res://../tests/fixtures/character_presentation/manifest.json")).value)
	var previous: RefCounted=Art._registry; Art._registry=registry
	var capture: String=""
	var current_oracle: String=""
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--capture-oracle="): capture=arg.trim_prefix("--capture-oracle=")
		if arg.begins_with("--write-current-oracle="): current_oracle=arg.trim_prefix("--write-current-oracle=")
	var ready: bool=FileAccess.file_exists(FAMILY_PATH) and FileAccess.file_exists(ESTATE_PATH)
	var output: Dictionary={}
	for kind in ["family_empty","family_child","family_adult","family_successor","family_child_away","family_unknown_age","family_bad_portrait","family_bad_provider","family_pending","family_unknown_tab","rest_initial","rest_active","rest_completed","rest_tied","rest_empty","estate_people","estate_build","estate_storage","estate_same_item","estate_no_manager","estate_nonowner","estate_detached","estate_unfed","estate_build_active","estate_build_done","estate_build_failed","estate_away","estate_missing","estate_unknown_tab"]:
		output[kind]=page_case(kind,not capture.is_empty() or not ready,not capture.is_empty())
	if not current_oracle.is_empty():
		var current_file=FileAccess.open(current_oracle,FileAccess.WRITE); current_file.store_string(JSON.stringify({"output":output,"sha256":digest(output),"snapshots":snapshot_counts},"\t",true,true)+"\n"); current_file.close()
	if not capture.is_empty():
		var file=FileAccess.open(capture,FileAccess.WRITE); file.store_string(JSON.stringify({"output":output,"sha256":digest(output),"snapshots":snapshot_counts},"\t",true,true)+"\n"); file.close(); print("FAMILY_ESTATE_ORACLE=",digest(output))
	else:
		check(digest(output)==ORACLE_SHA,"两页有限合同、画像与实际报价oracle；庄务仅含已审设施选择差量")
		check(ready,"两个有限页面叶存在")
		var main=host_for(F.create())
		var all_api: bool=true
		for method in ["family_provider_context","family_page","estate_available","estate_page"]:
			all_api=check(main.kernel.has_method(method),"真实Kernel完整用途接口 "+method) and all_api
		main.free()
		if ready: query_contract()
		if ready and all_api: main_route_checks(output)
	Art._registry=previous; Character.clear_cache()
	await process_frame
	print("FAMILY_ESTATE_SNAPSHOTS=",JSON.stringify(snapshot_counts,"",true))
	print("FAMILY_ESTATE_VIEW_CHECKS=",checks," FAILURES=",failures)
	quit(0 if failures==0 else 1)
