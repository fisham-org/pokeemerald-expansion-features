#!/usr/bin/env python3
"""Simple HTTP server for the Region Map Editor."""
import http.server
import socketserver
import os
import sys

PORT = int(sys.argv[1]) if len(sys.argv) > 1 else 8080

os.chdir(os.path.dirname(os.path.abspath(__file__)))

class Handler(http.server.SimpleHTTPRequestHandler):
    extensions_map = {
        **http.server.SimpleHTTPRequestHandler.extensions_map,
        '.js': 'application/javascript',
        '.css': 'text/css',
        '.html': 'text/html',
    }

    def log_error(self, format, *args):
        # Suppress broken pipe errors (browser closed connection before response finished)
        if len(args) >= 1 and isinstance(args[0], BrokenPipeError):
            return
        super().log_error(format, *args)


class ReusableTCPServer(socketserver.TCPServer):
    allow_reuse_address = True


with ReusableTCPServer(("", PORT), Handler) as httpd:
    print(f"Region Map Editor running at http://localhost:{PORT}")
    print("Press Ctrl+C to stop.")
    httpd.serve_forever()
