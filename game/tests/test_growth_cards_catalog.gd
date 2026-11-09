## 实际组合作者目录的GD准入门；显式输入原索引和同源schema，不代替安装信任链。
extends SceneTree
const Catalog=preload("res://core/content_catalog.gd")
const Strict=preload("res://core/strict_json.gd")
var checks: int=0
var failures: int=0

## 失败保留真实码和字段，不把解析失败算业务拒绝成功。
func check(value: bool,label: String) -> void:
	checks+=1
	if not value:
		failures+=1
		printerr("FAIL "+label)

## 重签目录摘要仅测试已知坏作者输入；安装资源检查由正式stage单独承担。
func load_index(index: Dictionary,schema: String) -> Dictionary:
	index.catalog_sha256=(JSON.stringify(index.catalog,"",true,true)+"\n").sha256_text()
	return Catalog._load_checked(JSON.stringify(index),schema)

## 正例使用实作者目录，反例只改变卡片定义，旧情景删卡后不新增空表。
func _initialize() -> void:
	var args: PackedStringArray=OS.get_cmdline_user_args()
	if args.size()!=2:
		printerr("Usage: test_growth_cards_catalog INDEX SCHEMA"); quit(2); return
	var parsed: Dictionary=Strict.parse_text(FileAccess.get_file_as_string(args[0]))
	if not parsed.ok or not parsed.value is Dictionary:
		printerr("Invalid explicit author index"); quit(2); return
	var index: Dictionary=parsed.value
	var schema: String=FileAccess.get_file_as_string(args[1])
	var valid: Dictionary=load_index(index.duplicate(true),schema)
	check(valid.ok,"actual two-card catalog loads: "+str(valid))
	var bad: Dictionary=index.duplicate(true)
	bad.catalog.cards.values()[0].category="unknown"
	var rejected: Dictionary=load_index(bad,schema)
	check(not rejected.ok and rejected.get("code")=="ContentEnum" and str(rejected.get("path","")).begins_with("/cards/"),"invalid card category rejected at schema: "+str(rejected))
	var legacy: Dictionary=index.duplicate(true)
	legacy.catalog.erase("cards")
	legacy.catalog.scenarios.erase("lifecards::scenario.qingchuan_card_life")
	var old: Dictionary=load_index(legacy,schema)
	check(old.ok and not old.get("data",{}).has("cards"),"legacy scenario retains missing card table")
	print("GROWTH_CARDS_CATALOG ",checks," checks ",failures," failures")
	quit(0 if failures==0 else 1)
