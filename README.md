# Usage
`make clean all` to compile.
Run server on one bash with `./server_app`.
Run clients on two different terminals with `./client_app`.
# Required libraries
+ libboost-all-dev
+ nlohmann-json3-dev
+ libcurl4-openssl-dev    
+ libsdl2-ttf-dev
# Compiling deck visualizer preview:
g++ main.cpp -I ../common -lSDL2 -lSDL2_image -lSDL2_ttf -lcurl
