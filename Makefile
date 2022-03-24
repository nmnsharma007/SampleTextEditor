kilo: team2_editor.c utils.c
	$(CC) -c team2_editor.c -Wall -Wextra -pedantic -std=c99
	$(CC) -c team2_editor.c -Wall -Wextra -pedantic -std=c99
	$(CC) -o grades team2_editor.o utils.c

clean:
	rm team2_editor.o utils.o grades