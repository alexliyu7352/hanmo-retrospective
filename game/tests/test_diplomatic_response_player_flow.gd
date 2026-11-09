## 新明确答复情景复用同一原Main按钮链，NPC答复不由helper提交命令。
extends RefCounted
const Diplomacy=preload("./test_diplomacy_player_flow.gd")
static func run(main: Node, evidence_path: String) -> Dictionary:
	return await Diplomacy.run(main,evidence_path,true)
