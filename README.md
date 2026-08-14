# W Server ── A concurrent HTTP/1.1 web server in C

W Server is a multi-threaded HTTP/1.1 web server with an LRU file cache and a CGI execution engine all written in C language from scratch.

## Architecture

   [!note](docs/Server%20Arch.png)

   - Thread pool: fixed N worker threads pulling from a mutex-protected queue, avoiding per-connection thread creation overhead
   - LRU cache: hash table for O(1) lookup combined with a doubly linked list for O(1) eviction, bounded by total bytes rather than entry count
   - CGI engine: fork/exec pattern with dup2 pipe redirection, environment variable translation from HTTP headers, handling timeout for hanging scripts

## Features

   - HTTP/1.1 Server with persistent keep-alive connections 
   - Multi-threaded request handling via a fixed thread pool
   - In-memory LRU file cache with configurable byte limit
   - CGI script execution with full environment variable translation
   - MIME type detection for common file types
   - Correct HTTP error responses (400, 403, 404, 405, 500, 504, 505)
   - Graceful handling of malformed requests and hanging CGI scripts

## Design Decisions

   - **Thread Pool Vs spawning**
      > I've used a thread pool instead of just spawning a thread per connection because thread creation is expensive (memory allocation for the stack, OS overhead) and under load (hundreds of connections per second) this collapses. Instead, I use a fixed N threads to handle requests when exist in parallel and keep them asleep when no need for them. 
   - **Cache Layer**
      > I've implemented the cache using a hash table for O(1) lookup combined with a doubly linked list for O(1) eviction. This hash table is bounded by total bytes of files in this structure rather than entry count.
   - 

## Limitations

   - Supports GET and POST only — PUT, DELETE, HEAD not implemented
   - No TLS/HTTPS support
   - No chunked transfer encoding
   - Cache invalidation not implemented — server restart required if files change on disk
   - CGI only, no FastCGI or WSGI support

## Refrences
   
   - The Internet's Architecture:
      - The Internet's Layered Network Architecture : https://www.codequoi.com/en/internet-layered-network-architecture/
   
   - HTTP/1.1 Specifications:
      - RFC 7230: Hypertext Transfer Protocol (HTTP/1.1) : https://www.rfc-editor.org/info/rfc7230/

   - Multi-threading :
      - Beej's Guide to Network Programming : https://beej.us/guide/bgnet/
      - Multi-threading and Multi-processing : https://www.baeldung.com/cs/multiprocessing-multithreading
      - multi-threading : https://embeddedprep.com/posix-threads-pthread/

   - UNIX Tools in C :
      - fork, exec : https://www.codequoi.com/en/creating-and-killing-child-processes-in-c/ - https://thelinuxcode.com/fork-exec-coding-c/
      - exec family : https://www.geeksforgeeks.org/c/exec-family-of-functions-in-c/
      - Pipe : https://www.codequoi.com/en/pipe-an-inter-process-communication-method/

   - Common Gateway Interface :
      - RFC 3875: The Common Gateway Interface (CGI) : https://www.rfc-editor.org/info/rfc3875/
      - CGI: https://www.geeksforgeeks.org/python/get-and-post-in-python-cgi/
      - [simple tutorial] (https://www.eskimo.com/~scs/cclass/handouts/cgi.html) 
      