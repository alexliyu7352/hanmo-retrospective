## 入战疲劳伤势快照与重伤切磋准入；纯小夹具经真实preview/begin，不准备内容包或改基础属性。
extends SceneTree
const Condition=preload("res://core/domains/personal/personal_condition.gd")
const Personal=preload("res://core/personal_combat.gd")
const Round=preload("res://core/personal_round.gd")
const Fixture=preload("res://tests/test_personal_directional_guard.gd")
var checks: int=0
var failures: int=0

## 逐项汇总源状态和战斗快照的真实边界，失败非零退出。
func check(value: bool, label: String) -> void:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)

## 作者现有武器、场地和切磋条款，不新增能力字段或健康存储。
static func data() -> Dictionary:
	var result: Dictionary=Fixture.data()
	result.required_capabilities=["personal_combat"]
	result.personal_objectives.spar.kind="sparring"
	result.personal_battlefields.yard.spawns=[{"side":0,"position":[250,200],"heading":0},{"side":1,"position":[310,200],"heading":180}]
	result.roles={"role.spar":{"place_id":"place.yard","services":["duel_start"],"sparring":{"battlefield_id":"yard","objective_id":"spar","fee":0,"wallet_id":"wallet.b"}}}
	return result

## 用实物持有和同地岗位身份构造当前合法源状态，不将职业标签当装备或权限。
static func state(fatigue: int=0, injury: int=0) -> Dictionary:
	var result: Dictionary={"actors":{},"inventories":{},"wallets":{},"roles":{"role.spar":{"actor_id":"b","enabled":true}},"battles":{},"interactions":{},"next_instance_id":1,"tick":7}
	for id in ["a","b"]:
		result.actors[id]={"id":id,"alive":true,"available":true,"action_id":"","location":{"kind":"Settlement","place_id":"place.yard"},"hp":100,"fatigue":fatigue if id=="a" else 0,"injury":injury if id=="a" else 0,"attributes":{"martial":101},"skills":{"martial":300},"wallet_id":"wallet."+id,"inventory_id":"bag."+id,"combat":{"weapon_id":"sword","learned":{},"heart_id":""}}
		result.wallets["wallet."+id]={"balance":100,"reserved":{}}
		result.inventories["bag."+id]={"lots":[{"item_id":"sword","quantity":1,"owner":{"kind":"actor","id":id},"reserved_by":""}]}
	return result

## 交互上下文必须匹配真实当值人和当前地点。
static func params() -> Dictionary:
	return {"context":{"place_id":"place.yard","role_id":"role.spar","provider_actor_id":"b"}}

## 分档边界、取较小效率、一次快照、重伤主动限制和遭遇自卫分别验证。
func _initialize() -> void:
	var definitions: Dictionary=data()
	for row in [[0,0,101],[39,0,101],[40,0,90],[69,0,90],[70,0,75],[89,0,75],[90,0,60],[100,0,60],[0,1,90],[0,2,75],[40,2,75],[70,1,75]]:
		var source: Dictionary=state(row[0],row[1])
		var quote: Dictionary=Personal.preview(source,definitions,"a","duel_start",params())
		check(quote.ok,"ordinary injured or tired actor may preview actual sparring")
		var actor_before: Dictionary=source.actors.a.duplicate(true)
		var started: Dictionary=Personal.begin(source,definitions,quote)
		check(started.ok and source.battles.values()[0].units.a.martial==row[2],"snapshot uses exact tier and minimum efficiency "+str(row))
		check(source.actors.a==actor_before,"snapshot does not overwrite base actor or impose new hp cap")
	var source: Dictionary=state()
	var preapproved: Dictionary=Personal.preview(source,definitions,"a","duel_start",params())
	source.actors.a.injury=3
	var untouched: Dictionary=source.duplicate(true)
	check(Personal.begin(source,definitions,preapproved).get("code")=="SevereInjuryCannotSpar" and source==untouched,"stale sparring quote cannot bypass severe injury before fee or instance allocation")
	source=state(100,3); source.actors.a.attributes.martial=1
	check(Condition.effective_martial(source.actors.a)==1,"effective martial retains explicit minimum one")
	source=state(0,3)
	var before: Dictionary=source.duplicate(true)
	check(Personal.preview(source,definitions,"a","duel_start",params()).get("code")=="SevereInjuryCannotSpar","severe injury cannot actively initiate sparring")
	check(source==before,"refused preview does not charge or advance anything")
	var passive: Dictionary={"controller_id":"a","fee":0,"wallet_id":"","battlefield_id":"yard","objective_id":"spar","source":{"kind":"encounter","encounter_id":"encounter.test"},"sides":{"a":0,"b":1},"protected_id":""}
	check(Personal.begin(source,definitions,passive).ok and source.battles.values()[0].units.a.martial==60,"severe injury retains defensive participation with effective snapshot")
	var saved: Dictionary=source.battles.values()[0].duplicate(true)
	var attack: Dictionary=Round.preview_attack(saved,definitions,"a",[250,200],0,"b")
	source.actors.a.fatigue=0; source.actors.a.injury=0
	check(source.battles.values()[0].units.a.martial==60 and Round.preview_attack(source.battles.values()[0],definitions,"a",[250,200],0,"b")==attack,"battle uses fixed entry snapshot rather than live rereading world conditions")
	print("PERSONAL_CONDITION_SNAPSHOT checks=",checks," failures=",failures)
	quit(1 if failures else 0)
