## M1离线片的旧世界来源与独立答案；不是公共wire，也不消费候选codec或native结果。
extends RefCounted
const Schema=preload("res://core/state_schema.gd")
const Inventory=preload("res://core/inventory_rules.gd")
const Strict=preload("res://core/strict_json.gd")
const Exporter=preload("res://tests/export_native_legacy_projection.gd")

## 原字段直接读取，外层按真实ID稳定排列，participants与批次位置完全保留。
static func old_answers(bundle: Dictionary,actor_ids: Array) -> Array:
	var state: Dictionary=bundle.state; var data: Dictionary=bundle.catalog.data; var result: Array=[]
	var ids: Array=actor_ids.duplicate(); ids.sort()
	var wallets: Array=state.wallets.keys(); wallets.sort(); var inventories: Array=state.inventories.keys(); inventories.sort(); var offices: Array=state.offices.keys(); offices.sort()
	for id in ids:
		var actor: Dictionary=state.actors[id]; var action_id: String=actor.action_id
		var held_money: Array=[]; var held_lots: Array=[]; var held_offices: Array=[]
		if not action_id.is_empty():
			for wallet_id in wallets:
				var wallet: Dictionary=state.wallets[wallet_id]
				if wallet.reserved.has(action_id): held_money.append({"wallet_id":wallet_id,"owner":wallet.owner.duplicate(true),"balance":wallet.balance,"amount":wallet.reserved[action_id],"spendable":Inventory.spendable(wallet)})
			for inventory_id in inventories:
				var inventory: Dictionary=state.inventories[inventory_id]
				for index in range(inventory.lots.size()):
					var lot: Dictionary=inventory.lots[index]
					if lot.reserved_by==action_id: held_lots.append({"inventory_id":inventory_id,"lot_index":index,"lot":lot.duplicate(true),"owner_reservation_count":Inventory.count(inventory,lot.item_id,[lot.owner],action_id)})
		for office_id in offices:
			var office: Dictionary=state.offices[office_id]
			if office.holder_actor_id!=id: continue
			var definition: Dictionary=data.offices[office_id]; var fact: Dictionary=state.facts.get(office.appointment_id,{})
			held_offices.append({"record":office.duplicate(true),"definition":definition.duplicate(true),"appointment_resolved":state.facts.has(office.appointment_id),"appointment_fact":fact.duplicate(true),"organization":data.organizations[definition.organization_id].duplicate(true)})
		var bag: Dictionary=state.inventories[actor.inventory_id]; var counts: Array=[]
		for lot in bag.lots: counts.append({"item_id":lot.item_id,"owner":lot.owner.duplicate(true),"reservation":lot.reserved_by,"count":Inventory.count(bag,lot.item_id,[lot.owner],lot.reserved_by)})
		result.append({"actor_id":id,"location":actor.location.duplicate(true),"action_id":action_id,"action":state.actions.get(action_id,{}).duplicate(true),"private_wallet":state.wallets[actor.wallet_id].duplicate(true),"wallet_spendable":Inventory.spendable(state.wallets[actor.wallet_id]),"private_inventory":bag.duplicate(true),"carried_weight":Inventory.weight(bag,data),"inventory_counts":counts,"action_money_reservations":held_money,"action_lot_reservations":held_lots,"offices":held_offices})
	return result

## 完整原Schema先验；源与oracle分文件输出，registry只保真实模块身份，不序列化Callable。
static func capture(bundle: Dictionary,root: String,label: String,kind: String,actor_ids: Array) -> Dictionary:
	var before: Dictionary=bundle.duplicate(true); var valid: Dictionary=Schema.validate(bundle.state,bundle.catalog,bundle.registry)
	if not valid.ok: return valid
	var modules: Array=[]; var module_ids: Array=bundle.registry.modules.keys(); module_ids.sort()
	for id in module_ids: modules.append({"id":id,"version":bundle.registry.modules[id].version})
	var source_commit: String=Strict.parse_text(FileAccess.get_file_as_string("res://data/native_legacy_export_source.json")).value.source_commit
	var source: Dictionary={"format":"HM-M1-OLD-SOURCE-CAPTURE/1","source_commit":source_commit,"evidence_kind":kind,"label":label,"state":bundle.state,"catalog":bundle.catalog,"rule_modules":modules}
	var encoded: String=JSON.stringify(source,"",true,true)+"\n"
	var roundtrip: Dictionary=Strict.parse_text(encoded)
	if not roundtrip.ok or roundtrip.value!=source: return {"ok":false,"code":"SourceRoundtripMismatch"}
	var answers: Array=old_answers(bundle,actor_ids)
	var oracle: Dictionary={"format":"HM-M1-OLD-ANSWERS/1","status":"source-oracle-before-public-wire-freeze","source_sha256":encoded.sha256_text(),"queries":actor_ids.duplicate(),"answers":answers}
	if bundle!=before: return {"ok":false,"code":"SourceChanged"}
	var path: String=root.path_join(label); DirAccess.make_dir_recursive_absolute(path)
	for row in [["source.json",encoded],["old-oracle.json",JSON.stringify(oracle,"",true,true)+"\n"]]:
		var file=FileAccess.open(path.path_join(row[0]),FileAccess.WRITE)
		if file==null: return {"ok":false,"code":"WriteFailed"}
		file.store_string(row[1]); file.close()
	var native: Dictionary=Exporter.encode_case(bundle,source_commit,actor_ids)
	if not native.ok: return native
	var native_path: String=root.get_base_dir().path_join("exports").path_join(label); DirAccess.make_dir_recursive_absolute(native_path)
	for filename in native.files:
		var file=FileAccess.open(native_path.path_join(filename),FileAccess.WRITE)
		if file==null: return {"ok":false,"code":"WriteFailed"}
		file.store_string(native.files[filename]); file.close()
	if bundle!=before: return {"ok":false,"code":"SourceChanged"}
	return {"ok":true,"id":label,"evidence_kind":kind,"source_sha256":encoded.sha256_text(),"actor_ids":actor_ids,"answers":answers}
