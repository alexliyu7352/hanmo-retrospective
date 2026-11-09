## 真实军事短链上的未激活整编合同；只读提议/来源拒绝不冒充收编、写凭据或NPC同意。
extends "res://tests/test_siege_safe_departure.gd"
const Contract=preload("res://core/domains/siege/integration_contract.gd")

## 同一个合法世界重排字典并真实提交新军议；无关revision不改变语义身份，输出副本也不能回写世界。
func check_identity(k, prepared: Dictionary, expected: Dictionary) -> void:
	var before: Dictionary=k.snapshot(); var reordered: Dictionary=before.duplicate(true)
	for field in ["armies","offices","facts","military_units"]:
		var keys: Array=reordered[field].keys(); keys.reverse(); var reversed: Dictionary={}
		for key in keys: reversed[key]=reordered[field][key]
		reordered[field]=reversed
	var keys: Array=reordered.sieges[prepared.siege_id].armies.keys(); keys.reverse(); var armies: Dictionary={}
	for key in keys: armies[key]=reordered.sieges[prepared.siege_id].armies[key]
	reordered.sieges[prepared.siege_id].armies=armies
	var restored=Kernel.new()
	if not check(restored.new_game("scenario.alpha_life",content_path).ok and restored.restore(reordered).ok,"真实Kernel接受只重排字典的同态世界"): return
	var repeated: Dictionary=Contract.proposal(restored.snapshot(),restored.content().data,prepared.siege_id,before.player_actor_id)
	check(repeated.ok and repeated.proposal==expected,"字典重排不改变逐军官/军团/兵员提议身份")
	if not send(restored,"siege_plan",{"siege_id":prepared.siege_id,"orders":{"unit.military.a":{"action":"blockade","node_id":"supply"}}}): return
	var after: Dictionary=restored.snapshot()
	check(after.revision>before.revision and Contract.validate_proposal(after,restored.content().data,expected).ok,"真实军议只变全局revision而不使来源未变的接受提议失效")
	var decoded: Dictionary=Codec.decode(Codec.encode(after,0,"siege-integration-proposal",restored.content()),restored.content())
	check(decoded.ok and restored.restore(decoded.state).ok and Contract.validate_proposal(restored.snapshot(),restored.content().data,expected).ok,"当前真实世界冷恢复后重建相同提议")
	repeated.proposal.defender_armies[0].units[0].fit.clear()
	check(restored.snapshot()==after and k.snapshot()==before,"修改返回的名册副本不污染两支真实世界")
	if not advance(restored): return
	check(restored.snapshot().tick>before.tick and Contract.validate_proposal(restored.snapshot(),restored.content().data,expected).ok,"真实半日只变供给/士气与时间时不凭空发明同意过期政策")

## 明确的坏输入副本不写回Kernel；原军事validator仍负责五类守恒与不可变编组来源。
func check_rejections(k, prepared: Dictionary, proposed: Dictionary) -> void:
	var before: Dictionary=k.snapshot(); var data: Dictionary=k.content().data
	var id: String=prepared.siege_id
	check(Contract.proposal(before,data,id,"actor.military.b.provider").get("code")=="SiegeNegotiatorRequired","岗位提供者不是当前军团交涉负责人")
	for mutation in ["location","busy","appointment","duplicate_soldier","withdrawn","battle"]:
		var boundary: Dictionary=before.duplicate(true); var defender: String=prepared.defender
		var army_id: String=boundary.campaigns[prepared.campaigns.b].army_id
		match mutation:
			"location": boundary.actors[defender].location.place_id="west.entrance"
			"busy": boundary.actors[defender].action_id="action.unavailable"
			"appointment": boundary.offices["office.military.b"].enabled=false
			"duplicate_soldier": boundary.military_units["unit.military.b"].wounded.append(boundary.military_units["unit.military.b"].fit[0])
			"withdrawn": boundary.sieges[id].withdrawn_army_ids.append(army_id)
			"battle": boundary.sieges[id].pending_nodes={"inner":{}}
		var saved: Dictionary=boundary.duplicate(true)
		check(not Contract.validate_proposal(boundary,data,proposed).ok and boundary==saved,"提议拒绝改变的实际来源且不改坏输入副本："+mutation)
	var changed: Dictionary=before.duplicate(true)
	var person: String=changed.military_units["unit.military.b"].fit.pop_back()
	changed.military_units["unit.military.b"].wounded.append(person)
	var revised: Dictionary=Contract.proposal(changed,data,id,before.player_actor_id)
	check(revised.ok and revised.proposal.proposal_key!=proposed.proposal_key,"守恒分类边界变化也生成新提议身份，旧接受不能套用")
	check(Contract.validate_proposal(changed,data,proposed).get("code")=="StaleSiegeIntegrationProposal","精确兵员分类漂移使原提议过期")
	check(k.snapshot()==before,"全部反例未被提交或恢复为真实世界")

## 畸形围城、军令及提议只作输入拒绝反例，不能让错误类型成为脚本异常或成功报价。
func check_malformed(k, prepared: Dictionary, proposed: Dictionary) -> void:
	var original: Dictionary=k.snapshot(); var data: Dictionary=k.content().data
	for field in ["sieges","siege","campaigns","campaign","campaign_army","siege_campaign","siege_armies","siege_pending","siege_withdrawn","battles","battle_null","battle_units"]:
		var state: Dictionary=original.duplicate(true)
		match field:
			"sieges": state.sieges=[]
			"siege": state.sieges[prepared.siege_id]=[]
			"campaigns": state.campaigns=[]
			"campaign": state.campaigns[prepared.campaigns.a]=[]
			"campaign_army": state.campaigns[prepared.campaigns.a].army_id=[]
			"siege_campaign": state.sieges[prepared.siege_id].campaign_id=[]
			"siege_armies": state.sieges[prepared.siege_id].armies=[]
			"siege_pending": state.sieges[prepared.siege_id].pending_nodes=[]
			"siege_withdrawn": state.sieges[prepared.siege_id].withdrawn_army_ids=[]; state.sieges[prepared.siege_id].withdrawn_army_ids.append({})
			"battles": state.battles=[]
			"battle_null": state.battles={"bad":null}
			"battle_units": state.battles={"bad":{"kind":"military","phase":"active","units":[]}}
		var before: Dictionary=state.duplicate(true)
		var result: Variant=Contract.proposal(state,data,prepared.siege_id,original.player_actor_id)
		check(result is Dictionary and result.get("ok")==false and result.get("code") is String and state==before,"畸形嵌套输入结构化拒绝："+field)
	for value in [null,[],true,{}, {"format":Contract.FORMAT,"siege_id":[],"proposer_actor_id":original.player_actor_id,"proposal_key":"bad"}]:
		check(Contract.validate_proposal(original,data,value).get("code")=="InvalidSiegeIntegrationProposal","提议顶层与身份字段类型严格拒绝")
	var changed: Dictionary=proposed.duplicate(true); changed.recipient=[]
	check(Contract.validate_proposal(original,data,changed).get("code")=="StaleSiegeIntegrationProposal","嵌套接收者类型变更不能带入来源验证")
	check(k.snapshot()==original,"畸形输入不污染真实Kernel")

## 未来来源形状只作否定边界夹具；即使字典完整且借用真实旧行动ID，也不能通过未登记的接受来源门。
func declared_source(proposed: Dictionary, officer: Dictionary) -> Dictionary:
	return {"format":Contract.ACCEPTANCE_FORMAT,"kind":Contract.ACCEPTANCE_KIND,"actor_id":officer.actor_id,"decision":"accept","proposal_key":proposed.proposal_key,"siege_id":proposed.siege_id,"recipient_army_id":proposed.recipient.id,"source_army_id":officer.army_id,"unit_ids":officer.unit_ids.duplicate(),"formation_source_id":officer.formation_source_id,"appointment_sources":officer.appointment_sources.duplicate(true),"actor_action_source_id":officer.formation_source_id}

## 每位军官来源必须绑定同一提议、接收者、名单和原任命；结构匹配不能伪装成已执行actor-action。
func check_acceptance_sources(k, proposed: Dictionary) -> void:
	var before: Dictionary=k.snapshot(); var data: Dictionary=k.content().data
	var boundary: Dictionary=before.duplicate(true); var ids: Array=[]
	for officer in proposed.required_officers:
		var source_id: String="integration-test:"+officer.actor_id
		boundary.facts[source_id]=declared_source(proposed,officer); ids.append(source_id)
		check(Contract.acceptance(boundary,data,proposed,source_id).get("code")=="SiegeIntegrationAcceptanceSourceUnavailable","完整声明仍不能借旧编组行动冒充明确接受")
	check(Contract.candidate(boundary,data,proposed,ids).get("code")=="SiegeIntegrationAcceptanceSourceUnavailable","全员伪造完整声明也不会激活执行入口")
	check(Contract.candidate(boundary,data,proposed,[ids[0],ids[0]]).get("code")=="InvalidSiegeIntegrationAcceptanceSources","一位军官的来源不能代替另一位")
	var repeated: Dictionary=boundary.duplicate(true); repeated.facts[ids[1]]=repeated.facts[ids[0]].duplicate(true)
	check(Contract.candidate(repeated,data,proposed,ids).get("code")=="DuplicateSiegeIntegrationOfficerAcceptance","两个不同来源ID也不能冒充两名军官接受")
	for field in ["proposal_key","siege_id","recipient_army_id","source_army_id","formation_source_id","unit_ids","appointment_sources","actor_id"]:
		var bad: Dictionary=boundary.duplicate(true)
		bad.facts[ids[0]][field]=[] if field in ["unit_ids","appointment_sources"] else "changed"
		check(Contract.acceptance(bad,data,proposed,ids[0]).get("code")=="SiegeIntegrationAcceptanceMismatch","来源必须精确绑定原身份："+field)
	var refused: Dictionary=boundary.duplicate(true); refused.facts[ids[0]].decision="reject"
	check(not Contract.acceptance(refused,data,proposed,ids[0]).ok,"明确拒绝不能被重新解释为接受")
	check(k.preview("siege_strategy",{"siege_id":proposed.siege_id,"command":{"kind":"accept_integration","proposal_key":proposed.proposal_key}}).get("code")=="UnknownSiegeStrategicCommand","生产命令入口未偷偷登记或接受新协议")
	check(k.snapshot()==before,"未来来源反例没有写入真实世界或永久同意")

## 原世界只从正常任官、编组和围困生成；后续坏输入副本不提交或恢复为合法世界。
func run() -> void:
	if not ProjectSettings.get_setting("application/config/siege_departure_isolated",false): quit(1); return
	content_path=ProjectSettings.globalize_path("res://").trim_suffix("/").get_base_dir().path_join("build/safe-departure.hmcatalog.json")
	var k=Kernel.new(); var opened: Dictionary=k.new_game("scenario.alpha_life",content_path)
	if not check(opened.ok,"整编合同正式作者开局"): finish(); return
	var prepared: Dictionary=prepare(k)
	if prepared.is_empty(): finish(); return
	var state: Dictionary=k.snapshot(); var data: Dictionary=k.content().data
	var quoted: Dictionary=Contract.proposal(state,data,prepared.siege_id,state.player_actor_id)
	if not check(quoted.ok,"真实同地两守军可投影只读提议 "+JSON.stringify(quoted)): finish(); return
	check(quoted.get("production_active")==false and state==k.snapshot(),"提议始终标明生产未激活且世界不变")
	var proposal: Dictionary=quoted.proposal
	check(proposal.recipient.id==state.campaigns[prepared.campaigns.a].army_id and proposal.defender_armies.size()==2,"接收者固定现有主攻军，逐支保留真实守军")
	check(proposal.required_officers.size()==2,"两名真实守军军官分别列入接受要求")
	check(Contract.validate_proposal(state,data,proposal).ok,"原提议语义身份可重验")
	var tampered: Dictionary=proposal.duplicate(true); tampered.recipient.id="army.unknown"
	check(Contract.validate_proposal(state,data,tampered).get("code")=="StaleSiegeIntegrationProposal","调用者不能改接收者或伪造fallback")
	var no_sources: Dictionary=Contract.candidate(state,data,proposal,[])
	check(not no_sources.ok and no_sources.get("code")=="SiegeIntegrationAcceptanceRequired","整体意愿满足也不能代替逐将明确接受")
	for id in ["",true,"military.missing",state.offices["office.military.b"].appointment_id]:
		check(not Contract.acceptance(state,data,proposal,id).ok,"空白/布尔/缺失/原任官凭据不能冒充投效："+str(id))
	check_identity(k,prepared,proposal)
	check_rejections(k,prepared,proposal)
	check_acceptance_sources(k,proposal)
	check_malformed(k,prepared,proposal)
	check(state==k.snapshot(),"全部合同查询不分配ID、不写资产/历史/序号/RNG")
	print("SIEGE_INTEGRATION_CONTRACT checks=",checks," failures=",failures," production_active=false")
	quit(0 if failures==0 else 1)
