## 按作者约定处理实际雇工与住宿费用，资源和恢复效果均进入同一世界。
extends RefCounted
const Values=preload("res://core/base/value_checks.gd")
const Location=preload("res://core/world_location.gd")
const Interaction=preload("res://core/interaction_rules.gd")
const Factory=preload("res://core/world_factory.gd")
const Inventory=preload("res://core/inventory_rules.gd")

## 只读核验当前服务者、公私资金归属和可用余额，不在报价时收费。
static func preview(state: Dictionary, data: Dictionary, actor_id: String, params: Dictionary) -> Dictionary:
	var activity: Variant=params.get("activity")
	if not activity is String or not activity in ["rest","work"]: return {"ok":false,"code":"UnknownService"}
	var valid: Dictionary=Interaction.validate(state,data,actor_id,params.get("context"),activity)
	if not valid.ok: return valid
	var role: Dictionary=data.roles[valid.context.role_id]
	if not role.get("activities",{}).has(activity): return {"ok":false,"code":"ServiceNotOffered"}
	var terms: Dictionary=role.activities[activity]
	var payer: Dictionary=state.wallets[state.actors[actor_id].wallet_id]
	var provider: Dictionary=state.wallets[terms.wallet_id]
	if payer.id==provider.id: return {"ok":false,"code":"SelfService"}
	var owner: Dictionary={"kind":"organization","id":role.organization_id} if role.organization_id!=null else {"kind":"actor","id":valid.context.provider_actor_id}
	if provider.owner!=owner: return {"ok":false,"code":"ServiceAuthority"}
	if Inventory.spendable(payer)<terms.cost: return {"ok":false,"code":"InsufficientMoney"}
	if Inventory.spendable(provider)<terms.wage: return {"ok":false,"code":"EmployerCannotPay"}
	if provider.balance+terms.cost>2147483647 or payer.balance+terms.wage>2147483647: return {"ok":false,"code":"MoneyOverflow"}
	return {"ok":true,"duration":terms.duration,"cost":terms.cost,"wage":terms.wage,"context":valid.context,"terms":terms.duplicate(true)}

## 在核心接受行动时支付约定费用并预留雇主工资，防止同一笔资金重复承诺。
static func begin(state: Dictionary, action: Dictionary, approved: Dictionary) -> Dictionary:
	var terms: Dictionary=approved.terms
	action.params.terms=terms.duplicate(true)
	var personal: Dictionary=state.wallets[state.actors[action.actor_id].wallet_id]
	var provider: Dictionary=state.wallets[terms.wallet_id]
	personal.balance-=terms.cost; provider.balance+=terms.cost
	provider.reserved[action.id]=terms.wage
	return {"ok":true}

## 完成后转移真实报酬并按条款恢复体力；有效住宿缓解一级伤势，随后由核心释放占用。
static func finish(state: Dictionary, action: Dictionary) -> Dictionary:
	var terms: Dictionary=action.params.terms
	var actor: Dictionary=state.actors[action.actor_id]
	var provider: Dictionary=state.wallets[terms.wallet_id]
	if provider.reserved.get(action.id,-1)!=terms.wage or provider.balance<terms.wage: return {"ok":false,"code":"ServiceEscrowMismatch"}
	provider.balance-=terms.wage; state.wallets[actor.wallet_id].balance+=terms.wage
	actor.hp=clampi(actor.hp+terms.hp_gain,0,100)
	actor.fatigue=clampi(actor.fatigue+terms.fatigue_delta,0,100)
	if action.params.activity=="rest" and terms.hp_gain>0 and actor.has("injury"): actor.injury=maxi(0,actor.injury-1)
	if action.params.activity=="work":
		var provider_id: String=action.params.context.provider_actor_id
		var relationship: Dictionary={}
		for edge in state.relations.values():
			if edge.from_actor==provider_id and edge.to_actor==actor.id: relationship=edge;break
		if relationship.is_empty():
			var id: String=Factory.next_id(state,"relation")
			relationship={"id":id,"from_actor":provider_id,"to_actor":actor.id,"affinity":0,"trust":0}
			state.relations[id]=relationship
		relationship.affinity=mini(100,relationship.affinity+1)
		relationship.trust=mini(100,relationship.trust+1)
	return {"ok":true,"success":true}

## 校验已冻结服务条款和预留，不重新要求当前岗位启用。
static func validate_action(state: Dictionary, data: Dictionary, action: Dictionary) -> Dictionary:
	var params: Dictionary=action.params
	if not Values.fields(params,{"activity":TYPE_STRING,"context":TYPE_DICTIONARY,"terms":TYPE_DICTIONARY}): return {"ok":false,"code":"InvalidServiceAction"}
	var ctx: Dictionary=params.context
	if not Values.fields(ctx,{"role_id":TYPE_STRING,"provider_actor_id":TYPE_STRING,"place_id":TYPE_STRING}) or not data.roles.has(ctx.role_id) or not state.actors.has(ctx.provider_actor_id) or not ctx.provider_actor_id in action.participants: return {"ok":false,"code":"InvalidServiceContext"}
	var role: Dictionary=data.roles[ctx.role_id]
	if ctx.place_id!=role.place_id or state.roles[role.id].actor_id!=ctx.provider_actor_id or Location.resolve(state,ctx.provider_actor_id).get("place_id")!=ctx.place_id or Location.resolve(state,action.actor_id).get("place_id")!=ctx.place_id: return {"ok":false,"code":"InvalidServicePresence"}
	if not role.get("activities",{}).has(params.activity) or params.terms!=role.activities[params.activity]: return {"ok":false,"code":"ServiceTermsMismatch"}
	var terms: Dictionary=params.terms
	if not state.wallets.has(terms.wallet_id) or state.wallets[terms.wallet_id].reserved.get(action.id)!=terms.wage or action.remaining_ticks>terms.duration: return {"ok":false,"code":"ServiceEscrowMismatch"}
	return {"ok":true}
