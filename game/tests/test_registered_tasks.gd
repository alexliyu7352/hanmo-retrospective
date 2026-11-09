## 任务注册迁移的公开入口、唯一校验、短存读及真实资产写入后整候选回滚。
extends SceneTree
const RuntimeCommands=preload("res://core/runtime/runtime_commands.gd")
const ServiceCommands=preload("res://core/domains/services/service_commands.gd")
const News=preload("res://core/domains/social/news_commands.gd")
const Discovery=preload("res://core/domains/interaction/discovery_commands.gd")
const Movement=preload("res://core/domains/travel/movement_commands.gd")
const DialogueCommands=preload("res://core/domains/interaction/dialogue_commands.gd")
const Rest=preload("res://core/long_rest.gd")
const Journey=preload("res://core/journey_commands.gd")
const Personal=preload("res://core/personal_combat.gd")
const Campaign=preload("res://core/campaign_rules.gd")
const MilitaryCombat=preload("res://core/military_combat.gd")
const SiegeWorld=preload("res://core/siege_world.gd")
const MilitaryAssets=preload("res://core/military_assets.gd")
const Parties=preload("res://core/party_rules.gd")
const Cultivation=preload("res://core/cultivation_rules.gd")
const Family=preload("res://core/family_rules.gd")
const Estate=preload("res://core/estate_rules.gd")
const Kernel=preload("res://core/world_kernel.gd")
const Registry=preload("res://core/runtime/module_registry.gd")
const Builtin=preload("res://core/runtime/builtin_modules.gd")
const Schema=preload("res://core/state_schema.gd")
const Codec=preload("res://core/save_codec.gd")
const Strict=preload("res://core/strict_json.gd")
const Step=preload("res://core/world_step.gd")
const Inventory=preload("res://core/inventory_rules.gd")
const P=preload("res://tests/policy_fixture.gd")
const F=preload("res://tests/companion_fixture.gd")
const Encounters=preload("res://core/encounter_rules.gd")
const ACTIONS=["accept_offer","deliver_message","report_offer"]
const COMMANDS=["accept_offer","cancel_task","deliver_message","report_offer","trade"]
const ACTION_PATH="res://core/domains/tasks/task_action.gd"
const MODULE_PATH="res://core/domains/tasks/task_module.gd"
const STATE_PATH="res://core/domains/tasks/task_state.gd"
var checks: int=0
var failures: int=0

## 探针只包装真实发行适配器；故障先记录实际资产变化再拒绝候选。
class Probe extends RefCounted:
	var counts: Dictionary={}
	var failure: String=""
	var mutated: bool=false
	var order: Array=[]
	var contexts: Array=[]
	## 在世界外计数，绝不为测试向生产状态插入字段。
	func hit(kind: String, stage: String) -> void:
		var key: String=kind+":"+stage
		counts[key]=counts.get(key,0)+1
	## 报价仍由原领域计算，参数或失败顺序不由探针改写。
	func preview(s: Dictionary, d: Dictionary, actor: String, kind: String, params: Dictionary) -> Dictionary:
		hit(kind,"preview")
		return load(MODULE_PATH).preview(s,d,actor,kind,params)
	## 准备仍为纯读，原报价传递至真实行动适配器。
	func prepare(s: Dictionary, d: Dictionary, actor: String, kind: String, params: Dictionary, offer: Dictionary) -> Dictionary:
		hit(kind,"prepare")
		return load(ACTION_PATH).prepare(s,d,actor,kind,params,offer)
	## 受领任务、钱包或交货预留确已建立后才注入失败。
	func start(s: Dictionary, d: Dictionary, a: Dictionary, offer: Dictionary) -> Dictionary:
		hit(a.kind,"start")
		var before: Dictionary=s.duplicate(true)
		var result: Dictionary=load(ACTION_PATH).start(s,d,a,offer)
		mutated=s!=before
		return {"ok":false,"code":"InjectedStart"} if failure=="start" else result
	## 保存实际字典序及hold/contact上下文，推进由真实适配器执行。
	func step(s: Dictionary, d: Dictionary, a: Dictionary, context: Dictionary) -> Dictionary:
		hit(a.kind,"step"); order.append(a.id); contexts.append(context.duplicate(true))
		return load(ACTION_PATH).step(s,d,a,context)
	## 完成后的真实资产、清算与关系变更先发生，再测试统一事务拒绝。
	func finish(s: Dictionary, d: Dictionary, a: Dictionary, tick: int) -> Dictionary:
		hit(a.kind,"finish")
		var before: Dictionary=s.duplicate(true)
		var result: Dictionary=load(ACTION_PATH).finish(s,d,a,tick)
		mutated=s.wallets!=before.wallets or s.inventories!=before.inventories or s.tasks!=before.tasks
		if failure=="tail": s.actors[a.actor_id].skills.commerce=-1
		return {"ok":false,"code":"InjectedFinish"} if failure=="finish" else result
	## 注册校验必须真正进入领域validator，失败不能被旧分支掩盖。
	func validate(s: Dictionary, d: Dictionary, a: Dictionary) -> Dictionary:
		hit(a.kind,"validate")
		return {"ok":false,"code":"InjectedValidate"} if failure=="validate" else load(ACTION_PATH).validate(s,d,a)
	## 即时取消先实际归还合同资产，不能误用行动中断接口。
	func apply(s: Dictionary, d: Dictionary, actor: String, kind: String, params: Dictionary, offer: Dictionary) -> Dictionary:
		hit(kind,"apply")
		var before: Dictionary=s.duplicate(true)
		var result: Dictionary=load(MODULE_PATH).apply(s,d,actor,kind,params,offer)
		mutated=s.wallets!=before.wallets or s.inventories!=before.inventories
		return {"ok":false,"code":"InjectedCancel"} if failure=="cancel" else result

## 只通过真实组合根替换任务Callable，沿用全部其他发行模块。
class Observed extends Kernel:
	var probe=Probe.new()
	## 各字段使用固定ABI包装，owner及其余metadata原样保留。
	func _module_descriptors() -> Array:
		var rows: Array=Builtin.descriptors()
		for row in rows:
			if row.id!="tasks": continue
			for command in row.commands:
				command.preview=probe.preview
				if command.mode=="instant": command.apply=probe.apply
			for action in row.actions:
				action.prepare=probe.prepare; action.start=probe.start; action.step=probe.step
				action.finish=probe.finish; action.validate=probe.validate
		return rows

## 引擎初始化后开始断言，缺少新文件是业务RED而非脚本解析失败。
func _init() -> void:
	call_deferred("run_checks")

## 保留每项错误标签与真实进程退出码。
func check(value: bool, label: String) -> bool:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)
	return value

## 真实官署发现入口，不提前建任务或手写预算。
func discovered():
	var k=Observed.new(); check(k.new_game().ok,"真实新局")
	check(F.send(k,"move_local",{"place_id":"west.office"}).ok and F.send(k,"inspect",{"prop_id":"notice.grain"}).ok,"从布告发现采购")
	return k

## 恢复已由正常命令构造的世界；注册表通过真实new_game建立。
func observed(source):
	var k=Observed.new()
	check(k.new_game().ok,"探针正式新局")
	check(k.restore(source.snapshot()).ok,"探针恢复真实流程状态")
	return k

## 所有保存点重新建Kernel再恢复，同一后继命令的结果及完整快照须相同。
func roundtrip(k, label: String, next_kind: String="wait", params: Dictionary={}) -> void:
	var before: Dictionary=k.snapshot(); var text: String=Codec.encode(before,7,"registered-tasks-"+label,k.content())
	var decoded: Dictionary=Codec.decode(text,k.content())
	if not check(not text.is_empty() and decoded.ok and decoded.state==before,label+" Codec精确状态"): return
	var restored=Kernel.new(); check(restored.new_game().ok and restored.restore(decoded.state).ok,label+" fresh Kernel恢复")
	check(restored.snapshot()==before,label+" 恢复不支付或分配身份")
	var command: Dictionary=P.command(k,next_kind,params)
	var result: Dictionary=k.submit(command)
	check(restored.submit(command)==result and restored.snapshot()==k.snapshot(),label+" 同命令短窗完全等价")

## 构造有效封装的损坏载荷，以证明解码拒绝来自真实Schema而非坏摘要。
func bad_text(state: Dictionary, k) -> String:
	var header: Dictionary=Strict.parse_text(Codec.encode(k.snapshot(),0,"negative",k.content()),Codec.MAX_BYTES).value
	header.payload_json=JSON.stringify(state,"",true,true)
	header.payload_length=header.payload_json.to_utf8_buffer().size(); header.payload_sha256=header.payload_json.sha256_text()
	return JSON.stringify(header,"",true,true)

## 旧Schema与新Schema共同执行的独立错误码向量；坏读档不得更改当前世界。
func reject(k, bad: Dictionary, code: String, label: String) -> void:
	var before: Dictionary=k.snapshot(); var catalog: Dictionary=k.content()
	check(Schema.validate(bad,catalog).get("code")==code,label+" 无注册原码 "+code)
	check(Schema.validate(bad,catalog,k._registry).get("code")==code,label+" 显式注册原码 "+code)
	check(Codec.encode(bad,0,"bad",catalog).is_empty(),label+" 拒绝编码")
	check(Codec.decode(bad_text(bad,k),catalog).get("code")==code,label+" 有效封装拒绝坏载荷")
	check(k.restore(bad).get("code")==code and k.snapshot()==before,label+" 恢复拒绝全世界不变")

## 在迁移前后均运行精确旧validator向量，不能用新旧共同委托当独立旧行为证据。
func validation_vectors() -> void:
	var k=P.accepting(); var s: Dictionary=k.snapshot(); var id: String=s.actions.keys()[0]; var tid: String=s.tasks.keys()[0]
	for field in ["id","offer_id","assignee","serial","status","budget_wallet_id","accepted_tick","deadline_tick","settlement_id"]:
		var bad: Dictionary=s.duplicate(true); bad.tasks[tid].erase(field)
		reject(k,bad,"InvalidTask","任务必填"+field)
	for field in ["serial","accepted_tick","deadline_tick"]:
		var bad: Dictionary=s.duplicate(true); bad.tasks[tid][field]=-1
		reject(k,bad,"InvalidTaskStatus","任务整数"+field)
	var bad: Dictionary=s.duplicate(true); bad.tasks[tid].status="invented"; bad.actions[id].params={}
	reject(k,bad,"InvalidTaskStatus","任务先于行动")
	bad=s.duplicate(true); bad.tasks[tid].budget_wallet_id="wallet.player"
	reject(k,bad,"TaskBudgetOwnership","预算归属")
	for field in ["offer_id","task_id","context"]:
		bad=s.duplicate(true); bad.actions[id].params.erase(field)
		reject(k,bad,"InvalidTaskAction","行动必填"+field)
	bad=s.duplicate(true); bad.actions[id].params.context={}; bad.actions[id].params.task_id="missing"
	reject(k,bad,"InvalidTaskAction","任务引用先于上下文")
	for field in ["place_id","provider_actor_id","role_id"]:
		bad=s.duplicate(true); bad.actions[id].params.context[field]="missing"
		reject(k,bad,"InvalidActionContext","上下文引用"+field)
	bad=s.duplicate(true); bad.tasks[tid].offer_id="offer.letter"
	reject(k,bad,"ActionTaskMismatch","任务与行动定义不一致")
	bad=s.duplicate(true); bad.wallets["wallet.office"].reserved[id]=1
	reject(k,bad,"AcceptanceEscrowMismatch","受领预留精确预算")
	bad=s.duplicate(true); bad.actions[id].remaining_ticks=0; bad.actions[id].params={}
	reject(k,bad,"InvalidAction","公共envelope先于载荷")
	bad=s.duplicate(true); bad.actions[id].participants.append("actor.player")
	reject(k,bad,"InvalidOccupation","重复参与者")
	bad=s.duplicate(true); bad.actors["actor.chen"].action_id=""
	reject(k,bad,"InvalidOccupation","正向占用")
	bad=s.duplicate(true); bad.actors["actor.shen"].action_id=id
	reject(k,bad,"DanglingOccupation","反向占用")
	bad=s.duplicate(true); bad.tasks[tid].settlement_id="arbitrary historical string"
	check(Schema.validate(bad,k.content()).ok,"settlement只要求String，不新增终态或引用约束")
	bad=s.duplicate(true); bad.roles["role.office"].enabled=false
	check(Schema.validate(bad,k.content()).ok,"保存不重新执行实时岗位权限")
	var r=P.grain_ready(); check(F.send(r,"report_offer",P.params(r)).ok,"真实采购报账预留")
	var rs: Dictionary=r.snapshot(); var rid: String=rs.actions.keys()[0]
	bad=rs.duplicate(true); bad.actions[rid].params.reward=-1
	reject(r,bad,"ReportEscrowMismatch","报酬范围先于货物")
	bad=rs.duplicate(true); bad.wallets["wallet.office"].reserved[rid]=0
	reject(r,bad,"ReportEscrowMismatch","报酬精确预留")
	bad=rs.duplicate(true)
	for lot in bad.inventories["bag:actor.player"].lots:
		if lot.reserved_by==rid: lot.reserved_by=""
	reject(r,bad,"ReportGoodsMismatch","必须本行动预留粮食")

## 原生产精确集合、权属与handler缺失必须关闭启动，不扩大Registry本身的协议。
func registration() -> bool:
	var built: Dictionary=Registry.build(Builtin.descriptors()); var commands: Array=built.commands.keys(); commands.sort()
	var actions: Array=built.actions.keys(); actions.sort()
	var expected_commands: Array=COMMANDS+MilitaryAssets.KINDS+Campaign.KINDS+MilitaryCombat.KINDS+SiegeWorld.KINDS+Parties.KINDS+Cultivation.KINDS+Family.KINDS+Estate.KINDS+Personal.KINDS+Journey.KINDS+Rest.KINDS+Encounters.KINDS+RuntimeCommands.KINDS+ServiceCommands.KINDS+News.KINDS+Discovery.KINDS+Movement.KINDS+DialogueCommands.KINDS; expected_commands.sort()
	var expected_actions: Array=ACTIONS+["trade"]+MilitaryAssets.ACTION_KINDS; expected_actions.sort()
	check(commands==expected_commands,"生产精确注册trade、四任务、军事、同行和修业命令")
	check(actions==expected_actions,"生产精确注册任务、交易和真实军事资产行动")
	check(built.validators.has("tasks.state"),"任务状态validator有真实绑定")
	var ready: bool=true
	for path in [ACTION_PATH,MODULE_PATH,STATE_PATH]:
		ready=check(ResourceLoader.exists(path),"真实领域适配文件 "+path) and ready
	if not ready or commands!=expected_commands: return false
	for kind in ACTIONS:
		var action: Dictionary=built.actions[kind]
		check(action.owner=="tasks" and built.commands[kind].owner=="tasks","任务命令行动同一owner "+kind)
		check(action.cancel==Registry.refuse_cancel and action.resume==Registry.refuse_resume,"canonical不支持中断 "+kind)
		for slot in ["prepare","start","step","finish","validate","cancel","resume"]:
			var descriptors: Array=Builtin.descriptors()
			for row in descriptors:
				if row.id=="tasks":
					for item in row.actions:
						if item.kind==kind: item.erase(slot)
			check(not Registry.build(descriptors).ok,"缺生命周期关闭 "+kind+":"+slot)
	var descriptors: Array=Builtin.descriptors()
	for row in descriptors:
		if row.id=="tasks": row.owns[0].owner="economy"
	check(not Registry.build(descriptors).ok,"非法owner claim关闭")
	descriptors=Builtin.descriptors()
	var displaced: Dictionary={}
	for row in descriptors:
		if row.id=="tasks": displaced=row.commands.pop_front()
	for row in descriptors:
		if row.id=="economy": row.commands.append(displaced)
	check(not Registry.build(descriptors).ok,"命令owner不匹配关闭")
	var s: Dictionary=P.accepting().snapshot(); var bad: Dictionary=s.duplicate(true); bad.tasks.values()[0].status="bad"
	check(built.validators["tasks.state"].callback.call(bad,P.fresh().content().data).get("code")=="InvalidTaskStatus","metadata callback真实执行")
	return true

## 开始、完成、预览和重放都必须真正经过注册回调且只发生一次资金转移。
func acceptance_and_cancel() -> void:
	var k=discovered(); var before: Dictionary=k.snapshot(); var params: Dictionary=P.params(k); params.extra={"nested":["retained"]}
	for _i in range(3): check(k.preview("accept_offer",params).ok,"受领纯预览")
	check(k.snapshot()==before,"连续预览保持完整状态RNG及实例ID")
	var command: Dictionary=P.command(k,"accept_offer",params); var result: Dictionary=k.submit(command)
	if not check(result.ok,"注册受领开始"): return
	var s: Dictionary=k.snapshot(); var action: Dictionary=s.actions[result.details.action_id]; var task: Dictionary=s.tasks.values()[0]
	check(action.id=="action:"+str(before.next_instance_id) and task.id=="task:"+str(before.next_instance_id+1) and task.serial==before.next_instance_id+1,"action先task及serial精确顺序")
	check(action.participants==["actor.player","actor.chen"] and action.params.extra==params.extra,"参与者及完整参数深拷贝")
	check(s.wallets[task.budget_wallet_id].balance==0 and s.wallets["wallet.office"].balance==before.wallets["wallet.office"].balance and s.wallets["wallet.office"].reserved[action.id]==650,"受领开始只预留不拨款")
	check(k.submit(command)==result and k.snapshot()==s,"开始请求幂等先于过时revision")
	check(k._registry.actions.accept_offer.cancel.call(s,k.content().data,action,"user").code=="ActionNotInterruptible" and k.snapshot()==s,"生命周期cancel不是cancel_task")
	roundtrip(k,"accepting","advance")
	var active: Dictionary=k.snapshot()
	check(active.tasks[task.id].status=="active" and active.wallets[task.budget_wallet_id].balance==650,"受领只在完成点拨款")
	check(k.submit(command)==result and k.snapshot()==active,"完成后重放开始请求不再拨款")
	for stage in ["preview","prepare","start","step","finish","validate"]: check(k.probe.counts.get("accept_offer:"+stage,0)>0,"真实受领生命周期 "+stage)
	check(not F.send(k,"accept_offer",P.params(k)).ok,"新请求不得双受领")
	roundtrip(k,"active")
	check(k.advance_one_tick().ok,"存读后wait正常完成")
	before=k.snapshot(); params=P.params(k); params.task_id="untrusted-other-task"
	var cancel: Dictionary=P.command(k,"cancel_task",params); result=k.submit(cancel); s=k.snapshot()
	check(result.ok and s.tick==before.tick and s.actions==before.actions,"即时cancel不创建行动或推进半日")
	check(s.tasks[task.id].status=="cancelled" and s.tasks[task.id].settlement_id=="settlement:"+str(before.next_instance_id),"cancel使用授权offer目标和旧ID顺序")
	check(not s.tasks[task.id].has("reported_tick") and not s.tasks[task.id].has("late") and s.relations==before.relations,"取消不补报账时间或关系")
	check(s.wallets["wallet.office"].balance==before.wallets["wallet.office"].balance+650 and k.probe.counts.get("cancel_task:apply",0)==1,"取消实际退预算且命中instant")
	check(k.submit(cancel)==result and k.snapshot()==s,"即时取消重放不双退")
	roundtrip(k,"cancelled")

## 每一个迁移种类移除绑定后公开入口、恢复及Step均明确拒绝旧成功路线。
func missing_handlers() -> void:
	var sources: Dictionary={"accept_offer":P.accepting(),"report_offer":P.grain_ready(),"deliver_message":message_at_receiver()}
	check(F.send(sources.report_offer,"report_offer",P.params(sources.report_offer)).ok,"缺注册测试真实报账")
	check(F.send(sources.deliver_message,"deliver_message",P.params(sources.deliver_message,"offer.letter","role.recipient")).ok,"缺注册测试真实交信")
	for kind in ACTIONS:
		var k=sources[kind]; var s: Dictionary=k.snapshot(); var registry: Dictionary=k._registry.duplicate(true); registry.actions.erase(kind)
		check(Schema.validate(s,k.content(),registry).get("code")=="InvalidActionKind","显式注册缺失不回退 "+kind)
		check(Schema.validate(s,k.content()).ok,"无注册Codec兼容仍严格接受 "+kind)
		var candidate: Dictionary=s.duplicate(true)
		check(Step.advance(candidate,k.content().data,registry).get("code")=="UnsupportedAction","Step缺失不旧完成 "+kind)
		k._registry=registry
		check(k.restore(s).get("code")=="InvalidActionKind" and k.snapshot()==s,"restore缺失拒绝 "+kind)
	for kind in ["accept_offer","report_offer","deliver_message","cancel_task"]:
		var k=discovered(); k._registry.commands.erase(kind)
		check(k.preview(kind,P.params(k)).get("code")=="UnsupportedAction","公开preview缺命令不回退 "+kind)

## 真实送信出发，到达固定收件人前保留任务所有的唯一信件。
func message_at_receiver():
	var k=P.queued(); var params: Dictionary=P.params(k,"offer.letter","role.sender")
	check(F.send(k,"discover_offer",params).ok and F.send(k,"accept_offer",params).ok and k.advance_one_tick().ok,"真实发现受领私人信件")
	check(F.send(k,"move_local",{"place_id":"west.street"}).ok and F.send(k,"travel",{"destination":"village.east"}).ok,"信件实际出发")
	check(finish_travel(k),"信件实际道路推进")
	check(F.send(k,"move_local",{"place_id":"east.home"}).ok,"当面找固定收件人")
	return k

## 普通接触正常道别，沿真实道路推进，不手改时钟或位置。
func finish_travel(k) -> bool:
	for _i in range(30):
		var s: Dictionary=k.snapshot()
		if s.actors[s.player_actor_id].location.kind=="Settlement": return true
		var contact: Dictionary=Encounters.pending_for(s,s.player_actor_id)
		if not contact.is_empty() and not F.send(k,"encounter_leave",{"encounter_id":contact.id}).ok: return false
		if not k.advance_one_tick().ok: return false
	return false

## 私信接缝与保存负例：信件只在完成转为收件人产权，ready和report均可恢复。
func message_flow() -> void:
	var k=observed(message_at_receiver()); var params: Dictionary=P.params(k,"offer.letter","role.recipient")
	var before: Dictionary=k.snapshot(); var tid: String=before.tasks.keys()[0]
	check(F.send(k,"deliver_message",params).ok,"注册交信开始")
	var s: Dictionary=k.snapshot(); var id: String=s.actions.keys()[0]
	check(s.inventories["bag:actor.he"]==before.inventories["bag:actor.he"] and s.tasks[tid].status=="active","开始交信不提前转物或ready")
	var bad: Dictionary=s.duplicate(true); bad.actions[id].params.context.provider_actor_id="actor.player"
	reject(k,bad,"InvalidMessageAction","固定收件人与预留校验")
	bad=s.duplicate(true)
	for lot in bad.inventories["bag:actor.player"].lots:
		if lot.reserved_by==id: lot.owner={"kind":"actor","id":"actor.player"}
	check(Schema.validate(bad,k.content()).ok,"保存helper保留actor-owner旧接受范围")
	roundtrip(k,"delivery-reserved","advance")
	check(k.snapshot().tasks[tid].status=="ready","交信完成置ready")
	var definition: Dictionary=k.content().data.offers["offer.letter"]
	check(Inventory.count(k.snapshot().inventories["bag:actor.he"],definition.goal.message_id,[{"kind":"actor","id":"actor.he"}])==1,"真实信件已转收件人产权")
	for stage in ["preview","prepare","start","step","finish","validate"]: check(k.probe.counts.get("deliver_message:"+stage,0)>0,"交信生命周期 "+stage)
	roundtrip(k,"message-ready")
	check(k.advance_one_tick().ok,"ready存读短窗等待完成")
	F.send(k,"move_local",{"place_id":"east.entrance"}); check(F.send(k,"travel",{"destination":"city.west"}).ok and finish_travel(k),"私信真实回程")
	F.send(k,"move_local",{"place_id":"west.tavern"}); params=P.params(k,"offer.letter","role.sender")
	check(F.send(k,"report_offer",params).ok,"私人回报真实预留")
	s=k.snapshot(); id=s.actions.keys()[0]; bad=s.duplicate(true); bad.tasks[tid].status="active"
	reject(k,bad,"UndeliveredMessageReport","未交信不可报账")
	roundtrip(k,"message-report","advance")
	check(k.snapshot().tasks[tid].status=="success","私人清算完成")
	roundtrip(k,"settled")

## 期限边界由真实wait推进；完成日期D附近与即时取消日期保留旧区别。
func deadlines() -> void:
	for delta in [-1,0,1,2,3]:
		var k=P.grain_ready(); var task: Dictionary=k.snapshot().tasks.values()[0]; var target: int=task.deadline_tick+delta
		while k.snapshot().tick<target-1:
			if not check(F.send(k,"wait").ok and k.advance_one_tick().ok,"真实等待至完成边界"): return
		var before: Dictionary=k.snapshot(); var offer: Dictionary=k.preview("report_offer",P.params(k))
		if delta==3:
			check(offer.get("code")=="OfferExpired" and k.snapshot()==before,"D+3预期完成拒绝且纯读")
			continue
		var expected: int=80 if delta<=0 else 40
		check(offer.ok and offer.reward==expected,"D"+str(delta)+" 报价全奖/半奖")
		check(F.send(k,"report_offer",P.params(k)).ok and k.advance_one_tick().ok,"边界实际完成")
		var after: Dictionary=k.snapshot(); var done: Dictionary=after.tasks[task.id]
		check(done.status=="success" and done.reported_tick==target and done.late==(delta>0),"完成使用真实日期及late")
		check(after.wallets["wallet.player"].balance==before.wallets["wallet.player"].balance+before.wallets[task.budget_wallet_id].balance+expected,"结算余预算加冻结报酬")
	for delta in [0,1]:
		var k=P.accepting(); k.advance_one_tick(); var task: Dictionary=k.snapshot().tasks.values()[0]
		while k.snapshot().tick<task.deadline_tick+delta:
			if not check(F.send(k,"wait").ok and k.advance_one_tick().ok,"真实等待取消边界"): return
		var tick: int=k.snapshot().tick
		check(F.send(k,"cancel_task",P.params(k)).ok and k.snapshot().tick==tick and k.snapshot().tasks[task.id].status==("cancelled" if delta==0 else "failed"),"取消按当前D/D+1，不推进")

## 已改变真实资源后返回失败或损坏全局Schema，完整快照含缓存/历史必须回滚。
func rollback() -> void:
	for mode in ["start","validate","finish","tail","cancel","report","delivery"]:
		var k
		var kind: String="accept_offer"
		var params: Dictionary={}
		if mode in ["report","cancel"]: k=observed(P.grain_ready()); kind="cancel_task" if mode=="cancel" else "report_offer"; params=P.params(k)
		elif mode=="delivery": k=observed(message_at_receiver()); kind="deliver_message"; params=P.params(k,"offer.letter","role.recipient")
		else: k=discovered(); params=P.params(k)
		if mode in ["finish","tail","report","delivery"]:
			check(F.send(k,kind,params).ok,"故障前真实开始 "+mode); kind="advance"; params={}
		var before: Dictionary=k.snapshot(); var command: Dictionary=P.command(k,kind,params)
		k.probe.failure="finish" if mode in ["report","delivery"] else mode
		var result: Dictionary=k.submit(command)
		check(not result.ok and k.probe.mutated and k.snapshot()==before,"实际写入后整候选回滚 "+mode+" "+str(result))
		k.probe.failure=""; result=k.submit(command); var after: Dictionary=k.snapshot(); var counts: Dictionary=k.probe.counts.duplicate(true)
		check(result.ok and after!=before,"故障修复同sequence成功一次 "+mode)
		check(k.submit(command)==result and k.snapshot()==after and k.probe.counts==counts,"重放不重复callback "+mode)

## 公共守卫与领域双坏输入保留原错误优先级，报价不得消耗任何随机数。
func error_precedence() -> void:
	var k=discovered(); var original: Dictionary=k.snapshot(); var params: Dictionary=P.params(k)
	var s: Dictionary=original.duplicate(true); s.actors["actor.player"].available=false
	check(k._preview(s,"accept_offer",{},"actor.player").code=="ActorUnavailable","公共不可用先于未知任务")
	var broken: Dictionary=params.duplicate(true); broken.offer_id="missing"; broken.context={}
	check(k._preview(original,"accept_offer",broken,"actor.player").code=="UnknownOffer","未知任务先于上下文")
	broken=params.duplicate(true); broken.context={}; s=original.duplicate(true); s.knowledge["actor.player"]={}; s.wallets["wallet.office"].balance=0
	check(k._preview(s,"accept_offer",broken,"actor.player").code=="InvalidInteractionContext","上下文先于来源和资金")
	check(k._preview(s,"accept_offer",params,"actor.player").code=="SourceUnknown","来源先于预算")
	s=original.duplicate(true); s.wallets["wallet.office"].balance=0
	check(k._preview(s,"accept_offer",params,"actor.player").code=="IssuerInsufficientFunds","预算失败旧码")
	check(k.snapshot()==original,"错误预览世界无变化")

## 延长定义验证duration来自报价；半日hold/contact不暂停任务且release只清自己的占用。
func preparation_and_clock() -> void:
	var k=discovered(); var s: Dictionary=k.snapshot(); var data: Dictionary=k.content().data; data.offers["offer.grain"].accept_ticks=3
	var params: Dictionary=P.params(k); params.extra={"nested":[1]}
	var module=load(MODULE_PATH); var action_script=load(ACTION_PATH)
	var offer: Dictionary=module.preview(s,data,"actor.player","accept_offer",params); var before: Dictionary=s.duplicate(true)
	var prepared: Dictionary=action_script.prepare(s,data,"actor.player","accept_offer",params,offer)
	check(prepared.duration==3 and s==before,"prepare纯读且duration非硬编码")
	params.extra.nested.append(2); offer.context.role_id="changed"
	check(prepared.params.extra.nested==[1] and prepared.params.context.role_id=="role.office","prepare完整深拷贝")
	check(module.preview(s,data,"actor.player","wrong",{}).code=="UnsupportedAction" and module.apply(s,data,"actor.player","wrong",{},{}).code=="UnsupportedAction","模块未声明kind明确拒绝")
	var source=P.accepting(); var observed_kernel=observed(source); var candidate: Dictionary=observed_kernel.snapshot(); var id: String=candidate.actions.keys()[0]
	check(Step.advance(candidate,observed_kernel.content().data,observed_kernel._registry,["actor.player","actor.chen"]).ok and candidate.actions.is_empty(),"任务不会因hold暂停且由共用时钟完成")
	check(observed_kernel.probe.contexts[0].hold==["actor.player","actor.chen"] and observed_kernel.probe.order==[id],"真实Step上下文与单次调用")
	candidate=source.snapshot(); var action: Dictionary=candidate.actions[id]; candidate.actors["actor.chen"].action_id="newer"
	Step.release(candidate,action)
	check(candidate.actors["actor.chen"].action_id=="newer" and candidate.actors["actor.player"].action_id=="","公共release只清自己的占用")

## 真实等待对白接收success=false，合同中断不是事务失败或假成功。
func dialogue_unavailable() -> void:
	var k=discovered()
	check(F.send(k,"talk",{"role_id":"role.office"}).ok and F.send(k,"dialogue_next").ok and F.send(k,"dialogue_choose",{"option_id":"accept"}).ok,"作者化对白进入受领确认")
	if not check(F.send(k,"dialogue_confirm").ok and k.snapshot().interactions["actor.player"].phase=="waiting","注册回执驱动waiting"): return
	var s: Dictionary=k.snapshot(); s.roles["role.office"].enabled=false
	check(k.restore(s).ok and k.advance_one_tick().ok,"真实对白等待收到中断仍提交半日")
	s=k.snapshot()
	check(s.tasks.values()[0].status=="cancelled" and s.wallets["wallet.office"].reserved.is_empty(),"受领不可用无拨款且释放预留")
	check(not s.interactions.has("actor.player") or s.interactions["actor.player"].phase!="waiting","失败outcome恢复或关闭对白")

## 元数据引用精确对应唯一领域实现，缺固定绑定字段不允许启动成功。
func metadata_contracts() -> void:
	var metadata: Dictionary=JSON.parse_string(FileAccess.get_file_as_string(Builtin.CONTRACT_PATH))
	var capabilities: Dictionary=JSON.parse_string(FileAccess.get_file_as_string(Builtin.CAPABILITY_PATH))
	for module in metadata.modules:
		if module.id!="tasks": continue
		for claim in module.owns:
			if claim.container=="tasks": check(claim.schema_refs==[{"path":"game/core/domains/tasks/task_state.gd","method":"validate"}],"tasks根唯一校验归属")
			if claim.container=="actions": check(claim.schema_refs==[{"path":"game/core/domains/tasks/task_action.gd","method":"validate"}],"任务行动唯一校验归属 "+claim.key)
			if claim.container=="knowledge": check(claim.schema_refs==[{"path":"game/core/base/world_shape.gd","method":"validate"}],"knowledge未越界迁移")
	for field in ["path","method","signature"]:
		var bad: Dictionary=metadata.duplicate(true)
		for module in bad.modules:
			if module.id=="tasks": module.validators[0].erase(field)
		check(Builtin.validate_metadata(bad,capabilities).get("code")=="InvalidBuiltinValidator","缺固定validator字段拒绝 "+field)
	var descriptors: Array=Builtin.descriptors()
	for module in descriptors:
		if module.id=="tasks": module.validators[0].erase("callback")
	check(Registry.build(descriptors).get("code")=="InvalidStateValidator","缺tasks.state真实callback拒绝")

## 真实交易形成公粮及私粮混合批次，report仍先用公粮且精确拆分剩余私粮。
func procurement_ownership() -> void:
	for cancel in [false,true]:
		var k=observed(P.accepting()); check(k.advance_one_tick().ok,"混合产权真实拨款")
		var task: Dictionary=k.snapshot().tasks.values()[0]; var owner: Dictionary={"kind":"task","id":task.id}; var private_owner: Dictionary={"kind":"actor","id":"actor.player"}
		F.send(k,"move_local",{"place_id":"west.street"}); F.send(k,"move_local",{"place_id":"west.grain"})
		for wallet in [task.budget_wallet_id,"wallet.player"]:
			check(F.send(k,"trade",{"shop_id":"shop.grain","item_id":"item.grain","side":"buy","quantity":6,"payer_wallet_id":wallet,"context":F.context(k,"role.merchant")}).ok and k.advance_one_tick().ok,"真实购买对应产权6份粮")
		F.send(k,"move_local",{"place_id":"west.street"}); F.send(k,"move_local",{"place_id":"west.office"})
		var before: Dictionary=k.snapshot(); var bag: String=before.actors["actor.player"].inventory_id
		check(Inventory.count(before.inventories[bag],"item.grain",[owner])==6 and Inventory.count(before.inventories[bag],"item.grain",[private_owner])==6,"公款与私款分别拥有实际货物")
		if cancel:
			check(F.send(k,"cancel_task",P.params(k)).ok,"混合产权取消")
			var after: Dictionary=k.snapshot()
			check(Inventory.count(after.inventories[bag],"item.grain",[owner])==0 and Inventory.count(after.inventories[bag],"item.grain",[private_owner])==6,"取消只退公粮，私粮完整保留")
			check(P.money(after)==P.money(before),"取消没有增发资金")
			continue
		var command: Dictionary=P.command(k,"report_offer",P.params(k)); var result: Dictionary=k.submit(command)
		if not check(result.ok,"混合产权报账开始"): return
		var pending: Dictionary=k.snapshot(); var aid: String=result.details.action_id
		check(Inventory.count(pending.inventories[bag],"item.grain",[owner],aid)==6 and Inventory.count(pending.inventories[bag],"item.grain",[private_owner],aid)==4,"报告严格先预留任务货后私货")
		check(Inventory.count(pending.inventories[bag],"item.grain",[private_owner])==2,"拆批余货不被锁住")
		check(pending.inventories[bag].revision==before.inventories[bag].revision+2 and pending.next_instance_id==before.next_instance_id+3,"action、拆批lot、result顺序与inventory revision")
		check(pending.inventories["store:org.office"]==before.inventories["store:org.office"] and pending.wallets["wallet.player"].balance==before.wallets["wallet.player"].balance,"开始不提前交粮或发奖")
		check(k.submit(command)==result and k.snapshot()==pending,"同批公粮重复请求不可二次预留")
		roundtrip(k,"procurement-report-reserved","advance")
		var settled: Dictionary=k.snapshot()
		check(P.money(settled)==P.money(before) and settled.tasks[task.id].status=="success","采购完整清算资金守恒")
		check(not F.send(k,"report_offer",P.params(k)).ok and k.snapshot()==settled,"新sequence不能再次交粮领赏")
		for stage in ["preview","prepare","start","step","finish","validate"]: check(k.probe.counts.get("report_offer:"+stage,0)>0,"回报真实生命周期 "+stage)

## 护卫记录直接承接旧validator错误与取消豁免；死亡由观察器产生失败而非手写合同。
func escort_validation_and_expiry() -> void:
	var k=P.fresh(true); F.send(k,"move_local",{"place_id":"west.tavern"}); var params: Dictionary=P.params(k,"offer.escort","role.sender")
	check(F.send(k,"discover_offer",params).ok and F.send(k,"accept_offer",params).ok,"护卫真实受领开始")
	var s: Dictionary=k.snapshot(); var tid: String=s.tasks.keys()[0]; var bad: Dictionary=s.duplicate(true)
	bad.tasks[tid].escort={}
	reject(k,bad,"PrematureEscortState","accepting护卫不得提前进度")
	check(k.advance_one_tick().ok,"护卫实际完成受领")
	s=k.snapshot()
	for variant in [{"field":"escort","value":{},"code":"InvalidEscortState"},{"field":"arrival_tick","value":s.tick+1,"code":"InvalidEscortTime"},{"field":"battle_ids","value":["missing"],"code":"MissingEscortBattle"},{"field":"failure","value":"invented","code":"InvalidEscortFailure"},{"field":"failure","value":"EscortPersonLost","code":"InvalidEscortFailureTime"}]:
		bad=s.duplicate(true)
		if variant.field=="escort": bad.tasks[tid].escort=variant.value
		else: bad.tasks[tid].escort[variant.field]=variant.value
		reject(k,bad,variant.code,"护卫旧validator "+variant.code)
	bad=s.duplicate(true); bad.tasks[tid].status="ready"
	reject(k,bad,"EscortNotArrived","不能伪造ready抵达")
	check(F.send(k,"report_offer",params).get("code")=="EscortNotArrived","实际未抵达不能领取")
	s.actors["actor.carter"].alive=false
	check(k.restore(s).ok and F.send(k,"wait").ok and k.advance_one_tick().ok,"合法死亡边界经观察器形成失败合同")
	var task: Dictionary=k.snapshot().tasks[tid]
	check(task.status=="ready" and task.escort.failure=="EscortPersonLost","观察器实际失败原因")
	while k.snapshot().tick<=task.deadline_tick+3:
		if not check(F.send(k,"wait").ok and k.advance_one_tick().ok,"失败護卫实际等待越过宽限"): return
	var before: Dictionary=k.snapshot(); var offer: Dictionary=k.preview("report_offer",params)
	check(offer.ok and offer.reward==0,"失败护卫越过宽限仍零奖回报")
	check(F.send(k,"report_offer",params).ok,"失败护卫实际报账预留")
	s=k.snapshot(); var aid: String=s.actors["actor.player"].action_id; bad=s.duplicate(true); bad.actions[aid].params.reward=1; bad.wallets["wallet.shen"].reserved[aid]=1
	reject(k,bad,"InvalidEscortReport","失败护卫不能预留成功报酬")
	check(k.advance_one_tick().ok,"失败护卫实际结算")
	check(k.snapshot().tasks[tid].status=="failed" and k.snapshot().wallets["wallet.player"].balance==before.wallets["wallet.player"].balance and k.snapshot().relations==before.relations,"失败结算不发成功奖励或加关系")

## 不同人物同半日受领任务，字典序及公共释放不因迁移重排。
func parallel_actions() -> void:
	var k=observed(P.accepting())
	check(F.send(k,"move_local",{"place_id":"west.street"},"actor.apprentice").ok and F.send(k,"move_local",{"place_id":"west.tavern"},"actor.apprentice").ok,"第二人物真实移动到委托处")
	var params: Dictionary=P.params(k,"offer.letter","role.sender")
	check(F.send(k,"discover_offer",params,"actor.apprentice").ok and F.send(k,"accept_offer",params,"actor.apprentice").ok,"第二人物独立受领私信")
	var before: Dictionary=k.snapshot(); var ids: Array=before.actions.keys(); ids.sort()
	check(ids.size()==2 and k.advance_one_tick().ok,"两个任务同半日完成")
	check(k.probe.order==ids and k.snapshot().tick==before.tick+1 and k.snapshot().actions.is_empty(),"字符串ID顺序且仅一个世界tick")
	for task in k.snapshot().tasks.values(): check(task.status=="active","两个真实合同均生效")

## 首先执行迁移前已存在的真值向量，再执行注册新行为；任何失败令进程非零。
func run_checks() -> void:
	validation_vectors(); error_precedence(); deadlines()
	if check(registration(),"登记前提必须成功，不能静默跳过注册任务回归"):
		acceptance_and_cancel(); missing_handlers(); message_flow(); rollback(); preparation_and_clock(); dialogue_unavailable(); metadata_contracts(); procurement_ownership(); escort_validation_and_expiry(); parallel_actions()
	print("REGISTERED_TASKS_CHECKS=",checks," FAILURES=",failures)
	quit(1 if failures else 0)
