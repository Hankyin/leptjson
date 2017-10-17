test:leptjson.c leptjson.h test.c
	gcc -c test.c 
	gcc -c  leptjson.c 
	gcc leptjson.o test.o -o leptjson
.PHONY:clean
clean:
	rm -rf *.o leptjson