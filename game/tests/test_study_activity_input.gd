## 实际Main学习/训练/货单输入的有限来源迁位门；正式作者开局，原按钮与真实Kernel提交，不伪造付款或题面。
extends SceneTree
const Kernel=preload("res://core/world_kernel.gd")
const Main=preload("res://ui/main.gd")
const Art=preload("res://ui/art_catalog.gd")
const Interaction=preload("res://core/interaction_rules.gd")
const Session=preload("res://ui/activity_session.gd")
const Study=preload("res://ui/study_screen.gd")
const OLD_MAIN="9ce4e8307010c1a094b086f99b1dbe9d6f52fa10c9f8452af9f1879c8b8777cc"
const OLD_SESSION="a9fab849ee0e50a0ed21c5b0f349519952d14087b761c792aabc1799bc69a80c"
const OLD_STUDY="797cee5638164f16d96c40724bf78245dbef0ab9d246eee4572bfdca13c6fdc6"
var checks: int=0
var failures: int=0
var fixture: Dictionary={}
var rows: Array=[]
var reads: Array=[]
var required: bool=false
var identities_completed: bool=false

## 只观测真实公共读和提交，preview/事务/世界时序完全由原Kernel执行。
class ObservedKernel extends Kernel:
	var snapshots: int=0
	var contents: int=0
	var commands: Array=[]
	var reject_late_context: bool=false
	## 只在新增适配防御门注入读取失败；preview和真实状态保持原义。
	func command_context(actor_id: String="") -> Dictionary:
		if reject_late_context:return {"ok":false,"code":"InvalidCommandContext"}
		return super.command_context(actor_id)
	## 计数后交原深副本，不替换读取内容。
	func snapshot() -> Dictionary:snapshots+=1;return super.snapshot()
	## 计数后交原目录深副本，不简化源身份。
	func content() -> Dictionary:contents+=1;return super.content()
	## 记录按钮真实请求以比较冻结版本、序号和参数。
	func submit(command: Dictionary) -> Dictionary:
		commands.append(command.duplicate(true));return super.submit(command)

## 引擎创建后运行真实控件短链。
func _init() -> void:call_deferred("run")
## 所有失败计入实际退出码。
func check(value: bool,label: String) -> bool:
	checks+=1
	if not value:failures+=1;printerr("FAIL: ",label)
	return value
## 等布局和原Main延迟一半日完成，不另造推进循环。
func settle() -> void:
	await process_frame;await process_frame;await process_frame
## 原正式目录和Art成功后挂真实Main；不执行任何存储请求。
func opened():
	var k=ObservedKernel.new();var result: Dictionary=k.new_game(fixture.scenario,fixture.catalog)
	if not check(result.ok,"正式学习作者开局 "+str(result)):return null
	var prepared: Dictionary=Art.prepare_content(k.content())
	if not check(prepared.ok,"正式tiny表现输入"):return null
	Art.commit_content(prepared)
	var m=Main.new();m.kernel=k;m._data=k.content().data;m._started=true;root.add_child(m);await settle()
	return m
## 只有前置用显式公共命令；学习功能自身通过实际按钮pressed。
func send(m,kind: String,params: Dictionary={}) -> bool:
	var s: Dictionary=m.kernel.snapshot();var actor: String=s.player_actor_id
	var result: Dictionary=m.kernel.submit({"kind":kind,"params":params,"actor_id":actor,"expected_revision":s.revision,"request_seq":s.request_sequences[actor]+1})
	return check(result.ok,"公共前置 "+kind+" "+str(result))
## 按真实元数据查可用按钮，模式只取原activity_params。
func button(node: Node,kind: String,mode: String="") -> Button:
	if node is Button and node.get_meta("command_kind","")==kind and not node.disabled and (mode.is_empty() or node.get_meta("activity_params",{}).get("challenge_mode",node.get_meta("activity_params",{}).get("mode",""))==mode):return node
	for child in node.get_children():
		var found: Button=button(child,kind,mode)
		if found!=null:return found
	return null
## 实际按钮点击后让原Main决定是否推进首半日。
func click(m,kind: String,mode: String="") -> bool:
	var b: Button=button(m.page,kind,mode)
	if not check(b!=null,"原实际按钮可用 "+kind+" "+mode):return false
	b.pressed.emit();await settle();return true
## 文字、按钮、正式quote与每个Session的身份按原树序记录；进程内instance只做明确归一。
func controls(node: Node,m,result: Array) -> void:
	if node is Label:result.append(["label",node.text,node.visible])
	elif node is Button:result.append(["button",node.text,node.get_meta("command_kind",""),node.get_meta("target_id",""),node.disabled,node.tooltip_text,node.get_meta("activity_params",{}),node.get_meta("activity_quote",{})])
	if node.has_meta("activity_session"):
		var identity: Dictionary=node.get_meta("activity_session")._identity.duplicate(true)
		check(identity.kernel_instance==m.kernel.get_instance_id(),"Session绑定真实Kernel实例")
		identity.kernel_instance="current_kernel_instance"
		result.append(["identity",identity])
	for child in node.get_children():controls(child,m,result)
## 同一已提交状态重绘不改全context/RNG；只记录本用途原输出和少量真实支付/行动来源。
func capture(m,label: String) -> void:
	var before: Dictionary=m.kernel.save_context();m.kernel.snapshots=0;m.kernel.contents=0
	seed(12626);var expected: int=randi();seed(12626);m._render();await settle()
	check(m.kernel.save_context()==before and randi()==expected,"重绘保持全候选与RNG "+label)
	reads.append({"snapshot":m.kernel.snapshots,"content":m.kernel.contents})
	var output: Array=[];controls(m.page,m,output)
	var s: Dictionary=m.kernel._state;var actor: Dictionary=s.actors[s.player_actor_id]
	rows.append({"label":label,"controls":output,"tick":s.tick,"revision":s.revision,"actor":actor.duplicate(true),"wallets":s.wallets.duplicate(true),"facts":s.facts.duplicate(true),"rng":s.rng.duplicate(true)})
	if required:
		for method in ["study_course_contexts","study_activity","activity_identity"]:check(m.kernel.has_method(method),"完整用途入口 "+method)
		if m.kernel.has_method("study_activity"):
			var read_before: Array=[m.kernel.snapshots,m.kernel.contents]
			var activity: Dictionary=m.kernel.study_activity();var original: Dictionary=activity.duplicate(true)
			if activity.get("active",false):activity.challenge_scopes.clear()
			check(m.kernel.study_activity()==original and m.kernel.save_context()==before,"当前活动有限返回隔离 "+label)
			var contexts: Array=m.kernel.study_course_contexts();var old_contexts: Array=contexts.duplicate(true)
			if not contexts.is_empty():contexts[0].context.clear()
			check(m.kernel.study_course_contexts()==old_contexts and m.kernel.save_context()==before,"岗位context返回隔离 "+label)
			m.kernel.activity_identity()
			check([m.kernel.snapshots,m.kernel.contents]==read_before,"三用途不借公开snapshot/content转接 "+label)
## 原起点到官署走真实地方通道，不把作者玩家或角色位置写进运行态。
func at_office(m) -> bool:
	var place: String=m.kernel._state.actors[m.kernel._state.player_actor_id].location.place_id
	if place!="base::west.street" and not send(m,"move_local",{"place_id":"base::west.street"}):return false
	return send(m,"move_local",{"place_id":"base::west.office"})
## 真实默认时长和换时长销毁旧行；自动课首课/部分取消由原Main与Kernel提供。
func automatic_course() -> void:
	var m=await opened()
	if m==null:return
	if not at_office(m):m.queue_free();await settle();return
	m._open_courses();await capture(m,"courses_default_two")
	var old: Button=button(m.page,"study_skill","automatic");var duration: OptionButton=m.find_child("StudyDuration",true,false)
	if not check(old!=null and duration!=null,"真实课程可选时长和报名入口"):m.queue_free();await settle();return
	duration.select(3);duration.item_selected.emit(3)
	var before: Dictionary=m.kernel.save_context();old.pressed.emit()
	check(m.kernel.save_context()==before,"同帧换课时旧行按钮关闭且不提交")
	# capture会重绘整页回默认2；换5半日后的真实局部树另单独记录。
	var rendered: Array=[];controls(m.page,m,rendered);rows.append({"label":"duration_five_local","controls":rendered});reads.append({"snapshot":0,"content":0})
	if await click(m,"study_skill","automatic"):
		await capture(m,"automatic_first_lesson")
		var s: Dictionary=m.kernel._state;var id: String=s.actors[s.player_actor_id].action_id
		check(s.tick==1 and s.facts[id].lessons.size()==1,"原Main自动开课只实际提供首半日")
		if await click(m,"study_cancel"):await capture(m,"automatic_partial_cancel")
	m.queue_free();await settle()
## 手动筹算按原公开选项完成三轮，题面不自己付款；实际半日后再中断。
func manual_course() -> void:
	var m=await opened()
	if m==null:return
	if not at_office(m):m.queue_free();await settle();return
	m._open_courses()
	if not await click(m,"study_skill","manual"):m.queue_free();await settle();return
	await capture(m,"manual_before_lesson")
	var s: Dictionary=m.kernel._state;var id: String=s.actors[s.player_actor_id].action_id;var tick: int=s.tick
	check(m.kernel.study_etiquette(s.player_actor_id,id).get("code")=="StudyChallengeNotOffered","旧礼议兼容口不把筹算实例认作礼议")
	for index in range(3):
		if not await click(m,"study_challenge_choose"):break
		if index==0:await capture(m,"manual_round_two")
	await capture(m,"manual_completed_problem")
	check(m.kernel._state.tick==tick and m.kernel._state.facts[id].lessons.is_empty(),"三次题面选择不抢跑课时或预留付款")
	if await click(m,"advance"):await capture(m,"manual_actual_lesson")
	if await click(m,"study_cancel"):await capture(m,"manual_partial_cancel")
	m.queue_free();await settle()
## 已有正式任官和训练作者只为当前活动页建立真实军阵；不改Camp三共体。
func training() -> void:
	var m=await opened()
	if m==null:return
	if not at_office(m):m.queue_free();await settle();return
	var context: Dictionary=Interaction.for_role(m.kernel._state,m._data,"base::role.office")
	if not send(m,"office_accept",{"office_id":"base::office.study","context":context}) or not send(m,"advance") or not send(m,"military_train",{"office_id":"base::office.study","unit_id":"base::unit.study","challenge_mode":"manual"}):m.queue_free();await settle();return
	await capture(m,"training_manual_start")
	if await click(m,"military_drill_draft"):await capture(m,"training_pending_move")
	if await click(m,"military_drill_confirm"):await capture(m,"training_confirmed_move")
	if await click(m,"military_drill_exit"):await capture(m,"training_exit_problem")
	if await click(m,"advance"):await capture(m,"training_actual_lesson")
	if await click(m,"military_train_cancel"):await capture(m,"training_partial_cancel")
	m.queue_free();await settle()
## 真实货单议价也共用Session，旧摘要/参数关系由原Bargain与preview核验。
func trade() -> void:
	var m=await opened()
	if m==null:return
	if not send(m,"talk",{"role_id":"base::role.merchant"}) or not send(m,"dialogue_next") or not send(m,"dialogue_choose",{"option_id":"goods"}) or not send(m,"dialogue_confirm"):m.queue_free();await settle();return
	await capture(m,"trade_before_bargain")
	if await click(m,"trade_bargain_open","manual"):
		await capture(m,"trade_manual_problem")
		if await click(m,"trade_bargain_exit"):await capture(m,"trade_exit_problem")
	m.queue_free();await settle()
## 真Main/真Kernel上的原身份八字段和晚回调边界；显式trusted alias源改动只测已有身份保护。
func identities() -> void:
	for mode in ["revision","epoch","source","kernel","hidden","closed","busy","storage"]:
		var m=await opened()
		if m==null:return
		var box=VBoxContainer.new();m.add_child(box);var session=Session.create(m,box)
		var identity: Dictionary=session._identity.duplicate(true);var actor: String=m.kernel._state.player_actor_id
		var expected: Dictionary=m._slot_world_identity();expected.kernel_instance=m.kernel.get_instance_id();expected.source_sha256=JSON.stringify(m.kernel.content().get("data",{}),"",true).sha256_text()
		check(identity==expected,"真实旧身份八字段完全一致 "+mode)
		var target: Button=session.command(box,"等候","wait",{});var reason: String="StaleActivitySession"
		match mode:
			"revision":send(m,"move_local",{"place_id":"base::west.street"})
			"epoch":m._scene_counter+=1
			"source":m.kernel._catalog.data.texts["test.source.alias"]="explicit read-boundary mutation"
			"kernel":
				var replacement=ObservedKernel.new();check(replacement.new_game(fixture.scenario,fixture.catalog).ok,"相同真实内容/初始revision的新Kernel");m.kernel=replacement
			"hidden":box.hide();reason="ActivitySessionClosed"
			"closed":m.remove_child(box);m.add_child(box);reason="ActivitySessionClosed"
			"busy":m._advancing=true;reason="ActorBusy"
			"storage":m._storage_intent="test_only";reason="ActorBusy"
		var before: Dictionary=m.kernel.save_context();var sent: int=m.kernel.commands.size()
		var result: Dictionary=session.activate(target.get_meta("activity_ticket"))
		check(result.get("code")==reason and m.kernel.save_context()==before and m.kernel.commands.size()==sent,"真实晚输入原首错/零提交 "+mode)
		m.queue_free();await settle()
	identities_completed=true

## 新有限口的未选中来源与late-context防御只做局部风险门，不扩坏历史矩阵。
func finite_boundaries() -> void:
	var m=await opened()
	if m==null:return
	var source=load("res://core/views/study_activity_input.gd")
	var copied: Dictionary=m.kernel.snapshot();copied.facts=null;copied.wallets=null
	check(source.active(copied)=={"active":false},"非课程先返回，不读facts/wallets")
	if not at_office(m):m.queue_free();await settle();return
	copied=m.kernel.snapshot();copied.facts=null;copied.wallets=null
	var contexts: Array=source.course_contexts(copied,m._data)
	check(contexts.size()==1 and contexts[0].role_id=="base::role.office","课程岗位仅原context，不提前报价读资金")
	var box=VBoxContainer.new();m.add_child(box);var session=Session.create(m,box)
	var target: Button=session.command(box,"等候","wait",{});var id: int=target.get_meta("activity_ticket")
	var before: Dictionary=m.kernel.save_context();var count: int=m.kernel.commands.size()
	m.kernel.reject_late_context=true
	var result: Dictionary=session.activate(id)
	m.kernel.reject_late_context=false
	check(result=={"ok":false,"code":"InvalidCommandContext"} and m.kernel.commands.size()==count and m.kernel.save_context()==before,"晚取seq的新适配失败原码拒绝且不提交")
	check(session._tickets[id].used,"晚取seq失败仍在原used置位之后，不能重用旧ticket")
	m.queue_free();await settle()

## 原生产来源固定后首次写typed oracle；迁位后逐项输出相等，只允许读取次数下降。
func oracle(args: PackedStringArray) -> void:
	for arg in args:
		if arg.begins_with("--record-oracle="):
			var path: String=arg.trim_prefix("--record-oracle=")
			if not check(failures==0 and FileAccess.get_file_as_string("res://ui/main.gd").sha256_text()==OLD_MAIN and FileAccess.get_file_as_string("res://ui/activity_session.gd").sha256_text()==OLD_SESSION and FileAccess.get_file_as_string("res://ui/study_screen.gd").sha256_text()==OLD_STUDY and not FileAccess.file_exists(path),"原12cbc实际输出首次冻结"):return
			var file=FileAccess.open(path,FileAccess.WRITE);file.store_var({"source":OLD_SESSION,"catalog":fixture.pack_set_digest,"rows":rows,"reads":reads},false);file.close();return
		if arg.begins_with("--oracle="):
			var file=FileAccess.open(arg.trim_prefix("--oracle="),FileAccess.READ);var old: Variant=file.get_var(false) if file!=null else null
			if not check(old is Dictionary and old.get("source")==OLD_SESSION and old.get("catalog")==fixture.pack_set_digest,"旧oracle同源/同正式输入"):return
			check(old.rows.size()==rows.size(),"原实际场景数一致")
			for i in mini(rows.size(),old.rows.size()):
				check(rows[i]==old.rows[i],"控件/身份/事实/资金/题面原输出 "+rows[i].label)
				check(reads[i].snapshot<=old.reads[i].snapshot and reads[i].content<=old.reads[i].content,"完整快照/目录拷贝不增加 "+rows[i].label)
				print("STUDY_READS=",JSON.stringify({"case":rows[i].label,"old":old.reads[i],"current":reads[i]}))
			return
	check(false,"明确给定记录或对照路径")
## 固定正式输入，不按安装列表[0]猜目录；所有对照在同一规则/作者身份运行。
func run() -> void:
	var args: PackedStringArray=OS.get_cmdline_user_args();required="--require-input" in args
	for arg in args:
		if arg.begins_with("--fixture="):
			var parsed: Variant=JSON.parse_string(FileAccess.get_file_as_string(arg.trim_prefix("--fixture=")))
			if parsed is Dictionary:fixture=parsed
	if not check(fixture.has("catalog") and fixture.has("scenario"),"显式正式学习输入"):quit(1);return
	root.size=Vector2i(1280,720)
	print("STUDY_STORAGE={requests:none,user_data_dir:",OS.get_user_data_dir(),"}")
	await automatic_course();await manual_course();await training();await trade();await identities();check(identities_completed,"全部真实身份分支完成，不以脚本中断计为成功")
	if required:await finite_boundaries()
	oracle(args)
	print("STUDY_ACTIVITY_INPUT_CHECKS=",checks," FAILURES=",failures," CASES=",rows.size());quit(0 if failures==0 else 1)
