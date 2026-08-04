#!/usr/bin/python3

import os
from urllib.parse import parse_qs

query = parse_qs(os.environ.get("QUERY_STRING", ""))

name = query.get("name", ["Anonymous"])[0]

print("Content-Type: text/html")
print()

print("<html><body>")
print(f"<h2>Hello, {name}!</h2>")
print("</body></html>")