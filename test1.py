import socket
import time

def connect():
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.connect(('127.0.0.1', 8001))
    return s

sockets = [connect() for i in range(15)]


for s in sockets:
    s.sendall(b'hello from ')

time.sleep(2)

for i, s in enumerate(sockets):
    s.sendall(f'number {i}!'.encode('utf-8'))

time.sleep(5)

for i, s in enumerate(sockets):
    s.shutdown(socket.SHUT_WR)

time.sleep(5)
