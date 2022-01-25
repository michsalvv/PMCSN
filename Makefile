all:
	gcc ./DES/rngs.c ./DES/rvgs.c utils.c test.c -lm -o test -g

clean:
	rm test
