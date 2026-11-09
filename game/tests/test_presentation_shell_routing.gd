## 外壳原行为准备门：真实Main/Composer路由与货单缓存时点；当前只冻结旧oracle，不实现新查询。
extends "res://tests/test_family_estate_views.gd"
const PersonalFixture=preload("res://tests/test_personal_area_strike.gd")
const MilitaryFixture=preload("res://tests/military_fixture.gd")
const Maps=preload("res://tests/test_world_terrain.gd")
const ActionRuntime=preload("res://core/runtime/action_runtime.gd")
const StudyRules=preload("res://core/domains/cultivation/skill_study_rules.gd")
const TradeAction=preload("res://core/domains/economy/trade_action.gd")
const WorldSchema=preload("res://core/state_schema.gd")
const SIEGE_ROUTE_ORACLE="44ca16462e9f0738a559bd7828c48a0b73650c2ab1e46ce5c41c02eaf9ce315e"
const SHELL_SEMANTIC_ORACLE="3557a587759d556ce06479f7b5cebbb3be65c32c8c1a381e3e499cf7dffb65a0"
var snapshots: Dictionary={}
var expected_outer_snapshots: int=0
var events: Array=[]

## 原Main仅关闭Native初始化与逐帧自动操作；_render、_apply_command_result和全部页面方法仍真实继承。
class ShellHost extends "res://ui/main.gd":
	## 本门不启动存储、音频或完整新局菜单初始化。
	func _ready() -> void: pass
	## 无后台自动推进，路由检查显式调用真实_render。
	func _process(_delta: float) -> void: pass

## 成交收尾只冻结进入重绘/延迟推进前的缓存；不拿小领域状态冒充可提交全世界。
class ResultHost extends ShellHost:
	var rendered: Array=[]
	## 记录原_apply_command_result到达的重绘时点，不在此门安排另一个世界半日。
	func _render() -> void: rendered.append({"message":_message,"receipt":_trade_receipt.duplicate(true),"advancing":_advancing})
	## 保留原deferred调用目标，但不执行全世界推进矩阵。
	func _finish_short() -> void: pass

## 只观察公共出口和真实snapshot；页面结果、preview和世界来源一律用原Backend。
class ShellKernel extends "res://core/world_kernel.gd":
	var reads: Array=[]
	var snapshot_count: int=0
	## 记录两份外壳原快照，并仍执行真实深副本。
	func snapshot() -> Dictionary: reads.append("snapshot"); snapshot_count+=1; return super.snapshot()
	## 缓存查询在Main清缓存之前执行，完整真实Kernel结果不由测试拼造。
	func trade_cache_context() -> Dictionary: reads.append("trade_cache_context"); return super.trade_cache_context()
	## 当次成功trade在原结果处理点读取，记录action ID以核无源分支也没有倒置时点。
	func trade_action_receipt(action_id: String) -> Dictionary:
		reads.append(["trade_action_receipt",action_id]); return super.trade_action_receipt(action_id)
	## 只记录后续总路由的有限位置口；目前Composer仍留原一份snapshot。
	func presentation_location() -> Dictionary: reads.append("presentation_location"); return super.presentation_location()
	## 个人战仍执行现有选择与有限页面。
	func personal_battle_view() -> Dictionary: reads.append("personal"); return super.personal_battle_view()
	## 野战仍执行现有两遍选择与有限页面。
	func military_battle_page(id: String="") -> Dictionary: reads.append("military"); return super.military_battle_page(id)
	## 只观察原动作上下文时点，不提前判断资格或活动窗口。
	func action_context() -> Dictionary: reads.append("action_context"); return super.action_context()
	## 记录每个原报价且不更改返回对象。
	func preview(kind: String, params: Dictionary={}, actor_id: String="") -> Dictionary:
		reads.append(["preview",kind,params.duplicate(true),actor_id]); return super.preview(kind,params,actor_id)
	## 普通场所读取必须仍留在选中该页之后。
	func place_sidebar() -> Dictionary: reads.append("place_sidebar"); return super.place_sidebar()
	## 行旅侧栏仍由原同源投影负责。
	func journey_sidebar() -> Dictionary: reads.append("journey_sidebar"); return super.journey_sidebar()
	## 不用对白完整视图代替raw phase缓存读取。
	func interaction_view(actor_id: String="") -> Dictionary: reads.append("interaction_view"); return super.interaction_view(actor_id)

## 标题所需表现枚举仍为空；纹理与原字符manifest同前门，不声明真实图片呈现。
class ShellArt extends DescriptorRegistry:
	## 无额外资源，不触发磁盘美术恢复。
	func ids(_group: String) -> Array: return []

## 复用既有有效人物/家产领域输入，加普通地图与空军务壳所必需的小作者定义。
func shell_fixture() -> Dictionary:
	var f: Dictionary=F.create()
	f.state.merge({"flags":{},"events":{},"news":{},"relations":{},"parties":{},"offices":{},"military_units":{},"armies":{},"campaigns":{}})
	f.data.merge({"offices":{},"garrisons":{},"military_orders":{},"strongholds":{},"routes":{},"world_maps":{},"dialogues":{},"props":{},"personal_skills":{},"personal_hearts":{}})
	f.data.settlements.town.map_xy=[40,40]; f.data.settlements.town.name_key="town"; f.data.texts.town="本城"
	f.data.settlements.other={"id":"other","entry_place":"other.room","map_xy":[100,40],"name_key":"other"}; f.data.texts.other="外城"
	f.data.places["other.room"]={"id":"other.room","name_key":"other","settlement_id":"other","parent_place":null,"passages":[]}
	f.data.routes.road={"id":"road","origin":"town","destination":"other","duration_ticks":3,"cost_per_tick":2}
	return f

## 页面领域夹具复用已验个人/野战规则；只检路由，不冒称完整世界战役来源或Schema。
func battle_source(f: Dictionary, personal: bool) -> void:
	var definitions: Dictionary=PersonalFixture.content() if personal else MilitaryFixture.data()
	for key in definitions:
		if key=="items": f.data.items.merge(definitions.items,true)
		else: f.data[key]=definitions[key].duplicate(true) if definitions[key] is Dictionary or definitions[key] is Array else definitions[key]
	if personal: f.data.personal_objectives.spar.kind="sparring"
	var battle: Dictionary=PersonalFixture.battle() if personal else MilitaryFixture.battle(definitions)
	battle.id="shell.personal" if personal else "shell.military"; battle.kind="personal" if personal else "military"
	var actor_ids: Array=[]
	if personal:
		for id in battle.units: actor_ids.append(id)
	else:
		for unit in battle.units.values(): actor_ids.append(unit.commander_actor_id)
	for id in actor_ids:
		var actor: Dictionary=f.state.actors.player.duplicate(true); actor.id=id; actor.wallet_id="wallet."+id; actor.inventory_id="bag."+id
		f.state.actors[id]=actor; f.data.actors[id]={"id":id,"name_key":"name."+id}; f.data.texts["name."+id]=id
		f.state.wallets[actor.wallet_id]={"id":actor.wallet_id,"owner":{"kind":"actor","id":id},"balance":200,"reserved":{}}
		f.state.inventories[actor.inventory_id]={"id":actor.inventory_id,"owner":{"kind":"actor","id":id},"capacity":1000,"revision":0,"lots":[]}
		f.state.request_sequences[id]=0
	f.state.player_actor_id=battle.controller_id if personal else battle.controller_actor_id
	f.state.battles[battle.id]=battle

## 普通课程使用旧叶门的80/650经验来源，经真实公共ActionRuntime开课，不伪造课时或付款。
func study_source(f: Dictionary, active: bool) -> void:
	f.data.required_capabilities.append("cultivation")
	f.data.roles.school={"id":"school","place_id":"room","organization_id":null,"services":["study_skill"],"skill_courses":[{"skill_id":"rites","mode":"teacher","wallet_id":"wallet.provider"}]}
	f.state.roles.school={"actor_id":"provider","enabled":true}; f.state.actors.player.skills.rites=80; f.state.actors.provider.skills.rites=650
	if not active: return
	var params: Dictionary={"skill_id":"rites","ticks":2,"context":{"place_id":"room","provider_actor_id":"provider","role_id":"school"}}
	var offer: Dictionary=StudyRules.preview(f.state,f.data,"player",params)
	if not check(offer.ok,"原课程领域报价可开始"): return
	check(ActionRuntime.begin(f.state,f.data,"player","study_skill",params,offer,registrations).ok,"原公共runtime准备真实活动课程")
	check(StudyRules.validate_state(f.state,f.data).ok,"课程活动来源通过原领域校验")

## 货单仅准备旧Economy所需真实商户和库存；完整世界提交/存档不在此门冒充。
func trade_source(f: Dictionary) -> Dictionary:
	f.data.required_capabilities.append("economy")
	f.data.shops={"shop":{"id":"shop","place_id":"room","representative_roles":["shop.role"],"inventory_id":"bag.provider","wallet_id":"wallet.provider"}}
	f.data.roles["shop.role"]={"id":"shop.role","place_id":"room","services":["trade"],"organization_id":null}; f.state.roles["shop.role"]={"actor_id":"provider","enabled":true}
	f.state.inventories["bag.provider"].lots.append({"id":"trade.stock","item_id":"grain","quantity":20,"owner":{"kind":"actor","id":"provider"},"reserved_by":""})
	return {"shop_id":"shop","item_id":"grain","quantity":2,"side":"buy","payer_wallet_id":"wallet.player","context":{"place_id":"room","provider_actor_id":"provider","role_id":"shop.role"}}

## 真实Main挂树但ready关闭；对象/字体/音乐节点仅保已有宿主结构。
func shell_host(f: Dictionary, result_only: bool=false):
	var main=ResultHost.new() if result_only else ShellHost.new()
	var helpers=Node.new()
	for node in [main.storage,main.journey_view,main._ui_font_scale,main._ambience]: helpers.add_child(node)
	main.set_meta("test_helpers",helpers)
	main._ambience.add_child(main._ambience.music); main._ambience.add_child(main._ambience.effect)
	main.kernel=ShellKernel.new(); main.kernel._state=f.state; main.kernel._catalog={"data":f.data}; main.kernel._registry=registrations; main._data=f.data
	main.page=VBoxContainer.new(); main.add_child(main.page); main._started=true; root.add_child(main)
	return main

## 辅助成员始终离树，释放时不触发音频_ready或Native存储初始化。
func dispose(main) -> void:
	main.get_meta("test_helpers").free(); main.free()

## 仅冻结实际稳定控件和路由节点，不记录缓存对象地址或新建节点自动编号。
func shell_controls(node: Node) -> Array:
	var output: Array=[]
	if str(node.name) in ["TitleStage","PlaceStage","PersonalBattleScreen","MilitaryBattleScreen","SiegeScreen","MilitaryCampScreen","FamilyScreen","EstateScreen","GrowthScreen","WorldMapCanvas","StudyActiveAction","StudyCourses"]: output.append({"route":str(node.name)})
	if node is Label: output.append({"text":node.text})
	if node is Button and not node is OptionButton: output.append({"button":node.text,"kind":node.get_meta("command_kind",""),"target":node.get_meta("target_id",""),"disabled":node.disabled})
	for child in node.get_children(): output.append_array(shell_controls(child))
	return output

## 同一真实_render覆盖本地页优先和地图开关；没有选择的页面不能提前查询其领域。
func routing_case(kind: String) -> Dictionary:
	var f: Dictionary=shell_fixture(); check(f.ok,"原小领域输入 "+kind)
	if kind in ["courses_offered","study_active"]: study_source(f,kind=="study_active")
	if kind in ["personal","military","personal_short_circuit","military_short_circuit"]: battle_source(f,kind.begins_with("personal"))
	if kind.ends_with("short_circuit"):
		f.state.sieges={"unused":false}; f.state.actors[f.state.player_actor_id].location={"kind":null}; f.data.roles={"unselected-course":false}
	var main=shell_host(f)
	match kind:
		"title": main._started=false
		"camp": main._military_open=true; main._family_open=true; main._growth_open=true; main._courses_open=false
		"family": main._family_open=true; main._estate_id="estate.z"; main._growth_open=true
		"estate": main._estate_id="estate.z"; main._growth_open=true
		"growth": main._growth_open=true; main.journey_view.opened=true
		"growth_on_road": main._growth_open=true; f.state.actors.player.location={"kind":"OnRoute","edge_id":"road","progress":100,"origin":"town"}
		"courses", "courses_offered", "study_active": main._courses_open=true; main._military_open=true; main._family_open=true
		"map": main.journey_view.opened=true
		"road": f.state.actors.player.location={"kind":"OnRoute","edge_id":"road","progress":100,"origin":"town"}
		"wilderness":
			var map: Dictionary=Maps.plain_map(200,100,20); map.id="map"; f.data.world_maps[map.id]=map; f.data.scenarios.fixture.world_map_id=map.id
			f.state.actors.player.location={"kind":"Wilderness","map_id":map.id,"point":[50,50],"origin":"town"}
	var before: Dictionary=f.duplicate(true); Character.clear_cache()
	main._render()
	var output: Dictionary={"controls":shell_controls(main.page),"reads":main.kernel.reads.duplicate(true),"cache":{"actor":main._trade_actor_id,"request":main._trade_request.duplicate(true),"receipt":main._trade_receipt.duplicate(true)},"epoch":main._scene_counter}
	snapshots[kind]=main.kernel.snapshot_count
	check(main.kernel.snapshot_count==(0 if kind=="title" else expected_outer_snapshots),"当前阶段真实外壳snapshot计数 "+kind)
	check(main.kernel.reads.is_empty() if kind=="title" else main.kernel.reads[0]=="trade_cache_context","缓存查询仍在重绘/页面读取之前 "+kind)
	check(f==before,"原路由不改输入 "+kind)
	dispose(main); return output

## 在真实重绘前置点核缓存寿命；家事页遮住trade时不需要构造完整对白节点。
func cache_case(kind: String) -> Dictionary:
	var f: Dictionary=shell_fixture(); var main=shell_host(f); main._family_open=true
	main._trade_actor_id="player"; main._trade_request={"shop_id":"old.shop","quantity":2}; main._trade_receipt={"action_id":"old.action","quote":{"owner":{"kind":"actor","id":"player","extra":"kept"},"quantity":2}}
	f.state.interactions.player={"phase":"trade"}
	if kind=="phase_left": f.state.interactions.player.phase="say"
	elif kind=="actor_changed": main._trade_actor_id="other"
	elif kind=="no_session": f.state.interactions={}
	main._render()
	var result: Dictionary={"actor":main._trade_actor_id,"request":main._trade_request.duplicate(true),"receipt":main._trade_receipt.duplicate(true),"reads":main.kernel.reads.duplicate(true),"epoch":main._scene_counter}
	check(main._trade_receipt.is_empty()==(kind!="same_trade"),"原三缓存清除条件 "+kind)
	dispose(main); return result

## 成交专属读取位只消费原action receipt；原结果处理不从失败响应或其它命令取得它。
func receipt_case(kind: String) -> Dictionary:
	var f: Dictionary=shell_fixture(); var main=shell_host(f,true)
	var receipt: Dictionary={"owner":{"kind":"actor","id":"player","extra":{"unchanged":1}},"item_id":"grain","quantity":2,"total":10,"side":"buy","market_revision":9,"context":{"private":"not-a-new-view"}}
	var action: Dictionary={"id":"trade.action","kind":"trade","actor_id":"player","remaining_ticks":1,"params":{"receipt":receipt}}
	f.state.actions[action.id]=action; f.state.actors.player.action_id=action.id
	main._trade_receipt={"action_id":"previous","quote":{"side":"sell"}}
	if kind=="missing_action": f.state.actions.erase(action.id); f.state.actors.player.action_id=""
	elif kind=="missing_receipt": action.params={}
	var result: Dictionary={"ok":kind!="failed","code":"InsufficientFunds","details":{"action_id":action.id}}
	main._apply_command_result("inspect" if kind=="other_command" else "trade",result)
	var output: Dictionary={"reads":main.kernel.reads.duplicate(true),"receipt":main._trade_receipt.duplicate(true),"rendered":main.rendered.duplicate(true)}
	var receipt_reads: Array=main.kernel.reads.filter(func(row): return row is Array and row[0]=="trade_action_receipt")
	check(receipt_reads.size()==(0 if kind in ["failed","other_command"] else 1) and main.kernel.snapshot_count==0,"成功trade专属读点不用整态 "+kind)
	check(receipt_reads.is_empty() or main.kernel.reads.find("action_context")<main.kernel.reads.find(receipt_reads[0]),"收据读取保持在原action_context之后 "+kind)
	check(main._trade_receipt.action_id==("trade.action" if kind=="success" else "previous"),"成功专属有源才更新receipt "+kind)
	if kind=="success":
		check(main._trade_receipt.quote.owner==receipt.owner and main._trade_receipt.quote.owner!={"kind":"actor","id":"player"},"owner额外字段保原完整字典相等")
		main._trade_receipt.quote.owner.extra.unchanged=2
		check(receipt.owner.extra.unchanged==1,"收据owner原深副本不写回行动")
	dispose(main); return output

## 实际_trade确认扩展owner仍显示公物；参数源由真实报价与原TradeAction载荷校验建立。
func receipt_display_case() -> Dictionary:
	var f: Dictionary=shell_fixture(); var request: Dictionary=trade_source(f); var main=shell_host(f,true)
	var quote: Dictionary=main.kernel.preview("trade",request)
	if not check(quote.ok,"真实货单报价用于收据显示"): dispose(main); return {}
	var receipt: Dictionary=quote.duplicate(true); receipt.owner.extra={"proof":"preserved"}
	var action: Dictionary={"id":"shown.trade","actor_id":"player","kind":"trade","remaining_ticks":1,"params":{"context":request.context.duplicate(true),"receipt":receipt}}
	check(TradeAction.validate(f.state,f.data,action).ok,"原交易行动receipt校验未封闭owner额外字段")
	f.state.actions[action.id]=action; main._trade_actor_id="player"; main._trade_request=request.duplicate(true); main._trade_receipt={"action_id":action.id,"quote":receipt.duplicate(true)}
	var box=VBoxContainer.new(); main.add_child(box)
	main._trade(box,{"command":{"shop_id":"shop"},"context":request.context})
	var words: Array=[]
	for row in shell_controls(box):
		if row.has("text"): words.append(row.text)
	check("\n".join(words).contains("公物已入行囊") and not "\n".join(words).contains("已入本人行囊"),"完整owner额外字段保持原公共财物说明")
	var output: Dictionary={"controls":shell_controls(box),"request":main._trade_request.duplicate(true),"receipt":main._trade_receipt.duplicate(true)}
	dispose(main); return output

## 预先声明允许移除整态读和裁剪收据无显示字段；完整owner、控件及所有其它读取顺序仍进固定摘要。
func semantic_output(raw: Dictionary) -> Dictionary:
	var output: Dictionary=raw.duplicate(true)
	for group in ["routes","cache","receipts"]:
		for row in output[group].values():
			row.reads=row.reads.filter(func(item): return not (item is String and item in ["snapshot","trade_cache_context","presentation_location"]) and not (item is Array and item[0]=="trade_action_receipt"))
			if row.has("receipt"): row.receipt=receipt_projection(row.receipt)
			if row.has("cache"): row.cache.receipt=receipt_projection(row.cache.receipt)
			for render in row.get("rendered",[]): render.receipt=receipt_projection(render.receipt)
	output.receipt_display.receipt=receipt_projection(output.receipt_display.receipt)
	return output

## 只投影合同五显示字段，owner整个嵌套值保留，缺字段不补默认。
func receipt_projection(receipt: Dictionary) -> Dictionary:
	var result: Dictionary=receipt.duplicate(true)
	if result.has("quote"):
		var quote: Dictionary={}
		for key in ["owner","item_id","quantity","total","side"]:
			if result.quote.has(key): quote[key]=result.quote[key]
		result.quote=quote
	return result

## 新纯来源直接与原有限表达式比较；不替代真实Main读点和后续整包快照计数。
func source_checks(sidebar, trade) -> void:
	var f: Dictionary=shell_fixture(); var before: Dictionary=f.duplicate(true)
	check(trade.cache_context(f.state)=={"actor_id":"player","is_trade":false},"没有原trade阶段时缓存应清")
	f.state.interactions.player={"phase":"trade"}
	check(trade.cache_context(f.state)=={"actor_id":"player","is_trade":true},"原raw phase为trade才保缓存")
	f.state.interactions.player.phase="unknown"
	check(not trade.cache_context(f.state).is_trade,"未知phase不默认trade")
	for location in [{"kind":"Settlement","place_id":"room","settlement_id":"town"},{"kind":"OnRoute","edge_id":"road"},{"kind":"Wilderness","point":[40,40]},{"kind":"Following","party_id":"unused"},{"kind":"InArmy","army_id":"unused"}]:
		f.state.actors.player.location=location
		var expected: Dictionary={"kind":location.kind}
		if location.has("place_id"): expected.place_id=location.place_id
		check(sidebar.raw_location(f.state)==expected,"只保raw位置且不解引用 "+location.kind)
	f=before.duplicate(true)
	var owner: Dictionary={"kind":"actor","id":"player","extra":{"proof":1}}
	f.state.actions.trade={"id":"trade","params":{"receipt":{"owner":owner,"item_id":"grain","quantity":2,"total":7,"side":"buy","market_revision":123,"context":{"unused":"private"}}}}
	var result: Dictionary=trade.action_receipt(f.state,"trade")
	check(result=={"action_id":"trade","quote":{"owner":owner,"item_id":"grain","quantity":2,"total":7,"side":"buy"}},"只投影五显示字段而完整保owner")
	result.quote.owner.extra.proof=2
	check(owner.extra.proof==1,"owner嵌套值与原action深隔离")
	check(trade.action_receipt(f.state,"missing").is_empty(),"找不到action不补历史收据")
	f.state.actions.empty={"id":"empty","params":{}}
	check(trade.action_receipt(f.state,"empty").is_empty(),"没有receipt不创建成功收据")
	f.state.actions.empty.params.receipt={}
	check(trade.action_receipt(f.state,"empty")=={"action_id":"empty","quote":{}},"已有空receipt保持存在位，不与没有receipt混同")
	var kernel=ShellKernel.new(); kernel._state=f.state; kernel._catalog={"data":f.data}; kernel._registry=registrations
	check(kernel.presentation_location()==sidebar.raw_location(f.state),"实际Kernel经同Backend提供raw位置")
	check(kernel.trade_cache_context()==trade.cache_context(f.state),"实际缓存薄口不另查对白")
	check(kernel.trade_action_receipt("trade")==trade.action_receipt(f.state,"trade") and kernel.snapshot_count==0,"实际收据薄口同五字段且不用snapshot")

## Field正式短链在siege_start后的只读原件；仅验证第三路由，不重复任何任官、战争或结算准备。
func siege_route_case() -> Dictionary:
	var parsed: Dictionary=Strict.parse_text(FileAccess.get_file_as_string("res://../tests/fixtures/presentation_shell_routing/actual_siege_source.json"))
	if not check(parsed.ok and parsed.value.format=="HM-TEST-SIEGE-SOURCE/1","真实围城来源JSON格式及整数读取"): return {}
	var source: Dictionary=parsed.value
	if not check(WorldSchema.validate(source.state,source.catalog,registrations).ok,"真实围城来源在当前规则完整Schema有效"): return {}
	var f: Dictionary={"state":source.state,"data":source.catalog.data}; var main=shell_host(f); main.kernel._catalog=source.catalog
	main._courses_open=true; main._military_open=true; main._family_open=true; main._growth_open=true
	var before: Dictionary=f.duplicate(true); main._render()
	check(main.page.find_child("SiegeScreen",true,false)!=null,"实际围城第三优先覆盖所有普通页开关")
	check(main.kernel.snapshot_count==expected_outer_snapshots and not "place_sidebar" in main.kernel.reads and not "journey_sidebar" in main.kernel.reads and not "interaction_view" in main.kernel.reads,"围城命中不读普通场所/行旅/对白用途")
	check(f==before,"只读真实围城路由不改人物/军团/钱货/RNG")
	var reads: Array=main.kernel.reads.filter(func(item): return not (item is String and item in ["snapshot","trade_cache_context","presentation_location"]))
	var output: Dictionary={"controls":shell_controls(main.page),"reads":reads}
	dispose(main); return output

## 原行为一次冻结，后续有限口红绿将继续使用相同输出，不用生成数据冒充产品已改。
func _run() -> void:
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--expect-outer-snapshots="): expected_outer_snapshots=int(arg.trim_prefix("--expect-outer-snapshots="))
	registrations=Registry.build(Builtin.descriptors())
	if not check(registrations.ok,"原完整受信registry"): quit(1); return
	registry=ShellArt.new(Strict.parse_text(FileAccess.get_file_as_string("res://../tests/fixtures/character_presentation/manifest.json")).value)
	var previous: RefCounted=Art._registry; Art._registry=registry
	if "--capture-siege" in OS.get_cmdline_user_args():
		var siege_output: Dictionary=siege_route_case()
		var siege_file=FileAccess.open("user://shell-siege-oracle.json",FileAccess.WRITE); siege_file.store_string(JSON.stringify({"output":siege_output,"sha256":digest(siege_output)},"\t",true,true)); siege_file.close()
		Art._registry=previous; Character.clear_cache(); await process_frame
		print("SHELL_SIEGE_ORACLE=",digest(siege_output)," CHECKS=",checks," FAILURES=",failures); quit(0 if failures==0 else 1); return
	var output: Dictionary={"routes":{},"cache":{},"receipts":{}}
	for kind in ["title","scene","camp","family","estate","growth","growth_on_road","courses","courses_offered","study_active","map","road","wilderness","personal","military","personal_short_circuit","military_short_circuit"]: output.routes[kind]=routing_case(kind)
	for kind in ["same_trade","phase_left","actor_changed","no_session"]: output.cache[kind]=cache_case(kind)
	for kind in ["success","missing_action","missing_receipt","failed","other_command"]: output.receipts[kind]=receipt_case(kind)
	output.receipt_display=receipt_display_case()
	var semantic: Dictionary=semantic_output(output)
	if "--capture-oracle" in OS.get_cmdline_user_args():
		var file=FileAccess.open("user://shell-oracle.json",FileAccess.WRITE); file.store_string(JSON.stringify({"output":output,"sha256":digest(output),"semantic_sha256":digest(semantic),"snapshots":snapshots},"\t",true,true)); file.close()
	else: check(digest(semantic)==SHELL_SEMANTIC_ORACLE,"固定原外壳控件/缓存/收据显示与非snapshot读取顺序")
	var sidebar=load("res://core/views/scene_journey_sidebar.gd").new(); var trade=load("res://core/views/trade_sheet.gd").new()
	var ready: bool=check(sidebar.has_method("raw_location"),"现侧栏叶有唯一raw_location")
	ready=check(trade.has_method("cache_context"),"现TradeSheet有cache_context") and ready
	ready=check(trade.has_method("action_receipt"),"现TradeSheet有action_receipt") and ready
	if ready: source_checks(sidebar,trade)
	check(digest(siege_route_case())==SIEGE_ROUTE_ORACLE,"额外固定真实围城原路由oracle")
	Art._registry=previous; Character.clear_cache(); await process_frame
	print("SHELL_ORACLE=",digest(output)," SEMANTIC=",digest(semantic)," CHECKS=",checks," FAILURES=",failures); print("SHELL_SNAPSHOTS=",JSON.stringify(snapshots,"",true))
	quit(0 if failures==0 else 1)
