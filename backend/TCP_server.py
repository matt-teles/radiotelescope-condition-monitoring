import socket
import time

HOST = "0.0.0.0"
PORT = 5005

# configures IPv4 with TCP protocol
with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as server:
    server.bind((HOST, PORT))
    server.listen(1)

    print("Waiting for connection...")

    conn, addr = server.accept()

    with conn:
        print("connected:", addr)

        sample_count = 0
        last_report = time.monotonic()

        while True:
            data = conn.recv(1024)

            if not data:
                break

            sample_count += data.count(b"\n")
            now = time.monotonic()

            if now - last_report >= 1.0:
                print(f"Received: {sample_count} samples/s")

                sample_count = 0
                last_report = now