## 整编提议与接受来源的生产未激活合同；本片不登记命令、不生成同意或移交任何人产。
extends RefCounted
const Assets=preload("res://core/military_assets.gd")
const Campaign=preload("res://core/campaign_rules.gd")
const Location=preload("res://core/world_location.gd")
const FORMAT="HM-SIEGE-INTEGRATION-PROPOSAL/1"
const ACCEPTANCE_FORMAT="HM-SIEGE-INTEGRATION-ACCEPTANCE/1"
const ACCEPTANCE_KIND="siege_integration_acceptance"

## 只从当前人产账投影候选身份；成功仅代表结构可描述，原交涉条件和资产可执行性仍须中央重验。
static func proposal(state: Dictionary, data: Dictionary, siege_id: String, actor_id: String) -> Dictionary:
	if not state.get("sieges") is Dictionary: return _fail("InvalidSiegeIntegrationParticipants")
	var siege: Variant=state.sieges.get(siege_id)
	if not siege is Dictionary or siege.get("id")!=siege_id: return _fail("UnknownSiege")
	if siege.get("phase")!="active": return _fail("SiegeNotActive")
	if not siege.get("pending_nodes") is Dictionary or not siege.pending_nodes.is_empty(): return _fail("SiegeIntegrationBattlePending")
	if not siege.get("armies") is Dictionary or not siege.get("withdrawn_army_ids") is Array: return _fail("InvalidSiegeIntegrationParticipants")
	if not siege.get("campaign_id") is String or not siege.get("stronghold_id") is String: return _fail("InvalidSiegeIntegrationParticipants")
	for id in siege.armies:
		if not id is String or not siege.armies[id] in [0,1] or typeof(siege.armies[id])!=TYPE_INT: return _fail("InvalidSiegeIntegrationParticipants")
	var withdrawn: Array=[]
	for id in siege.withdrawn_army_ids:
		if not id is String or not siege.armies.has(id) or id in withdrawn: return _fail("InvalidSiegeIntegrationParticipants")
		withdrawn.append(id)
	if not state.get("battles") is Dictionary: return _fail("InvalidSiegeIntegrationBattle")
	for battle in state.battles.values():
		if not battle is Dictionary: return _fail("InvalidSiegeIntegrationBattle")
		if battle.get("kind")=="military" and (not battle.get("units") is Dictionary or not battle.get("phase") is String): return _fail("InvalidSiegeIntegrationBattle")
	var valid: Dictionary=Assets.validate_state(state,data)
	if not valid.ok: return valid
	valid=Campaign.validate_state(state,data)
	if not valid.ok: return valid
	var campaign: Dictionary=state.campaigns.get(siege.get("campaign_id"),{})
	var recipient: Dictionary=state.armies.get(campaign.get("army_id"),{})
	if recipient.is_empty() or campaign.get("status")!="operating" or siege.armies.get(recipient.id)!=0 or recipient.id in siege.withdrawn_army_ids: return _fail("SiegeIntegrationRecipientUnavailable")
	var target: Dictionary=data.get("strongholds",{}).get(siege.get("stronghold_id"),{})
	if target.is_empty() or data.military_orders[campaign.template_id].target_stronghold_id!=target.id: return _fail("SiegeIntegrationTargetMismatch")
	var place: Dictionary=Location.resolve(state,recipient.commander_actor_id)
	if place.is_empty() or not Campaign.at_settlement(state,data,recipient.id,target.settlement_id): return _fail("SiegeIntegrationNotCoLocated")
	var receiving: Dictionary=_army(state,data,recipient.id,place,target.settlement_id)
	if not receiving.ok: return receiving
	var permitted: bool=false; var defenders: Array=[]; var officers: Array=[]
	for army_id in _ids(siege.armies):
		if army_id in siege.withdrawn_army_ids: continue
		var army: Dictionary=state.armies.get(army_id,{})
		if army.is_empty(): return _fail("InvalidSiegeIntegrationParticipants")
		var order: Dictionary=state.campaigns[army.order_id]
		if army.commander_actor_id==actor_id and Assets.ready_office(state,data,actor_id,order.office_id).ok: permitted=true
		if siege.armies[army_id]!=1: continue
		if army.organization_id==recipient.organization_id: return _fail("SiegeIntegrationAllegianceMismatch")
		var projected: Dictionary=_army(state,data,army_id,place,target.settlement_id)
		if not projected.ok: return projected
		defenders.append(projected.army); officers.append_array(projected.army.officers)
	if not permitted: return _fail("SiegeNegotiatorRequired")
	if defenders.is_empty(): return _fail("SiegeIntegrationDefendersRequired")
	var units: Array=receiving.army.unit_ids.duplicate()
	for army in defenders: units.append_array(army.unit_ids)
	for battle in state.get("battles",{}).values():
		if battle.get("kind")!="military" or battle.get("phase")=="closed": continue
		for id in units:
			if battle.get("units",{}).has(id): return _fail("SiegeIntegrationBattlePending")
	var body: Dictionary={"format":FORMAT,"siege_id":siege_id,"campaign_id":campaign.id,"stronghold_id":target.id,"proposer_actor_id":actor_id,"recipient":receiving.army,"defender_armies":defenders,"required_officers":officers}
	body.proposal_key=JSON.stringify(body,"",true,true).sha256_text()
	return {"ok":true,"production_active":false,"proposal":body}

## 以当前实际来源重建全部身份；不以全局revision作签名，另一个同意命令不会仅因序号递增令提议失效。
static func validate_proposal(state: Dictionary, data: Dictionary, proposed: Variant) -> Dictionary:
	if not proposed is Dictionary or proposed.get("format")!=FORMAT or not proposed.get("siege_id") is String or not proposed.get("proposer_actor_id") is String or not proposed.get("proposal_key") is String: return _fail("InvalidSiegeIntegrationProposal")
	var fresh: Dictionary=proposal(state,data,proposed.siege_id,proposed.proposer_actor_id)
	if not fresh.ok: return fresh
	if fresh.proposal!=proposed: return _fail("StaleSiegeIntegrationProposal")
	return {"ok":true,"production_active":false}

## 接受只读永久来源ID；结构匹配仍不能替代尚未登记的中央actor-action来源校验，当前始终不授予接受资格。
static func acceptance(state: Dictionary, data: Dictionary, proposed: Variant, source_id: Variant) -> Dictionary:
	var checked: Dictionary=validate_proposal(state,data,proposed)
	if not checked.ok: return checked
	if not source_id is String or source_id.is_empty(): return _fail("InvalidSiegeIntegrationAcceptanceSource")
	var source: Variant=state.get("facts",{}).get(source_id)
	if source==null: return _fail("SiegeIntegrationAcceptanceSourceUnavailable")
	if not source is Dictionary: return _fail("InvalidSiegeIntegrationAcceptanceSource")
	var fields: Array=["format","kind","actor_id","decision","proposal_key","siege_id","recipient_army_id","source_army_id","unit_ids","formation_source_id","appointment_sources","actor_action_source_id"]
	if source.size()!=fields.size(): return _fail("InvalidSiegeIntegrationAcceptanceSource")
	for field in fields:
		if not source.has(field): return _fail("InvalidSiegeIntegrationAcceptanceSource")
	if source.format!=ACCEPTANCE_FORMAT or source.kind!=ACCEPTANCE_KIND or source.decision!="accept" or not source.actor_id is String or not source.actor_action_source_id is String or source.actor_action_source_id.is_empty(): return _fail("InvalidSiegeIntegrationAcceptanceSource")
	if source.proposal_key!=proposed.proposal_key or source.siege_id!=proposed.siege_id or source.recipient_army_id!=proposed.recipient.id: return _fail("SiegeIntegrationAcceptanceMismatch")
	var matched: bool=false
	for officer in proposed.required_officers:
		if officer.actor_id!=source.actor_id: continue
		matched=source.source_army_id==officer.army_id and source.unit_ids==officer.unit_ids and source.formation_source_id==officer.formation_source_id and source.appointment_sources==officer.appointment_sources
	if not matched: return _fail("SiegeIntegrationAcceptanceMismatch")
	# 不能把伪造的完整字典或旧任官/编组事实当作已执行的明确接受；真实写命令与来源owner不属于本片。
	return _fail("SiegeIntegrationAcceptanceSourceUnavailable")

## 整体候选必须逐将有唯一来源；本片只返回拒绝，不存在绕过中央资产与旧军令关闭的成功转移路径。
static func candidate(state: Dictionary, data: Dictionary, proposed: Variant, source_ids: Variant) -> Dictionary:
	var checked: Dictionary=validate_proposal(state,data,proposed)
	if not checked.ok: return checked
	if not source_ids is Array: return _fail("InvalidSiegeIntegrationAcceptanceSources")
	var seen: Array=[]
	for id in source_ids:
		if not id is String or id.is_empty() or id in seen: return _fail("InvalidSiegeIntegrationAcceptanceSources")
		seen.append(id)
	if source_ids.size()!=proposed.required_officers.size(): return _fail("SiegeIntegrationAcceptanceRequired")
	var actors: Array=[]
	for id in source_ids:
		var source: Variant=state.get("facts",{}).get(id)
		if not source is Dictionary or not source.get("actor_id") is String: continue
		if source.actor_id in actors: return _fail("DuplicateSiegeIntegrationOfficerAcceptance")
		actors.append(source.actor_id)
	for id in source_ids:
		checked=acceptance(state,data,proposed,id)
		if not checked.ok: return checked
	return _fail("SiegeIntegrationAcceptanceSourceUnavailable")

## 冻结实际军团、逐部五类兵员及各军官原任命；私人借调只保留来源，是否可释放由资产owner决定。
static func _army(state: Dictionary, data: Dictionary, army_id: String, place: Dictionary, settlement_id: String) -> Dictionary:
	var army: Dictionary=state.armies[army_id]; var campaign: Dictionary=state.campaigns[army.order_id]
	if army.status!="active" or campaign.status!="operating": return _fail("SiegeIntegrationArmyUnavailable")
	if not Campaign.at_settlement(state,data,army_id,settlement_id): return _fail("SiegeIntegrationNotCoLocated")
	var authority: Dictionary=Assets.ready_office(state,data,army.commander_actor_id,campaign.office_id)
	if not authority.ok: return authority
	var units: Array=[]; var people: Dictionary={army.commander_actor_id:[]}
	for id in _strings(army.unit_ids):
		var unit: Dictionary=state.military_units[id]
		if not unit.mobilizing_action_id.is_empty() or not unit.get("training_action_id","").is_empty(): return _fail("SiegeIntegrationUnitBusy")
		var projected: Dictionary={}
		for field in ["id","source_garrison_id","organization_id","army_id","commander_actor_id","troop_type_id","captors","retainer_sources"]: projected[field]=unit[field].duplicate(true) if unit[field] is Dictionary else unit[field]
		for field in ["member_ids"]+Assets.GROUPS: projected[field]=_strings(unit[field])
		units.append(projected)
		if not people.has(unit.commander_actor_id): people[unit.commander_actor_id]=[]
		people[unit.commander_actor_id].append(id)
	for id in army.member_actor_ids:
		var actor: Dictionary=state.actors[id]
		if not actor.alive or not actor.available or not actor.action_id.is_empty(): return _fail("SiegeIntegrationActorUnavailable")
		if Location.resolve(state,id)!=place: return _fail("SiegeIntegrationNotCoLocated")
	var officers: Array=[]
	for id in _ids(people):
		if not id in army.member_actor_ids: return _fail("SiegeIntegrationOfficerSourceMismatch")
		var appointments: Array=[]
		for office_id in _ids(state.offices):
			var office: Dictionary=state.offices[office_id]
			if office.holder_actor_id==id: appointments.append(office.duplicate(true))
		officers.append({"actor_id":id,"army_id":army_id,"unit_ids":people[id].duplicate(),"formation_source_id":army.formation_source,"appointment_sources":appointments})
	return {"ok":true,"army":{"id":army_id,"organization_id":army.organization_id,"campaign_id":campaign.id,"formation_source_id":army.formation_source,"commander_actor_id":army.commander_actor_id,"office_id":campaign.office_id,"garrison_id":army.garrison_id,"inventory_id":army.inventory_id,"wallet_id":army.wallet_id,"location":place.duplicate(true),"unit_ids":_strings(army.unit_ids),"member_actor_ids":_strings(army.member_actor_ids),"prisoner_ids":_strings(army.prisoner_ids),"retainers":army.retainers.duplicate(true),"units":units,"officers":officers}}

## 对来源键排序，字典插入顺序不参与同一提议的签名。
static func _ids(value: Dictionary) -> Array:
	var result: Array=value.keys(); result.sort(); return result

## 复制并稳定排列已由真实资产validator校验的身份数组，不改原名册次序。
static func _strings(value: Array) -> Array:
	var result: Array=value.duplicate(); result.sort(); return result

## 失败明确标注未激活，调用方不能把合同缺口解释成已经接受或完成交接。
static func _fail(code: String) -> Dictionary:
	return {"ok":false,"code":code,"production_active":false}
