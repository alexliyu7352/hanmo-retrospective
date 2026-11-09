## 在真实旧/新Kernel分别采集同一同行短窗；全快照和真实Codec字节参与比较，不由新实现生成旧期望。
extends SceneTree
const F=preload("res://tests/companion_fixture.gd")
const Codec=preload("res://core/save_codec.gd")
var records: Array=[]
var failed: bool=false

## 等引擎就绪后执行；仅显式参数指定输出目标。
func _init() -> void:
	call_deferred("run_trace")

## 每步保存原返回与完整世界，成功路径不手写快照或忽略RNG/ID。
func send(k, kind: String, params: Dictionary={}, actor: String="") -> Dictionary:
	var result: Dictionary=F.send(k,kind,params,actor)
	records.append({"kind":kind,"result":result,"state":k.snapshot()})
	return result

## 在隔离副本上测试多个同时成立的前提，冻结旧Kernel真实首错且验证只读性。
func guard(k, label: String, state: Dictionary, kind: String, params: Dictionary={}, actor: String="") -> void:
	if actor.is_empty(): actor=state.player_actor_id
	var before: Dictionary=state.duplicate(true)
	var result: Dictionary=k._preview(state,kind,params,actor)
	if before!=state: failed=true
	records.append({"guard":label,"kind":kind,"result":result,"input":before,"after":state})

## 真实邀请、重复拒绝、同行移动、费用、途中拒绝、抵达解除和重启均进入正式入口。
func run_trace() -> void:
	var k=F.fresh()
	var state: Dictionary=k.snapshot()
	if state.is_empty(): quit(1); return
	var actor: String=state.player_actor_id
	send(k,"join_party",{"context":F.context(k)})
	send(k,"move_local",{"place_id":"west.tavern"})
	guard(k,"unknown_actor",k.snapshot(),"join_party",{},"missing")
	for kind in ["join_party","dismiss_companion"]:
		state=k.snapshot(); state.actors[actor].alive=false; state.actors[actor].available=false
		guard(k,"domain_busy_before_actor_unavailable",state,kind)
		state=k.snapshot(); state.actors[actor].action_id="existing_action"; state.actors[actor].available=false
		guard(k,"domain_busy_before_unavailable",state,kind)
		state=k.snapshot(); state.actors[actor].location={"kind":"Following","party_id":"missing"}; state.actors[actor].available=false
		guard(k,"following_before_party",state,kind)
		state=k.snapshot(); state.actors[actor].location={"kind":"InArmy","army_id":"missing"}; state.actors[actor].available=false
		guard(k,"in_army_before_party",state,kind)
		state=k.snapshot(); state.battles.probe={"kind":"personal","phase":"planning","units":{actor:{}}}; state.actors[actor].available=false
		guard(k,"battle_before_party",state,kind)
		state=k.snapshot(); state.armies.probe={"status":"active","member_actor_ids":[actor]}; state.sieges.probe={"phase":"planning","armies":{"probe":0},"withdrawn_army_ids":[]}; state.actors[actor].available=false
		guard(k,"siege_before_party",state,kind)
		state=k.snapshot(); state.encounters.probe={"status":"pending","participants":[actor],"id":"probe"}; state.actors[actor].available=false
		guard(k,"encounter_before_party",state,kind)
	send(k,"join_party",{"context":{}})
	state=k.snapshot(); state.actors["actor.carter"].action_id="busy_provider"
	guard(k,"provider_busy",state,"join_party",{"context":F.context(k)})
	if not send(k,"join_party",{"context":F.context(k)}).ok: failed=true
	send(k,"join_party",{"context":F.context(k)})
	send(k,"dismiss_companion",{"actor_id":actor})
	var saved: String=Codec.encode(k.snapshot(),3,"party-registration",k.content())
	var decoded: Dictionary=Codec.decode(saved,k.content())
	var clone=F.fresh()
	if saved.is_empty() or not decoded.ok or not clone.restore(decoded.state).ok or clone.snapshot()!=k.snapshot(): failed=true
	records.append({"save":saved,"decoded":decoded,"restored":clone.snapshot()})
	send(k,"move_local",{"place_id":"west.street"})
	send(k,"travel",{"destination":"village.east"})
	send(k,"dismiss_companion",{"actor_id":"actor.carter"})
	for _tick in range(2):
		var result: Dictionary=k.advance_one_tick(); records.append({"advance":result,"state":k.snapshot()})
		if not result.ok: failed=true
	if not send(k,"dismiss_companion",{"actor_id":"actor.carter"}).ok: failed=true
	send(k,"dismiss_companion",{"actor_id":"actor.carter"})
	var out: String=""
	for argument in OS.get_cmdline_user_args():
		if argument.begins_with("--trace="): out=argument.trim_prefix("--trace=")
	if out.is_empty(): failed=true
	else:
		var file=FileAccess.open(out,FileAccess.WRITE)
		if file==null: failed=true
		else: file.store_string(JSON.stringify(records,"",true,true)+"\n"); file.close()
	print("PARTY_TRACE_RECORDS=",records.size()," FAILED=",failed)
	quit(1 if failed else 0)
