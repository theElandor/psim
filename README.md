# Required libraries
+ libboost-all-dev
+ nlohmann-json3-dev
+ libcurl4-openssl-dev    
+ libsdl2-ttf-dev

# Deck visualizer
For now, the only sort of working part is the deck visualizer.
To compile the demo, use:
```
g++ main.cpp -I ../common -lSDL2 -lSDL2_image -lSDL2_ttf -lcurl
```
then run the executable.
