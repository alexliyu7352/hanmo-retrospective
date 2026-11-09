## 住宿与雇工共用原service命令适配；收费、工资预留与完工均留在既有领域规则。
extends RefCounted
const Services=preload("res://core/service_rules.gd")
const KINDS=["service"]

## 唯一服务命令集合不包括另有来源集合的长期休整。
static func command_kinds() -> Array:
	return KINDS.duplicate()

## 服务登记明确旧行动桥；真实提供者参与者与支付时点仍由Kernel旧开工维持。
static func descriptor() -> Dictionary:
	return {"commands":[{"kind":"service","mode":"action","preview":preview,"apply":null,"compat_action":"legacy_begin_action","gate_id":"legacy_kernel"}]}

## 服务报价严格委托原校验，不复制余额、条款或提供者身份规则。
static func preview(state: Dictionary, data: Dictionary, actor_id: String, kind: String, params: Dictionary) -> Dictionary:
	if kind!="service": return {"ok":false,"code":"UnsupportedAction"}
	return Services.preview(state,data,actor_id,params)

## 服务必须具备唯一旧行动桥及同owner载荷校验；长期休整由其独立合同验证。
static func command_contract_status(registry: Dictionary) -> Dictionary:
	var commands: Dictionary=registry.get("commands",{})
	if not commands.has("service"): return {"ok":false,"code":"MissingBuiltinCommand","detail":"service"}
	var entry: Dictionary=commands.service
	if entry.get("owner")!="services" or entry.get("mode")!="action" or entry.get("gate_id")!="legacy_kernel" or entry.get("compat_action")!="legacy_begin_action" or entry.get("apply")!=null: return {"ok":false,"code":"InvalidBuiltinCommand","detail":"service"}
	if registry.get("legacy_action_validators",{}).get("service",{}).get("owner")!="services": return {"ok":false,"code":"MissingBuiltinAction","detail":"service"}
	return {"ok":true}
