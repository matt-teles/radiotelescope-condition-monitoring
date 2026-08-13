import socket
import time
import sys

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
        start_time = time.monotonic()
        last_report = start_time


        while True:
            data = conn.recv(1024)

            if not data:
                break

            sample_count += data.count(b"\n")

            now = time.monotonic()

            if now - last_report >= 5.0:
                elapsed = now - start_time
                sample_rate = sample_count / elapsed
                # Clear terminal and move cursor to the top-left corner.
                print("\033[2J\033[H", end="")

                print(f"Connected: {addr}")
                print(f"Total samples: {sample_count}")
                print(f"Elapsed time: {elapsed:.2f} s")
                print(f"Average rate: {sample_rate:.2f} samples/s")

                last_report = now