CCCBDisplay: main.c image.c ledwand.c embedded_audio.c
	gcc -std=gnu99 -pedantic -Wall -Wextra -Wno-unused-parameter -g -O3 -o $@ `pkg-config gstreamer-0.10 --cflags` $^ `pkg-config gstreamer-0.10 --libs` -lz -lgstapp-0.10
	#clang -g -O3 -o $@ -std=gnu99 \
	#	-pedantic -Weverything -Wno-system-headers -Wno-documentation \
	#	`pkg-config gstreamer-0.10 --cflags` $^ `pkg-config gstreamer-0.10 --libs` -lgstapp-0.10 \
	#	-isystem-prefix 'gst/' -isystem-prefix 'gstreamer-0.10/'

.PHONY: clean
clean:
	rm -f CCCBDisplay main.o image.o ledwand.o

