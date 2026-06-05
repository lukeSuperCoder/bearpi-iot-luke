#!/usr/bin/env python3
"""Launch emoji controller page on localhost with a simple HTTP server."""

import http.server
import socketserver
import webbrowser
import os
import sys

PORT = 8000

os.chdir(os.path.dirname(os.path.abspath(__file__)))

handler = http.server.SimpleHTTPRequestHandler

try:
    with socketserver.TCPServer(("", PORT), handler) as httpd:
        url = f"http://localhost:{PORT}/emoji_controller.html"
        print(f"Serving at {url}")
        print("Press Ctrl+C to stop")
        webbrowser.open(url)
        httpd.serve_forever()
except KeyboardInterrupt:
    print("\nStopped.")
except OSError as e:
    if "Address already in use" in str(e):
        print(f"Port {PORT} already in use. Try: lsof -i :{PORT}")
    else:
        raise
