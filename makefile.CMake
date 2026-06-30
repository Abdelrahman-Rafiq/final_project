server:
    @echo "Building server..."
    @mkdir -p build/server
    @cd build/server && cmake ../../server && make 