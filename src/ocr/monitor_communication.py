import socket

with socket.socket() as s:
    s.connect(('127.0.0.1', 5000))
    while True:
        data = s.recv(64).decode('ascii')
        if data:
            print(data, end='', flush=True)
