server:
    @echo "Building server..."
    @mkdir -p build/server
    @cd build/server && cmake ../../server && make 
    gcc -g -o ./src/Server/server.exe ./src/Server/server.c ./src/helpers/helpers.c ./src/requestParser/Request.c