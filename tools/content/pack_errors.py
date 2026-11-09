"""为包发现、选择与编译提供稳定诊断；不把结构通过声明为运行验证。"""
from __future__ import annotations


class PackError(ValueError):
    """携带可定位错误码和上下文；失败调用者不得发布部分内容集。"""

    def __init__(self, code: str, message: str, details: dict | None = None):
        """保留诊断原义和机器上下文，不隐式吞掉底层失败。"""
        super().__init__(message)
        self.code = code
        self.message = message
        self.details = dict(details or {})

    def to_dict(self) -> dict:
        """输出可直接用于CLI或编辑器的诊断，明确未执行运行验收。"""
        return {'code': self.code, 'message': self.message,
                'details': self.details, 'runtime_verified': False}
