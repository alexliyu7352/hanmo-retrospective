## 普通十技艺作者样本的专用新锁门；只验证真实课程边界和原三章初始化隔离，不代替长成长或实窗验收。
extends SceneTree
const Kernel=preload("res://core/world_kernel.gd")
const Codec=preload("res://core/save_codec.gd")
const Interaction=preload("res://core/interaction_rules.gd")
const PLAYER="qingstudy::actor.apprentice"
var checks: int=0
var failures: int=0
var fixture: Dictionary={}

## 拒绝在共享工程裸跑；prepare生成的专用标记与目录是本门前置。
func _init() -> void:
	call_deferred("_run")

## 记录可定位失败，检查未通过时返回false供调用者停止依赖动作。
func check(value: bool, label: String) -> bool:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)
	return value

## 所有运行变化走真实请求序号和revision，测试不直接写权威状态。
func submit(kernel, kind: String, params: Dictionary={}) -> Dictionary:
	var state: Dictionary=kernel.snapshot()
	return kernel.submit({"kind":kind,"params":params,"actor_id":PLAYER,"request_seq":state.request_sequences[PLAYER]+1,"expected_revision":state.revision})

## 仅去掉明确允许不同的三项内容身份；人物、钱粮、地点、随机与实例序号完整比较。
func gameplay_state(state: Dictionary) -> Dictionary:
	var result: Dictionary=state.duplicate(true)
	for key in ["content_id","content_version","content_digest"]: result.erase(key)
	return result

## 老三章分别由旧原件和新增后的原件初始化，不拿JSON节点相等代替真实世界相等。
func old_scenarios() -> void:
	for id in fixture.contract.baseline_scenario_sha256:
		var old=Kernel.new(); var current=Kernel.new()
		var before: Dictionary=old.new_game("qingstudy::"+id,fixture.catalogs.baseline)
		var after: Dictionary=current.new_game("qingstudy::"+id,fixture.catalogs.candidate)
		if not check(before.ok and after.ok,"旧章双目录正常初始化 "+id+" "+str(before)+" "+str(after)): return
		check(gameplay_state(old.snapshot())==gameplay_state(current.snapshot()),"旧章完整有效世界未泄漏新人物/课程/资产/RNG "+id)
		check(old.snapshot().content_digest!=current.snapshot().content_digest,"新作者源仍有不同内容身份 "+id)

## 实际原样第四场景从零经验和原盘缠开始，老师资格来自本场显式初态。
func authored_sample() -> void:
	var kernel=Kernel.new()
	var result: Dictionary=kernel.new_game("qingstudy::"+fixture.contract.scenario_id,fixture.catalogs.candidate)
	if not check(result.ok,"第四样本真实新游戏 "+str(result)): return
	var state: Dictionary=kernel.snapshot()
	check(state.actors.size()==9 and state.roles.size()==15,"仅启用学员、八位真人和明确十五岗位")
	check(state.wallets[state.actors[PLAYER].wallet_id].balance==120,"样本初始盘缠仍是120，不借测试充值")
	for route in fixture.contract.routes:
		check(state.actors[PLAYER].skills[route.skill_id]==0,"样本本人未预赠技能 "+route.skill_id)
		for teacher in route.teacher_actor_ids:
			check(state.actors["qingstudy::"+teacher].skills[route.skill_id]==1200,"本场对应真人四级资格 "+teacher+" "+route.skill_id)

## 近门槛与主教师不可用是显式作者测试变体；每条成长、费用和取消仍由正常Kernel执行。
func course_case(row: Dictionary) -> void:
	var kernel=Kernel.new()
	var result: Dictionary=kernel.new_game("qingstudy::"+row.scenario_id,fixture.catalogs.boundaries)
	if not check(result.ok,"边界变体正常启动 "+row.scenario_id+" "+str(result)): return
	var initial: Dictionary=kernel.snapshot()
	var context: Dictionary=Interaction.for_role(initial,kernel.content().data,"qingstudy::"+row.role_id)
	var query: Dictionary=kernel.study_courses(PLAYER,context,2)
	if not check(query.ok,"真实在场岗位查询 "+row.scenario_id+" "+str(query)): return
	var quoted: Dictionary={}
	for course in query.courses:
		if course.skill_id==row.skill_id: quoted=course
	if not check(quoted.get("eligible",false),"当前课程具资格 "+row.scenario_id+" "+str(quoted)): return
	var fee: int=80 if row.kind=="facility" else 120
	check(quoted.offer.cost==fee and quoted.offer.duration==2 and quoted.offer.expected_xp==row.target_xp-row.xp_before,"按目标等级冻结真实价格及上限 "+row.scenario_id)
	check(not kernel.study_courses(PLAYER,context,1).ok and not kernel.study_courses(PLAYER,context,7).ok and kernel.snapshot()==initial,"越界课时拒绝且全部查询只读 "+row.scenario_id)
	if row.kind=="alternate":
		var unavailable: Dictionary=Interaction.for_role(initial,kernel.content().data,"qingstudy::"+row.unavailable_role_id)
		check(not kernel.study_courses(PLAYER,unavailable,2).ok and kernel.snapshot()==initial,"作者明确不可用主教师拒绝，另一真人可开课 "+row.skill_id)
	var params: Dictionary={"skill_id":row.skill_id,"ticks":2,"context":context}
	result=submit(kernel,"study_skill",params)
	if not check(result.ok,"正常注册开课 "+row.scenario_id+" "+str(result)): return
	var state: Dictionary=kernel.snapshot()
	var id: String=state.actors[PLAYER].action_id
	var terms: Dictionary=state.facts[id].terms
	var payer: String=terms.payer_wallet_id; var payee: String=terms.recipient_wallet_id
	check(state.wallets[payer].balance==initial.wallets[payer].balance and state.wallets[payer].reserved[id]==fee,"开始只预留真实资金 "+row.scenario_id)
	for number in range(2):
		result=submit(kernel,"advance")
		if not check(result.ok,"实际半日通过全Schema "+row.scenario_id+" "+str(number)+" "+str(result)): return
	state=kernel.snapshot()
	check(state.actors[PLAYER].skills[row.skill_id]==row.target_xp,"两课真实增长到设施/教师上限 "+row.scenario_id)
	check(state.wallets[payer].balance==initial.wallets[payer].balance-fee and state.wallets[payee].balance==initial.wallets[payee].balance+fee,"双方唯一账本守恒 "+row.scenario_id)
	check(state.actions.is_empty() and state.wallets[payer].reserved.is_empty() and state.actors[context.provider_actor_id].action_id.is_empty(),"结课双方与资金预留释放 "+row.scenario_id)
	var saved: Dictionary=kernel.save_context()
	var encoded: String=Codec.encode(saved.state,1,"ordinary-route",saved.catalog,saved.registry)
	var decoded: Dictionary=Codec.decode(encoded,saved.catalog,saved.registry)
	check(not encoded.is_empty() and decoded.ok and decoded.state==state,"完成事实v2完整Schema不改奖励 "+row.scenario_id)
	if row.skill_id=="rites" and row.kind=="facility": cancellation(row)

## 一条正常资产路径在一课后取消并重放同请求，精确证明只有已提供费用和唯一释放。
func cancellation(row: Dictionary) -> void:
	var kernel=Kernel.new()
	if not check(kernel.new_game("qingstudy::"+row.scenario_id,fixture.catalogs.boundaries).ok,"取消独立原初态启动"): return
	var initial: Dictionary=kernel.snapshot()
	var context: Dictionary=Interaction.for_role(initial,kernel.content().data,"qingstudy::"+row.role_id)
	if not check(submit(kernel,"study_skill",{"skill_id":row.skill_id,"ticks":2,"context":context}).ok,"取消前真实开课"): return
	var id: String=kernel.snapshot().actors[PLAYER].action_id
	if not check(submit(kernel,"advance").ok,"取消前只执行一课"): return
	var state: Dictionary=kernel.snapshot(); var terms: Dictionary=state.facts[id].terms
	var command: Dictionary={"kind":"study_cancel","params":{"action_id":id},"actor_id":PLAYER,"request_seq":state.request_sequences[PLAYER]+1,"expected_revision":state.revision}
	var result: Dictionary=kernel.submit(command)
	if not check(result.ok,"真实取消指令完成 "+str(result)): return
	state=kernel.snapshot()
	check(state.tick==1 and state.actors[PLAYER].skills[row.skill_id]==mini(300,row.xp_before+20),"取消只结算已提供一课20XP并遵守设施上限")
	check(state.wallets[terms.payer_wallet_id].balance==80 and state.wallets[terms.recipient_wallet_id].balance==initial.wallets[terms.recipient_wallet_id].balance+40,"已提供一课付40，未提供40只解预留")
	check(state.wallets[terms.payer_wallet_id].reserved.is_empty() and state.actions.is_empty() and state.actors[context.provider_actor_id].action_id.is_empty(),"取消释放双方及同一预留")
	check(kernel.submit(command)==result and kernel.snapshot()==state,"重复取消请求不再扣费、退钱或推进日期")

## 经真实岗位会话选择并确认已有服务；不能用直调service掩盖玩家入口缺失。
func dialogue_service(kernel, role_id: String, activity: String) -> bool:
	var talked: Dictionary=submit(kernel,"talk",{"role_id":role_id})
	if not check(talked.ok,"新岗位有可打开的角色绑定会话 "+role_id+" "+str(talked)): return false
	if not check(submit(kernel,"dialogue_choose",{"option_id":activity}).ok,"新岗位公开选项可选择 "+activity): return false
	var pending: Dictionary=kernel.snapshot().interactions[PLAYER]
	if not check(pending.phase=="action" and pending.pending_command=={"kind":"service","activity":activity},"会话只提出同一既有服务，尚未执行 "+activity): return false
	return check(submit(kernel,"dialogue_confirm").ok,"正常确认进入原服务行动 "+activity)

## 只核新增岗位复用的有限雇工/住宿接口；工资来自刚支付的学费，不声称长循环经济已验收。
func livelihood() -> void:
	for pair in [["rites","alternate"],["infantry","master"]]:
		var row: Dictionary={}
		for entry in fixture.cases:
			if entry.skill_id==pair[0] and entry.kind==pair[1]: row=entry
		var kernel=Kernel.new()
		if not check(not row.is_empty() and kernel.new_game("qingstudy::"+row.scenario_id,fixture.catalogs.boundaries).ok,"有限生计作者边界启动 "+str(pair)): return
		var initial: Dictionary=kernel.snapshot()
		var context: Dictionary=Interaction.for_role(initial,kernel.content().data,"qingstudy::"+row.role_id)
		var work: Dictionary={"activity":"work","context":context}
		check(kernel.preview("service",work).get("code")=="EmployerCannotPay" and kernel.snapshot()==initial,"零余额教习不能凭空支付工资 "+str(pair))
		if not check(submit(kernel,"study_skill",{"skill_id":row.skill_id,"ticks":2,"context":context}).ok,"真实学费流开始 "+str(pair)): return
		for index in range(2):
			if not check(submit(kernel,"advance").ok,"实际学费课时 "+str(pair)+" "+str(index)): return
		var paid: Dictionary=kernel.snapshot()
		var payer: String=paid.actors[PLAYER].wallet_id
		var role: Dictionary=kernel.content().data.roles["qingstudy::"+row.role_id]
		var employer: String=role.activities.work.wallet_id
		check(paid.wallets[payer].balance==0 and paid.wallets[employer].balance==120,"本次学费是同一雇主可用资金 "+str(pair))
		if not dialogue_service(kernel,"qingstudy::"+row.role_id,"work"): return
		var working: Dictionary=kernel.snapshot()
		check(working.wallets[employer].reserved[working.actors[PLAYER].action_id]==30 and working.wallets[payer].balance==0,"雇工先预留30不提前发薪 "+str(pair))
		if not check(submit(kernel,"advance").ok,"实际一半日领取工资 "+str(pair)): return
		var earned: Dictionary=kernel.snapshot()
		check(earned.wallets[payer].balance==30 and earned.wallets[employer].balance==90,"原钱包实际支付工资并守恒 "+str(pair))
		if not dialogue_service(kernel,"qingstudy::"+row.role_id,"rest"): return
		if not check(submit(kernel,"advance").ok,"实际住宿半日完成 "+str(pair)): return
		var rested: Dictionary=kernel.snapshot()
		check(rested.wallets[payer].balance==25 and rested.wallets[employer].balance==95 and rested.actors[PLAYER].fatigue==0 and rested.actions.is_empty() and rested.wallets[employer].reserved.is_empty(),"住宿恢复与收支沿同账完成 "+str(pair))

## 仅运行准备器定义的三十个有界课程，不长刷等级、不重跑旧叶门和不启动图形。
func _run() -> void:
	if not ProjectSettings.get_setting("application/config/ordinary_routes_isolated",false):
		printerr("Dedicated ordinary routes fixture required"); quit(1); return
	fixture=JSON.parse_string(FileAccess.get_file_as_string("res://tests/ordinary_routes.json"))
	if "--livelihood-only" in OS.get_cmdline_user_args():
		livelihood()
		print("ORDINARY_SKILL_LIVELIHOOD_CHECKS=%d FAILURES=%d"%[checks,failures])
		quit(0 if failures==0 else 1); return
	old_scenarios()
	if failures==0: authored_sample()
	if failures==0:
		for row in fixture.cases:
			course_case(row)
			if failures>0: break
	if failures==0: livelihood()
	print("ORDINARY_SKILL_ROUTES_CHECKS=%d FAILURES=%d"%[checks,failures])
	quit(0 if failures==0 else 1)
