
all: threadlibrary.c
		gcc -shared -fPIC  -o libthread.so threadlibrary.c -lrt -pthread
clean:
		rm -rf *o *so *out