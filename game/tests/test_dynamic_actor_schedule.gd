## 动态人物仅凭显式收养行旅来源或本人军政权源进入有限调度，短存读始终保留同一份进度与资产。
extends SceneTree
const Kernel=preload("res://core/world_kernel.gd")
const F=preload("res://tests/companion_fixture.gd")
const Codec=preload("res://core/save_codec.gd")
const Strict=preload("res://core/strict_json.gd")
const Behavior=preload("res://core/domains/identity/actor_behavior.gd")
const Schedule=preload("res://core/travel_schedule.gd")
const MilitarySchedule=preload("res://core/military_schedule.gd")
const Family=preload("res://core/family_rules.gd")
const Identity=preload("res://core/actor_identity.gd")
const Rules=preload("res://core/rules_migration.gd")
const Schema=preload("res://core/state_schema.gd")
const Inventory=preload("res://core/inventory_rules.gd")
const Location=preload("res://core/world_location.gd")
var checks: int=0
var failures: int=0
var path: String=""
var selected_case: String="all"

## 正式准备器目录由测试入口显式传入，不复用生产生成目录制造新的行为配置。
func _init() -> void:
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--dynamic-content="): path=arg.trim_prefix("--dynamic-content=")
		if arg.begins_with("--case="): selected_case=arg.trim_prefix("--case=")
	call_deferred("run_checks")

## 断言失败保留具体边界，并最终使测试进程失败。
func check(value: bool, label: String) -> bool:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)
	return value

## 所有正式行为走同一版本命令入口，不借测试直接改内核私有状态。
func send(k, kind: String, params: Dictionary={}, actor_id: String="") -> bool:
	var result: Dictionary=F.send(k,kind,params,actor_id)
	return check(result.ok,kind+" "+str(params)+"："+str(result))

## 短存读完整比较含请求缓存、随机状态、行程、资产与社会知闻的世界。
func restore_same(k, label: String) -> bool:
	var before: Dictionary=k.snapshot()
	var bytes: String=Codec.encode(before,0,"dynamic-schedule",k.content())
	var decoded: Dictionary=Codec.decode(bytes,k.content())
	return check(not bytes.is_empty() and decoded.ok and k.restore(decoded.state).ok and k.snapshot()==before,label+"完整Codec恢复不重置任何进度")

## 收养仍当面办理两个半日；生产初始化发生在完整完工候选，不直接调用底层造人。
func adopt(k, name: String="新行者") -> String:
	if Location.resolve(k.snapshot(),k.snapshot().player_actor_id).get("place_id")!="west.estate":
		if not send(k,"move_local",{"place_id":"west.estate"}): return ""
	var before: Dictionary=k.snapshot()
	var command: Dictionary={"kind":"family_adopt","params":{"context":F.context(k,"role.estate_steward"),"display_name":name},"actor_id":before.player_actor_id,"request_seq":before.request_sequences[before.player_actor_id]+1,"expected_revision":before.revision}
	if not check(k.submit(command).ok,"显式家事命令冻结收养条款"): return ""
	if not restore_same(k,"初始化前") or not check(k.advance_one_tick().ok and k.advance_one_tick().ok,"两个真实半日完成收养"): return ""
	var state: Dictionary=k.snapshot(); var child: String=""
	for id in state.family.children:
		if not before.family.children.has(id): child=id
	if not check(not child.is_empty(),"成功收养只有一个永久新身份"): return ""
	var fact: Dictionary=state.facts[state.family.children[child]]
	check(fact.kind=="family_adoption" and fact.success and fact.child_actor_id==child,"完成与孩子来源绑定后才初始化")
	check(k.submit(command).ok and k.snapshot()==state,"同一收养请求重放不重生、不扣费、不重置游标")
	restore_same(k,"初始化后")
	return child

## 新动态缺省不取得原型旅程；旧四字段来源即便原型有计划也只能保留未授权状态。
func check_default(catalog_path: String) -> void:
	var k=Kernel.new()
	if not check(k.new_game("scenario.alpha_life",catalog_path).ok,"默认分支经正式作者目录创建"): return
	var child: String=adopt(k)
	if child.is_empty(): return
	var state: Dictionary=k.snapshot(); var data: Dictionary=k.content().data
	check(not state.itineraries.has(child) and not child in Behavior.scheduled_ids(state,data,"travel_plan"),"无显式来源启用，新动态不继承原型行程")
	check(child in Behavior.scheduled_ids(state,data,"social"),"新增行旅开关不关闭既有动态social")
	var source: Dictionary=state.duplicate(true); source.ruleset_id=Rules.ARCHITECTURE_PREVIOUS
	for actor in source.actors.values(): actor.erase("appearance")
	var old: Dictionary=source.duplicate(true); var upgraded: Dictionary=Rules.upgrade(source)
	check(upgraded.ok and upgraded.migrated and source==old,"旧规则迁移只操作深副本")
	if upgraded.ok:
		var expected: Dictionary=state.duplicate(true); expected.ruleset_id=Rules.CURRENT
		check(upgraded.state==expected and not upgraded.state.itineraries.has(child),"旧标记只追加legacy和版本，不补授权或行程")
		check(k.restore(source).ok and k.snapshot()==expected,"旧四字段来源通过完整恢复门")
		check(k.advance_one_tick().ok and not k.snapshot().itineraries.has(child),"迁入后下一半日仍不补动态行程")
	var legacy: Dictionary=Identity.definition(state,data,child)
	legacy.social.kind="selfish"; legacy.travel_plan.start_tick=99
	check(data==k.content().data and Behavior.definition(state,data,child).travel_plan.start_tick==5,"行为返回深副本，不回写原型配置")
	check(Behavior.definition(state,data,"absent").is_empty() and Behavior.scheduled_ids(state,data,"arbitrary_script").is_empty(),"缺身份和未知行为查询安全为空")
	var broken: Dictionary=state.duplicate(true); broken.actors[child].prototype_id="absent"
	check(Behavior.definition(broken,data,child).is_empty() and not child in Behavior.scheduled_ids(broken,data,"social"),"缺原型身份不触发配置索引异常")
	broken=state.duplicate(true); broken.itineraries[child]={"index":0,"ready_tick":0,"travelling":false}
	var before: Dictionary=broken.duplicate(true)
	check(Behavior.initialize_schedule(broken,data,child).get("code")=="UnexpectedItinerary" and broken==before,"未授权额外行程拒绝，不能初始化洗白")
	check(not k.restore(broken).ok,"完整Schema也拒绝未授权动态行程")

## 调度出口的忙碌与玩家排除为只读边界；保存配置集合不随当前可执行性删减。
func check_query_boundaries(state: Dictionary, data: Dictionary, child: String) -> void:
	var before: Dictionary=state.duplicate(true); var original: Dictionary=data.duplicate(true)
	check(child in Behavior.scheduled_ids(state,data,"travel_plan"),"显式授权人物进入稳定配置集合")
	var ids: Array=Behavior.scheduled_ids(state,data,"travel_plan"); var sorted: Array=ids.duplicate(); sorted.sort()
	check(ids==sorted and state==before and data==original,"行为列表稳定排序且查询无副作用")
	for mode in ["dead","unavailable","busy","party","talking","provider","player","interior"]:
		var candidate: Dictionary=state.duplicate(true)
		match mode:
			"dead": candidate.actors[child].alive=false
			"unavailable": candidate.actors[child].available=false
			"busy": candidate.actors[child].action_id="occupied"
			"party": candidate.parties["probe"]={"members":[child]}
			"talking": candidate.interactions[child]={"context":{"provider_actor_id":"actor.shen"}}
			"provider": candidate.interactions["actor.shen"]={"context":{"provider_actor_id":child}}
			"player": candidate.player_actor_id=child
			"interior": candidate.actors[child].location.place_id="west.estate"
		var captured: Dictionary=candidate.duplicate(true)
		check(child in Behavior.scheduled_ids(candidate,data,"travel_plan") and not _proposed(Schedule.proposals(candidate,data),child) and candidate==captured,"不可执行仍保留合法配置与行程："+mode)
	for mode in ["dead","unavailable"]:
		var candidate: Dictionary=state.duplicate(true); candidate.actors[child]["alive" if mode=="dead" else "available"]=false
		check(Schema.validate(candidate,{"ok":true,"data":data,"content_id":state.content_id,"content_version":state.content_version,"content_digest":state.content_digest}).ok,"保存不因当前停止行旅删除合法游标："+mode)

## 查找提案只比较实际永久ID，不能以同原型或显示名字替代参与者。
func _proposed(proposals: Array, actor_id: String) -> bool:
	for row in proposals:
		if row.actor_id==actor_id: return true
	return false

## 直接在有效保存快照中制造坏来源与坏游标，只验证拒绝和原世界不变，不将伪造状态当正向玩法。
func check_invalid_saves(k, child: String) -> void:
	var state: Dictionary=k.snapshot(); var data: Dictionary=k.content().data
	for mode in ["index_negative","index_end","ready_negative","ready_float","missing_itinerary","bad_flag","false_flag","failed","pending","child_binding","source_binding"]:
		var broken: Dictionary=state.duplicate(true); var fact: Dictionary=broken.facts[broken.family.children[child]]
		match mode:
			"index_negative": broken.itineraries[child].index=-1
			"index_end": broken.itineraries[child].index=Behavior.definition(state,data,child).travel_plan.destinations.size()
			"ready_negative": broken.itineraries[child].ready_tick=-1
			"ready_float": broken.itineraries[child].ready_tick=0.5
			"missing_itinerary": broken.itineraries.erase(child)
			"bad_flag": fact.terms.policy.enable_travel_plan=1
			"false_flag": fact.terms.policy.enable_travel_plan=false
			"failed": fact.kind="family_adoption_failed"; fact.success=false
			"pending": fact.kind="family_adoption_pending"; fact.success=false
			"child_binding": fact.child_actor_id="actor.apprentice"
			"source_binding": broken.facts[child].source="missing-source"
		check(not k.restore(broken).ok and k.snapshot()==state, "完整恢复拒绝坏动态来源/进度且原世界不变："+mode)
		check(Codec.encode(broken,0,"invalid-dynamic",k.content()).is_empty(),"坏来源/进度不能形成可保存封装："+mode)
	var bad: Dictionary=state.duplicate(true); bad.itineraries[child].index=-1
	var before: Dictionary=bad.duplicate(true)
	check(Behavior.initialize_schedule(bad,data,child).get("code")=="InvalidItinerary" and bad==before,"初始化拒绝坏游标而不读档修补")
	var old: Dictionary=state.duplicate(true); old.ruleset_id=Rules.ARCHITECTURE_PREVIOUS
	before=old.duplicate(true)
	check(Rules.upgrade(old).get("code")=="AmbiguousLegacyActorSchedule" and old==before,"新增授权不能伪装旧规则标记迁入")
	check(not k.restore(old).ok and k.snapshot()==state,"恢复门拒绝混合版本新权限")

## 全世界都空闲时先提交真实等待，不能用裸advance绕过无行动的时间推进守卫。
func advance_world(k) -> Dictionary:
	if k.snapshot().actions.is_empty():
		var waiting: Dictionary=F.send(k,"wait")
		if not waiting.ok: return waiting
	return k.advance_one_tick()

## 新人先靠真实短工取得独立旅费，室内和缺钱均不自动出发；行程经正式预览开始与计费。
func check_enabled(catalog_path: String) -> void:
	var k=Kernel.new()
	if not check(k.new_game("scenario.alpha_life",catalog_path).ok,"显式行旅作者目录创建"): return
	var child: String=adopt(k)
	if child.is_empty(): return
	var state: Dictionary=k.snapshot(); var data: Dictionary=k.content().data
	check(state.itineraries[child]=={"index":0,"ready_tick":5,"travelling":false},"出生早于绝对start时等待原世界日期")
	check(state.wallets[state.actors[child].wallet_id].balance==0 and state.inventories[state.actors[child].inventory_id].lots.is_empty(),"初始化不继承原型钱粮")
	check(not _proposed(Schedule.proposals(state,data),child),"室内出生不会自动走向城门或瞬移")
	check_invalid_saves(k,child)
	if not send(k,"move_local",{"place_id":"west.street"},child): return
	check(k.preview("travel",{"destination":"village.east"},child).get("code")=="InsufficientFunds","零私产人物必须由真实旅行预览拒绝")
	if not send(k,"move_local",{"place_id":"west.tavern"},child): return
	if not send(k,"service",{"activity":"work","context":F.context(k,"role.host")},child) or not check(advance_world(k).ok,"新人实际短工完成"): return
	check(k.snapshot().wallets[k.snapshot().actors[child].wallet_id].balance==80,"旅费来自本人工作收入")
	if not send(k,"move_local",{"place_id":"west.street"},child): return
	if not restore_same(k,"出发前"): return
	while k.snapshot().tick<4:
		if not check(advance_world(k).ok,"等到绝对出发日期前一半日"): return
	state=k.snapshot(); check(state.actors[child].action_id.is_empty(),"ready_tick之前没有自动提案执行")
	var query: Dictionary=state.duplicate(true); query.itineraries[child].ready_tick=query.tick
	check_query_boundaries(query,data,child)
	var funds: int=state.wallets[state.actors[child].wallet_id].balance
	var departure: Dictionary=advance_world(k)
	if not check(departure.ok,"ready_tick等于当前半日触发出发："+str(departure)): return
	state=k.snapshot(); var action_id: String=state.actors[child].action_id
	if not check(not action_id.is_empty() and state.actions[action_id].kind=="travel" and state.itineraries[child].travelling,"正式Kernel提案通过预览才安装行程"): return
	check(state.wallets[state.actors[child].wallet_id].balance==funds and state.actions[action_id].start_tick==5,"开始半日不提前扣下一步费用")
	var candidate: Dictionary=state.duplicate(true); var row: Dictionary=candidate.itineraries[child].duplicate(true)
	check(Behavior.initialize_schedule(candidate,data,child).ok and candidate.itineraries[child]==row and candidate==state,"重复初始化不重置出发后的游标或世界")
	if not restore_same(k,"在途"): return
	if not check(advance_world(k).ok,"在途下一半日实际前行"): return
	check(k.snapshot().wallets[k.snapshot().actors[child].wallet_id].balance<funds,"正常行旅扣本人真实私产")
	var arrived: bool=false
	for _half in range(24):
		state=k.snapshot()
		if state.actors[child].action_id.is_empty(): arrived=true; break
		if not check(advance_world(k).ok,"有限真实半日完成动态行旅"): return
	if not check(arrived,"动态旅人按正式地形行程抵达"): return
	state=k.snapshot()
	check(Location.resolve(state,child).get("settlement_id")=="village.east" and not state.itineraries[child].travelling and state.itineraries[child].ready_tick==state.tick+2,"抵达只设置真实dwell绝对时间")
	if not restore_same(k,"抵达停留"): return
	if not check(advance_world(k).ok and k.snapshot().actors[child].action_id.is_empty(),"停留未满不重复出发"): return
	if not check(advance_world(k).ok and not k.snapshot().actors[child].action_id.is_empty(),"停留到期按游标再次出发"): return
	restore_same(k,"再次出发")
	# 监护人仍在庄园，第二个新人于start之后完成，不补走此前历史。
	var second: String=adopt(k,"后来者")
	if not second.is_empty():
		state=k.snapshot(); check(state.itineraries[second].ready_tick==state.actors[second].created_tick and state.itineraries[second].index==0,"出生晚于start时ready使用created_tick，绝不相对延迟")

## 有效临界计数在完工时失败，外层候选必须回滚人物、费用、事实、RNG与时钟。
func check_failed_candidate(catalog_path: String) -> void:
	var k=Kernel.new()
	if not check(k.new_game("scenario.alpha_life",catalog_path).ok,"失败候选夹具创建"): return
	if not send(k,"move_local",{"place_id":"west.estate"}) or not send(k,"family_adopt",{"context":F.context(k,"role.estate_steward")}) or not check(k.advance_one_tick().ok,"收养只走首半日"): return
	var state: Dictionary=k.snapshot(); state.next_instance_id=2147483647
	if not check(k.restore(state).ok,"已准入的极限计数保存点有效"): return
	var before: Dictionary=k.snapshot(); var outcome: Dictionary=k.advance_one_tick()
	check(not outcome.ok and k.snapshot()==before,"创建计数耗尽时整个完工候选原子回滚")
	check(k.snapshot().family.children.is_empty(),"失败或未完成收养没有行旅授权或人物")

## 汇总正式目录、来源、短存读和军政权限验证；不把这些短点冒充长期性能或完整发行。
func run_checks() -> void:
	var parsed: Dictionary=Strict.parse_text(FileAccess.get_file_as_string(path)) if not path.is_empty() else {}
	if not check(parsed.get("ok",false) and parsed.get("value") is Dictionary,"明确提供正式编译夹具索引"): finish(); return
	var fixtures: Dictionary=parsed.value
	if not check(selected_case in ["all","default","travel","failure","military","private"],"定点入口只能选择已知用例"): finish(); return
	if selected_case in ["all","default"]: check_default(fixtures.travel_default.catalog)
	if selected_case in ["all","travel"]: check_enabled(fixtures.travel_enabled.catalog)
	if selected_case in ["all","failure"]: check_failed_candidate(fixtures.travel_enabled.catalog)
	if selected_case in ["all","military"]: check_military(fixtures.military.catalog)
	if selected_case in ["all","private"]: check_private_social()
	finish()

## 只有全部行为断言实际运行并通过，才返回成功退出码。
func finish() -> void:
	print("DYNAMIC_ACTOR_SCHEDULE_CHECKS=",checks," FAILURES=",failures)
	quit(1 if failures else 0)
## 只读见证半日末的原生军政调度；随后原样执行父类，不增删提案、不替世界制造权源。
class MilitaryWitnessKernel:
	extends "res://core/world_kernel.gd"
	var military_witness: Array=[]

	## 任命完工同半日会立即自动受令，在正式入口前保存提案及真实预览，避免事后伪造中间态。
	func _schedule_military(state: Dictionary) -> Dictionary:
		var before: Dictionary=state.duplicate(true)
		var proposals: Array=MilitarySchedule.proposals(state,_catalog.data)
		var quotes: Array=[]
		for proposal in proposals: quotes.append(_preview(state,proposal.kind,proposal.params,proposal.actor_id))
		military_witness.append({"tick":state.tick,"state":before,"proposals":proposals.duplicate(true),"quotes":quotes,"unchanged":state==before})
		return super._schedule_military(state)

## 按本人永久ID读取当前一步；空字典明确表示没有自动权限，不以同原型的静态军官替代。
func military_request(k, actor_id: String) -> Dictionary:
	for proposal in MilitarySchedule.proposals(k.snapshot(),k.content().data):
		if proposal.actor_id==actor_id: return proposal
	return {}

## 读取实际半日留下的最后一次匹配见证，缺少真实提案时返回空供调用者断言失败。
func military_witness(k, actor_id: String, kind: String) -> Dictionary:
	for index in range(k.military_witness.size()-1,-1,-1):
		var row: Dictionary=k.military_witness[index]
		for offset in range(row.proposals.size()):
			var proposal: Dictionary=row.proposals[offset]
			if proposal.actor_id==actor_id and proposal.kind==kind:
				return {"tick":row.tick,"state":row.state,"proposal":proposal,"quote":row.quotes[offset],"unchanged":row.unchanged}
	return {}

## 反例只在候选上走Kernel正式预览，不恢复坏状态；预览失败也必须保留候选和活世界全部字节语义。
func military_reject(k, candidate: Dictionary, kind: String, params: Dictionary, actor_id: String, code: String, label: String) -> bool:
	var before: Dictionary=candidate.duplicate(true); var live: Dictionary=k.snapshot()
	var quoted: Dictionary=k._preview(candidate,kind,params,actor_id)
	return check(not quoted.ok and quoted.get("code")==code and candidate==before and k.snapshot()==live,label+"："+str(quoted))

## 所有正常身份、岗位和军令来自正式命令；仅失权分支撤销既得岗位，不伪造正向任命或占领事实。
func check_military(path: String) -> void:
	var k=MilitaryWitnessKernel.new()
	if not check(not path.is_empty() and k.new_game("scenario.alpha_life",path).ok,"独立正式军事目录可创建动态权限验证世界"): return
	var data: Dictionary=k.content().data; var state: Dictionary=k.snapshot(); var parent: String=state.player_actor_id
	var prototype: String="actor.military.a.commander"; var office_id: String="office.military.a"
	var initial: Dictionary=state.duplicate(true)
	var static_ids: Array=Behavior.scheduled_ids(state,data,"military_plan")
	check(static_ids==["actor.military.a.commander","actor.military.b.commander"],"静态军事原型按永久ID稳定排序，生产计划不受动态门槛改变")
	for actor_id in static_ids:
		var proposal: Dictionary=military_request(k,actor_id)
		check(proposal.get("kind")=="office_accept" and proposal.params.office_id==data.actors[actor_id].military_plan.office_id,"既有静态军官仍按原计划提出开局受任："+actor_id)
		if proposal.get("kind")=="office_accept": check(k.preview(proposal.kind,proposal.params,actor_id).ok,"静态开局任命仍通过真实当面预览："+actor_id)
	check(k.snapshot()==initial,"静态配置、排序和任命预览不改变世界或作者计划")
	# 只冻结两位静态自动军官，防止其抢占测试岗位；人物和部队仍保留在完整Schema世界。
	for actor_id in static_ids: state.actors[actor_id].available=false
	if not check(k.restore(state).ok,"静态军官不可用状态通过正式恢复冻结"): return
	if not send(k,"move_local",{"place_id":"west.estate"}) or not send(k,"family_adopt",{"context":F.context(k,"role.estate_steward"),"display_name":"军政后继"}): return
	if not check(k.advance_one_tick().ok and k.advance_one_tick().ok,"两次真实半日完成军事原型收养"): return
	state=k.snapshot()
	if not check(state.family.children.size()==1,"真实收养恰好生成一名动态本人"): return
	var child: String=state.family.children.keys()[0]
	check(state.actors[child].prototype_id==prototype and state.facts[child].source==state.family.children[child],"动态军事人物具备真实创建与收养来源")
	check(Behavior.definition(state,data,child).military_plan==data.actors[prototype].military_plan and not state.itineraries.has(child),"军事原型只提供计划配置，不顺带创建未授权行旅")
	if not send(k,"move_local",{"place_id":"west.street"},child) or not send(k,"move_local",{"place_id":"west.street"}): return
	state=k.snapshot(); var no_office: Dictionary=state.duplicate(true)
	var context: Dictionary=F.context(k,"role.military.a")
	var accept_params: Dictionary={"template_id":"order.military.a","context":context}
	check(not child in Behavior.scheduled_ids(state,data,"military_plan") and military_request(k,child).is_empty(),"只有原型计划但无本人岗位，不提出自动office_accept或军令")
	military_reject(k,state,"campaign_accept",accept_params,child,"CampaignOfficeRequired","无任命本人不能因原型同名军权受令")
	var forged: Dictionary=state.duplicate(true)
	forged.offices[office_id].enabled=true; forged.offices[office_id].holder_actor_id=child
	var rejected: Dictionary=k.restore(forged)
	check(not rejected.ok and rejected.get("code")=="OfficeAppointmentSourceMissing" and k.snapshot()==state,"手写动态岗位缺真实受任来源，正式恢复拒绝且不污染活世界")
	# 另一人真实受任不是该动态人物的权源；这条分支完成后回到已验证的无任命保存点。
	if not send(k,"office_accept",{"office_id":office_id,"context":context}) or not check(k.advance_one_tick().ok,"前代经真实一半日受任同一岗位"): return
	state=k.snapshot()
	check(state.offices[office_id].holder_actor_id==parent and military_request(k,child).is_empty() and not child in Behavior.scheduled_ids(state,data,"military_plan"),"前代本人岗位不会因收养或相同原型授予后代")
	military_reject(k,state,"campaign_accept",accept_params,child,"CampaignOfficeRequired","他人现任岗位不能被动态人物借来受令")
	military_reject(k,state,"office_accept",{"office_id":office_id,"context":context},child,"OfficeOccupied","他人已占岗位仍由真实受任预览拒绝抢任")
	if not check(k.restore(no_office).ok,"回到真实收养后的无任命保存点"): return
	# 正向唯一任命入口由测试明确请求；调度器不会替动态人物调用office_accept。
	if not send(k,"office_accept",{"office_id":office_id,"context":context},child): return
	state=k.snapshot(); var appointment: String=state.actors[child].action_id
	check(not state.offices[office_id].enabled and military_request(k,child).is_empty(),"显式任命尚在办理时不提前取得动态调度权")
	if not restore_same(k,"动态正式受任办理中") or not check(k.advance_one_tick().ok,"本人真实一半日完成正式任命"): return
	state=k.snapshot(); var accepted: Dictionary=state.duplicate(true)
	var witnessed: Dictionary=military_witness(k,child,"campaign_accept")
	if not check(not witnessed.is_empty(),"本人任命完成后实际调度提出campaign_accept"): return
	check(witnessed.unchanged and witnessed.quote.ok and witnessed.proposal.params==accept_params,"自动受令提案走正式当面预览且见证查询无状态副作用")
	check(witnessed.state.offices[office_id].appointment_id==appointment and witnessed.state.facts[appointment].command=="office_accept" and witnessed.state.facts[appointment].success and witnessed.state.facts[appointment].actor_id==child,"调度权由本人本次真实已完成任命提供，绝非静态原型或他人历史")
	check(military_witness(k,child,"office_accept").is_empty(),"完整收养和任命流程从未出现动态自动任命提案")
	var campaign_id: String=""
	for campaign in state.campaigns.values():
		if campaign.actor_id==child: campaign_id=campaign.id
	if not check(not campaign_id.is_empty() and state.campaigns[campaign_id].status=="accepted" and state.campaigns[campaign_id].army_id.is_empty(),"调度受令生成本人真实军令，尚未凭空形成军团"): return
	if not restore_same(k,"动态正式受令后"): return
	var form: Dictionary=military_request(k,child)
	if not check(form.get("kind")=="army_form" and k.preview(form.kind,form.params,child).ok,"本人合法军令只对现有驻军和公库提出真实可行编组"): return
	var bad: Dictionary=state.duplicate(true); bad.offices[office_id].enabled=false
	military_reject(k,bad,form.kind,form.params,child,"OfficeAuthorityRequired","失去本人岗位后真实编组预览拒绝继续征发")
	bad=state.duplicate(true); bad.strongholds["stronghold.military.a"].owner_organization_id="org.military.b"
	military_reject(k,bad,form.kind,form.params,child,"OccupationSourceMissing","缺真实占领来源的失效营地归属不能提供编组权")
	check(not k.restore(bad).ok and k.snapshot()==state,"失效归属候选也无法经完整恢复冒充合法易帜")
	# 撤销真实既得岗位，不触碰其任命事实、受令事实或本人ID，验证未编组军令仍可收尾。
	var lost: Dictionary=accepted.duplicate(true); lost.offices[office_id].enabled=false
	if not check(k.restore(lost).ok,"已有本人军令失权状态保持真实历史且可恢复"): return
	var cancel: Dictionary=military_request(k,child)
	if not check(child in Behavior.scheduled_ids(k.snapshot(),data,"military_plan") and cancel.get("kind")=="campaign_cancel","失权本人已有军令不会被行为查询提前滤掉，可提出取消"): return
	if not check(k.preview(cancel.kind,cancel.params,child).ok,"失权未编组取消通过本人真实军令预览") or not send(k,cancel.kind,cancel.params,child): return
	state=k.snapshot()
	check(state.campaigns[campaign_id].status=="closed" and state.campaigns[campaign_id].objective_result.kind=="cancelled" and military_request(k,child).is_empty(),"取消关闭本人军令，不重新受任、不保留自动作战出口")
	if not restore_same(k,"动态失权取消后") or not check(k.restore(accepted).ok,"回到原真实受令保存点验证已编组收尾"): return
	if not send(k,form.kind,form.params,child): return
	state=k.snapshot(); var forming: String=state.actors[child].action_id
	check(state.military_units["unit.military.a"].mobilizing_action_id==forming,"正式编组开始确实保留已有驻军，未生成替代兵员")
	military_reject(k,state,form.kind,form.params,child,"ActorBusy","同一本人已有编组行动时真实预览拒绝重入")
	# 只解除候选的人身占用以单独命中单位保留检查；单位锁来自刚才的真实命令，坏候选不入存档。
	bad=state.duplicate(true); bad.actors[child].action_id=""
	military_reject(k,bad,form.kind,form.params,child,"GarrisonUnitUnavailable","同一真实行动保留的单位不能被第二次编组预览夺取")
	if not restore_same(k,"动态真实驻军编组中") or not check(k.advance_one_tick().ok,"真实半日完成驻军、公粮与军饷编组"): return
	state=k.snapshot(); var army_id: String=state.campaigns[campaign_id].army_id
	if not check(not army_id.is_empty() and state.armies[army_id].commander_actor_id==child,"编组实际生成本人指挥的唯一现存军团"): return
	var army: Dictionary=state.armies[army_id]
	check(army.unit_ids==["unit.military.a"] and army.formation_source==forming and state.facts[forming].success and state.military_units["unit.military.a"].fit.size()==8,"军团与八名原驻军均有真实完工来源，不从原型复制军队")
	check(state.wallets[army.wallet_id].balance==10 and state.wallets["wallet.military.a"].balance==accepted.wallets["wallet.military.a"].balance-10 and state.wallets[state.actors[child].wallet_id]==accepted.wallets[accepted.actors[child].wallet_id],"编组真实使用公款十钱，动态本人空私柜不被自动补钱或扣款")
	# 保留真实军团占用，只在拒绝候选回退待编组标签，隔离复用本人和活动部队的防重入口。
	bad=state.duplicate(true); bad.campaigns[campaign_id].status="accepted"; bad.campaigns[campaign_id].army_id=""
	military_reject(k,bad,form.kind,form.params,child,"MilitaryAssemblyRequired","已在真实活动军团中的本人不能再次组军")
	var foreign_unit: Dictionary=accepted.duplicate(true)
	foreign_unit.military_units["unit.military.a"]=state.military_units["unit.military.a"].duplicate(true)
	military_reject(k,foreign_unit,form.kind,form.params,child,"GarrisonUnitUnavailable","已编入真实军团的单位不能被另一次待编组预览借走")
	if not restore_same(k,"动态真实军团编成后"): return
	lost=state.duplicate(true); lost.offices[office_id].enabled=false
	if not check(k.restore(lost).ok,"实际军团失权仍保留完整编组和本人军令来源"): return
	var returning: Dictionary=military_request(k,child)
	if not check(child in Behavior.scheduled_ids(k.snapshot(),data,"military_plan") and returning.get("kind")=="campaign_return" and k.preview(returning.kind,returning.params,child).ok,"已编组失权本人仍可通过正式预览提出返营"): return
	if not send(k,returning.kind,returning.params,child) or not restore_same(k,"动态失权返营请求后"): return
	var unload: Dictionary=military_request(k,child)
	if not check(unload.get("kind")=="army_return" and unload.params.army_id==army_id and k.preview(unload.kind,unload.params,child).ok,"本次同城军团已在真实营地，失权本人仍可合法卸装交还"): return
	if not send(k,unload.kind,unload.params,child) or not check(k.advance_one_tick().ok,"一半日实际归营清算并触发当面报告"): return
	state=k.snapshot(); witnessed=military_witness(k,child,"campaign_report")
	if not check(not witnessed.is_empty() and witnessed.quote.ok and witnessed.unchanged,"失权已归营本人仍实际提出并通过正式报告预览"): return
	check(witnessed.state.campaigns[campaign_id].status=="awaiting_report" and not witnessed.state.offices[office_id].enabled and witnessed.proposal.params.context==context,"报告提案取本人真实交接收据和现存合法办理人，不借原型岗位")
	check(state.campaigns[campaign_id].status=="closed" and state.armies[army_id].status=="closed" and state.inventories[army.inventory_id].lots.is_empty() and state.wallets[army.wallet_id].balance==0,"失权军令经真实人产交还和报告关闭，随军公物不悬空")
	var receipt: Dictionary=state.facts[state.campaigns[campaign_id].settlement_id]
	check(receipt.reason=="reported" and receipt.report_context==context and military_request(k,child).is_empty(),"关闭依据真实当面回报，之后不自动重任或重接同令")
	restore_same(k,"动态失权归营报告后")

## 旧动态来源继续参与真实社会转粮；私人寄信人和收信人始终以永久ID判定，岗位指向不等于私诺移交。
func check_private_social() -> void:
	var k=Kernel.new()
	if not check(k.new_game("scenario.alpha_life","res://data/generated/alpha_life.hmcatalog.json").ok,"既有动态社会与私人权限世界创建"): return
	if not send(k,"move_local",{"place_id":"west.tavern"}): return
	var state: Dictionary=k.snapshot(); var data: Dictionary=k.content().data
	# 静态同原型若同时夺粮会先占用host，本例只冻结它，隔离动态本人的既有社会出口。
	state.actors["actor.shen"].available=false
	state.facts["legacy-social-source"]={"kind":"family_adoption"}
	state.facts["legacy-recipient-source"]={"kind":"family_adoption"}
	var sender: Dictionary=Identity.instantiate(state,data,"actor.shen","另一位寄信人",-18*720,"west.tavern","legacy-social-source")
	var receiver: Dictionary=Identity.instantiate(state,data,"actor.he","另一位收信人",-18*720,"east.home","legacy-recipient-source")
	if not check(sender.ok and receiver.ok and k.restore(state).ok,"已支持的旧动态身份来源经完整恢复，不补收养行旅开关"): return
	check(not state.itineraries.has(sender.actor_id) and sender.actor_id in Behavior.scheduled_ids(state,data,"social"),"旧四字段以外的旧动态来源也保留原社会行为且无行旅")
	var candidate: Dictionary=state.duplicate(true); candidate.roles["role.sender"].actor_id=sender.actor_id
	var params: Dictionary={"offer_id":"offer.letter","context":{"role_id":"role.sender","provider_actor_id":sender.actor_id,"place_id":"west.tavern"}}
	var before: Dictionary=candidate.duplicate(true)
	check(k._preview(candidate,"accept_offer",params,candidate.player_actor_id).get("code")=="PrivatePromiseNotDelegated" and candidate==before,"同原型动态人即便成为当前岗位办理人，也不能冒充原私诺发令人")
	candidate=state.duplicate(true); candidate.roles["role.recipient"].actor_id=receiver.actor_id
	params.context={"role_id":"role.recipient","provider_actor_id":receiver.actor_id,"place_id":"east.home"}
	before=candidate.duplicate(true)
	check(k._preview(candidate,"deliver_message",params,"actor.he").get("code")=="WrongRecipient" and candidate==before,"实际在场且任该岗位的动态同原型仍不是私信指定收信人")
	for _half in range(3):
		if not check(advance_world(k).ok,"旧动态社会通过正常半日推进"): return
	state=k.snapshot(); var actual: bool=false
	for event in state.social.events.values():
		if event.kind=="theft" and event.actor_id==sender.actor_id and event.target_actor_id=="actor.host": actual=true
	check(actual and Inventory.count(state.inventories[state.actors[sender.actor_id].inventory_id],"item.grain",[{"kind":"actor","id":sender.actor_id}])==1,"动态社会从真实对方存粮转入独立本人库存")
	check(state.social.last_tick==state.tick and not state.social.memories[sender.actor_id].is_empty(),"社会事件保留真实本人、发生时刻和因果记忆")
	if not restore_same(k,"旧动态转粮和因果消息"): return
	var snapshot: Dictionary=k.snapshot(); var replay: Dictionary=preload("res://core/social_rules.gd").step(snapshot,data,snapshot.tick)
	check(replay.ok and not replay.applied and snapshot==state,"同last_tick重放不重复转粮、不改消息来源")
