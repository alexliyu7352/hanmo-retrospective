## 公库法定归属只读解析器；占领凭据链决定当前旗号，绝不替人物移动、改写财物或收编敌兵。
extends RefCounted

## 无占领时沿用作者归属；有占领时必须能沿完整且无环的永久回执追溯到初始组织。
static func garrison_owner(state: Dictionary, data: Dictionary, garrison_id: String, limit_tick: int=-1) -> Dictionary:
	if not data.get("garrisons",{}).has(garrison_id): return _fail("UnknownGarrison")
	var original: String=data.garrisons[garrison_id].organization_id
	var found: Array=[]
	for stronghold in data.get("strongholds",{}).values():
		if stronghold.get("garrison_id")==garrison_id: found.append(stronghold.id)
	if found.size()>1: return _fail("AmbiguousGarrisonStronghold")
	if found.is_empty(): return {"ok":true,"organization_id":original,"occupation_id":"","chain":[]}
	var stronghold_id: String=found[0]
	var runtime: Variant=state.get("strongholds",{}).get(stronghold_id,{})
	if not runtime is Dictionary or not runtime.get("occupation_id","") is String or not runtime.get("owner_organization_id",original) is String: return _fail("InvalidStrongholdOwnership")
	var current: String=runtime.get("owner_organization_id",original)
	var id: String=runtime.get("occupation_id",""); var latest: String=id; var chain: Array=[]
	var ceiling: int=state.tick+1 if limit_tick<0 else limit_tick
	while not id.is_empty():
		if id in chain: return _fail("OccupationChainCycle")
		var valid: Dictionary=receipt(state,data,id,ceiling)
		if not valid.ok: return valid
		var fact: Dictionary=state.facts[id]
		if fact.stronghold_id!=stronghold_id or fact.garrison_id!=garrison_id or fact.to_organization_id!=current or not fact.get("assets_applied",false): return _fail("OccupationChainMismatch")
		if not chain.is_empty() and fact.tick>state.facts[chain[-1]].tick: return _fail("OccupationChainDateMismatch")
		chain.append(id); current=fact.from_organization_id; id=fact.previous_occupation_id
	if current!=original: return _fail("OccupationSourceMissing")
	return {"ok":true,"organization_id":runtime.get("owner_organization_id",original),"occupation_id":latest,"stronghold_id":stronghold_id,"chain":chain}

## 当前链上的历史归属用于冻结征发与归营凭据；后来的易帜不会把旧军团改旗。
static func historical_owner(state: Dictionary, data: Dictionary, garrison_id: String, occupation_id: String) -> Dictionary:
	var current: Dictionary=garrison_owner(state,data,garrison_id)
	if not current.ok: return current
	if occupation_id.is_empty(): return {"ok":true,"organization_id":data.garrisons[garrison_id].organization_id}
	if not occupation_id in current.chain: return _fail("OccupationHistoryMissing")
	return {"ok":true,"organization_id":state.facts[occupation_id].to_organization_id}

## 只有作者明确列出的公共容器随城易帜；其他容器仍返回其作者产权，不扩大到私人庄园。
static func public_owner(state: Dictionary, data: Dictionary, field: String, container_id: String) -> Dictionary:
	if not field in ["inventory_id","wallet_id"]: return _fail("InvalidPublicContainerField")
	var table: String="inventories" if field=="inventory_id" else "wallets"
	var list_key: String="public_inventory_ids" if field=="inventory_id" else "public_wallet_ids"
	if not data.get(table,{}).has(container_id): return _fail("UnknownPublicContainer")
	var matches: Array=[]
	for stronghold in data.get("strongholds",{}).values():
		if container_id in stronghold.get(list_key,[]): matches.append(stronghold)
	if matches.size()>1: return _fail("AmbiguousPublicContainer")
	if matches.is_empty(): return {"ok":true,"owner":data[table][container_id].owner.duplicate(),"occupation_id":""}
	var resolved: Dictionary=garrison_owner(state,data,matches[0].garrison_id)
	if not resolved.ok: return resolved
	return {"ok":true,"owner":{"kind":"organization","id":resolved.organization_id},"occupation_id":resolved.occupation_id}

## 永久占领只能引用正式 Siege 终局与同一实际军团、军令、据点；历史读取不要求军团永远留在城内。
static func receipt(state: Dictionary, data: Dictionary, id: String, limit_tick: int) -> Dictionary:
	var fact: Variant=state.get("facts",{}).get(id)
	if not fact is Dictionary or fact.get("kind")!="military_occupation" or fact.get("phase")!="completed": return _fail("OccupationReceiptRequired")
	for key in ["siege_id","campaign_id","army_id","stronghold_id","garrison_id","from_organization_id","to_organization_id","previous_occupation_id"]:
		if not fact.get(key) is String: return _fail("InvalidOccupationReceipt")
	if not _whole(fact.get("tick"),0,limit_tick) or not data.get("strongholds",{}).has(fact.stronghold_id) or not data.get("garrisons",{}).has(fact.garrison_id): return _fail("InvalidOccupationReceipt")
	var definition: Dictionary=data.strongholds[fact.stronghold_id]
	if definition.garrison_id!=fact.garrison_id or fact.from_organization_id==fact.to_organization_id or not data.organizations.has(fact.from_organization_id) or not data.organizations.has(fact.to_organization_id): return _fail("OccupationOrganizationMismatch")
	for pair in [["public_inventory_ids","inventory_id"],["public_wallet_ids","wallet_id"]]:
		var ids: Variant=fact.get(pair[0]); var authored: Variant=definition.get(pair[0])
		if not _strings(ids) or not _strings(authored) or _sorted(ids)!=_sorted(authored) or not data.garrisons[fact.garrison_id][pair[1]] in ids: return _fail("OccupationPublicManifestMismatch")
	var siege: Variant=state.get("sieges",{}).get(fact.siege_id)
	if not siege is Dictionary or siege.get("id")!=fact.siege_id or siege.get("stronghold_id")!=fact.stronghold_id or siege.get("campaign_id")!=fact.campaign_id or not siege.get("phase") in ["result","closed"] or not siege.get("result") is Dictionary: return _fail("OccupationSiegeResultRequired")
	if not siege.result.get("kind") in ["occupied","surrendered"] or siege.result.get("winner_side")!=0 or not _whole(siege.result.get("tick"),0,fact.tick) or siege.get("armies",{}).get(fact.army_id)!=0 or fact.army_id in siege.get("withdrawn_army_ids",[]): return _fail("OccupationSiegeVictoryRequired")
	var army: Variant=state.get("armies",{}).get(fact.army_id); var campaign: Variant=state.get("campaigns",{}).get(fact.campaign_id)
	if not army is Dictionary or not campaign is Dictionary or army.get("order_id")!=fact.campaign_id or campaign.get("army_id")!=fact.army_id or army.get("organization_id")!=fact.to_organization_id or not data.get("military_orders",{}).has(campaign.get("template_id")): return _fail("OccupationArmySourceMismatch")
	if data.military_orders[campaign.template_id].target_stronghold_id!=fact.stronghold_id: return _fail("OccupationCampaignTargetMismatch")
	return {"ok":true}

## 应用前额外核验当前旧旗、真实在场统帅、守军结清和公共预留；失败不改一件财物。
static func occupation_quote(state: Dictionary, data: Dictionary, receipt_id: String) -> Dictionary:
	var valid: Dictionary=receipt(state,data,receipt_id,state.tick+1)
	if not valid.ok: return valid
	var fact: Dictionary=state.facts[receipt_id]
	var current: Dictionary=garrison_owner(state,data,fact.garrison_id)
	if not current.ok: return current
	if fact.get("assets_applied",false):
		return {"ok":true,"already_completed":true} if receipt_id in current.chain else _fail("OccupationHistoryMissing")
	if current.organization_id!=fact.from_organization_id or current.occupation_id!=fact.previous_occupation_id: return _fail("OccupationStaleOwner")
	var army: Dictionary=state.armies[fact.army_id]
	var commander: Variant=state.actors.get(army.commander_actor_id)
	if army.status!="active" or not commander is Dictionary or not commander.get("alive",false) or not commander.get("available",false) or commander.get("location",{}).get("kind")!="Settlement" or commander.location.get("settlement_id")!=data.strongholds[fact.stronghold_id].settlement_id: return _fail("OccupationArmyMustBePresent")
	var strength: int=0
	for unit_id in army.unit_ids:
		var unit: Dictionary=state.military_units.get(unit_id,{})
		if unit.get("army_id")!=army.id: return _fail("OccupationArmySourceMismatch")
		strength+=unit.get("fit",[]).size()
	if strength==0: return _fail("OccupationGuardRequired")
	for unit in state.military_units.values():
		if unit.garrison_id==fact.garrison_id and unit.organization_id!=fact.to_organization_id and (not unit.fit.is_empty() or not unit.wounded.is_empty()): return _fail("GarrisonDefendersUnresolved")
	var owner: Dictionary={"kind":"organization","id":fact.from_organization_id}
	for id in fact.public_wallet_ids:
		var wallet: Variant=state.wallets.get(id)
		if not wallet is Dictionary or wallet.get("owner")!=owner: return _fail("OccupationPublicOwnerMismatch")
		if not wallet.get("reserved",{}).is_empty(): return _fail("OccupationPublicAssetsReserved")
	for id in fact.public_inventory_ids:
		var inventory: Variant=state.inventories.get(id)
		if not inventory is Dictionary or inventory.get("owner")!=owner: return _fail("OccupationPublicOwnerMismatch")
		for lot in inventory.lots:
			if lot.owner==owner and not lot.reserved_by.is_empty(): return _fail("OccupationPublicAssetsReserved")
	return {"ok":true,"already_completed":false}

## 稳定存档拒绝悬空或未应用占领；公共容器产权必须与同一永久链一致。
static func validate_state(state: Dictionary, data: Dictionary) -> Dictionary:
	var referenced: Dictionary={}
	for id in data.get("garrisons",{}):
		var current: Dictionary=garrison_owner(state,data,id,state.tick)
		if not current.ok: return current
		for receipt_id in current.chain: referenced[receipt_id]=true
	for id in state.facts:
		var fact: Variant=state.facts[id]
		if fact is Dictionary and fact.get("kind")=="military_occupation":
			if not referenced.has(id) or not fact.get("asset_manifest") is Dictionary or not fact.get("custody_transfers") is Array: return _fail("OccupationAssetsNotApplied")
			var manifest: Dictionary=_manifest(fact,data)
			if not manifest.ok: return manifest
			for pair in [["public_inventory_ids","inventories"],["public_wallet_ids","wallets"]]:
				if not fact.asset_manifest.get(pair[1]) is Dictionary or _sorted(fact.asset_manifest[pair[1]].keys())!=_sorted(fact[pair[0]]): return _fail("OccupationPublicManifestMismatch")
	for stronghold in data.get("strongholds",{}).values():
		for pair in [["public_inventory_ids","inventory_id","inventories"],["public_wallet_ids","wallet_id","wallets"]]:
			for id in stronghold.get(pair[0],[]):
				var owner: Dictionary=public_owner(state,data,pair[1],id)
				if not owner.ok: return owner
				if state.get(pair[2],{}).get(id,{}).get("owner")!=owner.owner: return _fail("OccupationPublicOwnerMismatch")
	return {"ok":true}

## 接收清单保存每批实物、私人寄存与余额快照；伪造产权、负数或重复批次不能成为合法历史。
static func _manifest(fact: Dictionary, data: Dictionary) -> Dictionary:
	var old_owner: Dictionary={"kind":"organization","id":fact.from_organization_id}
	var new_owner: Dictionary={"kind":"organization","id":fact.to_organization_id}
	for table in ["wallets","inventories"]:
		if not fact.asset_manifest.get(table) is Dictionary: return _fail("OccupationPublicManifestMismatch")
		for entry in fact.asset_manifest[table].values():
			if not entry is Dictionary or entry.get("from_owner")!=old_owner or entry.get("to_owner")!=new_owner: return _fail("OccupationManifestOwnerMismatch")
			if table=="wallets":
				if not _whole(entry.get("balance"),0,2147483647): return _fail("OccupationManifestBalanceMismatch")
			else:
				if not entry.get("public_lots") is Array or not entry.get("protected_lots") is Array: return _fail("OccupationManifestLotsMismatch")
				var ids: Array=[]
				for lot in entry.public_lots+entry.protected_lots:
					if not lot is Dictionary or not lot.get("id") is String or lot.id.is_empty() or lot.id in ids or not data.items.has(lot.get("item_id")) or not _whole(lot.get("quantity"),1,2147483647): return _fail("OccupationManifestLotsMismatch")
					ids.append(lot.id)
				for lot in entry.protected_lots:
					if not lot.get("owner") is Dictionary or lot.owner==old_owner or not lot.get("reserved_by") is String: return _fail("OccupationProtectedLotMismatch")
	return {"ok":true}

## 只接受互异非空字符串清单，拒绝重复公共容器形成重复转移。
static func _strings(value: Variant) -> bool:
	if not value is Array: return false
	var seen: Dictionary={}
	for id in value:
		if not id is String or id.is_empty() or seen.has(id): return false
		seen[id]=true
	return true

## 历史序列比较使用排序副本，不改变作者或存档输入。
static func _sorted(values: Array) -> Array:
	var result: Array=values.duplicate(); result.sort(); return result

## 边界计数必须为整数，避免存档小数绕过日期与账目检查。
static func _whole(value: Variant, minimum: int, maximum: int) -> bool:
	return typeof(value)==TYPE_INT and value>=minimum and value<=maximum

## 失败只返回稳定错误码，由调用方丢弃候选或向玩家解释。
static func _fail(code: String) -> Dictionary:
	return {"ok":false,"code":code}
