# 军团野战有限页面局部门

复用 `military_march_kernel/prepare_fixture.py` 的正式 tiny 作者 compose/stage，再复制现有 UI 与精确旧 e31bf73 Screen/Canvas 字节。两张 2×2 原测试图仅用于资源与控件输入，不是正式美术或视觉验收。原 wav 从同一 Git 对象读取；不下载资源。

在已包含中央 b5444c7 门面/Composer 与本片候选的工作树中：

```sh
python tests/fixtures/military_battle_presentation_input/prepare_fixture.py \
  --root "$PWD" --output "$PWD/.local/military-page-check" \
  --fixture-art /absolute/path/to/original/tiny/art
Godot_v4.7.2-stable_linux.x86_64 --headless \
  --path "$PWD/.local/military-page-check/game" --editor --import
Godot_v4.7.2-stable_linux.x86_64 --headless \
  --path "$PWD/.local/military-page-check/game" \
  --script res://tests/test_military_battle_presentation_input.gd
Godot_v4.7.2-stable_linux.x86_64 --headless \
  --path "$PWD/.local/military-page-check/game" \
  --script res://tests/test_military_battle_screen.gd
```

按本项目要求先 source env.sh 并将 HOME/XDG/temp 定点到独立测试目录；固定编辑器，不运行默认完整 verify。`--refresh` 仅更新当前候选，不重录旧 oracle；不适用于更改已 stage 的作者/能力元数据。

新门核有限页与深副本、原 Round 查询/几何、原 Canvas 绘制参数、局部查询计数、五戳失效、真 Main 一条任官编组接敌/军令/半日/结果关闭链。旧 UI 门保真实 Viewport 输入；其旧钱包形状与落后于现有近战规则的断言已显式适配。固定旧原始文件和所有红日志保留。

425/0 新门与54/0邻门是此片已执行身份的局部证据；不是多人全部会战、完整 GUI、正式美术或存读验收。新源码若变化须重跑其受影响门，不能沿用数字当新身份验收。
