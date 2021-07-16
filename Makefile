a3q1: a3q1.c
	clang -Wall -g -Wpedantic -Wextra -Werror a3q1.c -o a3q1 -lpthread 
clean:
	rm -rf a3q1.o a3q1.dSYM a3q1 