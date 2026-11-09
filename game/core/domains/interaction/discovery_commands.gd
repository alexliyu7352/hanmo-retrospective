## 场所线索命令适配；只验证原交互来源，由注入的任务写入口记录首次获知。
extends RefCounted
const KINDS=["inspect","discover_offer"]
const Interaction=preload("res://core/interaction_rules.gd")

## 注入任务拥有的写入口并登记原两个命令，不声明第二份knowledge所有权。
static func descriptor(remember: Callable) -> Dictionary:
	var commands: Array=[]
	for kind in KINDS:
		commands.append({"kind":kind,"mode":"instant","gate_id":"legacy_kernel","preview":preview,"apply":apply.bind(remember)})
	return {"commands":commands}

## 查看道具仍逐条验证全部offer，不能因第一条可见而泄漏后续线索。
static func preview(state: Dictionary, data: Dictionary, actor_id: String, kind: String, params: Dictionary) -> Dictionary:
	match kind:
		"inspect":
			var prop_id: Variant=params.get("prop_id")
			if not prop_id is String or not data.props.has(prop_id): return {"ok":false,"code":"UnknownProp"}
			var prop: Dictionary=data.props[prop_id]
			if state.actors[actor_id].location.get("place_id")!=prop.place_id: return {"ok":false,"code":"NotAtPlace"}
			for offer_id in prop.offer_ids:
				var discovery: Dictionary=Interaction.discovery(state,data,actor_id,offer_id,params)
				if not discovery.ok: return discovery
			return {"ok":true,"duration":0}
		"discover_offer":
			if not params.get("offer_id") is String: return {"ok":false,"code":"InvalidOffer"}
			return Interaction.discovery(state,data,actor_id,params.offer_id,params)
	return {"ok":false,"code":"UnsupportedAction"}

## 任务拥有的remember由组合根绑定；交互域不反向加载任务或Kernel。
static func apply(state: Dictionary, data: Dictionary, actor_id: String, kind: String, params: Dictionary, offer: Dictionary, remember: Callable) -> Dictionary:
	match kind:
		"inspect":
			for offer_id in data.props[params.prop_id].offer_ids:
				remember.call(state,actor_id,offer_id,{"kind":"prop","id":params.prop_id})
			return {"ok":true}
		"discover_offer":
			remember.call(state,actor_id,params.offer_id,offer.source)
			return {"ok":true}
	return {"ok":false,"code":"UnsupportedAction"}
