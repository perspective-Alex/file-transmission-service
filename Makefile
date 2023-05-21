#CC=g++-4.9
CC=g++
CXX_FLAGS=-Wall -std=c++11

BUILD_DIR=build
DATA_DIR=data/GIF

build_client:
	mkdir -p ${BUILD_DIR}
	${CC} ${CXX_FLAGS} -I include src/data.cpp src/client.cpp -o ${BUILD_DIR}/client

build_server:
	mkdir -p ${BUILD_DIR}
	${CC} ${CXX_FLAGS} -I include src/data.cpp src/test.cpp src/server.cpp -o ${BUILD_DIR}/server

build: build_client build_server

run_client: build_client
	cd ${BUILD_DIR} && ./client ../${DATA_DIR}

run_server: build_server
	cd ${BUILD_DIR} && ./server

run: build_server build_client
	cd ${BUILD_DIR} && ./server &
	cd ${BUILD_DIR} && ./client ../${DATA_DIR}
