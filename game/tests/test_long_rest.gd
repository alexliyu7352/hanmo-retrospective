## 用第二份合法粮产配置验证休整缺粮、取消、读档和坏日期；所有世界变化仍由正常命令完成。
extends SceneTree
const Kernel=preload("res://core/world_kernel.gd")
const F=preload("res://tests/companion_fixture.gd")
const Strict=preload("res://core/strict_json.gd")
const Schema=preload("res://core/state_schema.gd")
const Codec=preload("res://core/save_codec.gd")
const Rules=preload("res://core/rules_migration.gd")
var checks: int=0
var failures: int=0

## 在测试独立profile中准备作者变体，不改正式作者内容或玩家保存槽。
func _init() -> void:
	call_deferred("run_checks")

## 每个失败给出真实行为与非零退出结果。
func check(value: bool, label: String) -> void:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)

## 合法作者配置把庄园口粮改为指定实物数量、产量归零，以正常模拟触发缺粮。
func fresh(food: int, resident_owner: bool=false, military: bool=true):
	var header: Dictionary=Strict.parse_text(FileAccess.get_file_as_string("res://data/generated/alpha_life.hmcatalog.json")).value
	if not military:
		for table in preload("res://core/military_content.gd").TABLES: header.catalog.erase(table)
		header.catalog.required_capabilities.erase("military")
		for actor in header.catalog.actors.values(): actor.erase("military_plan")
		for role in header.catalog.roles.values():
			for service in preload("res://core/military_content.gd").SERVICES: role.services.erase(service)
	for lot in header.catalog.inventories["inventory.estate"].goods:
		if lot.item_id=="item.grain": lot.quantity=food
	header.catalog.estates["estate.riverside"].output_per_worker=0
	if resident_owner:
		header.catalog.estates["estate.riverside"].resident_actor_ids.append("actor.player")
		header.catalog.actors["actor.player"].initial_place="west.estate"
	header.catalog_sha256=(JSON.stringify(header.catalog,"",true,true)+"\n").sha256_text()
	var path: String="user://long-rest-"+str(food)+".json"
	var file=FileAccess.open(path,FileAccess.WRITE); file.store_string(JSON.stringify(header)); file.close()
	var k=Kernel.new()
	check(k.new_game("scenario.alpha_life",path).ok,"原内容管线允许实际粮产变体")
	check(resident_owner or F.send(k,"move_local",{"place_id":"west.estate"}).ok,"正常回庄安排休整")
	return k

## 休整消费与庄务同日发生，供不起口粮时完成中断并释放占用；命令与存档均不能重复结算。
func run_checks() -> void:
	var k=fresh(3)
	check(F.send(k,"long_rest",{"estate_id":"estate.riverside","duration":180}).ok,"尚有当前口粮时可安排休整，未承诺全年库存")
	var state: Dictionary=k.snapshot(); var id: String=state.actors[state.player_actor_id].action_id
	check(k.advance_one_tick().ok,"庄务与休整经过同一真实半日")
	state=k.snapshot()
	check(state.tick==1 and state.actors[state.player_actor_id].action_id.is_empty() and state.facts[id].stop_reason=="RestFoodShortage" and state.facts[id].consumed==0,"庄中实际吃尽存粮后结束休整，无负库存或虚假供养")
	var before: Dictionary=state.duplicate(true)
	check(not F.send(k,"rest_cancel").ok and k.snapshot()==before,"已结束不能重复取消、返粮或倒退日期")
	check(k.preview("long_rest",{"estate_id":"estate.riverside","duration":1}).get("code")=="RestFoodShortage","没粮不能重新开始休整")
	var saved: Dictionary=Codec.decode(Codec.encode(state,0,"rest-failed",k.content()),k.content())
	check(saved.ok and k.restore(saved.state).ok,"缺粮终局仍保存明确已过日期和来源")
	k=fresh(100)
	check(F.send(k,"long_rest",{"estate_id":"estate.riverside","duration":180}).ok and k.advance_one_tick().ok,"第二份粮食配置实际推进一次")
	state=k.snapshot(); id=state.actors[state.player_actor_id].action_id
	check(state.facts[id].elapsed==1 and state.facts[id].consumed==1,"本人从真实私仓吃一份，居民口粮由独立庄务承担")
	for field in ["elapsed","duration","completed_tick","phase","actor_id"]:
		var bad: Dictionary=state.duplicate(true)
		bad.facts[id][field]=null
		check(not Schema.validate(bad,k.content()).ok,"损坏休整来源字段明确拒绝："+field)
	var advanced: Dictionary=state.duplicate(true); advanced.actions[id].remaining_ticks-=1
	check(not Schema.validate(advanced,k.content()).ok,"不能只删剩余半日冒充已经度过一天")
	for field in ["family","estates"]:
		var bad: Dictionary=state.duplicate(true); bad[field]=false
		check(Schema.validate(bad,k.content()).get("code")=="InvalidWorldShape","坏家庭或庄务集合在解引用之前明确拒绝："+field)
	check(F.send(k,"rest_cancel").ok and k.snapshot().tick==1 and k.snapshot().facts[id].consumed==1,"取消保持已吃粮食和日期，释放本人占用")
	var active: Dictionary=k.snapshot().duplicate(true)
	active.player_actor_id="actor.chen"
	check(not Schema.validate(active,k.content()).ok,"没有合法接续链不能直接改主角ID")
	var old: Dictionary=k.snapshot(); old.ruleset_id=Rules.WORLD; old.erase("family")
	check(Rules.upgrade(old).get("code") in ["AmbiguousLegacyFamily","AmbiguousLegacyMilitary"],"不能把新休整事实伪装成旧规则版本以绕过来源约束")
	k=fresh(100,false,false); old=k.snapshot(); old.ruleset_id=Rules.WORLD; old.erase("family")
	for actor in old.actors.values(): actor.erase("appearance")
	var migrated: Dictionary=Rules.upgrade(old)
	check(migrated.ok and migrated.state.estates==old.estates and migrated.state.social==old.social and migrated.state.tick==old.tick,"上一地形版本迁移保留私庄与社会原日期")
	k=fresh(100,true)
	check(F.send(k,"long_rest",{"estate_id":"estate.riverside","duration":1}).ok and k.advance_one_tick().ok,"庄主同时登记居民时仍可正常休整")
	state=k.snapshot()
	var last: Dictionary=state.facts[state.estates["estate.riverside"].last_production_id]
	var resting: Dictionary={}
	for fact in state.facts.values():
		if fact.get("kind")=="long_rest": resting=fact
	check(resting.get("consumed")==1 and not "actor.player" in last.fed,"居民庄主实际吃一份私粮，不因忙碌被庄务与休整同时漏掉")
	print("LONG_REST_CHECKS=",checks," FAILURES=",failures)
	quit(0 if failures==0 else 1)
