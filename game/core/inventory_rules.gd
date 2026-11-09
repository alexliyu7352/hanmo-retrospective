## Shared inventory and escrow operations; every call mutates only the kernel's private candidate.
extends RefCounted
const Factory = preload("res://core/world_factory.gd")

## Compute spendable money without treating a reservation as an additional balance.
static func spendable(wallet: Dictionary) -> int:
	var reserved: int = 0
	for value in wallet.reserved.values(): reserved+=value
	return wallet.balance-reserved

## Compute all container weight, including other owners' and reserved goods.
static func weight(inventory: Dictionary, data: Dictionary) -> int:
	var total: int = 0
	for lot in inventory.lots: total+=lot.quantity*data.items[lot.item_id].weight
	return total

## Count only matching ownership and reservation, not all goods physically carried.
static func count(inventory: Dictionary, item_id: String, owners: Array, reservation: String = "") -> int:
	var total: int = 0
	for lot in inventory.lots:
		if lot.item_id==item_id and lot.owner in owners and lot.reserved_by==reservation: total+=lot.quantity
	return total

## Add a distinct physical lot, preserving its real owner and a unique identity.
static func add(state: Dictionary, inventory_id: String, item_id: String, quantity: int, owner: Dictionary, reservation: String = "") -> void:
	if quantity<=0: return
	state.inventories[inventory_id].lots.append({"id":Factory.next_id(state,"lot"),"item_id":item_id,"quantity":quantity,"owner":owner.duplicate(true),"reserved_by":reservation})
	state.inventories[inventory_id].revision+=1

## Reserve exactly the requested quantity, splitting a lot rather than reserving its unused remainder.
static func reserve(state: Dictionary, inventory_id: String, item_id: String, quantity: int, owners: Array, action_id: String) -> Dictionary:
	var inventory: Dictionary=state.inventories[inventory_id]
	if count(inventory,item_id,owners)<quantity: return {"ok":false,"code":"InsufficientGoods"}
	var remaining: int=quantity
	for owner in owners:
		for lot in inventory.lots.duplicate():
			if lot.item_id!=item_id or lot.owner!=owner or not lot.reserved_by.is_empty(): continue
			var used: int=mini(remaining,lot.quantity)
			if used==lot.quantity:
				lot.reserved_by=action_id
			else:
				lot.quantity-=used
				add(state,inventory_id,item_id,used,owner,action_id)
			remaining-=used
			if remaining==0: break
		if remaining==0: break
	inventory.revision+=1
	return {"ok":true}

## Transfer existing goods to another container; optional ownership change represents an actual handover.
static func transfer(state: Dictionary, source_id: String, destination_id: String, item_id: String, quantity: int, owners: Array, reservation: String = "", new_owner: Dictionary = {}) -> Dictionary:
	if source_id==destination_id: return {"ok":false,"code":"SameContainer"}
	var source: Dictionary=state.inventories[source_id]
	if count(source,item_id,owners,reservation)<quantity: return {"ok":false,"code":"InsufficientGoods"}
	var remaining: int=quantity
	for owner in owners:
		for lot in source.lots:
			if lot.item_id!=item_id or lot.owner!=owner or lot.reserved_by!=reservation: continue
			var used: int=mini(remaining,lot.quantity)
			lot.quantity-=used
			add(state,destination_id,item_id,used,owner if new_owner.is_empty() else new_owner)
			remaining-=used
			if remaining==0: break
		if remaining==0: break
	source.lots=source.lots.filter(func(lot): return lot.quantity>0)
	source.revision+=1
	return {"ok":true}

## Release only this action's reservations after success or interruption.
static func release(state: Dictionary, action_id: String) -> void:
	for wallet in state.wallets.values(): wallet.reserved.erase(action_id)
	for inventory in state.inventories.values():
		var changed: bool=false
		for lot in inventory.lots:
			if lot.reserved_by==action_id:
				lot.reserved_by=""
				changed=true
		if changed: inventory.revision+=1

## Resolve a receiver's real container without creating a scene-owned copy of its assets.
static func receiver_inventory(state: Dictionary, owner: Dictionary) -> String:
	return state.actors[owner.id].inventory_id if owner.kind=="actor" else "store:"+owner.id
