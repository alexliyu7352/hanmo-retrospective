## 母版测试检查重组和 alpha 外缘，输出实际组合证据供目视验收。
extends SceneTree
const Art = preload("res://src/layer_art.gd")

## 运行完整母版互斥分层和像素核对，不把数学一致误称为审美通过。
func _init() -> void:
	var master: Image = Art.load_source("res://assets/master.png")
	assert(master != null)
	var layers: Dictionary = Art.partition(master)
	var composed: Image = Art.compose(layers)
	var max_error := 0
	var opaque_pixel_count := 0
	var alpha_error := 0
	var original := master.get_data()
	var actual := composed.get_data()
	for i in range(0, original.size(), 4):
		if original[i + 3] > 0:
			opaque_pixel_count += 1
			for channel in 4:
				max_error = maxi(max_error, absi(original[i + channel] - actual[i + channel]))
		alpha_error = maxi(alpha_error, absi(original[i + 3] - actual[i + 3]))
	assert(max_error <= 1)
	assert(alpha_error == 0)
	for slot in layers:
		assert(layers[slot].save_png("res://evidence/master-" + slot + ".png") == OK)
	assert(composed.save_png("res://evidence/master-reassembled.png") == OK)
	assert(Art.proof_plate(composed).save_png("res://evidence/master-background-proof.png") == OK)
	var result := {"ok": true, "engine": Engine.get_version_info().string, "canvas": [1024, 1536], "covered_pixels": opaque_pixel_count, "maximum_visible_channel_error": max_error, "maximum_alpha_error": alpha_error, "art_review": "pending_actual_pixel_review"}
	var file := FileAccess.open("res://evidence/master-test.json", FileAccess.WRITE)
	file.store_string(JSON.stringify(result, "\t") + "\n")
	print(JSON.stringify(result))
	quit()
