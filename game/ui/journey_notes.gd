## 旧行旅手记入口仅转交核心文本投影；保留原常量与方法，不另存一份文字或知识算法。
extends RefCounted
const Notes=preload("res://core/views/journey_notes.gd")
const TimeRules=Notes.TimeRules
const Conditions=Notes.Conditions
const Identity=Notes.Identity
const STATUS=Notes.STATUS

## 旧名称入口仍按原目录规则取字，不在表现层补写作者信息。
static func name_of(data: Dictionary, record: Dictionary) -> String:
	return Notes.name_of(data,record)

## 旧完整状态调用仍得到同一手记文本，权威算法只保留在核心。
static func describe(state: Dictionary, data: Dictionary) -> String:
	return Notes.describe(state,data)

## 旧近事入口保持本人知识与来源处理，只返回核心已有的有序文字。
static func social_notes(state: Dictionary, data: Dictionary, actor_id: String) -> Array[String]:
	return Notes.social_notes(state,data,actor_id)
