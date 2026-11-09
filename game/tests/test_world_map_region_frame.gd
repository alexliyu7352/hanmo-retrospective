## 区域底景消费者的坐标/像素比例窄门；此门不冒充目录schema或正式包安装验收。
extends SceneTree
const Canvas=preload("res://ui/world_map_canvas.gd")
var checks: int=0
var failures: int=0

## 图像读取放到根树初始化后，不创建任何世界。
func _init() -> void: call_deferred("_run")

## 每个失败保留明确表现边界。
func check(value: bool,label: String) -> void:
	checks+=1
	if not value: failures+=1; printerr("FAIL: ",label)

## 实际原PNG配原点集校验；不会把渲染矩形写入地点或通路数据。
func _run() -> void:
	var image=Image.load_from_file("res://assets/art/world-map/v0.2/qingchuan-region.png")
	if image==null or image.is_empty(): printerr("缺少原区域PNG"); quit(1); return
	var texture=ImageTexture.create_from_image(image)
	var canvas=Canvas.new()
	var source_bounds=Rect2(34,59,2,1)
	var valid: Dictionary=canvas._region_frame({"map_world_rect":[32,57,6,4]},texture,source_bounds)
	check(valid.get("ok",false) and valid.get("frame")==Rect2(32,57,6,4),"原1536x1024地面与6x4坐标框严格同长宽比")
	check(canvas._region_frame({"map_world_rect":[34,59,1,1]},texture,source_bounds).get("code")=="MapArtBoundsMismatch","裁掉原节点的画框不能启用")
	check(canvas._region_frame({"map_world_rect":[32,57,6,3]},texture,source_bounds).get("code")=="MapArtAspectMismatch","不能为凑画面而拉伸原图")
	check(canvas._region_frame({"map_world_rect":[32,57,6,INF]},texture,source_bounds).get("code")=="InvalidMapArtFrame","非有限坐标拒绝")
	check(canvas._region_frame({},texture,source_bounds).get("code")=="MapArtFrameMissing","没有明确映射不猜原图地理范围")
	check(canvas._region_frame({"map_world_rect":[32,57,6,4]},null,source_bounds).get("code")=="MapArtFrameMissing","无真实纹理不安装画框")
	canvas.size=Vector2(1200,800); canvas._bounds=valid.frame
	var origin: Vector2=canvas.screen_point(Vector2(32,57)); var far: Vector2=canvas.screen_point(Vector2(38,61))
	check(is_equal_approx((far-origin).x/(far-origin).y,1.5),"正向画面投影保持真实PNG比例")
	for point in [Vector2(35,60),Vector2(34,60),Vector2(36,60),Vector2(35,59)]:
		check(canvas.world_point(canvas.screen_point(point)).distance_to(point)<.001,"原地点正逆投影不变："+str(point))
	canvas.free()
	print("WORLD_MAP_REGION_FRAME_CHECKS=",checks," FAILURES=",failures," REGISTRY_SCHEMA=NOT_TESTED WORLD_WRITES=0")
	quit(0 if failures==0 else 1)
