CFLAGS = -Wall -Wextra -g -lm

build: server subscriber

subscriber: ./pcom_hw2_udp_client/subscriber.cpp
	g++ $(CFLAGS) ./pcom_hw2_udp_client/subscriber.cpp -o subscriber

server: ./pcom_hw2_udp_client/server.cpp
	g++ $(CFLAGS) ./pcom_hw2_udp_client/server.cpp -o server

clean:
	rm -f server subscriber