## 统一解析实际位置与同行归属；同伴只引用队伍，不保存可独立移动的地点副本。
extends RefCounted
## 按人物稳定ID查找队伍；只读查询不创建隐含队伍或成员。
static func party_id(state: Dictionary, actor_id: String) -> String:
	for id in state.get("parties",{}):
		var party: Variant=state.parties[id]
		if party is Dictionary and actor_id in party.get("members",[]): return id
	return ""
## 按真实成员查活动军团，俘虏只有位置跟随权，不能被当成本军行动成员。
static func army_id(state: Dictionary, actor_id: String) -> String:
	for id in state.get("armies",{}):
		var army: Variant=state.armies[id]
		if army is Dictionary and army.get("status")=="active" and actor_id in army.get("member_actor_ids",[]): return id
	return ""

## 返回同一军团或同行队伍的人物副本；没有集体归属时只有本人。
static func members(state: Dictionary, actor_id: String) -> Array:
	var army: String=army_id(state,actor_id)
	if not army.is_empty(): return state.armies[army].member_actor_ids.duplicate()
	var id: String=party_id(state,actor_id)
	return state.parties[id].members.duplicate() if not id.is_empty() else [actor_id]

## 解析领队或统帅唯一位置；坏引用、循环及嵌套集体返回空值，不能让人物出现多份地点。
static func resolve(state: Dictionary, actor_id: String) -> Dictionary:
	var actor: Variant=state.get("actors",{}).get(actor_id)
	if not actor is Dictionary or not actor.get("location") is Dictionary: return {}
	var location: Dictionary=actor.location
	var leader_id: String=""
	if location.get("kind")=="Following":
		var party: Variant=state.get("parties",{}).get(location.get("party_id"))
		if not party is Dictionary or not actor_id in party.get("members",[]): return {}
		leader_id=party.get("leader_id","")
	elif location.get("kind")=="InArmy":
		var army: Variant=state.get("armies",{}).get(location.get("army_id"))
		if not army is Dictionary or army.get("status")!="active" or not (actor_id in army.get("member_actor_ids",[]) or actor_id in army.get("prisoner_ids",[])): return {}
		leader_id=army.get("commander_actor_id","")
	else: return location.duplicate(true)
	var leader: Variant=state.actors.get(leader_id)
	if not leader is Dictionary or not leader.get("location") is Dictionary: return {}
	if leader.location.get("kind") in ["Following","InArmy"]: return {}
	return leader.location.duplicate(true)
