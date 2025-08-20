# Required libraries
+ libboost-all-dev
+ nlohmann-json3-dev
+ libcurl4-openssl-dev    
+ libsdl2-ttf-dev

# Quick load deck demo
Still working on clients loading decks.
Compile with
```
make clean all
```
then run a server with
```
./server_app
```
and two clients on different terminals with
```
./client_app
```
then type "upload <path>" on the client with priority to see your deck visualized.
