## A0.4策略测试仅用正常命令构造任务，失效场景通过隔离快照显式恢复。
extends RefCounted
const Kernel=preload("res://core/world_kernel.gd")
const F=preload("res://tests/companion_fixture.gd")

## 使用既有真实作者包建立独立世界，不补造发奖或完成事实。
static func fresh(alpha: bool=false):
	var k=Kernel.new()
	var result: Dictionary=k.new_game("scenario.alpha_life" if alpha else "scenario.two_stories","res://data/generated/alpha_life.hmcatalog.json" if alpha else "res://data/generated/two_stories.hmcatalog.json")
	assert(result.ok,str(result))
	return k

## 真实入场令寄信事件入队，重复读取不负责创建事件。
static func queued():
	var k=fresh(); assert(F.send(k,"move_local",{"place_id":"west.tavern"}).ok)
	return k

## 当前作者的任务参数始终取本局岗位当前人选。
static func params(k, offer: String="offer.grain", role: String="role.office") -> Dictionary:
	return {"offer_id":offer,"context":F.context(k,role)}

## 正常到官署阅读告示，再开始尚未完成的预算受领。
static func accepting():
	var k=fresh()
	assert(F.send(k,"move_local",{"place_id":"west.office"}).ok)
	assert(F.send(k,"inspect",{"prop_id":"notice.grain"}).ok)
	assert(F.send(k,"accept_offer",params(k)).ok)
	return k

## 在隔离快照中换成现存的正式岗位继任者，不伪造岗位、组织或新人物。
static func replace_officer(k) -> Dictionary:
	var s: Dictionary=k.snapshot()
	s.actors["actor.apprentice"].location=s.actors["actor.chen"].location.duplicate(true)
	s.roles["role.office"].actor_id="actor.apprentice"
	assert(k.restore(s).ok)
	return params(k)

## 用合同预算真实购买粮食，保留任务归属，然后返回官署。
static func grain_ready():
	var k=accepting(); assert(k.advance_one_tick().ok)
	var task: Dictionary=k.snapshot().tasks.values()[0]
	assert(F.send(k,"move_local",{"place_id":"west.street"}).ok)
	assert(F.send(k,"move_local",{"place_id":"west.grain"}).ok)
	assert(F.send(k,"trade",{"shop_id":"shop.grain","item_id":"item.grain","side":"buy","quantity":10,"payer_wallet_id":task.budget_wallet_id,"context":F.context(k,"role.merchant")}).ok)
	assert(k.advance_one_tick().ok)
	assert(F.send(k,"move_local",{"place_id":"west.street"}).ok)
	assert(F.send(k,"move_local",{"place_id":"west.office"}).ok)
	return k

## 各钱包余额总和用于证明中断和重办都不增发资金。
static func money(s: Dictionary) -> int:
	var total: int=0
	for wallet in s.wallets.values(): total+=wallet.balance
	return total

## 返回严格的下一条幂等命令；重复使用时不得改变任何世界字段。
static func command(k, kind: String, values: Dictionary={}) -> Dictionary:
	var s: Dictionary=k.snapshot(); var actor: String=s.player_actor_id
	return {"kind":kind,"params":values,"actor_id":actor,"request_seq":s.request_sequences[actor]+1,"expected_revision":s.revision}
