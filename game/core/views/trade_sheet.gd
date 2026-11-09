## 既有货单初始化的有限运行数据；仅供原有效交互输入，不授权交易、计算价格或校验外观。
extends RefCounted
const Inventory=preload("res://core/inventory_rules.gd")
const Identity=preload("res://core/actor_identity.gd")

## 沿原商品与任务插入顺序列出选项；可否付款和成交仍由真实trade预览裁决。
static func query(state: Dictionary, data: Dictionary, shop_id: String, provider_actor_id: String, receipt_action_id: String="") -> Dictionary:
	var actor: Dictionary=state.actors[state.player_actor_id]
	var provider: Dictionary=Identity.definition(state,data,provider_actor_id)
	var provider_display_name: String=_name(data,provider)
	var inventory: Dictionary=state.inventories[data.shops[shop_id].inventory_id]
	var items: Array=[]
	for item in data.items.values():
		if item.kind=="commodity":
			items.append({"item_id":item.id,"stock":Inventory.count(inventory,item.id,[inventory.owner])})
	var task_payers: Array=[]
	for task in state.tasks.values():
		if task.assignee==actor.id and task.status in ["active","ready"] and data.offers[task.offer_id].template=="procurement":
			task_payers.append({"offer_id":task.offer_id,"wallet_id":task.budget_wallet_id})
	return {"actor_id":actor.id,"personal_wallet_id":actor.wallet_id,"provider_display_name":provider_display_name,"items":items,"task_payers":task_payers,"receipt_action_pending":state.actions.has(receipt_action_id)}

## 重绘前只沿原raw会话阶段判断缓存寿命，不读取完整对白、行情或命令资格。
static func cache_context(state: Dictionary) -> Dictionary:
	var actor_id: String=state.get("player_actor_id","")
	return {"actor_id":actor_id,"is_trade":state.get("interactions",{}).get(actor_id,{}).get("phase","")=="trade"}

## 成功trade后的原读取点只取当次行动收据显示值；缺源不补历史，完整owner保持字典相等语义。
static func action_receipt(state: Dictionary, action_id: String) -> Dictionary:
	var action: Dictionary=state.actions.get(action_id,{})
	if not action.has("params") or not action.params.has("receipt"): return {}
	var receipt: Dictionary=action.params.receipt; var quote: Dictionary={}
	for key in ["owner","item_id","quantity","total","side"]:
		if receipt.has(key): quote[key]=receipt[key]
	return {"action_id":action.id,"quote":quote.duplicate(true)}

## 姓名取字逐字沿原Main规则；身份缺失的原回退不升级成外观或新错误协议。
static func _name(data: Dictionary, record: Dictionary) -> String:
	return record.get("display_name",data.texts.get(record.get("name_key",record.get("title_key","")),record.get("id","")))
