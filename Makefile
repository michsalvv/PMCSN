all:
	gcc main.c ./DES/rngs.c ./DES/rvgs.c -lm -o main

clean:
	rm main
