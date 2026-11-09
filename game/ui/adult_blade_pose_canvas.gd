## 完整姿态供原战场按原绘制层序消费；只持有注册图与有限像素缓存，没有世界或时钟。
extends RefCounted
const Binding = preload("res://ui/adult_blade_pose_binding.gd")
const Player = preload("res://ui/adult_blade_pose_player.gd")
var _player: Node2D
var _binding: Dictionary = {}
var _packets: Dictionary = {}
var _source_sizes: Dictionary = {}

## 仅接Character给出的显式绑定与当前注册表；任何装载失败清空，宿主绘原身份回退。
func configure(descriptor: Dictionary, registry: RefCounted) -> Dictionary:
	_binding = {}; _packets = {}; _source_sizes = {}
	if _player != null: _player.free(); _player = null
	var binding: Variant = descriptor.get("battle_pose_binding")
	if not binding is Dictionary or descriptor.get("actor_id") != "actor.player" or binding.get("actor_id") != descriptor.actor_id: return {"ok": false, "reason": "NoExplicitBodyBinding"}
	var loaded: Dictionary = Binding.sources(binding, registry)
	if not loaded.ok: return loaded
	_player = Player.new()
	var accepted: Dictionary = _player.configure_sources(loaded.definition, loaded.images)
	if not accepted.ok: _player.free(); _player = null; return accepted
	_binding = binding.duplicate(true)
	for key in loaded.definition.poses:
		var size: Array = loaded.definition.poses[key].size
		_source_sizes[key] = Vector2(size[0], size[1])
	return {"ok": true, "profile_id": binding.profile_id}

## 每帧复核公开武器和朝向，消费宿主原Playback帧；不镜像、拉伸或自行补造另一武器动作。
func packet(actor_id: String, public_unit: Dictionary, actor: Dictionary, frame: Dictionary, logical_height: float) -> Dictionary:
	if _binding.is_empty() or actor_id != _binding.actor_id or public_unit.get("weapon_id") != "item.sword": return {"ok": false, "reason": "UnsupportedPublicWeaponOrActor"}
	var heading: Variant = actor.get("heading", public_unit.get("heading"))
	if typeof(heading) != TYPE_INT or heading < 0 or heading >= 360 or cos(deg_to_rad(heading)) < 0.5: return {"ok": false, "reason": "UnsupportedSourceFacing"}
	if not is_finite(logical_height) or logical_height < 16.0 or logical_height > 256.0: return {"ok": false, "reason": "UnsupportedLogicalHeight"}
	var selected: Dictionary = _player.show_frame(_binding, actor, frame, Vector2.ZERO, logical_height)
	if not selected.ok: return selected
	var cache_key: String = JSON.stringify([selected.pose_key, selected.uniform_scale, selected.alpha])
	if not _packets.has(cache_key):
		var size: Vector2i = Vector2i((_source_sizes[selected.pose_key] * selected.uniform_scale).ceil()) + Vector2i(2, 2)
		var foot: Vector2 = selected.ground_anchor * selected.uniform_scale + Vector2.ONE
		var capture: Dictionary = _player.image_copy(size, foot)
		if not capture.ok: return capture
		if _packets.size() >= 12: _packets.erase(_packets.keys()[0])
		_packets[cache_key] = {"texture": ImageTexture.create_from_image(capture.image), "offset": -foot, "image": capture.image}
	var result: Dictionary = selected.duplicate(true)
	result["texture"] = _packets[cache_key].texture
	result["offset"] = _packets[cache_key].offset
	return result

## 离线证据复制与真正draw_texture使用同一缓存像素，不冒充Viewport截图。
func image_copy(packet_value: Dictionary) -> Image:
	var texture: Variant = packet_value.get("texture")
	return texture.get_image().duplicate() if texture is Texture2D else null

## RefCounted释放时归还未入树的采样节点；不保留上一世界的人物图。
func _notification(what: int) -> void:
	if what == NOTIFICATION_PREDELETE and is_instance_valid(_player): _player.free()
