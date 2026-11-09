#!/usr/bin/env python3
"""保留0.5命令兼容入口；发布实现统一由release.py维护，不复制规则或版本常量。"""
from release import assertion_count, write_launcher, run_suite, export_native, main

if __name__ == '__main__':
    raise SystemExit(main())
