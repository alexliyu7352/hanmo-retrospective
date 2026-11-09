## 军粮纯查询与实际供养逐项对账；继承领域夹具的明确边界，不将夹具战损称为原生游玩。
extends "res://tests/test_military_assets.gd"
const Logistics=preload("res://ui/military_logistics.gd")
const Canvas=preload("res://ui/military_battle_canvas.gd")

## 每次查询必须完全只读，且当前库存、名册预估与下一次正式扣粮收据相等。
func check_debit(f: Dictionary, army_id: String) -> void:
	var before: Dictionary=f.state.duplicate(true)
	var quote: Dictionary=Military.supply_preview(f.state,f.data,army_id)
	check(quote.ok and Military.supply_preview(f.state,f.data,army_id)==quote and f.state==before,"重复供粮查询不改世界、随机数或库存预留")
	var result: Dictionary=advance(f)
	var receipt: Dictionary={}
	for id in result.get("supply_events",[]):
		if f.state.facts[id].army_id==army_id: receipt=f.state.facts[id]
	check(not receipt.is_empty() and receipt.required==quote.required and receipt.mouth_ids.size()==quote.mouth_count and receipt.shortage==quote.shortage,"口数、需粮与下半日短缺全部匹配真实供粮收据")
	var after: Dictionary=Military.supply_preview(f.state,f.data,army_id)
	check(after.available==quote.available-mini(quote.available,quote.required) and receipt.consumed==mini(quote.available,quote.required),"实际库存只减少纯查询所示可消费口粮")

## 军官、两部兵员和借调真人按并集计口，战损、伤员、俘虏及非本军批次均走真实来源校验。
func check_rosters() -> void:
	var f: Dictionary=fresh(); check(F.add_estate(f).ok,"口粮夹具具备真实私庄部曲")
	var own_order: String=commission(f); var enemy_order: String=commission(f,true)
	var pledge: Dictionary=Military.preview(f.state,f.data,f.actor,"army_pledge",{"order_id":own_order,"estate_id":f.estate,"actor_ids":[f.retainer]})
	check(Military.apply(f.state,f.data,f.actor,"army_pledge",pledge).ok,"正常借调后才能计入编组口数")
	var params: Dictionary={"order_id":own_order,"unit_ids":[f.unit,f.second_unit],"retainer_ids":[f.retainer],"food_quantity":40,"money":40}
	var before: Dictionary=f.state.duplicate(true)
	var formation: Dictionary=Military.preview(f.state,f.data,f.actor,"army_form",params)
	check(formation.ok and formation.provisions.mouth_count==13 and formation.provisions.required==13 and formation.provisions.available==40 and formation.provisions.full_ticks==3,"编组报价包含十名原兵、两军官和一借调真人的真实口粮估算")
	check(Military.preview(f.state,f.data,f.actor,"army_form",params)==formation and f.state==before,"编组的口粮预估不提前扣粮或改变正式草案")
	var own: String=form(f,own_order,[f.unit,f.second_unit],[f.retainer],40)
	check(Military.supply_preview(f.state,f.data,own)==formation.provisions,"编组完成后的真实军团口数与编组预览一致，借调不重复计粮")
	check_debit(f,own)
	var other: String=form(f,enemy_order,[f.enemy_unit],[],70)
	var battle: Dictionary=battle_result(f,own,other); var unit: Dictionary=battle.units[f.unit]
	unit.dead.append(unit.fit.pop_front()); unit.wounded.append(unit.fit.pop_front()); unit.deserted.append(unit.fit.pop_front())
	unit.fit.erase(f.retainer); unit.captured.append(f.retainer)
	var settled: Dictionary=Military.settle_battle(f.state,f.data,battle.id,battle.units,{f.retainer:other})
	check(settled.ok,"领域战损通过正式结算转移真实俘虏责任")
	actor_changes(f,settled.actor_changes)
	var own_quote: Dictionary=Military.supply_preview(f.state,f.data,own)
	var enemy_quote: Dictionary=Military.supply_preview(f.state,f.data,other)
	check(own_quote.mouth_count==10 and enemy_quote.mouth_count==8,"伤员仍计粮，死者逃散不计，俘虏改由实际看管军团供养")
	check_debit(f,own); check_debit(f,other)
	var isolated: Dictionary=f.state.duplicate(true); var original: Dictionary=f.state.duplicate(true)
	var bag: String=isolated.armies[own].inventory_id
	var quote: Dictionary=Military.supply_preview(isolated,f.data,own)
	Inventory.add(isolated,bag,quote.food_item_id,50,{"kind":"actor","id":f.actor})
	Inventory.add(isolated,bag,quote.food_item_id,50,isolated.inventories[bag].owner,"fixture.reservation")
	check(Military.supply_preview(isolated,f.data,own)==quote and f.state==original,"隔离批次用例证明私人货物与预留粮食不能计作本军可吃口粮")
	isolated.actors[f.actor].alive=false
	check(Military.supply_preview(isolated,f.data,own).mouth_count==quote.mouth_count-1,"只读边界明确排除已死亡真人军官")
	check(not Military.supply_preview(f.state,f.data,"army.missing").ok,"未知军团不显示虚构续航")
	var returned: Dictionary=advance(f,start(f,f.enemy,"army_return",{"army_id":other}))
	check(returned.get("success",false) and not Military.supply_preview(f.state,f.data,other).ok,"真实回营关闭的军团不再提供活动供粮估算")

## 零粮仍允许真实规则产生缺粮后果；表现仅警示下一半日，不禁止指令或替玩家补粮。
func check_shortage() -> void:
	var f: Dictionary=fresh(); var order: String=commission(f)
	var army: String=form(f,order,[f.unit],[],0)
	var quote: Dictionary=Military.supply_preview(f.state,f.data,army)
	check(quote.available==0 and quote.required==7 and quote.full_ticks==0 and quote.shortage==7,"零粮按当前七口计算下一半日短缺")
	var label=Label.new(); Logistics.update(label,quote)
	check(label.text.contains("下个半日缺粮 7份") and label.text.contains("每半日需 7份"),"缺粮提示明确给出实际差额而不伪称已死亡或已补给")
	check_debit(f,army)
	var supplied: Dictionary=advance(f,start(f,f.actor,"army_resupply",{"army_id":army,"food_quantity":10,"money":0}))
	check(supplied.get("success",false),"补给仍从真实公仓装运")
	quote=Military.supply_preview(f.state,f.data,army); Logistics.update(label,quote)
	check(quote.full_ticks==1 and quote.shortage==0 and not label.text.contains("缺粮") and label.text.contains("按当前"),"仅剩一个完整半日仍如实估算，不用任意阈值渲染缺粮")
	Logistics.update(label,{})
	check(not label.visible and label.text.is_empty() and label.tooltip_text.is_empty(),"失效报价清除旧文字，不留下过期续航")
	label.free()

## 墙标采用一个可三角化的连续垛口多边形；缩放与居中由格半径决定，完全不依赖图符字体。
func check_wall_geometry() -> void:
	var canvas=Canvas.new(); var center=Vector2(90,70)
	var outline: PackedVector2Array=canvas._wall_outline(center)
	check(outline.size()==12 and not Geometry2D.triangulate_polygon(outline).is_empty(),"垛口墙体是一个完整可填充原生轮廓")
	check(Geometry2D.is_point_in_polygon(center+Vector2(0,8),outline),"三个垛口下方具有连续墙身，不再是三个空心方框")
	canvas._radius=20.0; var smaller: PackedVector2Array=canvas._wall_outline(center)
	var scaled: bool=true
	for index in outline.size():
		if not (smaller[index]-center).is_equal_approx((outline[index]-center)*.5): scaled=false
	check(scaled,"每个墙体顶点均随格半径等比缩放且保留位置锚点")
	canvas.free()

## 仅运行本次领域与几何范围；完整既有军事回归由独立脚本另行执行。
func _run() -> void:
	check_rosters(); check_shortage(); check_wall_geometry()
	print("MILITARY_LOGISTICS_CHECKS=",checks," FAILURES=",failures)
	quit(1 if failures else 0)
