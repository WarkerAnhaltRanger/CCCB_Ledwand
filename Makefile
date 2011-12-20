CCCBDisplay: main.c image.c ledwand.c
	gcc -o $@ `pkg-config gstreamer-0.10 --cflags` $^ `pkg-config gstreamer-0.10 --libs` -lgstapp-0.10

.PHONY: clean
clean:
	rm -f CCCBDisplay main.o image.o ledwand.o
