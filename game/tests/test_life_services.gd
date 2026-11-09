## Validate actual NPC service payments, occupancy, recovery and restart in the production kernel.
extends SceneTree
const Kernel=preload("res://core/world_kernel.gd")
const Interaction=preload("res://core/interaction_rules.gd")
var count: int=0
var failures: int=0
## Schedule assertions after runtime initialization.
func _init() -> void: call_deferred("_run")
## Count explicit failures without engine assertion aborts.
func check(value: bool, text: String) -> void:
	count+=1
	if not value: failures+=1; printerr("FAIL: ",text)
## Submit through the actual versioned command gateway.
func cmd(k, kind: String, params: Dictionary={}) -> Dictionary:
	var s: Dictionary=k.snapshot()
	return k.submit({"kind":kind,"params":params,"actor_id":s.player_actor_id,"expected_revision":s.revision,"request_seq":s.request_sequences[s.player_actor_id]+1})
## Verify the new mechanisms without test-only shortcuts in production.
func _run() -> void:
	var k=Kernel.new()
	check(k.new_game("scenario.lived_city","res://data/generated/lived_city.hmcatalog.json").ok,"content creates world")
	var data: Dictionary=k.content().data
	check(data.roles.has("role.inn"),"inn is authored, not UI-only")
	if not data.roles.has("role.inn"): quit(1); return
	var context: Dictionary=Interaction.for_role(k.snapshot(),data,"role.inn")
	check(not k.preview("service",{"activity":"rest","context":context}).ok,"cannot buy lodging remotely")
	check(cmd(k,"move_local",{"place_id":"west.inn"}).ok,"real inn passage")
	var s: Dictionary=k.snapshot(); var id: String=s.player_actor_id
	s.actors[id].hp=30; s.actors[id].fatigue=65
	check(k.restore(s).ok,"legal injured test snapshot")
	context=Interaction.for_role(k.snapshot(),data,"role.inn")
	var before: Dictionary=k.snapshot()
	var preview: Dictionary=k.preview("service",{"activity":"rest","context":context})
	check(preview.ok and preview.duration==1 and preview.cost==20,"lodging declares time and fee")
	check(k.snapshot()==before,"service preview is pure")
	var request: Dictionary={"kind":"service","params":{"activity":"rest","context":context},"expected_revision":before.revision,"request_seq":before.request_sequences[id]+1}
	var r: Dictionary=k.submit(request)
	check(r.ok,"start actual lodging")
	check(k.submit(request)==r,"repeat submit returns same result")
	var running: Dictionary=k.snapshot()
	check(running.actors[id].hp==30 and running.wallets[before.actors[id].wallet_id].balance==before.wallets[before.actors[id].wallet_id].balance-20,"cost at start, recovery not yet granted")
	check(not k.preview("talk",{"role_id":"role.inn"}).ok,"occupied actor cannot begin another service")
	var other=Kernel.new(); check(other.new_game("scenario.lived_city","res://data/generated/lived_city.hmcatalog.json").ok and other.restore(running).ok,"restore pending lodging")
	check(other.advance_one_tick().ok,"finish after restore")
	s=other.snapshot()
	check(s.actors[id].hp==65 and s.actors[id].fatigue==35 and s.tick==1,"bounded recovery exactly once")
	check(s.wallets[before.actors[id].wallet_id].balance==before.wallets[before.actors[id].wallet_id].balance-20,"restore does not charge twice")
	check(cmd(other,"move_local",{"place_id":"west.street"}).ok,"leave inn")
	check(cmd(other,"move_local",{"place_id":"west.tavern"}).ok,"enter tavern")
	context=Interaction.for_role(other.snapshot(),data,"role.host")
	before=other.snapshot()
	check(cmd(other,"service",{"activity":"work","context":context}).ok,"host offers actual half-day work")
	check(other.advance_one_tick().ok,"work completion")
	s=other.snapshot()
	check(s.wallets[s.actors[id].wallet_id].balance==before.wallets[before.actors[id].wallet_id].balance+80,"wage credited once")
	check(s.wallets["wallet.host"].balance==before.wallets["wallet.host"].balance-80,"wage paid by real employer")
	var remembered: bool=false
	for edge in s.relations.values():
		if edge.from_actor=="actor.host" and edge.to_actor==id and edge.trust==1: remembered=true
	check(remembered,"completed work is a real social memory")
	s.actors["actor.host"].available=false
	check(other.restore(s).ok,"host becomes unavailable")
	check(not other.preview("service",{"activity":"work","context":context}).ok,"absent host cannot employ")
	s.actors["actor.host"].available=true; s.wallets["wallet.host"].balance=0
	check(other.restore(s).ok and not other.preview("service",{"activity":"work","context":context}).ok,"employer cannot mint wages")
	print("LIFE_SERVICE_CHECKS=",count," FAILURES=",failures)
	quit(0 if failures==0 else 1)
