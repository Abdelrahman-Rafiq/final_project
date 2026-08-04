#!/usr/bin/python3

import os

print("Content-Type: text/html")
print()

print("<html><body>")
print("<h2>CGI Environment Variables</h2>")
print("<table border='1'>")

for key in sorted(os.environ):
    print(f"<tr><td>{key}</td><td>{os.environ[key]}</td></tr>")

print("</table>")
print("</body></html>")