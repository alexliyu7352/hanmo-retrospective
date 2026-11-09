## 六个战略军令只适配现有规则与真实命令集合；行旅请求由共同协调出口处理。
extends RefCounted
const Campaign=preload("res://core/campaign_rules.gd")

## 公开唯一原领域集合，注册与完整世界覆盖锚不另造命令枚举。
static func command_kinds() -> Array:
	return Campaign.KINDS

## 仅原返程和撤令允许在真实行旅占用前报价，其他命令保持公共守卫顺序。
static func gate_id(kind: String) -> String:
	return "campaign_travel_redirect" if kind in ["campaign_return","campaign_cancel"] else "legacy_kernel"

## 发行描述绑定真实preview/apply；六命令均即时但不直接消费世界半日。
static func descriptor() -> Dictionary:
	var commands: Array=[]
	for kind in Campaign.KINDS:
		commands.append({"kind":kind,"mode":"instant","preview":preview,"apply":apply,"gate_id":gate_id(kind)})
	return {"commands":commands}

## 原特殊路径在行军中先看军令规则；正常空闲路径仍先检查军事能力。
static func preview(state: Dictionary, data: Dictionary, actor_id: String, kind: String, params: Dictionary) -> Dictionary:
	if not kind in Campaign.KINDS: return {"ok":false,"code":"UnsupportedAction"}
	var action_id: String=state.actors.get(actor_id,{}).get("action_id","")
	var redirect: bool=gate_id(kind)=="campaign_travel_redirect" and not action_id.is_empty() and state.actions.get(action_id,{}).get("kind")=="travel"
	if not redirect and not "military" in data.required_capabilities: return {"ok":false,"code":"MilitaryUnavailable"}
	return Campaign.preview(state,data,actor_id,kind,params)

## 在原候选内重验并执行唯一军令规则，保持原details及单数journey_request不变。
static func apply(state: Dictionary, data: Dictionary, actor_id: String, kind: String, params: Dictionary, _offer: Dictionary) -> Dictionary:
	if not kind in Campaign.KINDS: return {"ok":false,"code":"UnsupportedAction"}
	return Campaign.apply(state,data,actor_id,kind,params)
