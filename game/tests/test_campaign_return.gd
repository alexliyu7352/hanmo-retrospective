## 异营返程领域集成：真实任官、围城易帜与 Kernel 命令预览，行程使用 Terrain 半日；不冒充完整 Main 验收。
extends SceneTree
const F=preload("res://tests/garrison_ownership_fixture.gd")
const Military=preload("res://tests/military_combat_fixture.gd")
const Maps=preload("res://tests/test_world_terrain.gd")
const Campaign=preload("res://core/campaign_rules.gd")
const Schedule=preload("res://core/military_schedule.gd")
const Assets=preload("res://core/military_assets.gd")
const Factory=preload("res://core/world_factory.gd")
const Catalog=preload("res://core/content_catalog.gd")
const Kernel=preload("res://core/world_kernel.gd")
const Registry=preload("res://core/runtime/module_registry.gd")
const Builtin=preload("res://core/runtime/builtin_modules.gd")
const Travel=preload("res://core/travel_rules.gd")
const Journey=preload("res://core/terrain_journey.gd")
const Interaction=preload("res://core/interaction_rules.gd")
const Inventory=preload("res://core/inventory_rules.gd")
const Strict=preload("res://core/strict_json.gd")
var checks: int=0
var failures: int=0

## 引擎启动后运行独立领域输入，所有断言失败均反映在退出码。
func _init() -> void:
	call_deferred("run_checks")

## 记录行为证据，失败输出具体来源与命令。
func check(value: bool, label: String) -> bool:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)
	return value

## 三座作者营在同一小地图，额外办理人先于工厂存在；既有围城夹具仅承担战前到场边界。
func prepared() -> Dictionary:
	var f: Dictionary=F.fresh(); var map: Dictionary=Maps.plain_map(200,100,20)
	map.id="map.return"; map.budgets={"foot":3000,"army":3000,"boat":3000}; map.money_per_tick=1; map.fatigue_per_tick=1
	f.data.world_maps={map.id:map}; f.data.scenarios[f.state.scenario_id].world_map_id=map.id
	var west: String=f.data.places[f.data.garrisons[f.camp].place_id].settlement_id
	var east: String=f.data.places[f.data.garrisons[f.enemy_camp].place_id].settlement_id
	f.data.settlements[west].map_xy=[20,50]; f.data.settlements[east].map_xy=[180,50]
	f.report_role="role.return_c"; f.provider="actor.host"
	var role: Dictionary=f.data.roles[f.data.offices[f.enemy_office].appointing_role_id].duplicate(true)
	role.id=f.report_role; role.place_id=f.data.garrisons[f.friend_camp].place_id; role.initial_actor=f.provider; role.services=["military_report"]
	f.data.roles[role.id]=role; f.data.actors[f.provider].initial_place=role.place_id
	f.data.actors[f.enemy].military_plan={"office_id":f.enemy_office,"order_id":f.enemy_office,"unit_ids":[f.enemy_unit],"food_quantity":100,"money":40}
	for capability in ["military","world_terrain"]:
		if not capability in f.data.required_capabilities: f.data.required_capabilities.append(capability)
	var catalog: Dictionary=Catalog.read_prepared("res://data/generated/lived_city.hmcatalog.json")
	if not check(catalog.get("ok",false),"原准备目录提供真实初始化身份"): return {}
	catalog.data=f.data
	var created: Dictionary=Factory.create(catalog,f.state.scenario_id)
	if not check(created.get("ok",false),"原三营作者副本通过工厂初始化："+str(created.get("code",""))): return {}
	f.state=created.state; f.state.campaigns={}; f.state.sieges={}
	f.state.strongholds={"stronghold.test":{"id":"stronghold.test","owner_organization_id":f.data.garrisons[f.enemy_camp].organization_id,"occupation_id":"","siege_id":""}}
	if not check(Assets.initialize(f.state,f.data).ok,"作者三营正式初始化"): return {}
	var a: Dictionary=Military.mobilize(f); var b: Dictionary=Military.mobilize(f,true)
	if not check(a.ok and b.ok,"双方实际当面任官、领军与公粮编组："+str([a,b])): return {}
	f.army_a=a.details.army_id; f.army_b=b.details.army_id; f.campaign_id=f.state.armies[f.army_b].order_id
	if not check(F.win(f).ok,"正式破门、守点与防军合法撤离"): return {}
	f.before_occupation=f.state.duplicate(true)
	f.occupation_id=F.receipt(f)
	if not check(Assets.apply_occupation(f.state,f.data,f.occupation_id).ok,"实际易帜关闭旧军权，保留原旗野外军团"): return {}
	return f

## 领域夹具显式接受发行注册表后只借Kernel预览/应用；失败丢弃候选，不冒充完整new_game/submit。
func command(f: Dictionary, kind: String, params: Dictionary, actor_id: String="") -> Dictionary:
	if actor_id.is_empty(): actor_id=f.enemy
	var kernel=Kernel.new(); kernel._catalog={"data":f.data}
	kernel._registry=Registry.build(Builtin.descriptors())
	if not check(kernel._registry.get("ok",false),"领域夹具必须先接受真实发行注册表"): return {"ok":false,"code":"FixtureRegistryMissing"}
	var quote: Dictionary=kernel._preview(f.state,kind,params,actor_id)
	if not quote.ok: return quote
	var candidate: Dictionary=f.state.duplicate(true)
	var result: Dictionary=kernel._apply(candidate,kind,params,actor_id,quote)
	if result.ok: f.state=candidate
	return result

## 领域半日消费一次实际军粮和行旅预算，沿正式 Assets 完工回执交接；不执行无关社会或随机事件。
func advance(f: Dictionary) -> Dictionary:
	var finish_tick: int=f.state.tick+1
	var supplied: Dictionary=Assets.step(f.state,f.data,finish_tick)
	if not supplied.ok: return supplied
	var returns: Array=[]
	for id in f.state.actions.keys():
		var action: Dictionary=f.state.actions[id]; var result: Dictionary={}; var done: bool=false
		if action.kind=="travel":
			result=Travel.step(f.state,f.data,action); done=result.get("completed",false)
		else:
			action.remaining_ticks-=1; result=Assets.finish(f.state,f.data,action); done=result.get("success",false)
			if done and action.kind=="army_return": returns.append(result.details)
		if not result.ok: return result
		if done:
			for person in action.participants: f.state.actors[person].action_id=""
			f.state.actions.erase(id)
	var stepped: Dictionary=Campaign.step(f.state,f.data,finish_tick)
	if not stepped.ok: return stepped
	f.state.tick=finish_tick
	for receipt in returns:
		var accepted: Dictionary=Campaign.army_returned(f.state,f.data,receipt.order_id,receipt.receipt_id)
		if not accepted.ok: return accepted
	return {"ok":true,"journey_requests":stepped.journey_requests}

## 从调度的唯一提案逐步走过场所和地图，不用测试赋值把旧军团搬到替代营地。
func return_npc(f: Dictionary) -> bool:
	var saw_motion: bool=false; var walked: bool=false; var checked_restart: bool=false; var started_tick: int=f.state.tick
	for _round in range(40):
		var current: Dictionary=f.state.campaigns[f.campaign_id]
		if current.status in ["awaiting_report","closed"]: break
		var proposals: Array=Schedule.proposals(f.state,f.data)
		for proposal in proposals:
			if proposal.actor_id!=f.enemy: continue
			walked=walked or proposal.kind=="move_local"
			var done: Dictionary=command(f,proposal.kind,proposal.params)
			if not check(done.ok,"失职 NPC 正式提案："+str(proposal)+" -> "+str(done)): return false
		var action_id: String=f.state.actors[f.enemy].action_id
		if not action_id.is_empty() and f.state.actions[action_id].kind=="travel":
			saw_motion=true
			if not check(Journey.validate_action(f.state,f.data,f.state.actions[action_id]).ok,"每个真实余程均有永久原始计划"): return false
		var restarted: Dictionary={}
		if saw_motion and not checked_restart:
			restarted=f.duplicate(true); restarted.state=Strict.parse_text(JSON.stringify(f.state)).value
		if not check(advance(f).ok,"归营途中单一半日真实供给与行军"): return false
		if not restarted.is_empty():
			checked_restart=true
			if not check(advance(restarted).ok and restarted.state==f.state,"真实返程中途 JSON 重启后下一半日位置、军粮和永久事实完全一致"): return false
	return check(walked and saw_motion and f.state.tick>started_tick and f.state.campaigns[f.campaign_id].status in ["awaiting_report","closed"],"原营丢失后仍沿 passages 与 Terrain 实际抵达并卸装")

## 失职、无营、被俘与无人报告分别留有明确结果；重复查询与重启不会改旗、增粮或重复归营。
func run_checks() -> void:
	var f: Dictionary=prepared()
	if f.is_empty(): finish(); return
	var initial: Dictionary=f.state.duplicate(true); var campaign: Dictionary=f.state.campaigns[f.campaign_id]
	var chosen: Dictionary=Campaign.return_destination(f.state,f.data,f.campaign_id)
	check(chosen.ok and chosen.return_garrison_id==f.friend_camp and initial==f.state,"默认只读选取真实同旗友营，原营易帜不会改军團旗号")
	check(Campaign.return_destination(f.state,f.data,f.campaign_id,f.enemy_camp).get("code")=="CampaignFriendlyGarrisonRequired","不能明确选择已经敌占的原营")
	check(Campaign.return_destination(f.state,f.data,f.campaign_id,"invented").get("code")=="CampaignFriendlyGarrisonRequired","未知营地不被临时创建")
	var returned: Dictionary=command(f,"campaign_return",{"campaign_id":f.campaign_id,"garrison_id":f.friend_camp})
	if not check(returned.ok and returned.details.journey_request.is_empty() and f.state.campaigns[f.campaign_id].return_wait_reason=="CampaignNotAtCityExit","非城口先受理返程并等真实出城，不用失败报价回滚"): finish(); return
	var private_wallet: Dictionary=f.state.wallets[f.state.actors[f.enemy].wallet_id].duplicate(true)
	var old_army: Dictionary=f.state.armies[f.army_b].duplicate(true)
	var branch: Dictionary=f.duplicate(true)
	check_waits(branch)
	check_receipts(f.duplicate(true))
	check_npc_siege(f.duplicate(true))
	if not return_npc(f): finish(); return
	campaign=f.state.campaigns[f.campaign_id]; var army: Dictionary=f.state.armies[f.army_b]
	check(army.status=="closed" and army.garrison_id==f.enemy_camp and army.return_garrison_id==f.friend_camp and army.organization_id==old_army.organization_id,"异营清算保留编组原籍与原旗，实际驻军单独记账")
	check(f.state.military_units[f.enemy_unit].garrison_id==f.friend_camp and f.state.military_units[f.enemy_unit].source_garrison_id==f.enemy_camp,"幸存部队真实卸装入友营，初始兵员来源完整")
	check(f.state.wallets[private_wallet.id]==private_wallet and f.state.wallets[army.wallet_id].balance==0 and f.state.inventories[army.inventory_id].lots.is_empty(),"返程费用来自实际军饷，卸装后公钱货已交还且私人钱包不动")
	check(campaign.status=="awaiting_report" and Campaign.report_roles(f.state,f.data,campaign.id)==[f.report_role],"原职已失但实际友营有合法办理人时等待当面报告")
	check(Assets.validate_state(f.state,f.data).ok and Campaign.validate_state(f.state,f.data).ok,"异营交接稳定边界通过真实人产与军令双重来源校验")
	var saved: Dictionary=Strict.parse_text(JSON.stringify(f.state)).value
	check(saved==f.state and Assets.validate_state(saved,f.data).ok and Campaign.validate_state(saved,f.data).ok,"JSON 重启保留原营、实际交接营和所有永久凭据")
	var receipt_id: String=campaign.return_receipt_id; var snapshot: Dictionary=f.state.duplicate(true)
	check(Campaign.army_returned(f.state,f.data,campaign.id,receipt_id).ok and snapshot==f.state,"真实归营凭据重放幂等")
	var bad: Dictionary=f.state.duplicate(true); bad.facts[receipt_id].garrison_id=f.enemy_camp
	check(not Campaign.army_returned(bad,f.data,campaign.id,receipt_id).ok,"篡改实际交接营的永久回执拒绝")
	check_reporting(f.duplicate(true))
	var reported: Dictionary=command(f,"campaign_report",{"campaign_id":campaign.id,"context":Interaction.for_role(f.state,f.data,f.report_role)})
	check(reported.ok and f.state.campaigns[campaign.id].status=="closed" and f.state.facts[f.state.campaigns[campaign.id].settlement_id].reason=="reported","失职本人到实际友营当面报告，可合法结束旧令")
	finish()

## 不具备实际安置或行动能力时只等待；围城未撤离也不能被宏观返程偷走。
func check_waits(f: Dictionary) -> void:
	var no_friend: Dictionary=f.duplicate(true); no_friend.data.garrisons.erase(f.friend_camp)
	var before: Dictionary=no_friend.state.duplicate(true)
	var result: Dictionary=Campaign.apply(no_friend.state,no_friend.data,f.enemy,"campaign_return",{"campaign_id":f.campaign_id})
	check(result.ok and result.details.journey_request.is_empty() and no_friend.state.campaigns[f.campaign_id].return_wait_reason=="CampaignNoFriendlyGarrison","无友营保留 returning 与明确待安置原因")
	check(no_friend.state.armies==before.armies and no_friend.state.inventories==before.inventories and no_friend.state.wallets==before.wallets,"无友营不清空军团和公私资产")
	check(Schedule.proposals(no_friend.state,no_friend.data).is_empty(),"无友营 NPC 不反复提出必然失败的空路程")
	for _tick in range(3):
		var waiting: Dictionary=Campaign.step(no_friend.state,no_friend.data,no_friend.state.tick+1)
		check(waiting.ok and waiting.journey_requests.is_empty(),"待安置跨半日不重发空路线或捏造友营")
		no_friend.state.tick+=1
	for mode in ["dead","captured"]:
		var held: Dictionary=f.duplicate(true); held.state.actors[f.enemy].available=false
		if mode=="dead": held.state.actors[f.enemy].alive=false
		else: held.state.actors[f.enemy].location={"kind":"InArmy","army_id":f.army_a}
		var stepped: Dictionary=Campaign.step(held.state,held.data,held.state.tick+1)
		check(stepped.ok and stepped.journey_requests.is_empty() and held.state.campaigns[f.campaign_id].return_wait_reason=="CampaignCommanderUnavailable","统帅 "+mode+" 不输出无法行动的请求，不回滚死亡半日")
	var blocked: Dictionary=f.duplicate(true)
	for y in range(5): blocked.data.world_maps["map.return"].cells[y*10+5].terrain="water"
	result=Campaign.apply(blocked.state,blocked.data,f.enemy,"campaign_return",{"campaign_id":f.campaign_id,"garrison_id":f.friend_camp})
	check(result.ok and result.details.journey_request.is_empty() and blocked.state.campaigns[f.campaign_id].return_wait_reason=="CampaignNoFriendlyRoute" and blocked.state.campaigns[f.campaign_id].return_garrison_id==f.friend_camp,"友营在无桥河对岸时等待真实路线，不能跨水瞬移")
	var active: Dictionary=f.duplicate(true); active.state.sieges[f.siege_id].withdrawn_army_ids=[]; active.state.sieges[f.siege_id].phase="active"
	check(Campaign.preview(active.state,active.data,f.enemy,"campaign_return",{"campaign_id":f.campaign_id}).get("code")=="SiegePending" and Schedule.proposals(active.state,active.data).is_empty(),"active 围城必须先由正式退路处理，不自动出城")
	active.state.sieges[f.siege_id].phase="result"
	check(Schedule.proposals(active.state,active.data).is_empty(),"仍有玩家的围城结果不被 NPC 提前关闭")
	active.state.sieges[f.siege_id].withdrawn_army_ids=[f.army_a]
	var proposals: Array=Schedule.proposals(active.state,active.data)
	check(proposals.size()==1 and proposals[0].kind=="siege_close","没有玩家待确认的 NPC 围城结果走正式关闭提案")

## 真实占领回执当前时刻可即时消费；错误目标与未来回执不成为战功，重放不推进时间。
func check_receipts(f: Dictionary) -> void:
	var campaign: Dictionary=f.state.campaigns[f.state.armies[f.army_a].order_id]
	campaign.status="operating"; campaign.objective_result={}
	var tick: int=f.state.tick; var cursor: int=campaign.last_tick
	var bad: Dictionary=f.state.duplicate(true); bad.facts[f.occupation_id].tick=tick+2
	check(Campaign.consume_receipts(bad,f.data,[f.occupation_id],tick).get("code")=="CampaignReceiptTime","未来占领回执不能在当前时刻兑现")
	bad=f.state.duplicate(true); bad.facts[f.occupation_id].army_id=f.army_b
	check(Campaign.consume_receipts(bad,f.data,[f.occupation_id],tick).ok and bad.campaigns[campaign.id].objective_result.is_empty(),"另一军团回执不会变为本令目标")
	var result: Dictionary=Campaign.consume_receipts(f.state,f.data,[f.occupation_id],tick)
	check(result.ok and campaign.objective_result.get("source_id")==f.occupation_id and campaign.objective_result.kind=="occupied" and f.state.tick==tick and campaign.last_tick==cursor,"实际占领即时落账，不增加世界日期或半日游标")
	var before: Dictionary=f.state.duplicate(true)
	check(Campaign.consume_receipts(f.state,f.data,[f.occupation_id],tick).ok and before==f.state,"同 tick 交涉或占领凭据重放不重复入账")

## 已到城NPC通过已注册Kernel预览siege_start；此处仅查领域提案，不捏造围城或完整世界启动。
func check_npc_siege(f: Dictionary) -> void:
	f.state=f.before_occupation.duplicate(true)
	var campaign_id: String=f.state.armies[f.army_a].order_id
	var closed: Dictionary=command(f,"siege_close",{"siege_id":f.siege_id},f.actor)
	if not check(closed.ok,"旧战术结果通过正式关闭入口释放"): return
	var departed: Dictionary=command(f,"campaign_depart",{"campaign_id":campaign_id},f.actor)
	if not check(departed.ok and advance(f).ok,"实际同城驻军依据军令到场进入 operating"): return
	f.state.player_actor_id=f.provider
	f.data.actors[f.actor].military_plan={"office_id":f.office,"order_id":f.office,"unit_ids":[f.unit],"food_quantity":100,"money":40}
	var proposals: Array=Schedule.proposals(f.state,f.data); var request: Dictionary={}
	for proposal in proposals:
		if proposal.actor_id==f.actor: request=proposal
	if not check(request.get("kind")=="siege_start","普通 NPC 到城占领军令提出 siege_start 而非原地不动"): return
	var kernel=Kernel.new(); kernel._catalog={"data":f.data}
	kernel._registry=Registry.build(Builtin.descriptors())
	if not check(kernel._registry.get("ok",false),"围城提案夹具必须先接受真实发行注册表"): return
	var quote: Dictionary=kernel._preview(f.state,request.kind,request.params,request.actor_id)
	check(quote.ok,"NPC 围城提案具备真实到场、现任军权和现存守军："+str(quote))

## 暂时忙碌的合法办理人应等候；确已无权且无人可办时按真实交接凭据行政归档，不能伪造报告。
func check_reporting(f: Dictionary) -> void:
	var absent: Dictionary=f.duplicate(true); absent.state.actors[f.provider].action_id="fixture.busy"
	var result: Dictionary=Campaign.step(absent.state,absent.data,absent.state.tick+1)
	check(result.ok and absent.state.campaigns[f.campaign_id].status=="awaiting_report" and Campaign.report_roles(absent.state,absent.data,f.campaign_id).is_empty(),"办理人暂忙保留真实回报义务，不误作无人负责")
	var orphan: Dictionary=f.duplicate(true); orphan.state.roles[f.report_role].enabled=false
	result=Campaign.step(orphan.state,orphan.data,orphan.state.tick+1)
	var campaign: Dictionary=orphan.state.campaigns[f.campaign_id]; var receipt: Dictionary=orphan.state.facts.get(campaign.settlement_id,{})
	check(result.ok and campaign.status=="closed" and receipt.get("reason")=="authority_lost_returned" and receipt.return_receipt_id==campaign.return_receipt_id and not receipt.has("report_context"),"失职且确无合法报告人仅凭实际公物交接行政归档")
	var dead: Dictionary=f.duplicate(true); dead.state.actors[f.enemy].alive=false
	result=Campaign.step(dead.state,dead.data,dead.state.tick+1)
	campaign=dead.state.campaigns[f.campaign_id]
	check(result.ok and campaign.status=="closed" and dead.state.facts[campaign.settlement_id].reason=="responsible_actor_lost","原责任人交接后死亡不会永久锁住已归还的军令")

## 汇总仅包含本脚本真实执行的领域断言，保持发行工具可识别的统一格式。
func finish() -> void:
	print("CHECKS=",checks," FAILURES=",failures)
	quit(1 if failures else 0)
