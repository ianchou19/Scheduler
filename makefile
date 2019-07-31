scheduler: scheduler.cpp
	g++ -std=c++11 -g scheduler.cpp -o scheduler

clean:
	rm -f scheduler *~
