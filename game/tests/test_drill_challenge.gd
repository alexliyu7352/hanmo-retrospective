## 军阵操练纯叶门只核固定题面及步序；不启动战场、不创建士卒，不冒充真实训练接入。
extends SceneTree
const Drill=preload("res://core/domains/cultivation/drill_challenge.gd")
const Strict=preload("res://core/strict_json.gd")
var checks: int=0
var failures: int=0
var definition: Dictionary
var texts: Dictionary
const BINDING={"actor_id":"actor.teacher","source_id":"action:14"}
const SNAPSHOT={"skill_level":2,"auxiliary_attribute":60}

## 从小作者文件走真实严格JSON加载，不生成随机大地图或运行GUI。
func _init() -> void:
	definition=Strict.parse_text(FileAccess.get_file_as_string("res://../tests/fixtures/drill_challenge/definition.json")).value
	texts=Strict.parse_text(FileAccess.get_file_as_string("res://../tests/fixtures/drill_challenge/texts.json")).value
	call_deferred("run_checks")

## 所有断言保留来源标签和真实失败数，运行时诊断另行共同核查。
func check(value: bool, label: String) -> bool:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)
	return value

## 固定owner来源与seed创建值；真实military_training资格及消费位未在此伪造。
func create(mode: String="manual") -> Dictionary:
	return Drill.create(definition,texts,BINDING,SNAPSHOT,"53218",mode).state

## 一次明确草拟和确认才算一步，原值每次保持不变。
func move(state: Dictionary, group_id: String, target: Array) -> Dictionary:
	var step: int=state.moves.size()+1
	var drafted: Dictionary=Drill.draft_move(definition,texts,state,step,group_id,target)
	if not check(drafted.ok,"公开合法格可草拟 "+str(drafted)): return {}
	var confirmed: Dictionary=Drill.confirm_move(definition,texts,drafted.state,step,drafted.state.pending)
	if not check(confirmed.ok,"精确待确认步可以确认 "+str(confirmed)): return {}
	return confirmed.state

## 只重放作者四步见证，不做大空间求解，验证见证和玩家路径使用同一函数。
func solve() -> Dictionary:
	var state: Dictionary=create()
	for choice in definition.solution: state=move(state,choice.group_id,choice.target)
	return state

## 不合法模板、坐标、组类型、未知规则和无解见证在开始前拒绝。
func authoring() -> void:
	check(Drill.validate_definition(definition,texts).get("maximum_raw_score")==100,"作者四步见证同规则达到100")
	var bad: Dictionary=definition.duplicate(true); bad.script="spawn_soldiers()"
	check(not Drill.validate_definition(bad,texts).ok,"拒绝任意脚本与造兵字段")
	bad=definition.duplicate(true); bad.groups.pop_back()
	check(not Drill.validate_definition(bad,texts).ok,"恰好三个抽象编组")
	bad=definition.duplicate(true); bad.groups[1].id=bad.groups[0].id
	check(not Drill.validate_definition(bad,texts).ok,"编组ID唯一")
	bad=definition.duplicate(true); bad.groups[1].origin=bad.groups[0].origin
	check(not Drill.validate_definition(bad,texts).ok,"起点不重叠")
	bad=definition.duplicate(true); bad.groups[0].targets[0]=[5,0]
	check(not Drill.validate_definition(bad,texts).ok,"横坐标仅0到4")
	bad=definition.duplicate(true); bad.groups[0].targets[0]=[1,4]
	check(not Drill.validate_definition(bad,texts).ok,"纵坐标仅0到3")
	bad=definition.duplicate(true); bad.groups[0].targets[0]=[1.0,0]
	check(not Drill.validate_definition(bad,texts).ok,"坐标拒绝浮点")
	bad=definition.duplicate(true); bad.groups[0].targets.append(bad.groups[0].targets[0])
	check(not Drill.validate_definition(bad,texts).ok,"合法目标集合不能重复")
	bad=definition.duplicate(true); bad.groups[1].kind="foot"
	check(not Drill.validate_definition(bad,texts).ok,"公开编组种类不能冒用远程/骑兵目标")
	bad=definition.duplicate(true); bad.objectives.supply.points=100
	check(not Drill.validate_definition(bad,texts).ok,"每项目标固定25，不接受作者额外分")
	bad=definition.duplicate(true); bad.objectives.screen.front_group_id="missing"
	check(not Drill.validate_definition(bad,texts).ok,"目标只能引用本题编组")
	bad=definition.duplicate(true); bad.solution.pop_back()
	check(not Drill.validate_definition(bad,texts).ok,"见证必须四次合法确认移动")
	bad=definition.duplicate(true); bad.solution[0].target=[0,3]
	check(not Drill.validate_definition(bad,texts).ok,"见证也不能落到被占目标格")
	bad=definition.duplicate(true); bad.solution[1].target=[3,3]
	check(Drill.validate_definition(bad,texts).get("code")=="UnsolvedDrillTemplate","见证入劣地不算可解100题面")
	var missing: Dictionary=texts.duplicate(); missing.erase(definition.objectives.cavalry.text_key)
	check(not Drill.validate_definition(definition,missing).ok,"全部目标必须真实公开显示")
	bad=definition.duplicate(true); bad.id+="\n"
	check(not Drill.validate_definition(bad,texts).ok,"完整ID拒绝尾换行")

## 草拟、撤销和确认边界可精确区分，不允许旧确认误走新一步或撤销已确认轨迹。
func pending_and_scoring() -> void:
	var state: Dictionary=create(); var original: Dictionary=state.duplicate(true)
	var view: Dictionary=Drill.view(definition,texts,state)
	check(view.ok and view.width==5 and view.height==4 and view.groups.size()==3 and not view.has("solution") and state==original,"公开棋盘但不暴露作者解证据，查询不改状态")
	check(not Drill.draft_move(definition,texts,state,1,"guard",[0,3]).ok,"不能覆盖远程当前位置")
	check(not Drill.draft_move(definition,texts,state,2,"guard",[1,0]).ok,"旧/跳步号拒绝")
	check(not Drill.draft_move(definition,texts,state,1.0,"guard",[1,0]).ok,"步号同样要求整数")
	var drafted: Dictionary=Drill.draft_move(definition,texts,state,1,"horse",[3,3])
	check(drafted.ok and drafted.state.moves.is_empty() and state==original,"可草拟劣地选项但未确认不计轨迹")
	var cancelled: Dictionary=Drill.cancel_pending(definition,texts,drafted.state,1,drafted.state.pending)
	check(cancelled.ok and cancelled.state==original,"撤销未确认目标回到原值")
	var changed: Dictionary=Drill.draft_move(definition,texts,drafted.state,1,"guard",[1,0])
	check(not Drill.confirm_move(definition,texts,changed.state,1,drafted.state.pending).ok,"旧待确认内容不能误确认新目标")
	var first: Dictionary=Drill.confirm_move(definition,texts,changed.state,1,changed.state.pending)
	check(first.ok and first.state.moves.size()==1 and first.state.pending==null,"确认才追加一次步序")
	check(not Drill.cancel_pending(definition,texts,first.state,1,changed.state.pending).ok,"已经确认的步不能被旧撤销重写")
	var best: Dictionary=solve()
	check(best.result.raw_score==100 and best.result.score==100 and best.result.objectives=={"supply":true,"passage":true,"screen":true,"cavalry":true},"四项各25且无礼议/议价额外基础分")
	check(best.result.performance_bp==12000 and Drill.scaled_result(definition,texts,best,7).value==8,"仅返回≤120%的整数正常基线修正")
	var bad_path: Dictionary=create()
	bad_path=move(bad_path,"guard",[2,1]); bad_path=move(bad_path,"ranged",[2,2]); bad_path=move(bad_path,"horse",[3,3]); bad_path=move(bad_path,"horse",[4,1])
	check(bad_path.result.raw_score==75 and not bad_path.result.objectives.cavalry,"骑兵离开劣地也不抹掉已确认轨迹，前三仍按终盘")
	var automatic: Dictionary=create("automatic")
	check(automatic.result.score==59 and automatic.result.performance_bp==10000 and automatic.result.objectives==null,"自动只取本人基线，不伪造战术目标达成")
	var exiting: Dictionary=Drill.draft_move(definition,texts,first.state,2,"horse",[3,3])
	var exited: Dictionary=Drill.exit_challenge(definition,texts,exiting.state)
	check(exited.ok and exited.state.result.score==59 and exited.state.pending==null and exited.state.moves.size()==1,"中途退出按自动分，待确认不进入记录")
	check(Drill.exit_challenge(definition,texts,best).state==best and not Drill.draft_move(definition,texts,best,1,"guard",[1,0]).ok,"完成不能重开换模式或刷新成绩")

## 保存完整待选和已确认轨迹，源变化与任何浮点/积分/起点伪造明确拒绝。
func saving() -> void:
	var state: Dictionary=move(create(),"guard",[1,0])
	state=Drill.draft_move(definition,texts,state,2,"horse",[4,1]).state
	var decoded: Dictionary=Strict.parse_text(JSON.stringify(state))
	check(decoded.ok and decoded.value==state and Drill.validate_state(definition,texts,decoded.value).ok,"包含待确认步的StrictJSON原样可读")
	var first: Dictionary=Drill.confirm_move(definition,texts,state,2,state.pending)
	var second: Dictionary=Drill.confirm_move(definition,texts,decoded.value,2,decoded.value.pending)
	check(first==second and first.state.seed=="53218","存读后同一步同结果，不重新抽种")
	var best: Dictionary=solve()
	var changed: Dictionary=texts.duplicate(); changed[definition.prompt_key]+="变更"
	check(Drill.validate_state(definition,changed,best).get("code")=="DrillSourceChanged","公开来源变化不沿用旧成绩")
	var forged: Dictionary=best.duplicate(true); forged.result.score=101
	check(not Drill.validate_state(definition,texts,forged).ok,"拒绝伪造终态分数")
	forged=best.duplicate(true); forged.moves[0].from=[1,0]
	check(not Drill.validate_state(definition,texts,forged).ok,"保存起点必须等于此前真实抽象位置")
	forged=best.duplicate(true); forged.result.choices[0].target[0]=1.0
	check(not Drill.validate_state(definition,texts,forged).ok,"结果内重复步序也严格拒绝浮点")
	forged=best.duplicate(true); forged.result.objectives.supply=1
	check(not Drill.validate_state(definition,texts,forged).ok,"目标真值不可用整数冒充布尔")
	forged=best.duplicate(true); forged.pending={"group_id":"guard","target":[1,1]}
	check(not Drill.validate_state(definition,texts,forged).ok,"完成终态不能携带新待选步")
	forged=best.duplicate(true); forged.seed="053218"
	check(not Drill.validate_state(definition,texts,forged).ok,"种子身份拒绝非规范文本")
	var detached: Dictionary=Drill.view(definition,texts,best)
	detached.result.choices.clear()
	check(best.moves.size()==4 and best.result.choices.size()==4,"视图只给副本，不能回改实例")

## 只跑本纯叶有限边界，不重复旧课程/议价门，也不把结果标成真实部队训练完成。
func run_checks() -> void:
	authoring(); pending_and_scoring(); saving()
	print("DRILL_CHALLENGE_CHECKS=%d FAILURES=%d"%[checks,failures])
	quit(0 if failures==0 else 1)
