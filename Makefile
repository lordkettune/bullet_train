SOURCES = context.c lex.c parse.c thread.c

default:
	gcc $(SOURCES) -D BT_BUILD_DLL -D BT_DEBUG -shared -std=c11 -Wall -O2 -s -o bullet_train.dll

test: default
	gcc __test.c -o test.exe -L. -lbullet_train

clean:
	del /f bullet_train.dll test.exe
