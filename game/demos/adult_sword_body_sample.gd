## 独立成人刀剑分层样本；调用者必须显式传入已检查真实纹理和唯一rig，不登记正式美术或改主场景。
extends Node2D
const BodyLayers = preload("res://ui/adult_sword_body_layers.gd")
const ActionPose = preload("res://ui/personal_battle_action_pose.gd")
var body: Node2D

## 创建不带身份、时钟或规则访问的叶节点；不会自行挑选资源或播放动作。
func _init() -> void:
	body = BodyLayers.new()
	body.name = "AdultSwordBodyLayers"
	add_child(body)

## 消费现已结算公开演员和播放帧；脚点投影由调用者传入，不重写既有事件或推进播放时钟。
func show_frame(texture: Texture2D, definition: Dictionary, actor_id: String, actor: Dictionary, frame: Dictionary, projected_foot: Vector2) -> Dictionary:
	var pose: Dictionary = ActionPose.sample(actor_id, actor, frame)
	var result: Dictionary = body.configure(texture, definition, pose)
	if result.get("ok", false): body.position = projected_foot
	return result


## 有限整臂两态只消费公开演员和帧；双纹理与可选独立肩图均由调用者提供，不自行选资源。
func show_full_arm_frame(base_texture: Texture2D, arm_texture: Texture2D, base_definition: Dictionary, arm_definition: Dictionary, actor_id: String, actor: Dictionary, frame: Dictionary, projected_foot: Vector2, shoulder_texture: Texture2D = null) -> Dictionary:
	var pose: Dictionary = ActionPose.sample(actor_id, actor, frame)
	var result: Dictionary = body.configure_full_arm(base_texture, arm_texture, base_definition, arm_definition, pose, shoulder_texture)
	if result.get("ok", false): body.position = projected_foot
	return result

## 返回有限真实像素组合供离线核缝；守势标签仅来自原pose，不升级成成功格挡事件。
func image_copy(canvas: Vector2i, foot_origin: Vector2, display_scale: float = 1.0) -> Dictionary:
	return body.image_copy(canvas, foot_origin, display_scale)
