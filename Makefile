out := sbkc
obj := main.o
	
cxxflags := -Wall -g `llvm-config --cflags` -std=gnu++11
ldflags  := -lrt -pthread -ldl -ltinfo
	
all: $(out)

run: all
	./$(out) ~/tmp/kc/test.bc
	
$(out): $(obj)
	g++ -o $@ $(ldflags) `llvm-config --ldflags` `llvm-config --libs` $(obj) 

%.o: %.cpp
	g++ -c -o $@ $(cxxflags) $< 
