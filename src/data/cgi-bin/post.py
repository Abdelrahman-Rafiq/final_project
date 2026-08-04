#!/usr/bin/python3

import os
import sys

length = int(os.environ.get("CONTENT_LENGTH", "0"))

body = sys.stdin.read(length)

print("Content-Type: text/html")
print()

print("<html><body>")
print("<h2>POST Body Received</h2>")
print(f"<pre>{body}</pre>")
print("</body></html>")