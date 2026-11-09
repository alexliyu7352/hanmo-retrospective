## A1.7剩余非军事命令的受信迁移清单；只描述现有路由，不提供可执行回调或接管Kernel。
## 领域集合引用原KINDS；散落Kernel的命令只在此组合表登记。新领域不得借此兼容入口扩展。
extends RefCounted
const News=preload("res://core/domains/social/news_commands.gd")
const Discovery=preload("res://core/domains/interaction/discovery_commands.gd")
const Movement=preload("res://core/domains/travel/movement_commands.gd")
const DialogueCommands=preload("res://core/domains/interaction/dialogue_commands.gd")
const RuntimeCommands=preload("res://core/runtime/runtime_commands.gd")
const ServiceCommands=preload("res://core/domains/services/service_commands.gd")
const Personal=preload("res://core/personal_combat.gd")
const Encounters=preload("res://core/encounter_rules.gd")
const Parties=preload("res://core/party_rules.gd")
const Journey=preload("res://core/journey_commands.gd")
const Rest=preload("res://core/long_rest.gd")
const Cultivation=preload("res://core/cultivation_rules.gd")
const Estate=preload("res://core/estate_rules.gd")
const Family=preload("res://core/family_rules.gd")

# 本表是迁移内联命令时的组合根单一清单；测试另从Kernel真实分派发现集合，不读取本表作期望。
const INLINE_COMMANDS=[
]

## 返回新副本，供后续逐域适配与独立源覆盖验证；本清单不能直接传给Registry.build。
static func descriptors() -> Array:
	var result: Array=[]
	for kind in RuntimeCommands.KINDS:
		var is_action: bool=kind in RuntimeCommands.ACTION_KINDS
		_append(result,RuntimeCommands,"runtime",[kind],RuntimeCommands.ACTION_KINDS,"legacy_kernel" if is_action else "advance_before_actor",["legacy_begin_action"] if is_action else ["military_preview_step","siege_preview_step","travel_can_step","advance_world"])
	_append(result,ServiceCommands,"services",ServiceCommands.KINDS,ServiceCommands.KINDS,"legacy_kernel",["legacy_begin_action","context_provider_participant"])
	_append(result,Personal,"personal",Personal.KINDS,[],"personal_before_world",["military_battle_guard","personal_combat_capability","advance_held_sides_and_repreview_before_begin"])
	_append(result,Encounters,"interaction",Encounters.KINDS,[],"encounter_before_pending",["drive_dialogue_after_encounter_talk"])
	_append(result,Parties,"travel",Parties.KINDS,[],"party_before_actor",["domain_preview_before_actor_unavailable_and_busy"])
	_append(result,Journey,"travel",Journey.KINDS,[],"journey_before_actor",["travel_wait_advances_with_actor_held","domain_replan_may_release_travel"])
	_append(result,Rest,"services",Rest.KINDS,["long_rest"],"rest_before_actor",["family_capability","legacy_begin_action","rest_cancel_then_release_action"])
	_append(result,Cultivation,"cultivation",Cultivation.KINDS,Cultivation.TRAINING_KINDS,"legacy_kernel",["cultivation_capability","legacy_begin_action_or_apply_instant"])
	_append(result,Estate,"estates",Estate.KINDS,["estate_build"],"legacy_kernel",["estates_capability","legacy_begin_action_or_apply_quote"])
	_append(result,Family,"family",Family.KINDS,["family_adopt"],"legacy_kernel",["family_capability","legacy_begin_action_or_apply_quote"])
	_append(result,News,"social",News.KINDS,[],"legacy_kernel",["npc_society_capability","validate_conversation","hear_news"])
	_append(result,Discovery,"interaction",Discovery.KINDS,[],"legacy_kernel",["discover_each_prop_offer","remember_source"])
	for kind in Movement.KINDS:
		var gate: String="rescue_before_actor" if kind=="rescue" else "legacy_kernel"
		var coordination: Array=[]
		match kind:
			"travel": coordination=["plan_travel","legacy_begin_action","party_participants"]
			"rescue": coordination=["army_return_guard","replace_travel_with_rescue"]
			"move_local": coordination=["party_admission","erase_interaction","events_on_enter"]
		_append(result,Movement,"travel",[kind],["travel"],gate,coordination)
	_append(result,DialogueCommands,"interaction",DialogueCommands.KINDS,[],"legacy_kernel",["trusted_recursive_preview_apply","drive_existing_dialogue","validate_shop_and_representative"])
	for inline in INLINE_COMMANDS:
		var entry: Dictionary=inline.duplicate(true)
		entry.source={"path":"game/core/world_kernel.gd","constant":""}
		entry.stage="inventory_only"
		result.append(entry)
	return result

## 源文件身份从已固定preload脚本取得，不从作者输入动态加载；集合只用于清单而非兜底执行。
static func _append(result: Array, source: Script, owner: String, kinds: Array, action_kinds: Array, gate_id: String, coordination: Array) -> void:
	for kind in kinds:
		result.append({"kind":kind,"owner":owner,"mode":"action" if kind in action_kinds else "instant","gate_id":gate_id,"source":{"path":"game/"+source.resource_path.trim_prefix("res://"),"constant":"KINDS"},"coordination":coordination.duplicate(true),"stage":"inventory_only"})

## 未知kind明确返回空描述；不产生默认owner、伪preview或通用apply。
static func find(kind: String) -> Dictionary:
	for entry in descriptors():
		if entry.kind==kind: return entry
	return {}
