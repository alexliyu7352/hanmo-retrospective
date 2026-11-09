## 核验地形与庄园的作者定义联系，引用存在仍不等于资产归属和初始位置正确。
extends RefCounted
const Terrain=preload("res://core/world_terrain.gd")

## 装载时检查地图分类、入口和庄园真人私产，失败保留原作者内容而不修造资源。
static func validate(data: Dictionary) -> Dictionary:
	for role in data.roles.values():
		if role.has("family"):
			var owner: Dictionary={"kind":"actor","id":role.initial_actor} if role.organization_id==null else {"kind":"organization","id":role.organization_id}
			if not "family_adopt" in role.services or data.wallets[role.family.wallet_id].owner!=owner: return fail("FamilyPaymentAuthority",role.id)
			if role.family.get("enable_travel_plan",false) and not data.actors[role.family.prototype_id].has("travel_plan"): return fail("FamilyTravelPlanMissing",role.id+"/family/enable_travel_plan")
		elif "family_adopt" in role.services: return fail("FamilyServiceNotOffered",role.id)
	for id in data.get("world_maps",{}):
		var map: Dictionary=data.world_maps[id]
		var valid: Dictionary=Terrain.validate_map(map)
		if not valid.ok: return {"ok":false,"code":valid.code,"path":"world_maps/"+id}
		for cell in map.cells:
			if cell.has("blocked_flag") and not data.flags.has(cell.blocked_flag): return fail("UnknownTerrainFlag",id)
	for scenario in data.scenarios.values():
		if not scenario.has("world_map_id"): continue
		var map: Dictionary=data.world_maps[scenario.world_map_id]
		for place in data.settlements.values():
			if not Terrain.point_valid(map,place.map_xy,"foot",{}): return fail("SettlementTerrainBlocked",place.id)
	var used_people: Dictionary={}; var used_assets: Dictionary={}
	for id in data.get("estates",{}):
		var e: Dictionary=data.estates[id]
		if not e.manager_actor_id in e.resident_actor_ids: return fail("EstateManagerNotResident",id)
		var owner: Dictionary={"kind":"actor","id":e.owner_actor_id}
		if data.wallets[e.wallet_id].owner!=owner or data.inventories[e.inventory_id].owner!=owner: return fail("EstateAssetOwnership",id)
		for asset in [e.wallet_id,e.inventory_id]:
			if used_assets.has(asset): return fail("SharedEstateAssets",id)
			used_assets[asset]=id
		for person in e.resident_actor_ids:
			if used_people.has(person) or data.actors[person].initial_place!=e.place_id: return fail("InvalidEstateResident",id)
			used_people[person]=id
		var duties: Array=[]
		for row in e.initial_duties:
			if not row.actor_id in e.resident_actor_ids or row.actor_id in duties or (row.actor_id==e.manager_actor_id and row.duty!="idle"): return fail("InvalidEstateDuty",id)
			duties.append(row.actor_id)
		for item_id in [e.food_item_id,e.output_item_id,e.construction.material_item_id]:
			if data.items[item_id].kind!="commodity": return fail("EstateNeedsCommodity",id)
		for level in e.initial_facilities.values():
			if level>e.construction.max_level: return fail("EstateInitialLevel",id)
	return {"ok":true}

## 将跨表错误附上稳定记录路径，供编辑器定位真实作者定义。
static func fail(code: String, id: String) -> Dictionary:
	return {"ok":false,"code":code,"path":id}
