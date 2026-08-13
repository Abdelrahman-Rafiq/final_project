#!/usr/bin/python3

body = "Hello CGI"

print("Content-Type: text/plain")
print(f"Content-Length: {len(body)}")
print()
print(body, end="")