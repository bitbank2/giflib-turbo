COMPILER=gcc
CFLAGS=-c -g -std=c99 -Wall -O3 -I/opt/homebrew/include
LINKFLAGS=-L/opt/homebrew/lib -lgif

old: gif_test_old

new: gif_test_new

sponge: gifsponge

wedge: gifwedge

gifwedge: gifwedge.o gif_lib.o getarg.o
	$(COMPILER) gifwedge.o getarg.o gif_lib.o -o gifwedge

gifsponge: gifsponge.o gif_lib.o
	$(COMPILER) gifsponge.o gif_lib.o -o gifsponge

gif_test_new: test.o gif_lib.o
	$(COMPILER) test.o gif_lib.o -o gif_test_new

gif_test_old: test.o
	$(COMPILER) test.o $(LINKFLAGS) -o gif_test_old

getarg.o: getarg.c getarg.h
	$(COMPILER) $(CFLAGS) getarg.c

gifwedge.o: gifwedge.c gif_lib.h
	$(COMPILER) $(CFLAGS) gifwedge.c

gifsponge.o: gifsponge.c gif_lib.h
	$(COMPILER) $(CFLAGS) gifsponge.c

gif_lib.o: gif_lib.c gif_lib.h
	$(COMPILER) $(CFLAGS) gif_lib.c

test.o: test.c
	$(COMPILER) $(CFLAGS) test.c

clean:
	rm -rf *.o gif_test*

