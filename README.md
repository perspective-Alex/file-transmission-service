### File transmission service

Client sends data to server using UDP with additional checksum control (and resend if necessary)

Backbone - UNIX sockets

##### Requirements
- `gcc (g++)` - version >= 4.9
- `make`
- (optional) `cmake` - version >= 3.5

##### Build

Using `cmake`:
```shell
mkdir build && cd build
cmake ..
cmake --build .
```

Using `make`:
```shell
make build
```

##### Run

Place you files into `data` directory. (Tested examples - `data/GIF`)

- Run server:
```shell
make run_server
```
- Replace `${DATA_DIR}` specified in Makefile with path to your own, run client:
```shell
make run_client
```

- Combined run (using one terminal):
```shell
make run
```