## 检验对向轨迹、同一道路识别和确定接触时刻，不能只比较半日结束坐标。
extends SceneTree
const F=preload("res://tests/companion_fixture.gd")
var checks: int=0
var failures: int=0
## 等待正式内容可由引擎装载。
func _init() -> void:
	call_deferred("run_checks")
## 记录逐项结果，不用运行退出码掩盖几何断言失败。
func check(value: bool, label: String) -> void:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)
## 用当前行旅核心创建两条正在执行的相向轨迹。
func run_checks() -> void:
	check(ResourceLoader.exists("res://core/road_contact.gd"),"存在只读道路轨迹接触机制")
	if failures: finish(); return
	var contacts=load("res://core/road_contact.gd")
	var k=F.fresh(true)
	check(not k.snapshot().is_empty(),"包含真实旅人的内容可运行")
	if failures: finish(); return
	check(F.send(k,"travel",{"destination":"village.east"}).ok,"玩家先出发")
	check(k.advance_one_tick().ok,"推进后NPC才选择真实行程")
	var s: Dictionary=k.snapshot(); var data: Dictionary=k.content().data
	check(s.actors["actor.walker"].location.kind=="OnRoute","NPC没有瞬移到途中")
	var before: Dictionary=s.duplicate(true); var hits: Array=contacts.detect(s,data)
	check(hits.size()==1,"半日内相向穿过产生一次接触")
	if not hits.is_empty():
		check(hits[0].anchors[s.player_actor_id].progress==150 and hits[0].anchors["actor.walker"].progress==50,"双方停在同一真实道路中点")
		check(hits[0].time_num*2==hits[0].time_den,"交会发生在半日轨迹中段")
	check(s==before and contacts.detect(s,data)==hits,"重复预览不推进状态或随机流")
	var different: Dictionary=data.duplicate(true)
	different.routes["road.east_west"].shape=[[190,250]]
	check(contacts.detect(s,different).is_empty(),"平行或不同折线道路不能误判相遇")
	var a: String=s.actors[s.player_actor_id].action_id
	var b: String=s.actors["actor.walker"].action_id
	s.actions[a].params.met_actions=[b]
	check(contacts.detect(s,data).is_empty(),"已会面的旅行实例不会反复触发")
	finish()
## 输出测试范围并结束，不把几何测试计为完整图形游玩。
func finish() -> void:
	print("ROAD_CONTACT_CHECKS=",checks," FAILURES=",failures)
	quit(0 if failures==0 else 1)
