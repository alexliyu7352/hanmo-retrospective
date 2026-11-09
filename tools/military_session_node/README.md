# 军令草案和确认的真实 Main 节点

复用已批准作者接敌输入和既有资源 stage。初次准入仅装该世界夹具，原红从真实 Main 系统菜单保存之后开始：实际 `MilitaryHeading` 下拉改向，正常菜单读回同一银行，新页必须恢复默认军令。后续世界、规则与存读皆由真实 Main/原生 Bridge 执行。

`run_node.py` 必须指定完整产品源 commit、固定 4.7.2 编辑器、既有只读参考 stage、原输入及全新输出目录。程序从该 commit 提取实际运行源码，复制参考资源与 SHA 锁定的原生库，每次单独隔离 XDG、编辑器便携目录和银行。稀疏恢复仓库不使用会遍历缺失无关 blob 的 `git archive`。

```sh
python3 tools/military_session_node/run_node.py \
  --reference-stage /absolute/approved/stage/game \
  --input /absolute/approved/red/input.json \
  --output /absolute/new/session-node \
  --editor "$GODOT_BIN" \
  --source-commit FULL_COMMIT_SHA \
  --mode full
```

`red` 只证明可达恢复缺陷。`full` 另核同源重绘、真实私有磁盘故障下失败读取保草案、旧输入/确认/战果关闭拒绝、当前军令只提交一次及单半日真实战果、正常返回标题后继续同银行、正常启程生成另一真实 world_id，以及第二独立进程空标题冷读同银行。失败读取仅在本次副本的槽已列出后破坏银行，断言失败后恢复其原字节；不修改旧来源或任何用户银行。

旧回调压力测试显式保留并隐藏真实旧控件，再发送其原信号；这与正常菜单可达原红分别报告，不能将保留控件称作真实鼠标可点击路径。原作者目标 `max_rounds=1`，有限合法一次军令即产生真实战果页，不伪造结果态或重跑旧军事矩阵。

输出有 `binding.json`、`report.json`、原始 `process.log`、`evidence/` 和实际私有 `state/.../saves/`。完整世界与银行保存在输出原件，仓库只存源及精简回执；节点是固定编辑器 headless 控件信号证据，不是导出包或 GUI 像素/鼠标验收。
