kilo: kilo.c utils.c
	$(CC) -c kilo.c -Wall -Wextra -pedantic -std=c99
	$(CC) -c utils.c -Wall -Wextra -pedantic -std=c99
	$(CC) -o grades kilo.o utils.c

clean:
	rm kilo