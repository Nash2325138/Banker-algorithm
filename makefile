all:
	g++ -Wall -std=c++11 -o banker banker.cpp
clean:
	rm -f banker log.txt log_no_color.txt