CXX = g++
#开启调试模式
CFLAGS += -D__DEBUG   
CFLAGS += -std=c++17 -O2 -Wall -g 

TARGET = server
OBJS = ../src/log/*.cpp ../src/pool/*.cpp ../src/time/*.cpp \
       ../src/http/*.cpp ../src/server/*.cpp \
       ../src/buffer/*.cpp ../src/main.cpp

all: $(OBJS)
	$(CXX) $(CFLAGS) $(OBJS) -o ../bin/$(TARGET)  -pthread -lmysqlclient

clean:
	rm -rf ../bin/$(OBJS) $(TARGET)




