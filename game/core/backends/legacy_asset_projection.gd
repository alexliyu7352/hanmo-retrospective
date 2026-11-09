## 旧世界的内部只读资产切片；不形成可花钱状态、玩家授权视图或完整NativeProjection。
extends RefCounted
const Schema=preload("res://core/state_schema.gd")

## 先核同一完整三元组并原样透传失败；成功仅复制既有来源与资产，保留产权附加字段和批次原序。
static func project(state: Dictionary, catalog: Dictionary, registry: Dictionary) -> Dictionary:
	var valid: Dictionary=Schema.validate(state,catalog,registry)
	if not valid.ok: return valid
	var source: Dictionary={}
	for key in ["ruleset_id","content_id","content_version","content_digest","engine_build","rng_backend","scenario_id","tick","revision"]:
		source[key]=state[key]
	var result: Dictionary={"ok":true,"scope":"legacy_assets_readonly","version":1,"source":source,"actors":[],"wallets":[],"inventories":[]}
	var actor_ids: Array=state.actors.keys(); actor_ids.sort()
	for id in actor_ids:
		var actor: Dictionary=state.actors[id]
		result.actors.append({"id":actor.id,"wallet_id":actor.wallet_id,"inventory_id":actor.inventory_id})
	var wallet_ids: Array=state.wallets.keys(); wallet_ids.sort()
	for id in wallet_ids:
		var wallet: Dictionary=state.wallets[id]
		result.wallets.append({"id":wallet.id,"owner":wallet.owner.duplicate(true),"balance":wallet.balance,"reserved":wallet.reserved.duplicate(true)})
	var inventory_ids: Array=state.inventories.keys(); inventory_ids.sort()
	for id in inventory_ids:
		var inventory: Dictionary=state.inventories[id]
		result.inventories.append({"id":inventory.id,"owner":inventory.owner.duplicate(true),"capacity":inventory.capacity,"revision":inventory.revision,"lots":inventory.lots.duplicate(true)})
	return result
