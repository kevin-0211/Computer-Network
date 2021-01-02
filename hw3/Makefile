CC = g++
OPENCV =  `pkg-config --cflags --libs opencv`

RECEIVER = receiver.cpp
SENDER= sender.cpp
AGENT = agent.cpp
RECV = receiver
SEND = sender
AGN = agent

all: server client agent
  
sender: $(SENDER)
	$(CC) $(SENDER) -o $(SEND)  $(OPENCV) 
receiver: $(RECEIVER)
	$(CC) $(RECEIVER) -o $(RECV)  $(OPENCV)
agent: $(AGENT)
	$(CC) $(AGENT) -o $(AGN)  $(OPENCV)

.PHONY: clean

clean:
	rm $(RECV) $(SEND) $(AGENT)