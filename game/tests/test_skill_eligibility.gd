## 只核普通XP派生与新显式武学前置；不替代战斗/存档集成或异步成长证明。
extends SceneTree
const Levels=preload("res://core/domains/cultivation/skill_levels.gd")
const Eligibility=preload("res://core/domains/cultivation/skill_eligibility.gd")
var checks: int=0
var failures: int=0

## 纯规则门延后执行，不加载内容、世界或资源。
func _init() -> void:
	call_deferred("_run")

## 每条错误计数并导致非零退出。
func check(value: bool, label: String) -> void:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)

## 用精确阈值、不同真实XP/属性及深复制反例证明资格没有第二可写等级。
func _run() -> void:
	for row in [[0,0],[99,0],[100,1],[299,1],[300,2],[649,2],[650,3],[1199,3],[1200,4]]:
		check(Levels.level(row[0])==row[1],"固定累计经验门槛 "+str(row))
	for bad in [-1,1201,1.0,true,null,"650"]: check(Levels.level(bad)==-1,"非法XP拒绝 "+str(bad))
	var actor: Dictionary={"attributes":{"martial":110},"skills":{"martial":649,"tactics":0},"combat":{"learned":{"plain":80,"counter":100,"heart":90},"heart_id":"heart"}}
	var requirement: Dictionary={"required_skill_level":{"skill_id":"martial","level":3}}
	check(not Eligibility.check(actor,requirement).ok,"武力属性110不能冒武艺XP三级")
	actor.skills.martial=650
	check(Eligibility.check(actor,requirement).ok,"真实650XP可用三级技能")
	check(Eligibility.check({},{}).ok,"无前置旧技能不收紧")
	check(not Eligibility.check(actor,{"required_skill_level":{"skill_id":"intelligence","level":3}}).ok,"技能只接受现有十项")
	check(not Eligibility.check(actor,{"required_skill_level":{"skill_id":"martial","level":3.0}}).ok,"前置级别拒浮点")
	var data: Dictionary={"personal_skills":{"plain":{},"counter":requirement,"heart":{"required_skill_level":{"skill_id":"tactics","level":3}}}}
	var before: Dictionary=actor.duplicate(true)
	var snapshot: Dictionary=Eligibility.learned_snapshot(actor,data)
	check(snapshot=={"plain":80,"counter":100},"仅过滤不合格已学技能，保掌握度")
	check(actor==before,"人物原掌握和装备从未修改")
	check(Eligibility.validate_snapshot(actor,data,{"learned":snapshot}).ok,"新前置规划快照有真实资格来源")
	snapshot.counter=40
	check(not Eligibility.validate_snapshot(actor,data,{"learned":snapshot}).ok,"新前置技能伪掌握度拒绝")
	check(actor==before,"快照是深副本")
	actor.skills.martial=649
	check(not Eligibility.validate_snapshot(actor,data,{"learned":{"counter":100}}).ok,"规划期混入未达门槛技能拒绝")
	print("SKILL_ELIGIBILITY_CHECKS=",checks," FAILURES=",failures)
	quit(0 if failures==0 else 1)
