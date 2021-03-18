CFLAGS=-Wall -g
LDFLAGS=-lglfw -lGL -lm -lGLEW -lX11 -lGLU -DGLEW_STATIC

build: main.o vector.o glhelper.o events.o
	gcc $(CFLAGS) -o build/a.out build/main.o build/vector.o build/events.o build/glhelper.o $(LDFLAGS)
	chmod +x build/a.out

main.o: main.c
	gcc $(CFLAGS) -c main.c -o build/main.o $(LDFLAGS)
glhelper.o: glhelper.c
	gcc $(CFLAGS) -c glhelper.c -o build/glhelper.o $(LDFLAGS)
vector.o: vector.c
	gcc $(CFLAGS) -c vector.c -o build/vector.o $(LDFLAGS)
events.o: events.c
	gcc $(CFLAGS) -c events.c -o build/events.o $(LDFLAGS)
tests.o: tests.c
	gcc $(CFLAGS) -c tests.c -o build/tests.o $(LDFLAGS)
clean:
	rm build/*.o

run: build
	@echo build/a.out
	@echo ""
	@build/a.out

test: tests.o vector.o events.o
	gcc build/tests.o build/vector.o -o build/tests
	chmod +x build/tests
	build/tests