## 在真实引擎验证个人战几何及同步碰撞；不建立世界、改存档或依赖渲染帧。
extends SceneTree
const G=preload("res://core/personal_geometry.gd")
var checks: int=0
var failures: int=0

## 等待引擎初始化后运行边界与多人物路径用例。
func _init() -> void:
	call_deferred("run_checks")

## 统计实际执行的断言；失败给出可定位文本及非零退出码。
func check(value: bool, label: String) -> void:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)

## 用三种武器空间、两类障碍与多条交叉路径覆盖正式几何接口。
func run_checks() -> void:
	var arena: Dictionary={"width":600,"height":400,"obstacles":[]}
	var wall: Dictionary={"x":280,"y":140,"width":40,"height":120,"blocks_movement":true,"blocks_sight":true}
	var blocked_arena: Dictionary=arena.duplicate(true); blocked_arena.obstacles.append(wall)
	var sword: Dictionary={"minimum_range":0,"maximum_range":70,"arc_degrees":120}
	var spear: Dictionary={"minimum_range":45,"maximum_range":135,"arc_degrees":60}
	var bow: Dictionary={"minimum_range":100,"maximum_range":400,"arc_degrees":90}
	check(G.point_valid(arena,[18,18]),"圆形身体恰好触及场地边缘可站立")
	check(not G.point_valid(arena,[17,18]),"中心在内但身体越界不能站立")
	check(not G.point_valid(arena,[20.0,20]),"坐标小数不能默默变成存档整数")
	check(G.point_valid(blocked_arena,[262,200]),"身体相切障碍允许")
	check(not G.point_valid(blocked_arena,[263,200]),"身体侵入障碍拒绝")
	check(G.point_valid(blocked_arena,[267,127]),"矩形角落按圆形距离判定")
	check(not G.point_valid(blocked_arena,[268,128]),"矩形角落真实重叠不能漏过")
	check(G.can_move(arena,[100,100],[160,180],100).ok,"三四五直线恰好达到移动上限")
	check(G.can_move(arena,[100,100],[160,181],100).get("code")=="MoveTooFar","超出圆形移动范围拒绝")
	check(G.can_move(blocked_arena,[200,200],[400,200],250).get("code")=="MovementBlocked","终点合法也不能穿墙")
	check(G.can_move(blocked_arena,[200,122],[400,122],250).ok,"身体沿障碍相切路径可走")
	check(not G.can_move(blocked_arena,[200,123],[400,123],250).ok,"身体扫掠而非仅中心路径挡墙")
	check(G.can_move(arena,[100,200],[400,200],300,18,[{"position":[250,200],"radius":18}]).get("code")=="BodyBlocked","静止人物阻挡整条移动路径")
	check(G.can_move(arena,[100,200],[400,200],300,18,[{"position":[250,236],"radius":18}]).ok,"相切身体不制造虚假阻挡")
	check(G.can_move(arena,[100,100],[100,100],0).ok,"原地等待是合法零距离移动")
	check(G.can_attack(arena,[100,100],[170,100],0,sword).ok,"刀剑外射程闭边界可命中")
	check(G.can_attack(arena,[100,100],[171,100],0,sword).get("code")=="OutOfRange","刀剑外射程之外拒绝")
	check(G.can_attack(arena,[100,100],[144,100],0,spear).get("code")=="InsideMinimumRange","长兵过近不能以同一挥击攻击")
	check(G.can_attack(arena,[100,100],[145,100],0,spear).ok,"长兵内射程闭边界可命中")
	check(not G.can_attack(arena,[100,100],[170,100],0,bow).ok,"弓的近身限制与刀剑不同")
	check(G.can_attack(arena,[100,100],[400,100],360,bow).ok,"弓远射与朝向归一化正确")
	check(G.can_attack(arena,[100,100],[100,150],90,sword).ok,"顺时针九十度向下")
	check(G.can_attack(arena,[100,100],[100,150],0,sword).get("code")=="WrongFacing","朝向改变能否攻击")
	check(G.can_attack(arena,[100,100],[200,200],0,{"minimum_range":0,"maximum_range":200,"arc_degrees":90}).ok,"攻击扇区闭边界包含四十五度")
	check(G.can_attack(arena,[100,100],[30,100],-180,sword).ok,"负朝向与左向一致")
	check(G.can_attack(blocked_arena,[100,200],[400,200],0,bow).get("code")=="LineBlocked","弓的射程满足仍受视线阻挡")
	check(not G.line_of_sight(blocked_arena,[100,140],[400,140]),"视线触及实体边缘仍被阻挡")
	var screen: Dictionary=blocked_arena.duplicate(true); screen.obstacles[0].blocks_movement=false
	check(G.can_move(screen,[200,200],[400,200],250).ok and not G.line_of_sight(screen,[200,200],[400,200]),"可通行遮视障碍两个属性独立")
	var fence: Dictionary=blocked_arena.duplicate(true); fence.obstacles[0].blocks_sight=false
	check(not G.can_move(fence,[200,200],[400,200],250).ok and G.line_of_sight(fence,[200,200],[400,200]),"挡路透视障碍两个属性独立")
	var units: Dictionary={"actor.a":{"position":[100,200],"radius":18},"actor.b":{"position":[300,200],"radius":18}}
	var plans: Dictionary={"actor.a":{"destination":[300,200]},"actor.b":{"destination":[100,200]}}
	var before: Dictionary={"arena":arena.duplicate(true),"units":units.duplicate(true),"plans":plans.duplicate(true)}
	var result: Dictionary=G.resolve_moves(arena,units,plans)
	check(result.blocked==["actor.a","actor.b"],"对向交换位置双方在接触前停止")
	check(result.positions["actor.a"][0]<result.positions["actor.b"][0] and result.positions["actor.b"][0]-result.positions["actor.a"][0]>=36,"交换路径不能互穿或身体重叠")
	check(result.positions["actor.a"]!=units["actor.a"].position,"碰撞前合法路程保留而非整轮原地取消")
	check(arena==before.arena and units==before.units and plans==before.plans,"求解保持所有输入不变")
	check(G.resolve_moves(arena,units,plans)==result,"重复查询得到相同坐标和阻挡顺序")
	var reversed_units: Dictionary={"actor.b":units["actor.b"],"actor.a":units["actor.a"]}
	var reversed_plans: Dictionary={"actor.b":plans["actor.b"],"actor.a":plans["actor.a"]}
	check(G.resolve_moves(arena,reversed_units,reversed_plans)==result,"字典插入顺序不改变交换结果")
	var crossing: Dictionary={"actor.a":{"position":[100,200],"radius":18},"actor.b":{"position":[200,100],"radius":18}}
	var crossed: Dictionary=G.resolve_moves(arena,crossing,{"actor.a":{"destination":[300,200]},"actor.b":{"destination":[200,300]}})
	check(crossed.blocked.size()==2 and crossed.positions["actor.a"][0]<200 and crossed.positions["actor.b"][1]<200,"十字交叉在中心前发生真实身体碰撞")
	var staggered: Dictionary={"actor.a":{"position":[100,200],"radius":10},"actor.b":{"position":[120,100],"radius":10}}
	var passed: Dictionary=G.resolve_moves(arena,staggered,{"actor.a":{"destination":[300,200]},"actor.b":{"destination":[120,300]}})
	check(passed.blocked.is_empty() and passed.positions["actor.a"]==[300,200] and passed.positions["actor.b"]==[120,300],"路径几何相交但抵达时刻不同可以先后通过")
	var diagonal_cross: Dictionary={"actor.a":{"position":[100,100],"radius":0},"actor.b":{"position":[101,100],"radius":0}}
	var swept: Dictionary=G.resolve_moves(arena,diagonal_cross,{"actor.a":{"destination":[101,101]},"actor.b":{"destination":[100,101]}})
	check(swept.blocked.size()==2 and swept.positions=={"actor.a":[100,100],"actor.b":[101,100]},"微步内部交叉也被检测而非仅比较整数端点")
	var following: Dictionary={"actor.a":{"position":[100,200],"radius":18},"actor.b":{"position":[136,200],"radius":18}}
	var parallel: Dictionary=G.resolve_moves(arena,following,{"actor.a":{"destination":[200,200]},"actor.b":{"destination":[236,200]}})
	check(parallel.blocked.is_empty() and parallel.positions["actor.a"]==[200,200],"保持接触距离的同向移动不误停")
	var still: Dictionary=G.resolve_moves(arena,units,{"actor.a":{"destination":[400,200]}})
	check(still.blocked==["actor.a"] and still.positions["actor.b"]==[300,200] and still.positions["actor.a"]==[264,200],"缺省计划表示静止且移动者停在身体接触点")
	var near_wall: Dictionary={"actor.a":{"position":[180,200],"radius":18},"actor.b":{"position":[216,200],"radius":18}}
	var chain: Dictionary=G.resolve_moves(blocked_arena,near_wall,{"actor.a":{"destination":[380,200]},"actor.b":{"destination":[416,200]}})
	check(chain.blocked.size()==2 and chain.positions["actor.a"]==[226,200] and chain.positions["actor.b"]==[262,200],"前人被墙挡住后同一微步传递后人的阻挡")
	var corner: Dictionary=G.resolve_moves(arena,{"actor.a":{"position":[20,20],"radius":18}},{"actor.a":{"destination":[0,0]}})
	check(corner.blocked==["actor.a"] and corner.positions["actor.a"]==[18,18],"身体在场地边界前停止")
	var diagonal: Dictionary=G.resolve_moves(arena,{"actor.a":{"position":[200,200],"radius":18}},{"actor.a":{"destination":[131,157]}})
	check(diagonal.blocked.is_empty() and diagonal.positions["actor.a"]==[131,157],"负向斜线最终落点保持整数精确")
	check(G.resolve_moves(arena,units,{"actor.unknown":{"destination":[200,200]}}).get("code")=="UnknownMoveActor","未知人物计划不能进入会话")
	var malformed: Dictionary=G.resolve_moves(arena,{"actor.a":{"position":[100,100],"radius":-1},"actor.b":{"position":[200,100],"radius":18}}, {})
	check(malformed.get("code")=="InvalidUnit" and malformed.positions.size()==2,"非法身体配置仍保留全部已读位置以供原子拒绝")
	check(G.resolve_moves(arena,{"actor.a":{"position":[100,100],"radius":18},"actor.b":{"position":[110,100],"radius":18}},{}).get("code")=="OverlappingUnits","初始重叠坏状态被显式拒绝")
	check(not G.point_valid({"width":600,"height":400,"obstacles":[{}]},[100,100]),"损坏障碍配置不可作为空地")
	check(G.can_attack(arena,[100,100],[170,100],0,{"minimum_range":80,"maximum_range":70,"arc_degrees":90}).get("code")=="InvalidWeapon","内外射程倒置显式失败")
	print("PERSONAL_GEOMETRY_CHECKS=",checks," FAILURES=",failures)
	quit(0 if failures==0 else 1)
