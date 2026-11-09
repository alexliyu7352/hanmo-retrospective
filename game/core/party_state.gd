## 校验同行集合与人物的双向引用；坏档不能产生多地分身、嵌套队伍或重复费用。
extends RefCounted
const Location=preload("res://core/world_location.gd")
## 只读验证，不通过修补缺失字段掩盖损坏的存档。
static func validate(state: Dictionary, data: Dictionary) -> Dictionary:
	if not state.get("parties") is Dictionary: return fail("MissingParties")
	var joined: Dictionary={}
	for id in state.parties:
		var p: Variant=state.parties[id]
		if not p is Dictionary: return fail("InvalidParty")
		if p.get("id")!=id or not p.get("leader_id") is String or not p.get("members") is Array or not p.get("terms") is Dictionary: return fail("InvalidParty")
		if typeof(p.get("created_tick"))!=TYPE_INT or p.created_tick<0 or p.created_tick>state.tick: return fail("InvalidPartyTime")
		if p.members.size()<2 or p.members.size()>16 or p.members[0]!=p.leader_id or p.terms.size()!=p.members.size()-1: return fail("InvalidPartyMembers")
		if not state.actors.has(p.leader_id): return fail("UnknownPartyLeader")
		var leader: Dictionary=state.actors[p.leader_id]
		if leader.location.get("kind") in ["Following","InArmy"]: return fail("NestedParty")
		for member in p.members:
			if not member is String or joined.has(member) or not state.actors.has(member): return fail("DuplicatePartyMember")
			if not Location.army_id(state,member).is_empty(): return fail("ArmyPartyOverlap")
			joined[member]=id
			if member==p.leader_id: continue
			var actor: Dictionary=state.actors[member]
			if actor.location!={"kind":"Following","party_id":id}: return fail("PartyLocationMismatch")
			var terms: Variant=p.terms.get(member)
			if not terms is Dictionary or not terms.get("role_id") is String or not data.roles.has(terms.role_id): return fail("InvalidCompanionTerms")
			if typeof(terms.get("cost_per_tick"))!=TYPE_INT or terms.cost_per_tick<0 or terms.cost_per_tick>100000: return fail("InvalidCompanionCost")
			var role: Dictionary=data.roles[terms.role_id]
			if not "join_party" in role.services or terms.cost_per_tick!=role.get("companion_cost_per_tick",6): return fail("InvalidCompanionTerms")
			if not leader.action_id.is_empty() and state.actions.has(leader.action_id) and state.actions[leader.action_id].kind in ["travel","rescue"] and actor.action_id!=leader.action_id: return fail("PartyOccupationMismatch")
	for actor in state.actors.values():
		if actor.location.get("kind")=="Following" and joined.get(actor.id)!=actor.location.get("party_id"): return fail("DanglingParty")
	return {"ok":true}
## 返回稳定诊断码，调用者保留原世界与原存档文件。
static func fail(code: String) -> Dictionary:
	return {"ok":false,"code":code}
