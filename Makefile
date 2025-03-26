bacon: main.o
	g++ -o bacon main.o -lcurl

main.o: main.cpp
	g++ -g -c main.cpp -I ~/rapidjson/include

clean:
	rm *.o bacon