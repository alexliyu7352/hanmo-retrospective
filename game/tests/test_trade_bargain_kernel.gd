## 正式目录交易议价唯一Kernel缺口：一次报价种子、中途v2、真实转钱货后的尾拒回滚及同请求唯一消费。
extends SceneTree
const Kernel=preload("res://core/world_kernel.gd")
const Codec=preload("res://core/save_codec.gd")
const Interaction=preload("res://core/interaction_rules.gd")
const Inventory=preload("res://core/inventory_rules.gd")
## 仅在真正转钱货并写消费后拒绝本候选，不伪造成功回执或替换资产算法。
class ProbeKernel extends "res://core/world_kernel.gd":
	var reject_tail: bool=false
	var quote_id: String=""
	var observed: Dictionary={}
	## 在真实economy validator之后注错，保存观察到的候选供守恒断言。
	func _module_descriptors() -> Array:
		var descriptors: Array=super._module_descriptors()
		for descriptor in descriptors:
			for entry in descriptor.validators:
				if entry.id!="economy.state": continue
				var original: Callable=entry.callback
				entry.callback=func(candidate: Dictionary,data: Dictionary) -> Dictionary:
					var checked: Dictionary=original.call(candidate,data)
					if not checked.ok or not reject_tail: return checked
					if candidate.facts.get(quote_id,{}).get("consumed_action_id","").is_empty(): return checked
					observed=candidate.duplicate(true)
					return {"ok":false,"code":"InjectedBargainTail"}
		return descriptors
var checks: int=0
var failures: int=0

## 单独runner只读取准备器的真实安装索引，不启动界面。
func _init() -> void:
	call_deferred("run")

## 所有失败影响退出状态；准备失败必须在访问缺字段前停止。
func check(value: bool, label: String) -> bool:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)
	return value

## 正式请求使用当前人物序号与revision，不直接写游戏事实或货物。
func command(k, kind: String, params: Dictionary={}) -> Dictionary:
	var state: Dictionary=k.snapshot(); var actor: String=state.player_actor_id
	return {"kind":kind,"params":params,"actor_id":actor,"request_seq":state.request_sequences[actor]+1,"expected_revision":state.revision}

## 两步选择后冷恢复同实例，余下一步完成，再验证实际交易候选和幂等。
func run() -> void:
	var index: Dictionary=JSON.parse_string(FileAccess.get_file_as_string("res://data/generated/installed-content.json"))
	var path: String="res://data/generated/"+index.sets[0].directory+"/catalog.hmcatalog.json"
	var k=ProbeKernel.new(); var created: Dictionary=k.new_game("base::scenario.procurement",path)
	if not check(created.ok,"实际Kernel新锁启动 "+str(created)): finish(k); return
	var before: Dictionary=k.snapshot(); var actor: String=before.player_actor_id
	var shop: Dictionary=k.content().data.shops["base::shop.grain"]
	var context: Dictionary=Interaction.for_role(before,k.content().data,"base::role.merchant")
	var request: Dictionary={"shop_id":shop.id,"item_id":"base::item.grain","quantity":2,"side":"buy","payer_wallet_id":before.actors[actor].wallet_id,"context":context}
	var opened: Dictionary=k.submit(command(k,"trade_bargain_open",{"trade":request,"mode":"manual"}))
	if not check(opened.ok,"真实instant开启报价 "+str(opened)): finish(k); return
	var state: Dictionary=k.snapshot(); var qid: String=opened.details.quote_id
	check(state.rng.minigame!=before.rng.minigame and state.wallets==before.wallets and state.inventories==before.inventories,"只一次抽种、不先收费或转货")
	var rng: Dictionary=state.rng.duplicate(true)
	var reopened: Dictionary=k.submit(command(k,"trade_bargain_open",{"trade":request,"mode":"automatic"}))
	check(reopened.ok and reopened.details.quote_id==qid and k.snapshot().rng==rng,"换模式重开同基准也不能重抽")
	for number in range(3):
		var view: Dictionary=k.trade_bargain(actor,qid)
		if not check(view.ok and view.phase=="active","本人真实题面可读 "+str(view)): finish(k); return
		var chosen: Dictionary=k.submit(command(k,"trade_bargain_choose",{"quote_id":qid,"step_id":view.round_id,"option_id":view.options[0].id}))
		if not check(chosen.ok,"确认当前公开提议 "+str(chosen)): finish(k); return
		check(k.snapshot().rng==rng and k.snapshot().tick==0,"选择不重抽或推进市场时间")
		if number==1:
			var saved: Dictionary=k.save_context(); var text: String=Codec.encode(saved.state,1,"trade-bargain-kernel",saved.catalog,saved.registry)
			var decoded: Dictionary=Codec.decode(text,saved.catalog,saved.registry)
			if not check(decoded.ok and decoded.state==k.snapshot(),"中途v2完整fact/选择校验 "+str(decoded.get("code",""))): finish(k); return
			var restored=ProbeKernel.new(); var loaded: Dictionary=restored.new_game("base::scenario.procurement",path)
			var result: Dictionary=restored.restore(decoded.state) if loaded.ok else loaded
			check(result.ok and restored.snapshot()==k.snapshot(),"另一Kernel恢复同报价/seed")
			k._registry={}; k=restored
	var offer: Dictionary=k.preview("trade",{"bargain_quote_id":qid})
	if not check(offer.ok and offer.discount_bp>=0 and offer.discount_bp<=800,"只有冻结终态取得真实总折扣报价 "+str(offer)): finish(k); return
	before=k.snapshot(); var payer: String=before.actors[actor].wallet_id; var bag: String=before.actors[actor].inventory_id
	var transfer: Dictionary=command(k,"trade",{"bargain_quote_id":qid})
	k.quote_id=qid; k.reject_tail=true
	var denied: Dictionary=k.submit(transfer)
	check(denied.get("code")=="InjectedBargainTail" and not k.observed.is_empty(),"唯一转移/消费后真实validator尾拒 "+str(denied))
	check(k.snapshot()==before,"整世界钱货/种子/序号/事实完全回滚")
	k.reject_tail=false
	var accepted: Dictionary=k.submit(transfer)
	if not check(accepted.ok,"原请求重试实际成交 "+str(accepted)): finish(k); return
	state=k.snapshot()
	check(state.wallets[payer].balance==before.wallets[payer].balance-offer.total and state.wallets[shop.wallet_id].balance==before.wallets[shop.wallet_id].balance+offer.total,"付款双方严格守恒且按冻结总价")
	check(Inventory.count(state.inventories[bag],request.item_id,[{"kind":"actor","id":actor}])==Inventory.count(before.inventories[bag],request.item_id,[{"kind":"actor","id":actor}])+2,"真实库存唯一增加两件")
	check(not state.facts[qid].consumed_action_id.is_empty() and state.rng==rng,"报价恰由本trade消费且不重抽")
	check(k.submit(transfer)==accepted and k.snapshot()==state,"重复成功请求不再次转钱货")
	var completed: Dictionary=k.submit(command(k,"advance"))
	check(completed.ok and k.snapshot().actors[actor].skills.commerce==before.actors[actor].skills.commerce+10,"原一半日trade完成仍只一次商术10XP")
	var saved: Dictionary=k.save_context(); var decoded: Dictionary=Codec.decode(Codec.encode(saved.state,2,"trade-bargain-done",saved.catalog,saved.registry),saved.catalog,saved.registry)
	check(decoded.ok and decoded.state==saved.state,"过期且已消费历史fact仍完整存读")
	finish(k)

## 测试包装闭包结束时解除注册引用，原日志不以资源泄漏冒充成功。
func finish(k) -> void:
	k._registry={}
	print("TRADE_BARGAIN_KERNEL_CHECKS=",checks," FAILURES=",failures)
	quit(0 if failures==0 else 1)
