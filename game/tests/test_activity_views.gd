## 原生功能控件与输入会话短门；Kernel替身仅记录协议，不冒称课程、训练或真实钱货规则已由该测试验收。
extends SceneTree
const Session=preload("res://ui/activity_session.gd")
const Study=preload("res://ui/study_screen.gd")
const Challenge=preload("res://ui/challenge_screen.gd")
const Bargain=preload("res://ui/bargain_screen.gd")
const StudyInput=preload("res://core/views/study_activity_input.gd")
var checks: int=0
var failures: int=0

## 查询返回已冻结公开形状；提交只记录原参数与版本，不假造费用、经验或资产结算。
class QueryKernel extends RefCounted:
	var state: Dictionary={"revision":4,"tick":0,"player_actor_id":"player","request_sequences":{"player":0},"actors":{"player":{"id":"player","action_id":"","location":{"kind":"Settlement","place_id":"camp","settlement_id":"town"}}},"facts":{},"actions":{},"wallets":{},"offices":{},"military_units":{}}
	var data: Dictionary={"texts":{},"roles":{},"offices":{},"garrisons":{}}
	var sent: Array=[]
	var blocked: bool=false
	var price: int=60
	var ticks_read: int=0
	var challenge_kind: String="etiquette"
	var phase: String="active"
	var pending: Variant=null
	var has_bargain: bool=false

	## 快照始终深复制，节点不能藉读取引用修改测试世界。
	func snapshot() -> Dictionary: return state.duplicate(true)
	## 替身只提供原当前人物/版本/序号形状，不把协议门当作真实Kernel来源验收。
	func command_context(actor_id: String="") -> Dictionary:
		var actor: String=state.player_actor_id if actor_id.is_empty() else actor_id
		return {"ok":true,"actor_id":actor,"revision":state.revision,"next_request_seq":state.request_sequences[actor]+1}
	## 界面协议夹具匹配已有同人同基准；真实owner扫描另由专测验证。
	func trade_bargain_source(actor_id: String, basis_sha256: String) -> Dictionary:
		var quote_id: String=""
		for id in state.facts:
			var fact: Dictionary=state.facts[id]
			if fact.get("kind")=="trade_bargain" and fact.get("actor_id")==actor_id and fact.get("basis_sha256")==basis_sha256:
				if not quote_id.is_empty(): return {"quote_id":"","duplicate":true}
				quote_id=id
		return {"quote_id":quote_id,"duplicate":false}
	## 内容摘要与实际data分别绑定，模拟同摘要但原源发生改变的迟到输入。
	func content() -> Dictionary: return {"content_digest":"catalog","pack_set_digest":"set","data":data.duplicate(true)}
	## 有限身份沿真实叶每次重算源；同摘要data变化和换Kernel仍由原断言实际驱动。
	func activity_identity() -> Dictionary: return StudyInput.identity(state,content())
	## 当前课程支付摘要复用同一叶，不为UI门伪造付款或课时。
	func study_activity() -> Dictionary: return StudyInput.active(state)
	## 替身仅核对调用协议形状；报价变化用于检验界面不提交旧金额。
	func preview(kind: String, params: Dictionary={}, actor_id: String="") -> Dictionary:
		if blocked: return {"ok":false,"code":"InsufficientFunds","revision":state.revision}
		var result: Dictionary={"ok":true,"revision":state.revision,"duration":2,"cost":price,"expected_training_gain":3}
		if kind=="trade": result.total=price; result.market_revision=2; result.discount_bp=400
		if kind=="trade_bargain_open": result.basis={"basis_sha256":"basis"}
		if kind=="study_skill" and (actor_id!="player" or params.context.get("role_id")!="teacher"): return {"ok":false,"code":"RoleUnavailable","revision":state.revision}
		return result
	## 只模拟公共revision成功递增；不为测试绕过实际课程或资产owner创建事实。
	func submit(command: Dictionary) -> Dictionary:
		if command.expected_revision!=state.revision: return {"ok":false,"code":"StalePreview"}
		sent.append(command.duplicate(true)); state.revision+=1; state.request_sequences.player=command.request_seq
		return {"ok":true,"details":{}}
	## 课程投影只公开本人XP与报价，不提供教师隐藏经验。
	func study_courses(actor: String, context: Dictionary, ticks: int=2) -> Dictionary:
		ticks_read=ticks
		return {"ok":true,"actor_id":actor,"courses":[{"skill_id":"rites","mode":"facility","xp":0,"level":0,"next_threshold":100,"eligible":true,"params":{"skill_id":"rites","ticks":ticks,"context":context.duplicate(true)},"offer":{"duration":ticks,"cost":price,"expected_xp":40,"challenge":{"kind":"etiquette","challenge_id":"topic"}}}]}
	## 两种课程题面用正式公开键；旧礼议与新议价均经通用owner命令。
	func study_challenge(_actor: String, _id: String) -> Dictionary:
		if phase=="completed": return {"ok":true,"phase":phase,"challenge_kind":challenge_kind,"result":{"score":80,"performance_bp":12000}}
		if challenge_kind=="etiquette": return {"ok":true,"phase":"active","challenge_kind":"etiquette","round":1,"node_id":"node.one","topic":"公开题面","stance":"公开立场","options":[{"id":"answer","text":"选择论据"}]}
		return {"ok":true,"phase":"active","challenge_kind":"bargain","round":1,"round_id":"round.one","prompt":"公开筹算","budget":60,"quantity":4,"conditions":[{"id":"cash","text":"现款","matched":true}],"options":[{"id":"cash","text":"现款支付"}]}
	## 固定军阵公开格与pending，不向界面提供作者解证据。
	func military_drill(_actor: String, _id: String) -> Dictionary:
		return {"ok":true,"phase":"active","width":5,"height":4,"step":1,"prompt":"守住通路","groups":[{"id":"foot","kind":"foot","text":"步阵","position":[0,0],"targets":[[1,0]]}],"objectives":{"passage":{"text":"控制通路"}},"pending":pending}
	## 真实报价的公开题面明确不是课程；完成后金额仍须另向trade预览取得。
	func trade_bargain(_actor: String, id: String) -> Dictionary:
		if phase=="completed": return {"ok":true,"quote_id":id,"consumed":false,"phase":phase,"result":{"score":80,"discount_bp":400}}
		var view: Dictionary=study_challenge("",""); view.challenge_kind="bargain"
		view.erase("node_id"); view.round_id="round.one"
		return view

## 最薄宿主只复刻原Main的控件/身份/结果入口，不运行布局、美术、音频或自动世界推进。
class Host extends Control:
	var kernel=QueryKernel.new()
	var _data: Dictionary=kernel.data
	var _started: bool=true
	var _advancing: bool=false
	var _storage_intent: Dictionary={}
	var _scene_counter: int=1
	var _message: String=""
	var results: Array=[]
	## 与原Main相同的页面身份投影，供新会话冻结。
	func _slot_world_identity() -> Dictionary:
		return {"started":_started,"scene_epoch":_scene_counter,"revision":kernel.state.revision,"actor_id":kernel.state.player_actor_id,"content_digest":"catalog","pack_set_digest":"set"}
	## 原生按钮保留命令元数据，实际发出pressed信号检验会话闭包。
	func _button(parent: Node, label: String, kind: String, target: String, callback: Callable, disabled: bool=false) -> Button:
		var button=Button.new(); button.text=label; button.disabled=disabled; button.set_meta("command_kind",kind); button.set_meta("target_id",target)
		button.pressed.connect(callback); parent.add_child(button); return button
	## 文本节点支持精确收费和退出语义断言。
	func _text(parent: Node, label: String, _size: int=16) -> Label:
		var node=Label.new(); node.text=label; parent.add_child(node); return node
	## 未本地化原因保留原代码，不让空描述掩盖拒绝。
	func _reason(code: String) -> String: return code
	## 定义显示名来自原作者字段。
	func _name(value: Dictionary) -> String: return value.get("name_key",value.get("id",""))
	## 与正式Main相同，结果处理后页面代次改变；测试不自动推进半日。
	func _apply_command_result(kind: String, result: Dictionary) -> void: results.append({"kind":kind,"result":result}); _render()
	## 重绘只模拟epoch变化，旧节点仍保留以便主动触发晚回调。
	func _render() -> void: _scene_counter+=1

## 场景树建立后运行所有新增功能节点门。
func _init() -> void: call_deferred("run")

## 每个断言决定实际退出码。
func check(value: bool, label: String) -> void:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)

## 每组独立宿主，旧节点可故意留下检验不会借新世界重发。
func make_host() -> Host:
	var host=Host.new(); root.add_child(host); return host

## 只按语义command_kind寻找按钮，不依赖像素位置。
func buttons(node: Node, kind: String) -> Array:
	var found: Array=[]
	for child in node.get_children():
		if child is Button and child.get_meta("command_kind","")==kind: found.append(child)
		found.append_array(buttons(child,kind))
	return found

## 原生文本合并仅用于费用和退出含义检查，不检查视觉布局。
func words(node: Node) -> String:
	var result: String=node.text if node is Label else ""
	for child in node.get_children(): result+="\n"+words(child)
	return result

## 每种迟到变化都保持提交数为零，包括source同摘要变化、换Kernel与关闭后再挂回。
func check_sessions() -> void:
	for mode in ["normal","revision","epoch","actor","source","kernel","hidden","closed","busy","storage","quote","denied"]:
		var host=make_host(); var box=VBoxContainer.new(); host.add_child(box); var session=Session.create(host,box)
		var button: Button=session.command(box,"继续","advance",{})
		match mode:
			"revision": host.kernel.state.revision+=1
			"epoch": host._scene_counter+=1
			"actor": host.kernel.state.player_actor_id="other"
			"source": host.kernel.data.texts.changed="different"
			"kernel": host.kernel=QueryKernel.new()
			"hidden": box.hide()
			"closed": host.remove_child(box); host.add_child(box)
			"busy": host._advancing=true
			"storage": host._storage_intent={"pending":true}
			"quote": host.kernel.price+=1
			"denied": host.kernel.blocked=true
		button.pressed.emit(); button.pressed.emit()
		check(host.kernel.sent.size()==(1 if mode=="normal" else 0),"晚回调/双击与会话边界："+mode)
		if mode=="normal": check(host.kernel.sent[0].expected_revision==4 and host.kernel.sent[0].request_seq==1,"提交保留显示时版本和原序号")
		host.queue_free()

## 报名明确时间/费用和模式，重新选时长立即使原报名节点失效。
func check_courses() -> void:
	var host=make_host(); var context: Dictionary={"role_id":"teacher","provider_actor_id":"tutor","place_id":"camp"}
	var box: Control=Study.render_courses(host,host,context)
	check(words(box).contains("总费用60钱") and words(box).contains("未提供部分释放"),"显示正式课程费用和部分结算说明")
	var actions: Array=buttons(box,"study_skill"); check(actions.size()==2,"自动和手动课程都有真实报名入口")
	var previous: Button=actions[1]; var duration: OptionButton=box.get_node("StudyDuration")
	duration.select(3); duration.item_selected.emit(3)
	check(host.kernel.ticks_read==5,"时长选项调用原课程查询")
	previous.pressed.emit(); check(host.kernel.sent.is_empty(),"局部时长重绘后旧报名按钮不可提交")
	actions=buttons(box,"study_skill"); actions[1].pressed.emit()
	check(host.kernel.sent.size()==1 and host.kernel.sent[0].params=={"context":context,"skill_id":"rites","ticks":5,"challenge_mode":"manual"},"手动报名精确透传岗位与时长，不代写XP")
	host.queue_free()

## 礼议、筹算与军阵都只把公开ID/步次交原owner；终态没有重开按钮。
func check_challenges() -> void:
	for scope in ["etiquette","bargain","training","trade"]:
		var host=make_host(); host.kernel.challenge_kind="bargain" if scope in ["bargain","trade"] else "etiquette"
		var owner: String="study" if scope in ["etiquette","bargain"] else scope
		var box: Control=Challenge.render(host,host,owner,"source:1")
		var kind: String="military_drill_draft" if scope=="training" else "trade_bargain_choose" if scope=="trade" else "study_challenge_choose"
		var choices: Array=buttons(box,kind); check(not choices.is_empty(),"题面有实际选择入口："+scope)
		choices[0].pressed.emit(); check(host.kernel.sent.size()==1 and host.kernel.sent[0].kind==kind,"题面选择仅发明确owner命令："+scope)
		if scope=="training": check(host.kernel.sent[0].params=={"action_id":"source:1","expected_step":1,"group_id":"foot","target":[1,0]},"军阵草案绑定实际步次与公开目标")
		else: check(host.kernel.sent[0].params.step_id==("node.one" if scope=="etiquette" else "round.one"),"公开节点/轮次精确透传")
		host.queue_free()
	for kind in ["military_drill_confirm","military_drill_cancel_pending","military_drill_exit"]:
		var host=make_host(); host.kernel.pending={"group_id":"foot","target":[1,0]}
		var box: Control=Challenge.render(host,host,"training","action:1")
		buttons(box,kind)[0].pressed.emit()
		check(host.kernel.sent[0].params.action_id=="action:1","军阵确认/撤销/退出固定原action："+kind)
		if kind!="military_drill_exit": check(host.kernel.sent[0].params.expected_pending==host.kernel.pending,"提交精确pending防覆盖新草案")
		host.queue_free()
	var host=make_host(); host.kernel.phase="completed"
	var box: Control=Challenge.render(host,host,"study","action:1")
	check(buttons(box,"study_challenge_choose").is_empty() and buttons(box,"study_challenge_exit").is_empty() and words(box).contains("120.00%"),"终态只显示owner结果，不重开或重新评分")
	host.queue_free()

## 真实议价始终绑定原商品数量上下文，确认只传quote与当前确认金额，课程分数不会混入交易。
func check_bargaining() -> void:
	var request: Dictionary={"shop_id":"shop","context":{"role_id":"merchant","provider_actor_id":"seller","place_id":"camp"},"item_id":"grain","quantity":4,"side":"buy","payer_wallet_id":"personal","quoted_total":60,"market_revision":2}
	var host=make_host(); var box: Control=Bargain.render(host,host,request)
	buttons(box,"trade_bargain_open")[0].pressed.emit()
	var expected: Dictionary=request.duplicate(true); expected.erase("quoted_total"); expected.erase("market_revision")
	check(host.kernel.sent[0].params=={"trade":expected,"mode":"manual"},"开启议价只绑定原真实货单，不从旧金额派生折扣")
	host.queue_free()
	host=make_host(); host.kernel.phase="completed"; host.kernel.state.facts["quote:1"]={"kind":"trade_bargain","actor_id":"player","basis_sha256":"basis"}
	box=Bargain.render(host,host,request)
	check(buttons(box,"trade_bargain_open").is_empty() and words(box).contains("议价后合计60钱"),"已有实例显示原结果与正式现报价，不重抽")
	buttons(box,"trade")[0].pressed.emit()
	check(host.kernel.sent[0].params=={"bargain_quote_id":"quote:1","quoted_total":60,"market_revision":2},"成交仅提交已绑定quote身份和确认总价，不注入任意折扣")
	host.queue_free()

## 本营训练候选以真实公职和单位字段投影，不把没有organization字段的运行office当作者定义。
func check_training_and_active() -> void:
	var host=make_host()
	host._data.offices.office={"id":"office","name_key":"本部教习","garrison_id":"garrison","organization_id":"org"}
	host._data.garrisons.garrison={"id":"garrison","place_id":"camp","training_wallet_id":"receiver","training_drill_id":"drill"}
	host.kernel.state.offices.office={"id":"office","enabled":true,"holder_actor_id":"player","appointment_id":"action:old"}
	host.kernel.state.military_units.unit={"id":"unit","garrison_id":"garrison","organization_id":"org","fit":["soldier"],"training":20,"fatigue":0}
	var box: Control=Study.render_training(host,host)
	check(buttons(box,"military_train").size()==2,"本人已任职本营有自动及手动首课入口")
	buttons(box,"military_train")[1].pressed.emit()
	check(host.kernel.sent[0].params=={"office_id":"office","unit_id":"unit","challenge_mode":"manual"},"军阵开课只交原office/unit，不注入士兵和倍率")
	host.queue_free()
	host=make_host(); host.kernel.state.actors.player.action_id="action:1"; host.kernel.state.actors.player.id="player"
	host.kernel.state.actions["action:1"]={"id":"action:1","kind":"study_skill","remaining_ticks":1}
	host.kernel.state.facts["action:1"]={"terms":{"payer_wallet_id":"payer","fee":60},"lessons":[{}]}
	host.kernel.state.wallets.payer={"reserved":{"action:1":30}}
	box=Study.render_active(host,host)
	check(words(box).contains("已支付30钱") and words(box).contains("尚预留30钱"),"活动视图显示实际已付与预留，不另算课程费用")
	buttons(box,"study_cancel")[0].pressed.emit()
	check(host.kernel.sent[0].params=={"action_id":"action:1"},"中断课程与退出小游戏是不同命令")
	host.queue_free()

## 只跑新原生控件与会话边界，不启动Main、美术、OS窗口或旧规则套件。
func run() -> void:
	check_sessions(); check_courses(); check_challenges(); check_bargaining(); check_training_and_active()
	print("ACTIVITY_VIEWS checks=",checks," failures=",failures)
	quit(0 if failures==0 else 1)
