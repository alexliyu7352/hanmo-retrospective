## 三个原线索命令的注册、旧源码全状态对照、有限可见性及候选失败回滚短门。
extends SceneTree
const Kernel=preload("res://core/world_kernel.gd")
const Builtin=preload("res://core/runtime/builtin_modules.gd")
const Registry=preload("res://core/runtime/module_registry.gd")
const News=preload("res://core/domains/social/news_commands.gd")
const Discovery=preload("res://core/domains/interaction/discovery_commands.gd")
const Knowledge=preload("res://core/domains/tasks/task_knowledge.gd")
const Codec=preload("res://core/save_codec.gd")
const F=preload("res://tests/companion_fixture.gd")
var checks: int=0
var failures: int=0
var comparisons: int=0
var legacy_path: String=""
var legacy=null

## 探针始终先调用实际适配器，之后才注入失败，避免用假返回值证明生产执行。
class Probe extends RefCounted:
	var hits: Dictionary={}
	var failure: String=""
	var changed: bool=false
	## 记录真实注册预览次数，规则与原公共守卫位置不变。
	func preview(state: Dictionary,data: Dictionary,actor: String,kind: String,params: Dictionary) -> Dictionary:
		hits[kind+":preview"]=hits.get(kind+":preview",0)+1
		return News.preview(state,data,actor,kind,params) if kind in News.KINDS else Discovery.preview(state,data,actor,kind,params)
	## 真实写入后失败或破坏尾验，所有状态、RNG、来源和回执都应被外层丢弃。
	func apply(state: Dictionary,data: Dictionary,actor: String,kind: String,params: Dictionary,offer: Dictionary) -> Dictionary:
		hits[kind+":apply"]=hits.get(kind+":apply",0)+1
		var before: Dictionary=state.duplicate(true)
		var result: Dictionary=News.apply(state,data,actor,kind,params,offer) if kind in News.KINDS else Discovery.apply(state,data,actor,kind,params,offer,Knowledge.remember)
		changed=state!=before
		if result.ok and failure=="apply": return {"ok":false,"code":"InjectedKnowledgeApply"}
		if result.ok and failure=="tail": state.actors[actor].fatigue=-1
		return result

## 正式Kernel只更换三个受测Callable，既有观察器和其他领域描述均保留。
class Observed extends Kernel:
	var probe=Probe.new()
	var mutation: String=""
	var selected: String=""
	## 在真实new_game入口验证缺项、重复、错owner、mode和gate均拒绝启动。
	func _module_descriptors() -> Array:
		var rows: Array=super._module_descriptors(); var moved: Dictionary={}
		for row in rows:
			for entry in row.commands.duplicate():
				if not entry.kind in News.KINDS+Discovery.KINDS: continue
				entry.preview=probe.preview; entry.apply=probe.apply
				if entry.kind!=selected: continue
				match mutation:
					"missing": row.commands.erase(entry)
					"duplicate": row.commands.append(entry.duplicate(true))
					"owner": moved=entry; row.commands.erase(entry)
					"mode": entry.mode="action"
					"gate": entry.gate_id="knowledge_early"
					"bridge": entry.compat_action="legacy_begin_action"
		if not moved.is_empty():
			for row in rows:
				if row.id=="tasks": row.commands.append(moved)
		return rows

## 旧源路径必须由外部按固定SHA提取，未提供时明确失败而非假冒对照完成。
func _init() -> void:
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--legacy-kernel="): legacy_path=arg.trim_prefix("--legacy-kernel=")
	call_deferred("run_checks")

## 每条断言有明确范围；任何失败都会影响退出码。
func check(value: bool,label: String) -> bool:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)
	return value

## 使用完整原Alpha内容及固定引擎，不从测试预填社会事实或知识。
func fresh():
	var k=Observed.new()
	check(k.new_game("scenario.alpha_life","res://data/generated/alpha_life.hmcatalog.json").ok,"真实Alpha开局")
	return k

## 每次使用提交世界的版本和新序号，失败后的序号不得提前消费。
func request(k,kind: String,params: Dictionary={}) -> Dictionary:
	var state: Dictionary=k.snapshot(); var actor: String=state.player_actor_id
	return {"kind":kind,"params":params,"actor_id":actor,"request_seq":state.request_sequences[actor]+1,"expected_revision":state.revision}

## 每次实际请求同时交给精确旧Kernel，完整state/result比较包含历史、来源、ID和RNG。
func send(k,kind: String,params: Dictionary={}) -> bool:
	var command: Dictionary=request(k,kind,params); var before: Dictionary=k.snapshot()
	if not check(legacy.restore(before).ok,"旧核恢复真实提交前世界 "+kind): return false
	var expected: Dictionary=legacy.submit(command); var hits: Dictionary=k.probe.hits.duplicate(true)
	var result: Dictionary=k.submit(command); var after: Dictionary=k.snapshot(); var final_hits: Dictionary=k.probe.hits.duplicate(true)
	if not check(result.ok,"正式命令成功 "+kind+" "+str(result)): return false
	check(expected==result and legacy.snapshot()==after,"旧源码完整state/result相同 "+kind); comparisons+=1
	if kind in News.KINDS+Discovery.KINDS:
		check(final_hits.get(kind+":preview",0)==hits.get(kind+":preview",0)+1 and final_hits.get(kind+":apply",0)==hits.get(kind+":apply",0)+1,"真实注册preview/apply各命中一次 "+kind)
	check(k.submit(command)==result and k.snapshot()==after and k.probe.hits==final_hits,"重复请求不二次进入处理器或改变全状态 "+kind)
	return true

## 保存原状态后经实际Codec与完整Schema恢复，知识来源不在测试中剔除。
func roundtrip(k,label: String) -> void:
	var before: Dictionary=k.snapshot(); var decoded: Dictionary=Codec.decode(Codec.encode(before,0,"registered-knowledge",k.content()),k.content())
	check(decoded.ok and k.restore(decoded.state).ok and k.snapshot()==before,"Codec完整往返 "+label)

## 生产元数据与Registry真实来源相符；失败启动不会替换已经可用的世界和注册表。
func contracts() -> bool:
	var registry: Dictionary=Registry.build(Builtin.descriptors())
	if not check(registry.ok,"生产Registry建立"): return false
	for kind in News.KINDS+Discovery.KINDS:
		if not check(registry.commands.has(kind),"注册原命令 "+kind): return false
		var entry: Dictionary=registry.commands[kind]
		check(entry.owner==("social" if kind in News.KINDS else "interaction") and entry.mode=="instant" and entry.gate_id=="legacy_kernel","实际owner、mode、gate保留 "+kind)
		var k=fresh(); var before: Dictionary=k.snapshot(); var content: Dictionary=k.content(); var old_registry: Dictionary=k._registry
		for mutation in ["missing","duplicate","owner","mode","gate","bridge"]:
			k.selected=kind; k.mutation=mutation
			var result: Dictionary=k.new_game()
			var code: String={"missing":"MissingBuiltinCommand","duplicate":"DuplicateCommand","owner":"InvalidBuiltinCommand","mode":"UnknownCommandAction","gate":"InvalidBuiltinCommand","bridge":"InvalidCompatActionDescriptor"}[mutation]
			check(result.get("code")==code and k.snapshot()==before and k.content()==content and k._registry==old_registry,"拒绝错误注册且原世界不变 "+kind+" "+mutation+" "+str(result))
	var owners: Array=[]
	for row in Builtin.descriptors():
		for claim in row.owns:
			if claim.container=="knowledge": owners.append(claim.owner)
	check(owners==["tasks"],"knowledge仍唯一属于tasks")
	check(Builtin.observer_contract_status(registry).ok and not registry.observers.is_empty(),"interaction既有Events observer仍完整")
	check(News.preview({}, {}, "missing","unknown",{}).get("code")=="UnsupportedAction" and News.apply({}, {}, "missing","unknown",{},{}).get("code")=="UnsupportedAction","social未知命令拒绝")
	check(Discovery.preview({}, {}, "missing","unknown",{}).get("code")=="UnsupportedAction" and Discovery.apply({}, {}, "missing","unknown",{},{},Knowledge.remember).get("code")=="UnsupportedAction","interaction未知命令拒绝")
	return true

## 仅用于首错比较的最小坏状态不提交，不把这些构造当作合法世界或真实战斗轨迹。
func guards(k,kind: String,params: Dictionary) -> void:
	var state: Dictionary=k.snapshot(); var actor: String=state.player_actor_id
	for variant in ["original","unavailable","busy","following","army","battle","siege","encounter","no_capability","no_capability_busy"]:
		var candidate: Dictionary=state.duplicate(true); var data: Dictionary=k._catalog.data.duplicate(true)
		match variant:
			"unavailable": candidate.actors[actor].available=false
			"busy","no_capability_busy": candidate.actors[actor].action_id="missing"
			"following": candidate.actors[actor].location={"kind":"Following","party_id":"missing"}
			"army": candidate.actors[actor].location={"kind":"InArmy","army_id":"missing"}
			"battle": candidate.battles={"probe":{"phase":"result","kind":"personal","units":{actor:{}}}}
			"siege":
				candidate.armies={"probe":{"status":"active","member_actor_ids":[actor]}}
				candidate.sieges={"probe":{"phase":"active","armies":{"probe":{}},"withdrawn_army_ids":[]}}
			"encounter": candidate.encounters={"probe":{"id":"probe","status":"pending","participants":[actor]}}
		if variant in ["no_capability","no_capability_busy"]: data.required_capabilities.erase("npc_society")
		var real_data: Dictionary=k._catalog.data; var old_data: Dictionary=legacy._catalog.data
		k._catalog.data=data; legacy._catalog.data=data.duplicate(true)
		var before: Dictionary=candidate.duplicate(true)
		var result: Dictionary=k._preview(candidate,kind,params,actor); var expected: Dictionary=legacy._preview(candidate,kind,params,actor)
		check(result==expected and candidate==before,"旧公共首错与预览只读 "+kind+" "+variant)
		var code: String={"unavailable":"ActorUnavailable","busy":"ActorBusy","no_capability_busy":"ActorBusy","following":"NotPartyLeader","army":"NotArmyCommander","battle":"BattlePending","siege":"SiegePending","encounter":"EncounterPending"}.get(variant,"")
		if not code.is_empty(): check(result.get("code")==code,"明确原首错 "+kind+" "+variant)
		k._catalog.data=real_data; legacy._catalog.data=old_data
	check(k._preview(state,kind,params,"missing").get("code")=="UnknownActor","未知人物首错 "+kind)

## 原preview拒绝时submit也须拒绝，完整世界和请求序号不变。
func refused(k,kind: String,params: Dictionary,code: String) -> void:
	var before: Dictionary=k.snapshot(); var command: Dictionary=request(k,kind,params)
	check(legacy.restore(before).ok,"失败对照旧核恢复")
	var result: Dictionary=k.submit(command); var expected: Dictionary=legacy.submit(command)
	check(result==expected and result.get("code")==code and k.snapshot()==before and legacy.snapshot()==before,"真实拒绝全状态不变 "+kind+" "+code); comparisons+=1

## 写入已真实发生后才失败，知识、消息、关系、回执与ID均不得泄漏到权威状态。
func rollback(k,kind: String,params: Dictionary) -> void:
	var before: Dictionary=k.snapshot()
	for failure in ["apply","tail"]:
		k.probe.failure=failure; k.probe.changed=false
		var result: Dictionary=k.submit(request(k,kind,params))
		check(not result.ok and k.snapshot()==before and k.probe.changed,"真实写入后整候选回滚 "+kind+" "+failure)
	k.probe.failure=""

## 道具与人物两个来源都保留第一次来源，不发任务预算或添加全知字段。
func discovery_paths() -> void:
	var k=fresh()
	refused(k,"inspect",{"prop_id":17},"UnknownProp"); refused(k,"inspect",{"prop_id":"missing"},"UnknownProp")
	refused(k,"inspect",{"prop_id":"notice.grain"},"NotAtPlace")
	refused(k,"discover_offer",{},"InvalidOffer"); refused(k,"discover_offer",{"offer_id":"missing"},"UnknownOffer")
	if not send(k,"move_local",{"place_id":"west.office"}): return
	var prop: Dictionary={"prop_id":"notice.grain"}; var actor: String=k.snapshot().player_actor_id
	var person: Dictionary={"offer_id":"offer.grain","context":F.context(k,"role.office")}
	guards(k,"inspect",prop); guards(k,"discover_offer",person)
	rollback(k,"inspect",prop)
	var before: Dictionary=k.snapshot()
	if not send(k,"inspect",prop): return
	check(k.snapshot().knowledge[actor]["offer.grain"]=={"source":{"kind":"prop","id":"notice.grain"},"tick":before.tick} and k.snapshot().wallets==before.wallets and k.snapshot().tasks==before.tasks,"看告示只记录原来源，零时长且不发预算")
	var known: Dictionary=k.snapshot().knowledge.duplicate(true)
	check(send(k,"inspect",prop) and send(k,"discover_offer",person) and k.snapshot().knowledge==known,"再次查看或别人讲述不改第一次来源")
	roundtrip(k,"prop-first")
	var other=fresh(); send(other,"move_local",{"place_id":"west.office"}); rollback(other,"discover_offer",person)
	if not send(other,"discover_offer",person): return
	known=other.snapshot().knowledge.duplicate(true)
	check(known[actor]["offer.grain"].source=={"kind":"actor","id":"actor.chen"} and send(other,"inspect",prop) and other.snapshot().knowledge==known,"人物先告知后读告示仍保留原人物来源")
	roundtrip(other,"actor-first")
	var missing: Dictionary=person.duplicate(true); missing.erase("context")
	refused(other,"discover_offer",missing,"MissingInteractionContext")
	missing=person.duplicate(true); missing.context.provider_actor_id="actor.zheng"
	refused(other,"discover_offer",missing,"PersonAbsent")
	refused(other,"discover_offer",{"offer_id":"offer.letter","prop_id":"notice.grain"},"SourceUnknown")
	var old_data: Dictionary=legacy._catalog.data; var data: Dictionary=k._catalog.data; var prop_known: Dictionary=k.snapshot().knowledge
	k._catalog.data=data.duplicate(true); k._catalog.data.props["notice.grain"].offer_ids.append("offer.letter"); legacy._catalog.data=k._catalog.data.duplicate(true)
	refused(k,"inspect",prop,"SourceUnknown")
	k._catalog.data=data; legacy._catalog.data=old_data
	check(k.snapshot().knowledge==prop_known,"第二条offer失败没有写入任何额外来源")

## 原作者对白自动discover仍走注册回调并完整恢复，没有迁移或新增对白命令。
func automatic_discovery() -> void:
	var k=fresh(); send(k,"move_local",{"place_id":"west.office"}); send(k,"talk",{"role_id":"role.office"})
	roundtrip(k,"dialogue-before-discovery"); send(k,"dialogue_next")
	var hits: int=k.probe.hits.get("discover_offer:apply",0)
	if not send(k,"dialogue_choose",{"option_id":"ask"}): return
	check(k.probe.hits.get("discover_offer:apply",0)==hits+1 and k.snapshot().knowledge[k.snapshot().player_actor_id].has("offer.grain"),"原自动对白节点命中注册discover一次")
	roundtrip(k,"dialogue-after-discovery")

## NPC事实从真实四半日生成，玩家只能当面听到逐条较早见闻，不能远程全知。
func news_paths() -> void:
	var k=fresh(); refused(k,"ask_news",{},"NoConversation")
	for _tick in range(4): send(k,"wait"); send(k,"advance")
	var state: Dictionary=k.snapshot(); var actor: String=state.player_actor_id
	check(not state.social.events.is_empty() and not state.social.memories.has(actor),"真实社会活动存在但远处玩家没有全知")
	if not send(k,"move_local",{"place_id":"west.tavern"}) or not send(k,"talk",{"role_id":"role.host"}): return
	guards(k,"ask_news",{}); rollback(k,"ask_news",{})
	var before: Dictionary=k.snapshot()
	if not send(k,"ask_news"): return
	state=k.snapshot(); var memories: Dictionary=state.social.memories[actor]
	check(memories.size()==1 and memories.values()[0].source_actor_id=="actor.host" and state.tick==before.tick and state.knowledge==before.knowledge,"问讯只转述一条真实旧闻并记在场来源，不写任务知识")
	roundtrip(k,"news-first"); send(k,"dialogue_next")
	check(k.snapshot().interactions[actor].phase=="choice" and k.preview("ask_news").ok,"choice阶段仍允许真实当面问讯")
	for _ask in range(4): send(k,"ask_news")
	before=k.snapshot(); send(k,"ask_news")
	check(k.snapshot().social==before.social and k.snapshot().relations==before.relations,"消息耗尽后重复问讯不刷关系和传播")
	var absent: Dictionary=k.snapshot(); absent.actors["actor.host"].location={"kind":"Settlement","settlement_id":"city.west","place_id":"west.street"}
	check(k.restore(absent).ok,"合法恢复提供者已离场的会话")
	refused(k,"ask_news",{},"PersonAbsent"); k.restore(before)
	var raw: Dictionary=k.snapshot(); raw.interactions[actor].phase="action"
	check(k._preview(raw,"ask_news",{},actor)==legacy._preview(raw,"ask_news",{},actor) and k._preview(raw,"ask_news",{},actor).get("code")=="NoConversation","非say/choice会话不能问讯")
	send(k,"end_interaction"); refused(k,"ask_news",{},"NoConversation"); roundtrip(k,"news-exhausted")

## 只运行这一迁移片与原生产规则短轨迹；精确旧核缺失时不降级成自比较。
func run_checks() -> void:
	if not contracts(): finish(); return
	if not check(not legacy_path.is_empty() and FileAccess.file_exists(legacy_path),"必须提供精确旧源码Kernel"): finish(); return
	legacy=load(legacy_path).new()
	if not check(legacy.new_game("scenario.alpha_life","res://data/generated/alpha_life.hmcatalog.json").ok,"旧Builtin与旧Kernel真实开局"): finish(); return
	for kind in News.KINDS+Discovery.KINDS: check(not legacy._registry.commands.has(kind),"旧核确实保留内联分派而非调用新adapter "+kind)
	discovery_paths(); automatic_discovery(); news_paths(); finish()

## 输出明确局部断言与全状态对照次数，不据此声称A1.7或完整发行完成。
func finish() -> void:
	print("REGISTERED_KNOWLEDGE_CHECKS=",checks," COMPARED_COMMANDS=",comparisons," FAILURES=",failures)
	quit(1 if failures else 0)
