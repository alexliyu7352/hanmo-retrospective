// 构建加速准入：时间内建宏不得进入PCH或缓存结果。
#pragma GCC poison __DATE__ __TIME__ __TIMESTAMP__
