CC = gcc
TAR = tar -cvf
FILES = chat.c
OUT_EXE = chat
TAR_BALL = chat.tar
TAR_FILES = README.txt Makefile

build: $(*.c)
	$(CC) -o $(OUT_EXE) $(FILES)

tar:
	$(TAR) $(TAR_BALL) $(FILES) $(TAR_FILES)

clean:
	rm -f *.o $(OUT_EXE)