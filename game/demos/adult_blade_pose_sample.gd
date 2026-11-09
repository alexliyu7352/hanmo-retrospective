## 独立成年男性单刃刀样本；显式源库、绑定与公开帧，不注册Main或正式人物。
extends Node2D
const Player = preload("res://ui/adult_blade_pose_player.gd")
var player: Node2D

## 样本只持表现叶，播放时钟沿用宿主传来的现有frame。
func _init() -> void:
	player = Player.new()
	add_child(player)

## 返回源库验证结果；源文件读取/hash校验由调用者完成。
func configure_sources(definition: Dictionary, images: Dictionary) -> Dictionary:
	return player.configure_sources(definition, images)

## 投影位置、身份标识与结果都由原宿主提供，此处只显示完整图。
func show_frame(binding: Dictionary, actor: Dictionary, frame: Dictionary, foot: Vector2, logical_height: float = 112.0) -> Dictionary:
	return player.show_frame(binding, actor, frame, foot, logical_height)
