"""Robust static server for Afterglow dev.
Threaded (no single-connection hangs), no-store so Ctrl+F5 is never needed,
quiet logging. Replaces `python -m http.server` which was hanging.

Usage: python scratch/serve.py [port] [root]
"""
import http.server
import socketserver
import os
import sys

PORT = int(sys.argv[1]) if len(sys.argv) > 1 else 5500
ROOT = sys.argv[2] if len(sys.argv) > 2 else r"F:\Afterglow"
os.chdir(ROOT)


class Handler(http.server.SimpleHTTPRequestHandler):
    def log_message(self, fmt, *args):
        pass

    def end_headers(self):
        self.send_header("Cache-Control", "no-store, must-revalidate")
        self.send_header("Pragma", "no-cache")
        super().end_headers()

    def guess_type(self, path):
        # .glb must be model/gltf-binary for some loaders; .ogg as audio/ogg
        if path.endswith(".glb"):
            return "model/gltf-binary"
        return super().guess_type(path)


class Server(socketserver.ThreadingTCPServer):
    allow_reuse_address = True
    daemon_threads = True


if __name__ == "__main__":
    with Server(("127.0.0.1", PORT), Handler) as httpd:
        print("serving %s on http://127.0.0.1:%d" % (ROOT, PORT), flush=True)
        httpd.serve_forever()
