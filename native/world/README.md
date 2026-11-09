# Offline legacy projection slices

首片实现artifact绑定的C++17／Flecs只读`Inventory.count`闭环。旧Kernel是唯一游戏真值；没有world_id、session替代、Bridge、Director、写入、恢复或模拟时间。设计来源为`docs/superpowers/plans/2026-10-04-native-projection-binding-decisions.md`。

依赖现已按用户授权从官方来源取得：三库精确Git commit／对象完整性、实际编译文件及许可证SHA均已核；项目局部CMake3.31.12便携包匹配官方SHA清单。源码和便携工具位于调用方本地缓存，具体字节见`native/dependencies.lock.json`。初始1d1c004／5c21bf5未取得依赖的WIP历史仍保全。

## Wire v1

每个输入文件及内嵌payload最多64MiB，CLI两输入合计最多64MiB；JSON结构最大深度64（根容器深度1），重复对象键拒绝。所有文本按UTF-8解析，不做Unicode规范化。未知外层字段拒绝。本wire协议不以裸JSON数字承载领域整数，全部使用下述tag；不接受float／bool／String与int互转。

TypedValue精确形状：

- `{"t":"null"}`
- `{"t":"bool","v":true}`
- `{"t":"int","v":"123"}`：有符号int64规范十进制，允许`0`、负数，拒绝`+1`、`01`、`-0`、空白和溢出。
- `{"t":"string","v":"text"}`
- `{"t":"array","v":[TypedValue,...]}`：保序。
- `{"t":"dict","v":{"key":TypedValue,...}}`：仅String key，完整键值相等；键的JSON出现顺序不参与Dictionary相等。

Artifact外层精确字段：`format="HM-NATIVE-INVENTORY-ARTIFACT/1"`、`binding_kind="artifact"`、`payload_sha256`（64位小写hex）、`payload_json`（String）。SHA对payload_json解码后的精确UTF-8字节计算。整个artifact文件也另算SHA用于query绑定；不以重排JSON后的摘要代替原字节身份。

payload_json解析为一个TypedValue dict，精确字段为：

| 字段 | TypedValue解码后的值 |
|---|---|
| source | 原9字段：ruleset_id/content_id/content_version/content_digest/engine_build/rng_backend/scenario_id为String，tick/revision为int[0,2147483647] |
| provenance | source_commit/schema_sha256为String，rule_modules为原id/version行数组；pack_set_digest可选String，缺省不补空串 |
| references | actors/organizations/tasks/actions四个String ID数组，各自唯一 |
| items | id/weight行数组，id唯一String，weight为int[1,10000] |
| inventories | 现资产叶的id/owner/capacity/revision/lots行；外层仅这5字段 |

inventory.id唯一；capacity为int[1,2147483647]，revision为int[0,2147483647]。owner必须为完整dict，含kind/id，kind为actor/organization/task且指向相应references。lot必须有id/item_id/quantity/owner/reserved_by，保留全部额外字段及owner额外键；lot.id跨库存唯一，quantity为int[1,2147483647]，item存在，非空reserved_by指向references.actions。按原item.weight验承重，所有乘加用有界int64检查。lots不排序、不合并。

仅可信GDScript测试导出器可从同一次save_context生成来源，先调用原Schema；坏旧三元组沿原失败，不补齐。native只验证此有限wire profile，不宣称代替完整旧Schema。原state库存外层未声明附加字段、不可表示Variant明确不支持，不能静默丢弃；owner/lot完整可表示的附加字段保全。

Queries外层精确字段：`format="HM-NATIVE-INVENTORY-QUERIES/1"`、`input_sha256`（整个artifact文件SHA）、`queries`数组。每行仅`query_id/inventory_id/item_id/owners/reservation`；除owners外均String，owners是TypedValue dict数组。query_id唯一且非空；owner形状含已知kind/id，但查询中不存在的owner ID仍按无匹配返回0；item和reservation无匹配同样返回0。不存在inventory为UnknownInventory。重复owners不重复计数。

结果精确语义：`format="HM-NATIVE-INVENTORY-RESULT/1"`、`input_sha256`、`queries_sha256`、`counts:[{query_id,count}]`，count为规范十进制String，顺序与queries一致。全部queries先验后执行；任一失败不发布部分counts。独立oracle使用相同counts形状与两个摘要，由原GDScript Inventory.count计算，不传给native。

## API与运行边界

`InventoryProjection::install_once(artifact_bytes)`：空实例解析全部来源并构造候选Flecs存储，成功后一次安装。失败不安装半份；成功后任何第二次安装返回AlreadyInstalled。领域ID作为StableId组件和明确索引，不能被Flecs名称层级解析。

`InventoryProjection::query_batch(query_bytes) const`：核SHA、全部类型／引用后，从已安装Flecs InventoryState读取，按旧count条件累加；不写组件、分配领域ID、推进时间或调用observer。返回值为独立值，无entity_t或可写组件引用。

`audit() const`只给输入绑定、真实安装次数、库存/批次数、各库存原lot ID顺序及从实际组件重编码的完整tag树SHA，用于证实扩展字段保留与只读保持性。该组件摘要使用C++ JSON规范紧凑输出，和精确原文件SHA用途分开。

CLI合同：`hanmo_sim readonly-count --input artifact.json --queries queries.json --output result.json --audit audit.json`。输入／查询／输出／审计路径必须互异，拒绝覆盖输入和已有输出；全部成功后才发布完整result，失败只写失败audit，退出非零。审计记录构建／依赖锁身份、输入／查询／结果SHA、安装次数、查询数、lot序、错误码／路径及退出码。oracle由外部测试驱动比较。

## 构建与验证

CMake生成同一`hanmo_world`静态库、CLI及本片测试；不自动联网FetchContent。依赖由调用方提供已经核对锁的本地路径。首闭环不需要godot-cpp、SCons、Emscripten或其他端SDK。

可复现构建命令（变量由调用方指定项目局部已核路径，不安装到系统）：

`SOURCE`为当前真Git检出根，`RUN`为本次独立构建／结果目录，`DEPS`下的flecs/json/picosha2须分别检出锁定commit，`CMAKE`是已核便携包内bin/cmake，`PYTHON`是项目已有解释器。`EXPORTS`是[可信导出与旧oracle准备流程](../../tests/fixtures/native_inventory_projection/README.md)生成的目录；已有冻结工件可直接复用，不需要再次开局。下方oracle-results和cli-failures必须是尚不存在的新目录。

```sh
"$CMAKE" -S "$SOURCE/native/world" -B "$RUN/build" -DCMAKE_BUILD_TYPE=Release \
  -DHANMO_FLECS_ROOT="$DEPS/flecs" -DHANMO_JSON_ROOT="$DEPS/json" -DHANMO_PICOSHA2_ROOT="$DEPS/picosha2"
"$CMAKE" --build "$RUN/build" --parallel 2
"$RUN/build/test_inventory_projection"
"$PYTHON" "$SOURCE/tests/fixtures/native_inventory_projection/run_oracle.py" \
  --exports "$EXPORTS" --executable "$RUN/build/hanmo_sim" --output "$RUN/oracle-results"
"$PYTHON" "$SOURCE/native/world/tests/check_cli_failures.py" --executable "$RUN/build/hanmo_sim" \
  --artifact "$EXPORTS/cases/actual_purchase/artifact.json" --queries "$EXPORTS/cases/actual_purchase/queries.json" --output "$RUN/cli-failures"
```

源码或锁变化触发CMake重配置以刷新审计摘要；冻结新源码提交后应再显式配置一次，令CLI编译身份记录该commit。仅文档变化可保留原编译源码commit，明确文档后继身份即可。已有导出工件继续按自身原source_commit和SHA绑定，不为切换native候选重复旧世界全门。Git取得的依赖记录实际Git身份／文件SHA，无不存在的archive摘要；CMake二进制包另记真实archive SHA和许可证。

测试分为：纯wire错误与边界、一次安装/只读/Flecs原序、CLI无部分输出、实际GDScript采购来源与独立oracle。缺依赖或许可时明确未运行；不把源文件存在、编译或JSON解析成功当作native查询通过。


## 第二profile：离线人物承诺关联查询

`HM-NATIVE-LEGACY-ARTIFACT/1`承载同次旧Schema已验的运行人物、资源、旧活动、旧任职与组织目录定义及必要来源引用。`ProjectionStore`与旧`InventoryProjection`共用同一Flecs存储实现、库存组件和count；旧v1只含库存，不补造人物/钱包，也不能请求新查询。完整合同见最小实施计划（未公开），真实导出与独立旧oracle见[新profile准备流程](../../tests/fixtures/native_legacy_projection/README.md)。

新CLI入口保持原四个路径参数和不覆盖发布约定：

```sh
"$RUN/build/hanmo_sim" actor-commitments \
  --input "$EXPORTS/actual_reporting/artifact.json" \
  --queries "$EXPORTS/actual_reporting/queries.json" \
  --output "$RUN/commitments.json" --audit "$RUN/commitments-audit.json"
```

输出只关联所选人物的raw location、私人资产引用、当前行动真实钱/货预留和任职历史；没有当前行动或已经交割无预留时返回空预留行，零金额预留仍存在。参与者与匹配批次原序保持，lot_index是原物理位置。disabled office可保缺失历史fact引用并明确unresolved，不产生授权。动态Actor与其创建Fact可同ID，catalog原型/组织领袖不补成真人。

结果的`results`整段沿上述TaggedValue编码；每个query_id对应actors按原ID排序。工件/查询绑定精确字节SHA，失败无部分结果；全字段重组摘要来自实际已安装组件，不通过查询dump全部资产。完整原lots顺序由audit验证，query仅返回匹配的真实预留批。

本profile仍为artifact绑定的开发真值读取；缺真实source_world_id，不能称完整M1。新多实体Bridge DTO、在线刷新、保存迁移和写回后置。既有Bridge继续只接受库存v1，并用同核心跑受影响兼容门；Linux C-only符号边界保持，headless冷editor首导入134的既有状态不因CLI结果改变。
