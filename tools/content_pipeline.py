#!/usr/bin/env python3
"""Entry point for developer-only content validation and preparation, not a game executable."""
import json
import sys
from pathlib import Path


def main() -> int:
    """Load the content tool and give a useful error when its developer dependency is absent."""
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
    try:
        from tools.content.cli import main as run
    except ImportError as exc:
        print(json.dumps({'status':'DEPENDENCY_MISSING','runtime_verified':False,
                          'message':str(exc),'install':'python3 -m pip install -r requirements-content.txt'}))
        return 2
    return run()


if __name__ == '__main__':
    raise SystemExit(main())
