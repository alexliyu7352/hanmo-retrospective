## 通过正式内容和世界命令检验地形行旅与庄园的共同时间、费用和存档，不替代实际界面验收。
extends SceneTree
const Kernel=preload("res://core/world_kernel.gd")
const F=preload("res://tests/companion_fixture.gd")
const Codec=preload("res://core/save_codec.gd")
const Encounters=preload("res://core/encounter_rules.gd")
const Inventory=preload("res://core/inventory_rules.gd")
var checks: int=0
var failures: int=0

## 等正式目录加载后执行命令，测试目录由外部独立存档配置隔离。
func _init() -> void:
	call_deferred("run_checks")

## 记录具体失败条件，任一失败保持非零退出状态。
func check(value: bool, label: String) -> void:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)

## 从原工程的集成作者包创建旅人；不改原始内容或传送人物。
func fresh():
	var k=Kernel.new()
	var loaded: Dictionary=k.new_game("scenario.alpha_life","res://data/generated/alpha_life.hmcatalog.json")
	check(loaded.ok,"集成地形与庄园世界正常建立："+str(loaded))
	return k

## 遇到正常旅人先道别，直到原计划实际完成；保护原世界推进与接触逻辑。
func arrive(k) -> bool:
	for _step in range(30):
		var state: Dictionary=k.snapshot()
		var actor: Dictionary=state.actors[state.player_actor_id]
		if actor.action_id.is_empty(): return true
		var meeting: Dictionary=Encounters.pending_for(state,actor.id)
		if not meeting.is_empty() and not F.send(k,"encounter_leave",{"encounter_id":meeting.id}).ok: return false
		var advanced: Dictionary=k.advance_one_tick()
		if not advanced.ok: printerr("JOURNEY_ADVANCE: ",advanced); return false
	return false

## 地形预览、自由落点和途中折返不得改变已消费的半日；庄园在同一时钟继续真实产粮。
func run_checks() -> void:
	var k=fresh()
	if failures: finish(); return
	var initial: Dictionary=k.snapshot()
	check(initial.actors[initial.player_actor_id].wallet_id=="wallet.player" and initial.wallets["wallet.estate"].balance==600,"私人钱柜与随身钱包是独立资产")
	var quote: Dictionary=k.preview("travel",{"destination":"village.east"})
	check(quote.ok and quote.get("terrain",false) and quote.points.size()>2 and quote.duration>=2,"真实河流使路径经渡口转折，费用按地形计算："+str(quote))
	check(k.snapshot()==initial and k.preview("travel",{"destination":[500,430]}).get("code")=="InvalidDestination","预览只读且不能在普通水面落脚")
	var started: Dictionary=F.send(k,"travel",{"destination":"village.east"})
	check(started.ok and k.snapshot().tick==0,"确认行程仅建立来源，启程不暗扣半日："+str(started))
	if not started.ok: finish(); return
	var progressed: Dictionary=k.advance_one_tick()
	check(progressed.ok,"实际地形前进一个半日："+str(progressed))
	if not progressed.ok: finish(); return
	var current: Dictionary=k.snapshot()
	check(current.actors[initial.player_actor_id].location.kind=="Wilderness" and current.actors[initial.player_actor_id].location.point!=[190,430],"行者保存实际原野落点，尚未抵达村口")
	check(current.wallets["wallet.player"].balance==initial.wallets["wallet.player"].balance-8 and current.estates["estate.riverside"].last_tick==1,"路费只扣一次，外出庄园同步经营")
	var meeting: Dictionary=Encounters.pending_for(current,current.player_actor_id)
	if not meeting.is_empty(): check(F.send(k,"encounter_leave",{"encounter_id":meeting.id}).ok,"正常道别后才可改道")
	current=k.snapshot()
	var origin: Dictionary=current.actors[current.player_actor_id].location.duplicate(true)
	check(F.send(k,"pause_travel").ok and F.send(k,"travel_wait").ok,"原野停步并度过一个真实半日")
	check(k.snapshot().actors[current.player_actor_id].location==origin and k.snapshot().wallets["wallet.player"].balance==current.wallets["wallet.player"].balance,"停步不移动也不重复收行路钱")
	meeting=Encounters.pending_for(k.snapshot(),current.player_actor_id)
	if not meeting.is_empty():
		check(k.preview("return").get("code")=="EncounterPending","停步时实际有人抵近，必须先处理眼前会面")
		check(F.send(k,"encounter_leave",{"encounter_id":meeting.id}).ok,"原地会面后明确道别，保持原暂停行程")
	var turn: Dictionary=F.send(k,"return")
	check(turn.ok and k.snapshot().actors[current.player_actor_id].location==origin,"折返从当前点重新规划，无瞬移或退款："+str(turn))
	var saved: Dictionary=Codec.decode(Codec.encode(k.snapshot(),0,"terrain-world",k.content()),k.content())
	check(saved.ok and k.restore(saved.state).ok,"途中改道与庄园生产账目一起保存恢复")
	check(arrive(k) and k.snapshot().actors[current.player_actor_id].location.get("place_id")=="west.street","按恢复的剩余路线真实回到城门")
	check(F.send(k,"travel",{"destination":[190,390]}).ok and arrive(k),"可正常抵达原野自由落点")
	current=k.snapshot()
	check(current.actors[current.player_actor_id].location.kind=="Wilderness" and current.actors[current.player_actor_id].action_id.is_empty(),"自由落点完成行动但保留合法原野位置")
	check(F.send(k,"wait").ok and k.advance_one_tick().ok and F.send(k,"travel",{"destination":"city.west"}).ok and arrive(k),"原野驻足后仍能重新启程回城")
	check(F.send(k,"move_local",{"place_id":"west.estate"}).ok,"庄园是城内实际可访问场所")
	current=k.snapshot()
	var stock: int=Inventory.count(current.inventories["inventory.estate"],"item.grain",[{"kind":"actor","id":current.player_actor_id}])
	check(F.send(k,"estate_withdraw",{"estate_id":"estate.riverside","item_id":"item.grain","quantity":10}).ok,"在本人庄园从真实仓库领取私粮")
	check(Inventory.count(k.snapshot().inventories["inventory.estate"],"item.grain",[{"kind":"actor","id":current.player_actor_id}])==stock-10,"领粮减仓储，行囊中的私粮拥有正确来源")
	var before_funding: Dictionary=k.snapshot()
	check(F.send(k,"estate_fund",{"estate_id":"estate.riverside","quantity":100}).ok,"庄主可在庄内将本人私钱划入独立钱柜")
	check(k.snapshot().wallets["wallet.player"].balance==before_funding.wallets["wallet.player"].balance-100 and k.snapshot().wallets["wallet.estate"].balance==700 and k.snapshot().wallets["wallet.office"]==before_funding.wallets["wallet.office"],"存钱仅在本人两处私产间转移，公库未动")
	var building: Dictionary=k.preview("estate_build",{"estate_id":"estate.riverside","facility":"farm"})
	check(building.ok and building.duration>1,"设施需实际钱料与多个半日，不是一键免费升级")
	check(F.send(k,"estate_build",{"estate_id":"estate.riverside","facility":"farm"}).ok,"从实际庄园发起扩田")
	for _step in range(building.get("duration",0)):
		var advanced: Dictionary=k.advance_one_tick()
		check(advanced.ok,"扩田期间生产、工期与世界时间共同推进："+str(advanced))
	check(k.snapshot().estates["estate.riverside"].facilities.farm==2 and k.snapshot().wallets["wallet.estate"].balance==580,"实际完工才升级，费用由私人钱柜支付")
	saved=Codec.decode(Codec.encode(k.snapshot(),1,"terrain-world",k.content()),k.content())
	check(saved.ok and k.restore(saved.state).ok,"经营建设完成后的私产与永久来源可保存恢复")
	finish()

## 汇总本脚本实际断言，不把规则通过等同于完整 Alpha 交付。
func finish() -> void:
	print("WORLD_JOURNEYS_CHECKS=",checks," FAILURES=",failures)
	quit(0 if failures==0 else 1)
