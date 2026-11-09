#!/usr/bin/env python3
"""Loopback development server for an actual exported Web directory, not a public hosting service."""
from __future__ import annotations
import argparse
from functools import partial
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path


class GameHandler(SimpleHTTPRequestHandler):
    """Serve exact resource MIME types; do not claim cross-origin isolation for a single-thread build."""
    extensions_map = {**SimpleHTTPRequestHandler.extensions_map, '.wasm':'application/wasm',
                      '.js':'text/javascript','.pck':'application/octet-stream','.hmsave':'application/json'}

    def end_headers(self) -> None:
        """Avoid stale shell resources during local iterative testing."""
        self.send_header('Cache-Control','no-store')
        super().end_headers()


def argument_parser() -> argparse.ArgumentParser:
    """Return conservative local defaults that can be explicitly overridden by the operator."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--directory',default='build/web')
    parser.add_argument('--host',default='127.0.0.1')
    parser.add_argument('--port',default=8060,type=int)
    return parser


def main() -> None:
    """Run a local server only when the chosen directory actually exists."""
    args = argument_parser().parse_args()
    directory = Path(args.directory).resolve()
    if not directory.is_dir():
        raise SystemExit(f'Directory does not exist: {directory}. Export the game first.')
    with ThreadingHTTPServer((args.host,args.port),partial(GameHandler,directory=str(directory))) as server:
        print(f'Serving {directory} at http://{args.host}:{server.server_port}/')
        try:
            server.serve_forever()
        except KeyboardInterrupt:
            pass


if __name__ == '__main__':
    main()
