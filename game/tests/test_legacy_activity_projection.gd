## 内部活动／职务子集：真实任免、多人编组和暂停来源；不声称授权视图或完整NativeProjection。
extends SceneTree
const Kernel=preload("res://core/world_kernel.gd")
const Schema=preload("res://core/state_schema.gd")
const Interaction=preload("res://core/interaction_rules.gd")
const Military=preload("res://core/military_assets.gd")
const LEAF="res://core/backends/legacy_activity_projection.gd"
const A="base::actor.military.a.commander"
const C="base::actor.military.c.commander"
const CAP="base::actor.military.a.cap"
const OFFICE="base::office.military.a"
const ROLE="base::role.military.a"
const MAXIMUM: int=2147483647
const SOURCE_FIELDS=["ruleset_id","content_id","content_version","content_digest","engine_build","rng_backend","scenario_id","tick","revision"]
const ACTION_FIELDS=["id","kind","actor_id","participants","start_tick","remaining_ticks","params"]
const OFFICE_FIELDS=["id","holder_actor_id","enabled","appointment_id"]
var ActivityProjection
var checks: int=0
var failures: int=0
var catalog_path: String=""
var examples: Array=[]
var appointed: Dictionary={}
var resigned: Dictionary={}
var forming: Dictionary={}
var waiting: Dictionary={}

## 引擎就绪后运行，先建立实际来源，缺叶以明确失败结束。
func _init() -> void: call_deferred("run")

## 累计实际断言并让任何失败产生非零退出。
func check(value: bool,label: String) -> bool:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)
	return value

## 只由正式组合目录开局，不给运行世界补任命、资产或人物。
func fresh():
	var kernel=Kernel.new()
	var made: Dictionary=kernel.new_game("base::scenario.march",catalog_path)
	if not check(made.ok,"既有军事作者真实开局："+str(made)): return null
	return kernel

## 每个变化经过真实公共提交与当前版本／序号。
func send(kernel,kind: String,params: Dictionary={},actor: String=A) -> bool:
	var state: Dictionary=kernel.snapshot()
	var result: Dictionary=kernel.submit({"kind":kind,"params":params,"actor_id":actor,"request_seq":state.request_sequences[actor]+1,"expected_revision":state.revision})
	return check(result.ok,"真实"+kind+"："+str(result))

## 当面任命与卸任复用原角色查询，不以静态岗位冒充权限。
func context(kernel) -> Dictionary:
	return Interaction.for_role(kernel.snapshot(),kernel.content().data,ROLE)

## 保存同一次Kernel取得的完整三元组，先由原Schema核验。
func capture(kernel,label: String) -> Dictionary:
	var bundle: Dictionary=kernel.save_context()
	if check(Schema.validate(bundle.state,bundle.catalog,bundle.registry).ok,"真实来源Schema："+label): examples.append({"label":label,"bundle":bundle})
	return bundle

## 任命到卸任再编组、暂停、等待和恢复，只使用已经存在的玩法。
func reachable_sources() -> bool:
	var kernel=fresh()
	if kernel==null: return false
	capture(kernel,"空行动与未任职记录")
	if not send(kernel,"office_accept",{"office_id":OFFICE,"context":context(kernel)}): return false
	var pending: Dictionary=capture(kernel,"任命双方已占用但尚未授职")
	var action_id: String=pending.state.actors[A].action_id
	check(pending.state.actions[action_id].participants==[A,"base::actor.military.a.provider"] and not pending.state.offices[OFFICE].enabled,"任命真实两人占用与尚未生效状态分开")
	if not send(kernel,"advance"): return false
	appointed=capture(kernel,"完成任命和原永久来源")
	var office: Dictionary=appointed.state.offices[OFFICE]
	var fact: Dictionary=appointed.state.facts[office.appointment_id]
	check(office.appointment_id==action_id and not appointed.state.actions.has(action_id) and fact.command=="office_accept" and fact.phase=="completed" and fact.success,"完成的任命action移除，office仍引用真实永久事实")
	check(Military.ready_office(appointed.state,appointed.catalog.data,A,OFFICE).ok,"原权限查询确认当前任职有效")
	if not send(kernel,"office_resign",{"office_id":OFFICE,"context":context(kernel)}): return false
	resigned=capture(kernel,"真实卸任仍保留任职者和任命历史")
	check(not resigned.state.offices[OFFICE].enabled and resigned.state.offices[OFFICE].holder_actor_id==A and resigned.state.offices[OFFICE].appointment_id==action_id,"实际卸任只关闭enabled，不能删旧holder或来源")
	check(Military.ready_office(resigned.state,resigned.catalog.data,A,OFFICE).get("code")=="OfficeAuthorityRequired","反例：非空holder与appointment不是当前办事授权")
	print("REACHABLE_OFFICE_COUNTEREXAMPLE enabled=false holder=",A," appointment=",action_id," ready_office=OfficeAuthorityRequired")
	if not send(kernel,"office_accept",{"office_id":OFFICE,"context":context(kernel)}) or not send(kernel,"advance"): return false
	if not send(kernel,"campaign_accept",{"template_id":"base::order.military.a","context":context(kernel)}): return false
	var order: String=kernel.snapshot().campaigns.keys()[0]
	if not send(kernel,"army_form",{"order_id":order,"unit_ids":["base::unit.military.a","base::unit.military.a.cap"],"food_quantity":300,"money":500}): return false
	forming=capture(kernel,"真实多人编组及原序载荷")
	action_id=forming.state.actors[A].action_id
	var participants: Array=forming.state.actions[action_id].participants
	var sorted: Array=participants.duplicate(); sorted.sort()
	check(participants==[A,CAP] and participants!=sorted,"实际主部→旁部参与者顺序与字典序不同")
	print("REACHABLE_PARTICIPANT_ORDER=",participants)
	if not send(kernel,"advance") or not send(kernel,"travel",{"destination":[700,300]}): return false
	capture(kernel,"军团真实行旅开始")
	if not send(kernel,"pause_travel"): return false
	var paused: Dictionary=capture(kernel,"用户主动暂停")
	action_id=paused.state.actors[A].action_id
	if not send(kernel,"advance",{},C): return false
	var held: Dictionary=capture(kernel,"另一人物推进世界，暂停行旅不推进")
	check(held.state.tick==paused.state.tick+1 and held.state.actions[action_id]==paused.state.actions[action_id] and held.state.actors[A].location==paused.state.actors[A].location,"真实世界tick增加但action的start/remaining/params均保持，不是固定due_tick")
	if not send(kernel,"resume_travel") or not send(kernel,"travel_wait"): return false
	var waited: Dictionary=capture(kernel,"原travel_wait临时hold")
	check(waited.state.tick==held.state.tick+1 and waited.state.actions[action_id].remaining_ticks==held.state.actions[action_id].remaining_ticks and waited.state.actions[action_id].start_tick==held.state.actions[action_id].start_tick,"半日hold继续保原起点与剩余，不重起时钟")
	print("REACHABLE_TIME_COUNTEREXAMPLE start=",waited.state.actions[action_id].start_tick," remaining=",waited.state.actions[action_id].remaining_ticks," world_tick_before=",paused.state.tick," world_tick_after=",waited.state.tick)
	if not send(kernel,"resume_travel") or not send(kernel,"advance"): return false
	capture(kernel,"恢复实际行进")
	var idle=fresh()
	if idle==null or not send(idle,"wait") or not send(idle,"wait",{},C): return false
	waiting=capture(idle,"两个人物各自普通wait包络")
	check(waiting.state.actions.size()==2,"两个独立真实行动并存，不被参与者或kind合并")
	return true

## 逐个原字段对照，外层可排序但参与者和任何params内数组不得重排。
func compare(bundle: Dictionary,label: String) -> Dictionary:
	var before: Dictionary=bundle.duplicate(true)
	var projected: Dictionary=ActivityProjection.project(bundle.state,bundle.catalog,bundle.registry)
	if not check(projected.get("ok",false),"原合法来源可投影："+label+" "+str(projected)): return {}
	var keys: Array=projected.keys(); keys.sort()
	check(keys==["actions","offices","ok","scope","source","version"] and projected.scope=="legacy_activity_readonly" and projected.version==1,"仅明确内部活动范围及版本，无授权／新到期／世界身份字段")
	var source: Dictionary={}
	for key in SOURCE_FIELDS: source[key]=bundle.state[key]
	check(projected.source==source,"仅原九项来源标量")
	for pair in [["actions",ACTION_FIELDS],["offices",OFFICE_FIELDS]]:
		var ids: Array=bundle.state[pair[0]].keys(); ids.sort(); var actual: Array=[]
		for row in projected[pair[0]]: actual.append(row.id)
		check(actual==ids,"外层ID完整排序："+pair[0]+" "+label)
		for row in projected[pair[0]]:
			var expected: Dictionary={}
			for key in pair[1]: expected[key]=bundle.state[pair[0]][row.id][key]
			check(row==expected,"精确原字段／原序／完整params："+row.id+" "+label)
	check(bundle==before and ActivityProjection.project(bundle.state,bundle.catalog,bundle.registry)==projected,"两读稳定且整份三元组、RNG、占用、ID、余额不变："+label)
	return projected

## 纯Schema可接受边界单独标记，不能冒称是玩家真实取得的任命或时间。
func accepted_boundaries() -> void:
	var extended: Dictionary=waiting.duplicate(true)
	var action_id: String=extended.state.actors[A].action_id
	extended.state.actions[action_id].start_tick=MAXIMUM
	extended.state.actions[action_id].params={"legacy_note":{"path":["second","first"],"nested":[{"count":MAXIMUM}]}}
	extended.state.actions[action_id].extra_envelope="outside-declared-subset"
	extended.state.offices[OFFICE].extra_office={"ignored":true}
	check(Schema.validate(extended.state,extended.catalog,extended.registry).ok,"纯边界：wait未来start整数上界与开放载荷沿原Schema，不补通用start<=tick")
	var projected: Dictionary=compare(extended,"纯输入开放字段及整数上界")
	if projected.is_empty(): return
	var before: Dictionary=extended.duplicate(true)
	projected.actions[0].participants.append("mutated")
	projected.actions[0].params.legacy_note.path.reverse()
	projected.actions[0].params.legacy_note.nested[0].count=0
	projected.actions[0].start_tick=0; projected.offices[0].enabled=true
	projected.source.tick=99; projected.offices.clear()
	check(extended==before,"修改深层params、参与者、职务数组和来源均不反写原输入")
	check(ActivityProjection.project(extended.state,extended.catalog,extended.registry).actions[0].params==before.state.actions[action_id].params,"下次读取不受旧输出嵌套修改污染")
	var legacy: Dictionary=resigned.duplicate(true)
	legacy.state.offices[OFFICE].appointment_id="legacy.missing_appointment"
	check(Schema.validate(legacy.state,legacy.catalog,legacy.registry).ok,"纯旧形状：disabled只要求holder/source同时非空，不能新增任命来源要求")
	compare(legacy,"原Schema认可的disabled旧来源引用")
	var reversed: Dictionary=forming.duplicate(true); var reordered: Dictionary={}
	var ids: Array=reversed.state.offices.keys(); ids.reverse()
	for id in ids: reordered[id]=reversed.state.offices[id]
	reversed.state.offices=reordered
	check(ActivityProjection.project(reversed.state,reversed.catalog,reversed.registry)==ActivityProjection.project(forming.state,forming.catalog,forming.registry),"仅外层插入次序不同不改变稳定投影")
	reversed=waiting.duplicate(true); reordered={}; ids=reversed.state.actions.keys(); ids.reverse()
	for id in ids: reordered[id]=reversed.state.actions[id]
	reversed.state.actions=reordered
	check(ActivityProjection.project(reversed.state,reversed.catalog,reversed.registry)==ActivityProjection.project(waiting.state,waiting.catalog,waiting.registry),"两个真实action的外层插入次序可变，输出仍按ID稳定排序")

## 所有坏输入均比较原Schema完整失败，不能提前清洗或抛弃某种行动以通过。
func expect_failure(bundle: Dictionary,label: String) -> void:
	var before: Dictionary=bundle.duplicate(true)
	var expected: Dictionary=Schema.validate(bundle.state,bundle.catalog,bundle.registry)
	check(not expected.ok,"原Schema确实拒绝："+label)
	check(ActivityProjection.project(bundle.state,bundle.catalog,bundle.registry)==expected and bundle==before,"原错误完整透传且输入不变："+label)

## 数值、占用、任命来源和整份三元组失败维持原校验先后。
func invalid_boundaries() -> void:
	var action_id: String=waiting.state.actors[A].action_id
	for field in ["start_tick","remaining_ticks"]:
		for value in [true,1.5,"1",-1,MAXIMUM+1]:
			var bad: Dictionary=waiting.duplicate(true); bad.state.actions[action_id][field]=value
			expect_failure(bad,field+"="+str(value))
	var bad: Dictionary=waiting.duplicate(true); bad.state.actions[action_id].remaining_ticks=MAXIMUM
	expect_failure(bad,"wait的领域剩余=1约束不能被通用32位范围代替")
	bad=waiting.duplicate(true); bad.state.actions[action_id].remaining_ticks=0; expect_failure(bad,"未完成行动不得零剩余")
	bad=waiting.duplicate(true); bad.state.actions[action_id].participants.append(A); expect_failure(bad,"重复参与者")
	bad=waiting.duplicate(true); bad.state.actors[A].action_id=""; expect_failure(bad,"参与者反向占用断开")
	bad=waiting.duplicate(true); bad.state.actions[action_id].kind="unknown_activity"; expect_failure(bad,"未注册kind")
	bad=appointed.duplicate(true); bad.state.offices[OFFICE].appointment_id="missing"; expect_failure(bad,"enabled任命缺真实成功来源")
	bad=resigned.duplicate(true); bad.state.offices[OFFICE].holder_actor_id=""; expect_failure(bad,"disabled任职者和来源空值不配对")
	bad=waiting.duplicate(true); bad.registry={}; expect_failure(bad,"缺完整受信注册")
	bad=waiting.duplicate(true); bad.catalog={}; expect_failure(bad,"缺同一内容目录")
	bad=waiting.duplicate(true); bad.state={}; expect_failure(bad,"缺完整运行世界")

## 正式准备身份缺失就停，先证真实来源再检查新叶及其有限复制合同。
func run() -> void:
	if not check(ProjectSettings.get_setting("application/config/legacy_activity_projection_isolated",false),"本片独立准备器标记"): finish(); return
	var index: Dictionary=JSON.parse_string(FileAccess.get_file_as_string("res://data/generated/installed-content.json"))
	var pinned: Dictionary=JSON.parse_string(FileAccess.get_file_as_string("res://data/military-march-fixture.json"))
	var matched: bool=false
	for entry in index.sets:
		if entry.pack_set_digest==pinned.pack_set_digest and entry.directory==pinned.directory: matched=true
	if not check(matched,"原正式stage目录身份匹配"): finish(); return
	catalog_path="res://data/generated/"+pinned.directory+"/catalog.hmcatalog.json"
	if not reachable_sources(): finish(); return
	if not check(FileAccess.file_exists(LEAF),"缺少正式只读活动投影叶"): finish(); return
	ActivityProjection=load(LEAF)
	if not check(ActivityProjection!=null and ActivityProjection.has_method("project"),"新叶接口存在"): finish(); return
	for example in examples: compare(example.bundle,example.label)
	accepted_boundaries(); invalid_boundaries()
	finish()

## 只报告这次局部来源和投影验证，不复用资产／march全门计数。
func finish() -> void:
	print("LEGACY_ACTIVITY_PROJECTION checks=",checks," failures=",failures," scope=legacy_activity_readonly native=NOT_RUN")
	quit(0 if failures==0 else 1)
