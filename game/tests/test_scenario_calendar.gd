## 历史起点只影响日期投影；不改变世界半日、周岁和期限的既有语义。
extends SceneTree
const Rules=preload("res://core/base/time_rules.gd")
var checks=0
var failures=0
## 纯日期测试不加载场景或运行世界阶段。
func _init(): call_deferred("run")
## 使用独立字面预期，不以待测函数生成答案。
func check(value,label):
 checks+=1
 if not value: failures+=1; printerr("FAIL:",label)
## 校验年末跨半日、整年、无历史日期和输入只读。
func run():
 var rules=Rules.new()
 check(rules.has_method("calendar_date"),"calendar projection entry exists")
 if rules.has_method("calendar_date"):
  var start={"year":189,"day_of_year":360,"half_day":1}
  var before=start.duplicate(true)
  for row in [[0,189,360,1],[1,190,1,0],[2,190,1,1],[720,190,360,1],[721,191,1,0]]:
   check(rules.call("calendar_date",row[0],start)=={"year":row[1],"day_of_year":row[2],"half_day":row[3]},"historical calendar boundary "+str(row[0]))
  check(rules.call("calendar_date",720,{})=={},"legacy date remains unspecified")
  check(start==before,"date input unchanged")
  check(Rules.age_years(720,-17280)==25 and Rules.day_index(720)==360 and Rules.deadline_met(9,10),"world age and deadlines keep same origin")
 print("SCENARIO_CALENDAR_CHECKS=",checks," FAILURES=",failures)
 quit(0 if failures==0 else 1)
