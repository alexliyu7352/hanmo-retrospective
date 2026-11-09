## 三种已知题面共用原生输入控件；仅向各自owner提交选择，绝不计算或发放世界奖励。
extends RefCounted
const Session=preload("res://ui/activity_session.gd")

## scope区分三种owner；trade可复用同次有限查询人物，其余旧入口仍在原时点取人，不授予货单资格。
static func render(host, parent: Control, scope: String, id: String, actor_id: String="") -> Control:
	var box=VBoxContainer.new(); box.name="ActivityChallenge"; parent.add_child(box)
	box.set_meta("activity_scope",scope); box.set_meta("activity_source_id",id)
	var session=Session.create(host,box)
	var actor: String=actor_id if scope=="trade" and not actor_id.is_empty() else host.kernel.command_context().actor_id
	var view: Dictionary
	match scope:
		"study": view=host.kernel.study_challenge(actor,id)
		"training": view=host.kernel.military_drill(actor,id)
		"trade": view=host.kernel.trade_bargain(actor,id)
		_: view={"ok":false,"code":"UnknownActivityScope"}
	if not view.ok:
		host._text(box,Session.reason(host,view),16); return box
	if view.get("consumed",false):
		host._text(box,"这份议价货单已经成交，不能再次使用。",17); return box
	var title: String="军阵操练" if scope=="training" else "真实货单议价" if scope=="trade" else "礼议练习" if view.get("challenge_kind")=="etiquette" else "议价筹算练习"
	host._text(box,title,22)
	if scope=="study": host._text(box,"练习只影响本课程表现；不会改变真实交易价格。退出练习采用自动成绩，课程仍继续。",15)
	elif scope=="training": host._text(box,"三组棋子为抽象训练题。首个实际半日前固定成绩；退出采用自动成绩，训练与实际供给收费仍继续。",15)
	else: host._text(box,"只绑定这份货单；退出采用自动成绩，尚不成交。需另行确认金额后才交割钱货。",15)
	if view.phase=="completed":
		var result: Dictionary=view.result
		host._text(box,"成绩 %d / 100"%result.score,20).name="ActivityScore"
		if scope=="trade": host._text(box,"折扣候选 %.2f%%；以货单当前正式报价为准。"%(float(result.discount_bp)/100.0),16)
		else: host._text(box,"表现 %.2f%%；收益仍由实际已提供课程结算。"%(float(result.performance_bp)/100.0),16)
		return box
	if scope=="training": _drill(host,box,session,id,view)
	else: _choices(host,box,session,scope,id,view)
	var params: Dictionary={"quote_id":id} if scope=="trade" else {"action_id":id}
	var kind: String="trade_bargain_exit" if scope=="trade" else "military_drill_exit" if scope=="training" else "study_challenge_exit"
	session.command(box,"退出练习，采用自动成绩" if scope!="trade" else "结束议价，采用自动成绩",kind,params).name="ActivityChallengeExit"
	return box

## 选项文字及公开条件全部取正式view，界面不推断供需、利益或评分。
static func _choices(host, box: Control, session, scope: String, id: String, view: Dictionary) -> void:
	host._text(box,"第 %d / 3 轮"%view.round,18)
	host._text(box,view.get("topic",view.get("prompt","")),20)
	if view.has("stance"): host._text(box,view.stance,17)
	if view.has("budget"): host._text(box,"题面预算 %d钱 · 数量 %d"%[view.budget,view.quantity],16)
	for condition in view.get("conditions",[]): host._text(box,("符合 · " if condition.matched else "不符合 · ")+condition.text,16)
	var step: String=view.get("node_id",view.get("round_id",""))
	for option in view.options:
		var params: Dictionary={"quote_id":id,"step_id":step,"option_id":option.id} if scope=="trade" else {"action_id":id,"step_id":step,"option_id":option.id}
		session.command(box,option.text,"trade_bargain_choose" if scope=="trade" else "study_challenge_choose",params).set_meta("activity_option_id",option.id)

## 固定5×4原生格子只选题面公开合法目标；待确认草案和撤销都仍提交原军阵命令。
static func _drill(host, box: Control, session, id: String, view: Dictionary) -> void:
	host._text(box,"第 %d / 4 步 · %s"%[view.step,view.prompt],18)
	for objective in view.objectives.values(): host._text(box,objective.text+" · 25分",16)
	var occupied: Dictionary={}
	for group in view.groups: occupied[str(group.position)]=group.text
	var board=GridContainer.new(); board.name="ActivityDrillBoard"; board.columns=5; box.add_child(board)
	for y in range(4):
		for x in range(5): host._text(board,"(%d,%d) %s"%[x,y,occupied.get(str([x,y]),"·")],15)
	if view.pending is Dictionary:
		host._text(box,"待确认：%s → %s"%[view.pending.group_id,str(view.pending.target)],17)
		var params: Dictionary={"action_id":id,"expected_step":view.step,"expected_pending":view.pending.duplicate(true)}
		session.command(box,"确认这一步","military_drill_confirm",params).name="ActivityDrillConfirm"
		session.command(box,"撤销尚未确认的草案","military_drill_cancel_pending",params).name="ActivityDrillUndoDraft"
	else:
		for group in view.groups:
			host._text(box,group.text+" · 当前位置 "+str(group.position),17)
			for target in group.targets:
				var params: Dictionary={"action_id":id,"expected_step":view.step,"group_id":group.id,"target":target.duplicate()}
				session.command(box,"拟移至 "+str(target),"military_drill_draft",params).set_meta("activity_group_id",group.id)
