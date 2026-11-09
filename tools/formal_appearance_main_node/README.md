# 正式组合 Main 两进程节点

仅使用固定 Godot 4.7.2 编辑器运行标准 native-resident source stage。不是导出包或 GUI 验收。`main_entry.gd` 继承 `../resident_delivery_node/package_entry.gd` 已有可见按钮、正常菜单存读、完整世界原 JSON 比较和正常退出流程；两个文件须保持同一相对目录。

本节点通过标题与章节按钮打开192组合控件，选择一个作者许可且不属于旧12 preset的完整九槽，取消并检查原空 Kernel 未换、世界/RNG/ID未生成、原生后端候选节点为零且未创建银行；重开后单次确认，检查原生完整外貌、出生年龄、身份、能力、初始地点与五条初始RNG。然后走正常菜单保存，在另一个新进程从正常标题继续，逐字比较完整世界原JSON及完整外貌序列化结果，观察idle不推进。冷导出只作证据，不输入产品恢复。

中央准备和核实正式1.7 stage、新库及真实Catalog后调用：

```sh
python3 tools/formal_appearance_main_node/run_node.py \
  --project /absolute/stage/game \
  --output /absolute/new-evidence-directory \
  --editor "$GODOT_BIN" \
  --expected-editor-sha256 8d106cbe6144c2dc7e881d61d2429c1a8a76e6b22ef48bd5e48dcf934953f71e \
  --native-library /absolute/stage/game/native_resident_bridge/libhanmo_world_bridge.so \
  --expected-native-sha256 ACTUAL_FROZEN_NEW_LIBRARY_SHA256 \
  --source-commit ACTUAL_STAGE_SOURCE_COMMIT \
  --catalog res://data/generated/ACTUAL_INSTALLED_CATALOG.hmcatalog.json \
  --pack-set-digest 3f811ca79c7ab9eb6e03902d17d7adfc420ee30897aaf3110800390cc4c60cd2
```

默认章节为 `society::scenario.qingchuan_spring_reserve`，人物为 `society::actor.apprentice`。保留 stage 的真实 storage profile，不覆盖项目配置。输出目录必须不存在且与 stage 分离；wrapper仅建立自己的XDG/TMP目录。调用方需先完成该stage的标准导入并保留生成缓存。每次尝试使用新的证据目录，不覆盖失败原件。

`report.json` 同时要求 producer/consumer 退出0、真实回执通过、不同PID、物理银行存在且原字节不变、stage完整非缓存文件不变。运行失败时先看对应 `process.log` 与 `result.json`，不替产品补命令或裁减世界字段。源码编译通过不等于此节点运行通过。
