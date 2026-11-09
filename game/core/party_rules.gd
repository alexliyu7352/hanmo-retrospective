## 同行领域规则：只接受当面约定，保留每个人的身份、私产与主行动占用。
extends RefCounted
const Location=preload("res://core/world_location.gd")
const Interaction=preload("res://core/interaction_rules.gd")
const Factory=preload("res://core/world_factory.gd")
const Social=preload("res://core/social_rules.gd")
const KINDS=["join_party","dismiss_companion"]
## 邀请检查当前岗位同意与成员空闲；解散只在真实据点内办理，不允许路上丢人。
static func preview(state: Dictionary, data: Dictionary, actor_id: String, kind: String, params: Dictionary) -> Dictionary:
	var actor: Dictionary=state.actors[actor_id]
	if not Location.army_id(state,actor_id).is_empty(): return {"ok":false,"code":"ArmyNotReturned"}
	if actor.location.kind=="Following": return {"ok":false,"code":"NotPartyLeader"}
	if not actor.alive or not actor.available or not actor.action_id.is_empty(): return {"ok":false,"code":"ActorBusy"}
	if actor.location.kind!="Settlement": return {"ok":false,"code":"PartyNeedsSettlement"}
	var id: String=Location.party_id(state,actor_id)
	if kind=="dismiss_companion":
		var target: Variant=params.get("actor_id")
		if id.is_empty() or not target is String or target==actor_id or not target in state.parties[id].members: return {"ok":false,"code":"NotCompanion"}
		if not state.actors[target].action_id.is_empty(): return {"ok":false,"code":"ParticipantBusy"}
		return {"ok":true,"duration":0,"party_id":id,"target":target}
	var valid: Dictionary=Interaction.validate(state,data,actor_id,params.get("context"),"join_party")
	if params.has("return_receipt_id"): valid=_returned_retainer(state,data,actor_id,params)
	if not valid.ok: return valid
	var target: String=valid.context.provider_actor_id
	if not Location.army_id(state,target).is_empty(): return {"ok":false,"code":"ArmyNotReturned"}
	if not Location.party_id(state,target).is_empty(): return {"ok":false,"code":"AlreadyInParty"}
	if not id.is_empty() and state.parties[id].members.size()>=16: return {"ok":false,"code":"PartyFull"}
	for owner in state.interactions:
		if owner!=actor_id and (owner==target or state.interactions[owner].context.provider_actor_id==target): return {"ok":false,"code":"ParticipantBusy"}
	var role: Dictionary=data.roles[valid.context.role_id]
	if Social.trust(state,target,actor_id)<role.get("companion_required_trust",0): return {"ok":false,"code":"CompanionTrustRequired"}
	return {"ok":true,"duration":0,"party_id":id,"target":target,"context":valid.context,"cost_per_tick":role.get("companion_cost_per_tick",6)}

## 军营交接后可当面重续原庄卫同行条款；只认实际归营本人、原借调与仍在场的庄主，不借旧岗位远程调人。
static func _returned_retainer(state: Dictionary, data: Dictionary, actor_id: String, params: Dictionary) -> Dictionary:
	var fact: Dictionary=state.facts.get(params.get("return_receipt_id"),{})
	var target: String=params.get("actor_id","")
	if fact.get("kind")!="military_return" or not target in fact.get("returned_retainers",[]) or not state.actors.has(target) or target==actor_id: return {"ok":false,"code":"RetainerReturnReceiptRequired"}
	var army: Dictionary=state.armies.get(fact.get("army_id"),{})
	var pledge: Dictionary=army.get("retainers",{}).get(target,{})
	var estate: Dictionary=state.estates.get(pledge.get("estate_id"),{})
	if army.get("status")!="closed" or estate.get("owner_actor_id")!=actor_id or estate.get("detached",{}).get(target)!=pledge.get("source_id"): return {"ok":false,"code":"EstateDetachmentMismatch"}
	var person: Dictionary=state.actors[target]
	if not person.alive or not person.available or not person.action_id.is_empty() or Location.resolve(state,target)!=Location.resolve(state,actor_id): return {"ok":false,"code":"PersonAbsent"}
	for role_id in state.roles:
		var role: Dictionary=data.roles[role_id]
		if state.roles[role_id].enabled and state.roles[role_id].actor_id==target and "join_party" in role.services:
			return {"ok":true,"context":{"place_id":Location.resolve(state,actor_id).place_id,"provider_actor_id":target,"role_id":role_id}}
	return {"ok":false,"code":"ServiceNotOffered"}
## 在候选世界保存双方约定或解除同行；不转移私人库存、不回滚已走时间。
static func apply(state: Dictionary, actor_id: String, kind: String, quote: Dictionary) -> Dictionary:
	var id: String=quote.party_id
	if kind=="join_party":
		if id.is_empty():
			id=Factory.next_id(state,"party")
			state.parties[id]={"id":id,"leader_id":actor_id,"members":[actor_id],"terms":{},"created_tick":state.tick}
		var party: Dictionary=state.parties[id]
		party.members.append(quote.target)
		party.terms[quote.target]={"role_id":quote.context.role_id,"cost_per_tick":quote.cost_per_tick}
		state.actors[quote.target].location={"kind":"Following","party_id":id}
	else:
		var party: Dictionary=state.parties[id]
		state.actors[quote.target].location=Location.resolve(state,actor_id)
		party.members.erase(quote.target); party.terms.erase(quote.target)
		if party.members.size()==1: state.parties.erase(id)
	return {"ok":true,"details":{"party_id":id,"person_id":quote.target}}
## 读取已接受的同行费用，不在每次移动时重新议价或使用公款。
static func extra_cost(state: Dictionary, actor_id: String) -> int:
	var id: String=Location.party_id(state,actor_id)
	var total: int=0
	if not id.is_empty():
		for terms in state.parties[id].terms.values(): total+=terms.cost_per_tick
	return total
## 启程前检查全部成员的真实可用性，不能把正在工作的人一起带走。
static func available(state: Dictionary, actor_id: String) -> bool:
	for member in Location.members(state,actor_id):
		var actor: Dictionary=state.actors[member]
		if not actor.alive or not actor.available or not actor.action_id.is_empty(): return false
	return true
