OPS= sw_tcp_server.c sw_encoding_package.c

CLIENT=sw_tcp_client.c sw_encoding_package.c 

tcp_server:$(OPS)
	$(CC) $(OPS) -pthread -o tcp_server

tcp_client:$(CLIENT)
	$(CC) $(CLIENT) -pthread -o tcp_client

clean:
	rm -rf *.o tcp_server tcp_client

deletefile:
	rm -rf *.pes *.es *.i shoes2.ts shoes1.ts ./recv/*
