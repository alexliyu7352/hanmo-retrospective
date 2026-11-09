# 原Inventory.count独立对照

此目录是离线C++17/Flecs切片的输入准备和外部对照入口，不改旧Schema、Inventory、Kernel、共享构建登记或旧测试。协议以 `native/world/README.md` 为准。

`prepare_fixture.py` 薄复用 `tests/fixtures/legacy_asset_projection/prepare_fixture.py`，保留既有作者、精确tiny原图校验与正式stage流程。它只添加隔离导出脚本及来源身份，不运行Godot、native或下载依赖。

## 执行边界

需要先获得本任务运行阶段许可。新增依赖的下载、安装和执行许可必须单独由父任务核实；本目录不隐式取得依赖。固定使用已授权Godot 4.7.2和项目Python；先source项目env.sh，再将HOME、XDG、缓存和临时路径覆盖到调用方独有的本地运行目录。所有prepared、exports和results目录必须全新，保留旧回执不覆盖。

准备命令（变量由调用方绑定已核路径）：

```sh
"$HANMO_PYTHON" tests/fixtures/native_inventory_projection/prepare_fixture.py \
  --source-root "$HANMO_SOURCE" --output "$HANMO_RUN/prepared" --original-art "$HANMO_ORIGINAL_TINY_ART"
"$GODOT_BIN" --headless --path "$HANMO_RUN/prepared/game" --editor --quit
"$GODOT_BIN" --headless --path "$HANMO_RUN/prepared/game" \
  --script res://tests/export_native_inventory_projection.gd -- --output "$HANMO_RUN/exports"
"$HANMO_PYTHON" tests/fixtures/native_inventory_projection/run_oracle.py \
  --exports "$HANMO_RUN/exports" --executable "$HANMO_SIM" --output "$HANMO_RUN/results"
```

若Godot需要导入资源，在同一独立工程执行已授权的headless editor导入；不复用其它活跃测试的缓存。当前目录存在或脚本可解析不等于原生闭环已通过；只有最后驱动的逐项真实回执能说明这一点。

## 输出与来源

导出目录最后写出 `cases.json`；每个 `cases/<id>/` 包含：

- `artifact.json`：精确来源9标量、真实来源commit及catalog的schema_sha256、registry的模块id/version、引用ID、item重量与全部库存。所有领域整数均为TypedValue规范十进制；owner和lot附加键完整保留。
- `queries.json`：绑定artifact原始字节SHA的查询，仅含协议声明的五字段。owners完整编码，重复值不清洗。
- `oracle.json`：在原完整Schema通过的三元组上调用现有 `Inventory.count` 得到的独立结果；它与native输出使用相同结果形状，但永不作为native参数。

清单说明真实玩法或纯Schema边界、每文件SHA和各库存lot原序。采购通过Kernel公共命令完成；同次save_context三元组、重复导出、活Kernel的前后值必须完全相等。`source_commit` 是准备时本地HEAD；准备回执另列导出脚本摘要，不把未提交候选冒称成发布SHA。

五组来源：

1. `actual_empty`：实际开局空包为0。
2. `actual_purchase`：任务公款13、私款2、合计15；重复owner仍13；空owners及不存在item/owner/reservation均0。
3. `actual_reporting`：真实交接拆批后未预留任务3、私人2、指定行动预留10；原批次顺序保持。
4. `schema_nested_extras`：完整旧Schema接受的纯输入边界，明确非玩法来源；完整嵌套owner、lot附加字段含Unicode、null、bool、数组、字典、超过2^53整数和int64两端。删owner附加键不可匹配；字典键顺序不影响匹配；bool与int不可互换；重复owner不重复计数。
5. `schema_integer_maximum`：完整旧Schema接受的原int32数量上界2147483647，明确非玩法来源。

导出器另外检查：坏原注册三元组沿原Schema首错返回；旧Schema允许但wire不支持的库存外层字段、float、非String字典键（含StringName）、Object、超64层JSON全部失败，不转型、删字段或生成伪成功输入。

`run_oracle.py` 仅向native传 `readonly-count --input artifact.json --queries queries.json --output result.json --audit audit.json`。它检查精确结果字段、两输入原字节绑定、查询顺序、规范十进制count和原oracle完全一致，保留每次stdout/stderr/audit，核对输入及oracle未被改写。审计必须成功、已安装且实际安装次数为1，完成查询数量精确，input/query/result/payload的字节SHA全部吻合；每库存lot原序与输入一致。另用Python独立紧凑排序UTF-8序列化原TypedValue库存树，核对native从实际Flecs组件重编码产生的逐库存SHA，覆盖完整owner/lot附加字段和类型保留。native内部重复只读及错误路径另由native测试负责；外部驱动不重新实现count来制造第二份期望。

2026-10-04的5c21bf5许可前检查点，仅使用已有Python与固定Godot完成导出299项／0失败，5组／23条原算法oracle；该检查点的native是NOT_RUN。首次15项／1失败保留：自有carrier用点赋值新增organizations键导致StringName，修为明确String键构造后通过。未放宽wire或原Schema；授权后另跑真实native对照回执，不能将这份原算法结果本身当作C++查询通过。
