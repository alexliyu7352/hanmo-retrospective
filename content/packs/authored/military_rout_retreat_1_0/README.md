# 溃军归路

独立场景 rout_watch::scenario.rout_road 依原 Military9 北营人物、粮额、军令和伤员恢复来源，改选原青溪陆路地图。正常受职、编军和接敌；不预设士气、伤亡、被俘或胜利。

仅在本场景明确选择的 qingxi::field.road 野外接敌启用 two_blocked_rounds_v1：士气归零后下轮自动向真实本方出口撤离；连续两轮无路可走且仍有真实相邻有效敌军才移入被俘池。出口离场保留当时伤兵和可战兵员，不再自动作战。同一战场用于围城时仍保原墙体/结算路径。

不预设新船队。原依赖钱货、匿名士卒及具名人物不变。旧场景未选 Military10 保留原算法/编码/RNG。当前 SOURCE_ONLY；尚未正常 compose、Installed 或完整 World 冷链。
