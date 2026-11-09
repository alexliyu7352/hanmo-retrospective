## 普通驻军训练的正式新锁Kernel接缝；真实任职/供给/课时/取消/v2，不复跑191叶矩阵或军阵未接功能。
extends SceneTree
const Kernel=preload("res://core/world_kernel.gd")
const Codec=preload("res://core/save_codec.gd")
const Interaction=preload("res://core/interaction_rules.gd")
var checks: int=0
var failures: int=0
const UNIT="base::unit.study"
const OFFICE="base::office.study"
const PAY="base::wallet.camp"
const RECEIVE="base::wallet.office"

## 单独运行准备器对应工程，缺件明确失败，不触图形或用户银行。
func _init() -> void:
	call_deferred("run")

## 前置失败立即停止后续解引用，所有检查都计入真实退出码。
func check(value: bool, label: String) -> bool:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)
	return value

## 所有操作沿当前revision与request_seq，不直接变更军官、部队或费用。
func send(k, kind: String, params: Dictionary={}) -> Dictionary:
	var state: Dictionary=k.snapshot(); var actor: String=state.player_actor_id
	return k.submit({"kind":kind,"params":params,"actor_id":actor,"expected_revision":state.revision,"request_seq":state.request_sequences[actor]+1})

## 正式任职后先一课取消，再完整四课；两次记录共同解释当前训练/疲劳真值。
func run() -> void:
	var index: Dictionary=JSON.parse_string(FileAccess.get_file_as_string("res://data/generated/installed-content.json"))
	var path: String="res://data/generated/"+index.sets[0].directory+"/catalog.hmcatalog.json"
	var k=Kernel.new();var made: Dictionary=k.new_game("base::scenario.procurement",path)
	if not check(made.ok,"真实作者profile经Kernel初始化 "+str(made)): finish();return
	var state: Dictionary=k.snapshot();var actor: String=state.player_actor_id
	check(state.military_units[UNIT].training==10 and state.military_units[UNIT].fatigue==0,"仅采用明确作者训练/疲劳初值")
	var context: Dictionary=Interaction.for_role(state,k.content().data,"base::role.office")
	var entered: Dictionary=send(k,"office_accept",{"office_id":OFFICE,"context":context})
	if not check(entered.ok,"原当面任职真实开工 "+str(entered)): finish();return
	var step: Dictionary=send(k,"advance")
	if not check(step.ok and k.snapshot().offices[OFFICE].holder_actor_id==actor,"真实半日完成任命来源 "+str(step)): finish();return
	var begin: Dictionary=send(k,"military_train",{"office_id":OFFICE,"unit_id":UNIT})
	if not check(begin.ok,"新训练正式注册行动 "+str(begin)): finish();return
	state=k.snapshot();var action_id: String=state.actors[actor].action_id
	check(state.wallets[PAY].reserved[action_id]==150 and state.military_units[UNIT].training_action_id==action_id,"同事务预留真实150与唯一unit")
	step=send(k,"advance")
	if not check(step.ok,"真实军粮后再提供首课 "+str(step)): finish();return
	state=k.snapshot()
	check(state.wallets[PAY].balance==463 and state.wallets[RECEIVE].balance==5037 and state.wallets[PAY].reserved[action_id]==113,"首课仅付37，余款113仍本行动预留")
	check(state.military_units[UNIT].training==10 and state.military_units[UNIT].fatigue==4,"累计差额首课0训练与作者4疲劳，无人物疲劳替代")
	var saved: Dictionary=k.save_context();var decoded: Dictionary=Codec.decode(Codec.encode(saved.state,1,"military-training",saved.catalog,saved.registry),saved.catalog,saved.registry)
	if not check(decoded.ok and decoded.state==state,"全Schema中途v2含供给/钱与课程来源 "+str(decoded.get("code",""))): finish();return
	var restored=Kernel.new();var recovery: Dictionary=restored.restore_with_catalog(decoded.state,saved.catalog,saved.registry)
	if not check(recovery.ok and restored.snapshot()==state,"另一Kernel恢复真实正在进行训练"): finish();return
	var cancel: Dictionary={"kind":"military_train_cancel","params":{"action_id":action_id},"actor_id":actor,"expected_revision":state.revision,"request_seq":state.request_sequences[actor]+1}
	var cancelled: Dictionary=restored.submit(cancel)
	if not check(cancelled.ok,"本人精确忙碌取消 "+str(cancelled)): finish();return
	state=restored.snapshot()
	check(state.actions.is_empty() and state.military_units[UNIT].training_action_id=="" and state.wallets[PAY].reserved.is_empty(),"原release清真人/单位/未用预留")
	check(state.wallets[PAY].balance==463 and restored.submit(cancel)==cancelled and restored.snapshot()==state,"已提供费用不退两次，重复请求原回执")
	begin=send(restored,"military_train",{"office_id":OFFICE,"unit_id":UNIT})
	if not check(begin.ok,"取消后可开始新一期真实课程"): finish();return
	for n in range(4):
		step=send(restored,"advance")
		if not check(step.ok,"完整四课逐半日 "+str(n)+" "+str(step)): finish();return
	state=restored.snapshot()
	check(state.military_units[UNIT].training==13 and state.military_units[UNIT].fatigue==20,"仅真实兵种0级正常3训练增量，保先前已提供疲劳")
	check(state.wallets[PAY].balance==313 and state.wallets[RECEIVE].balance==5187 and state.wallets[PAY].reserved.is_empty(),"4课37/38/37/38总150且全程公共钱守恒")
	check(state.actions.is_empty() and state.military_units[UNIT].training_action_id=="","完成不残留unit或人物占用")
	saved=restored.save_context();decoded=Codec.decode(Codec.encode(saved.state,2,"military-training-done",saved.catalog,saved.registry),saved.catalog,saved.registry)
	check(decoded.ok and decoded.state==state,"两期真实历史训练累计值完整v2可读")
	finish()

## 结论仅普通训练共享接线，不把军阵题面或更大军务循环记为已过。
func finish() -> void:
	print("MILITARY_TRAINING_KERNEL_CHECKS=",checks," FAILURES=",failures)
	quit(0 if failures==0 else 1)
