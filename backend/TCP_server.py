import socket

HOST = "0.0.0.0"
PORT = 5000

# configures IPv4 with TCP protocol
with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as server:
    server.bind((HOST, PORT))
    server.listen(1)

    print("Waiting for connection...")

    conn, addr = server.accept()

    with conn:
        print("connected:", addr)

        while True:
            data = conn.recv(1024)

            if not data:
                break

            print(data.decode(), end="")