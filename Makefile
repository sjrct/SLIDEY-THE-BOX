CFLAGS  = `pkg-config --cflags cairo` `pkg-config --cflags x11` -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers -Wno-unused-but-set-variable
LDFLAGS = `pkg-config --libs   cairo` `pkg-config --libs   x11` -lglfw -lGL -lGLEW -lm

all: project2

project2: project2.o
	g++ -o $@ $< $(LDFLAGS)

project2.o: project2.cpp cube.h plane.h stb_image.c
	g++ -c project2.cpp $(CFLAGS)

clean:
	-rm *.o
	-rm project2
