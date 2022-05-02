kilo: team2_editor.cpp 
	g++ -c team2_editor.cpp
	g++ -o grades team2_editor.o 

clean:
	rm team2_editor.o grades