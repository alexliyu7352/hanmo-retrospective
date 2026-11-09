## 验证远近标注与密集组选点仍绑定原地点；有限作者几何不冒充历史地理或GUI画质。
extends SceneTree
const Canvas=preload("res://ui/world_map_canvas.gd")
const Layout=preload("res://ui/world_map_marker_layout.gd")
var checks: int=0
var failures: int=0

## 控件树准备后再计算真实命中矩形。
func _init() -> void:call_deferred("_run")

## 每次只核可观察的分组、命中与来源不变，不用测试替代视觉评审。
func check(value: bool,label: String) -> void:
	checks+=1
	if not value:failures+=1;printerr("FAIL: ",label)

## 同坐标城村反序ID是回归反例；远处村落用于比较远近标注的实际命中范围。
func _run() -> void:
	var data: Dictionary={"settlements":{},"routes":{},"texts":{}}
	for row in [["a_village","village",[10,10]],["z_city","city",[10,10]],["far_village","village",[100,100]]]:
		data.settlements[row[0]]={"id":row[0],"kind":row[1],"map_xy":row[2]}
	var before: Dictionary=data.duplicate(true)
	var canvas=Canvas.new();canvas.data=data;canvas.view={"ok":true,"world_map_id":"","observer_position":[10,10],"route_open":{},"road_progress":{},"travel_overlay":{}}
	root.add_child(canvas);canvas.size=Vector2(1000,700);canvas._layout()
	await process_frame
	check(canvas._marker_groups.has("z_city") and canvas._marker_groups.z_city.ids==["a_village","z_city"],"缩远重叠组以真实城点代表，不由村ID顺序盖城名")
	var far_height: float=canvas._markers.far_village.size.y
	var emitted: Array=[];canvas.destinations_requested.connect(func(ids: Array):emitted.append(ids))
	canvas._markers.z_city.pressed.emit()
	check(emitted==[["a_village","z_city"]],"类型优先只改变代表，原城村均可展开选择")
	canvas.selected="a_village";canvas._layout()
	check(canvas._marker_groups.has("a_village"),"已选村落仍优先于城市")
	canvas.selected="";canvas.current_settlement="a_village";canvas._layout()
	check(canvas._marker_groups.has("a_village"),"本人所在村落仍优先于城市")
	canvas.current_settlement="";canvas.via=["a_village"];canvas._layout()
	check(canvas._marker_groups.has("a_village"),"途经村落标注不因层级被吞")
	canvas.via=[];canvas.zoom=2.0;canvas.pan=-canvas.screen_point(Vector2(100,100))+canvas.size/2;canvas._layout()
	check(canvas._markers.far_village.size.y>far_height,"放大后村名占用对应实际命中空间")
	var nearby_height: float=canvas._markers.far_village.size.y
	canvas.zoom=1.0;canvas.pan=Vector2.ZERO;canvas.selected="far_village";canvas._layout()
	check(canvas._markers.far_village.size.y==nearby_height and canvas._markers.far_village.tooltip_text.contains("far_village"),"缩远已选村仍保完整名字和可读提示")
	check(canvas.data==before,"分级没有增加地点、挪坐标或改作者内容")
	canvas.queue_free();await process_frame
	print("MAP_LABEL_LEVELS_CHECKS=",checks," FAILURES=",failures," SOURCE=FINITE_AUTHOR_GEOMETRY GUI=NOT_TESTED")
	quit(0 if failures==0 else 1)
