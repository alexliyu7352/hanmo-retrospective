## 用真实对白命令与解释器验证两项委托的输入边界；状态夹具不冒充旅行或结算履约。
extends SceneTree
const Dialogue=preload("res://core/dialogue_runner.gd")
const Commands=preload("res://core/domains/interaction/dialogue_commands.gd")
const Discovery=preload("res://core/domains/interaction/discovery_commands.gd")
const Knowledge=preload("res://core/domains/tasks/task_knowledge.gd")
const PLAYER="actor.player"
var data: Dictionary={}
var checks: int=0
var failures: int=0

## 直接读取当前作者源，避免将旧编译目录误当作本次验证输入。
func _init() -> void:
	var source: String=ProjectSettings.globalize_path("res://").path_join("../content/foundation/alpha_life.json")
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--source="): source=arg.trim_prefix("--source=")
	var authored: Variant=JSON.parse_string(FileAccess.get_file_as_string(source))
	if not authored is Dictionary: printerr("Cannot read author source: ",source); quit(2); return
	for key in authored:
		if authored[key] is Array:
			data[key]={}
			for row in authored[key]:
				if row is Dictionary and row.has("id"): data[key][row.id]=row
		else: data[key]=authored[key]
	call_deferred("run_checks")

## 每项失败保留条件组合与实际节点，不能因脚本退出而丢掉原红证据。
func check(value: bool,label: String) -> bool:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)
	return value

## 仅建立对白需要的双方在场与任务状态；资金固定用于观察纯对白不结算。
func fixture(letter: String,escort: String) -> Dictionary:
	var state: Dictionary={"tick":0,"next_instance_id":1,"actors":{},"roles":{},"tasks":{},"knowledge":{PLAYER:{}},"interactions":{},"wallets":{"wallet.player":{"balance":900}},"flags":{}}
	for id in [PLAYER,"actor.shen"]:
		state.actors[id]={"id":id,"alive":true,"available":true,"action_id":"","location":{"kind":"Settlement","place_id":"west.tavern"}}
	state.roles["role.sender"]={"actor_id":"actor.shen","enabled":true}
	for pair in [["offer.letter",letter],["offer.escort",escort]]:
		if pair[1]!="unknown": state.knowledge[PLAYER][pair[0]]={"kind":"actor","id":"actor.shen"}
		if not pair[1] in ["unknown","known"]:
			state.tasks[pair[0]]={"id":pair[0],"offer_id":pair[0],"assignee":PLAYER,"serial":1,"status":pair[1],"accepted_tick":1}
	return state

## 按生产解释器处理自动发现，真实来源校验及knowledge写入均复用现有拥有者。
func drive(state: Dictionary,actor: String) -> Dictionary:
	for _step in range(32):
		var result: Dictionary=Dialogue.pump(state,data,actor)
		if not result.ok: return result
		var session: Dictionary=state.interactions[actor]
		if session.phase!="action" or session.pending_command.kind!="discover_offer": return result
		var request: Dictionary=session.pending_command.duplicate(true)
		request.context=session.context.duplicate(true)
		var offer: Dictionary=Discovery.preview(state,data,actor,request.kind,request)
		if offer.ok: Discovery.apply(state,data,actor,request.kind,request,offer,Knowledge.remember)
		Dialogue.resolve(state,data,actor,offer)
	return {"ok":false,"code":"TestDriveLimit"}

## 继续与选择使用正式preview/apply，错误选项不能直接跳转作者节点。
func send(state: Dictionary,kind: String,params: Dictionary={}) -> bool:
	var offer: Dictionary=Commands.preview(state,data,PLAYER,kind,params)
	if not check(offer.ok,"preview "+kind+" "+str(offer)): return false
	var result: Dictionary=Commands.apply(state,data,PLAYER,kind,params,offer,drive)
	return check(result.ok,"apply "+kind+" "+str(result))

## 将问候或回应推进至可选事务，遇到提前结束立即报出具体遮蔽节点。
func reach_menu(state: Dictionary,label: String) -> bool:
	for _step in range(8):
		var view: Dictionary=Dialogue.view(state,data,PLAYER)
		if view.phase=="choice": return check(view.node_id=="menu",label+" 事务菜单 "+str(view))
		if not check(view.phase=="say",label+" 回应后仍可办事务 "+str(view)): return false
		if not send(state,"dialogue_next"): return false
	return check(false,label+" 事务菜单步数超限")

## 读取可见选项，不用图结构猜测条件在实际解释器中的结果。
func option_ids(state: Dictionary) -> Array:
	var result: Array=[]
	for option in Dialogue.view(state,data,PLAYER).get("options",[]): result.append(option.id)
	return result

## 送信各状态和护送各状态交叉验证，并在同一世界重复再访以捕捉永久遮蔽。
func state_matrix() -> void:
	for letter in ["unknown","known","accepting","active","ready","success","failed","cancelled"]:
		for escort in ["unknown","known","accepting","active","ready","success","failed","cancelled"]:
			var state: Dictionary=fixture(letter,escort)
			var money: Dictionary=state.wallets.duplicate(true)
			var tasks: Dictionary=state.tasks.duplicate(true)
			for visit in range(2):
				var label: String="letter="+letter+" escort="+escort+" visit="+str(visit)
				if not send(state,"talk",{"role_id":"role.sender"}): continue
				if letter=="success": check(Dialogue.view(state,data,PLAYER).text==data.texts["letter.remember"],label+" 保留真实送信回访")
				if not reach_menu(state,label): continue
				check("escort" in option_ids(state),label+" 护送入口可见")
				check(("details" in option_ids(state))==(letter!="success"),label+" 送信成功不重复招领旧信")
				check(("report" in option_ids(state))==(letter=="ready"),label+" 送信回报独立")
				if not send(state,"dialogue_choose",{"option_id":"escort"}): continue
				var view: Dictionary=Dialogue.view(state,data,PLAYER)
				var expected: String={"unknown":"escort.explain","known":"escort.explain","active":"escort.progress","accepting":"escort.progress","cancelled":"escort.failed","ready":"escort.ready","success":"escort.remember","failed":"escort.failed"}[escort]
				check(view.node_id==expected,label+" 护送只按自身状态 "+str(view))
				if view.phase!="say" or not send(state,"dialogue_next"): continue
				var options: Array=option_ids(state)
				check(("accept" in options)==(escort in ["unknown","known"]),label+" 只在可受托时接受")
				check(("report" in options)==(escort=="ready"),label+" 只在待交接时回报")
				check("back" in options,label+" 能回到其他事务")
				if not "accept" in options: check(Commands.preview(state,data,PLAYER,"dialogue_choose",{"option_id":"accept"}).get("code")=="InvalidDialogueInput",label+" 旧接受按钮不能绕过条件")
				if not "report" in options: check(Commands.preview(state,data,PLAYER,"dialogue_choose",{"option_id":"report"}).get("code")=="InvalidDialogueInput",label+" 旧回报按钮不能绕过条件")
				if "back" in options:
					send(state,"dialogue_choose",{"option_id":"back"})
					check(Dialogue.view(state,data,PLAYER).node_id=="menu",label+" 返回原事务菜单")
				check(state.wallets==money and state.tasks==tasks,label+" 问询和回访不付款或改变任务")

## 受领尚未拨款就中断仍按原规则可重试，主动取消的已受领任务不借对白复活。
func interrupted_acceptance() -> void:
	var state: Dictionary=fixture("success","cancelled")
	state.inventories={}; state.actions={}
	var task: Dictionary=state.tasks["offer.escort"]
	task.accepted_tick=0; task.deadline_tick=0; task.settlement_id=""; task.budget_wallet_id="wallet.interrupted"
	state.wallets[task.budget_wallet_id]={"owner":{"kind":"task","id":task.id},"balance":0,"reserved":{}}
	if not send(state,"talk",{"role_id":"role.sender"}) or not reach_menu(state,"无拨款中断"): return
	if not send(state,"dialogue_choose",{"option_id":"escort"}): return
	check(Dialogue.view(state,data,PLAYER).node_id=="escort.explain","无拨款中断仍按原规则再次说明约定")
	if send(state,"dialogue_next"): check(option_ids(state)==["accept","back","leave"],"可重试中断只有受托和返回，不显示回报")

## 真实动作成功只解释其回执；此处不执行或假造送达和资金结算。
func post_report_continuity() -> void:
	for node_id in ["reported","remember"]:
		var state: Dictionary=fixture("success","active")
		if not send(state,"talk",{"role_id":"role.sender"}): continue
		state.interactions[PLAYER].node_id=node_id
		state.interactions[PLAYER].phase="stepping"
		check(drive(state,PLAYER).ok,"恢复原有说话边界 "+node_id)
		check(reach_menu(state,node_id),"送信回应后同次会话可查办护送 "+node_id)

## 有界局部回归只报告对白状态覆盖；真实Main旅行与一次付款由独立履约门负责。
func run_checks() -> void:
	state_matrix()
	interrupted_acceptance()
	post_report_continuity()
	print("ALPHA_LETTER_ESCORT_CONTINUITY_CHECKS=",checks," FAILURES=",failures)
	print("alpha_letter_escort_continuity checks=",checks," failures=",failures)
	quit(1 if failures else 0)
