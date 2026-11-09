## V0.5短纵切片：同作者目录的呈现查询与无呈现对照；真实原生输入与视觉由独立门验收。
extends SceneTree
const Kernel=preload("res://core/world_kernel.gd")
const F=preload("res://tests/companion_fixture.gd")
const Query=preload("res://ui/scene_projection.gd")
const Scene=preload("res://ui/scene_presentation.gd")
const Art=preload("res://ui/art_catalog.gd")
const Encounters=preload("res://core/encounter_rules.gd")
const Sessions=preload("res://core/combat_session.gd")
const Round=preload("res://core/personal_round.gd")
const Codec=preload("res://core/save_codec.gd")
const Native=preload("res://storage/native_storage.gd")
const Strict=preload("res://core/strict_json.gd")
var checks: int=0
var failures: Array=[]
var commands: Array=[]
var scenes: Array=[]
var profiles: Dictionary={}
var k
var control
var folder: String
var catalog_path: String

## 必须显式传入prepare产物，不能偷偷回退生产目录或运行时修补配置。
func _init() -> void:
	call_deferred("run_checks")

## 每个检查留下具名失败，任何失败均阻止进程通过。
func check(value: bool, label: String) -> bool:
	checks+=1
	if not value: failures.append(label); printerr("FAIL: ",label)
	return value

## 所有证据保留完整JSON，摘要由外部入口对落盘字节绑定。
func write_json(name: String, value: Variant) -> void:
	var file=FileAccess.open(folder.path_join(name),FileAccess.WRITE)
	if not check(file!=null,"证据文件可写 "+name): return
	file.store_string(JSON.stringify(value,"",true,true)); file.close()

## 相同正常命令和request序列分别送入有投影与无投影的核心，完整比较所有字段。
func send(kind: String, params: Dictionary={}) -> Dictionary:
	var left: Dictionary=F.send(k,kind,params)
	var right: Dictionary=F.send(control,kind,params)
	commands.append({"kind":kind,"params":params.duplicate(true),"result":left.duplicate(true)})
	check(left==right and k.snapshot()==control.snapshot(),"完整状态/回执与无呈现同命令相等 "+kind)
	return left

## 唯一时钟通过正式Kernel半日推进，不写tick、年龄、战果或随机流。
func advance() -> bool:
	var left: Dictionary=k.advance_one_tick()
	var right: Dictionary=control.advance_one_tick()
	commands.append({"kind":"advance_one_tick","result":left.duplicate(true)})
	check(left==right and k.snapshot()==control.snapshot(),"完整半日状态与无呈现对照相等")
	return check(left.ok,"正常半日推进")

## 读取作者已登记映射和真实可见人物；不调用Art.configure或修改data/state。
func scene(place: String, kind: String) -> Dictionary:
	var before: Dictionary=k.snapshot()
	var projected: Dictionary=Query.query(before,k.content().data,place,kind,before.player_actor_id)
	if not check(projected.ok,"真实地点投影 "+place+" "+kind): return {}
	for person in projected.people:
		person.render_descriptor=Art.character(person.actor_id,before,k.content().data,"half_body",{"resolution":[64,80]})
	var descriptor: Dictionary=Scene.resolve(place,kind,before.player_actor_id,projected,Art.catalog())
	check(descriptor.ok,"正常唯一清单解析场景 "+place+" "+kind)
	check(k.snapshot()==before and k.snapshot()==control.snapshot(),"场景与人物解析不改变完整世界")
	scenes.append(descriptor)
	return projected

## 真实道路未完则推进，普通会面道别；仅在正式可战接触处停下。
func travel(stop_at_conflict: bool=false) -> Dictionary:
	for step in range(24):
		var state: Dictionary=k.snapshot()
		var contact: Dictionary=Encounters.pending_for(state,state.player_actor_id)
		if not contact.is_empty():
			if stop_at_conflict and k.preview("encounter_fight",{"encounter_id":contact.id}).ok: return contact
			if not check(send("encounter_leave",{"encounter_id":contact.id}).ok,"正常结束路遇"): return {}
		if k.snapshot().actors[state.player_actor_id].location.kind=="Settlement": return {}
		if not advance(): return {}
	check(false,"短样板行程在24半日内结束")
	return {}

## 将战斗关键中间态写入真实NativeStorage槽，供后续独立Godot进程继续读取。
func save_checkpoint() -> void:
	var state: Dictionary=k.snapshot()
	var store=Native.new(folder.path_join("saves"),k.content())
	var result: Dictionary=store.put("manual-1",Codec.encode(state,1,"manual-1",k.content()),0)
	check(result.ok,"NativeStorage实际写入关键战斗中间态")
	write_json("checkpoint.json",{"writer_pid":OS.get_process_id(),"state":state,"profiles":profiles})

## 独立进程由NativeStorage读取，同目录同引擎严格恢复完整状态与全部人物档案。
func resume() -> void:
	var checkpoint: Dictionary=Strict.parse_text(FileAccess.get_file_as_string(folder.path_join("checkpoint.json"))).value
	check(checkpoint.writer_pid!=OS.get_process_id(),"恢复确实由另一进程执行")
	var store=Native.new(folder.path_join("saves"),k.content())
	var stored: Dictionary=store.get_slot("manual-1")
	if check(stored.ok,"独立进程读取真实NativeStorage槽"):
		var decoded: Dictionary=Codec.decode(stored.text,k.content())
		if check(decoded.ok,"独立进程按同内容目录解码"):
			check(k.restore(decoded.state).ok,"完整恢复关键中间态")
			check(k.snapshot()==checkpoint.state,"跨进程完整世界逐字段相同")
			for actor in checkpoint.profiles:
				check(k.snapshot().actors[actor].appearance==checkpoint.profiles[actor],"跨进程保存实际人物特征 "+actor)
	write_json("resume.json",{"reader_pid":OS.get_process_id(),"writer_pid":checkpoint.writer_pid,"checks":checks,"failures":failures,"state":k.snapshot()})

## 只覆盖当前Alpha已实现的护卫与庄园，不扩写故事、出生事务、长期矩阵或图形输入。
func run_checks() -> void:
	folder=OS.get_environment("HANMO_SLICE_OUTPUT")
	catalog_path=OS.get_environment("HANMO_SLICE_CATALOG")
	if not check(not folder.is_empty() and not catalog_path.is_empty(),"显式目录和证据输出参数"): finish(); return
	DirAccess.make_dir_recursive_absolute(folder)
	k=Kernel.new(); control=Kernel.new()
	var loaded: Dictionary=k.new_game("scenario.alpha_life",catalog_path)
	var control_loaded: Dictionary=control.new_game("scenario.alpha_life",catalog_path)
	if not check(loaded.ok and control_loaded.ok,"两核心正常读取作者prepare目录 "+str(loaded)+" "+str(control_loaded)): finish(); return
	if OS.get_environment("HANMO_SLICE_MODE")=="resume": resume(); finish(); return
	var initial: Dictionary=k.snapshot()
	write_json("initial.json",initial)
	check(Art.ready_result().ok,"清单由正常res路径加载")
	for actor in ["actor.player","actor.carter","actor.apprentice","actor.shen"]:
		profiles[actor]=initial.actors[actor].appearance.duplicate(true)
		var avatar: Dictionary=Art.character(actor,initial,k.content().data,"avatar")
		var half: Dictionary=Art.character(actor,initial,k.content().data,"half_body")
		var battle: Dictionary=Art.character(actor,initial,k.content().data,"battle")
		check(avatar.ok and half.ok and battle.ok and avatar.actor_id==half.actor_id and half.actor_id==battle.actor_id and avatar.identity_key==battle.identity_key,"同人物三视图身份 "+actor)
		if actor=="actor.apprentice": check(half.proportion=="child","仅结构证明独立child基体，不冒充实际儿童素材")
	scene("west.street","world"); scene("west.street","city")
	check(send("move_local",{"place_id":"west.tavern"}).ok,"正常从城内走入酒肆")
	scene("west.tavern","interior")
	var params: Dictionary={"offer_id":"offer.escort","context":F.context(k,"role.sender")}
	check(send("discover_offer",params).ok and send("accept_offer",params).ok and advance(),"与实际委托人会面受托")
	check(send("join_party",{"context":F.context(k,"role.carter")}).ok,"同一车夫正式同行")
	check(send("move_local",{"place_id":"west.street"}).ok and send("travel",{"destination":"village.east"}).ok,"真实道路护送开始")
	var contact: Dictionary=travel(true)
	if not check(not contact.is_empty(),"既有道路接触进入护卫战"): finish(); return
	if not check(send("encounter_fight",{"encounter_id":contact.id}).ok,"正式护卫个人战"): finish(); return
	var battle: Dictionary=Sessions.for_actor(k.snapshot(),initial.player_actor_id)
	check(battle.units.has("actor.carter") and battle.units["actor.carter"].protected,"同一车夫是真实保护对象")
	save_checkpoint()
	for turn in range(26):
		if battle.phase!="planning": break
		var policy: Dictionary=battle.duplicate(true); policy.controller_id="actor.bandit"
		var plan: Dictionary=Round.make_ai(policy,k.content().data)[initial.player_actor_id]; plan.battle_id=battle.id
		if not check(send("battle_plan",plan).ok,"正式战斗轮结算"): break
		battle=Sessions.for_actor(k.snapshot(),initial.player_actor_id)
	if not check(battle.phase=="result" and battle.outcome=="victory","既有护卫战胜利"): finish(); return
	check(send("battle_close",{"battle_id":battle.id}).ok,"正常关闭战果")
	travel()
	check(send("move_local",{"place_id":"east.home"}).ok,"同一保护对象实际送达")
	check(k.snapshot().tasks.values()[0].status=="ready","护送完成尚未清算")
	check(send("move_local",{"place_id":"east.entrance"}).ok and send("travel",{"destination":"city.west"}).ok,"正常返城")
	travel()
	check(send("move_local",{"place_id":"west.tavern"}).ok,"返回原委托人")
	params={"offer_id":"offer.escort","context":F.context(k,"role.sender")}
	check(send("report_offer",params).ok and advance(),"正式一次回报清算")
	check(k.snapshot().tasks.values()[0].status=="success" and not send("report_offer",params).ok,"已完成委托不能重复领赏")
	check(send("move_local",{"place_id":"west.street"}).ok and send("move_local",{"place_id":"west.estate"}).ok,"回到同城真实庄园")
	var before: Dictionary=scene("west.estate","interior")
	check(send("estate_build",{"estate_id":"estate.riverside","facility":"granary"}).ok,"正式建设而非修改图层状态")
	for step in range(4):
		if k.snapshot().actors[initial.player_actor_id].action_id.is_empty(): break
		advance()
	var after: Dictionary=scene("west.estate","interior")
	var old_level: int=-1; var new_level: int=-1
	for layer in before.get("layers",{}).get("buildings",[]):
		if layer.target_ref=={"kind":"estate","id":"estate.riverside"}: old_level=layer.facilities.granary
	for layer in after.get("layers",{}).get("buildings",[]):
		if layer.target_ref=={"kind":"estate","id":"estate.riverside"}: new_level=layer.facilities.granary
	check(old_level>=0 and new_level==old_level+1 and after.revision>before.revision,"建设前后同对象真实等级和revision更新")
	write_json("rules.json",{"checks":checks,"failures":failures,"process_id":OS.get_process_id(),"commands":commands,"scenes":scenes,"profiles":profiles,"rendered_state":k.snapshot(),"headless_state":control.snapshot()})
	finish()

## 非零断言、明确退出码和完成标记必须同时具备，脚本错误不能误判PASS。
func finish() -> void:
	print("PRESENTATION_SLICE_CHECKS=",checks," FAILURES=",failures.size())
	quit(0 if failures.is_empty() else 1)
