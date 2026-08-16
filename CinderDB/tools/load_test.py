#!/usr/bin/env python3
import argparse
import socket
import threading
import time

def readline(connection):
    data = bytearray()
    while True:
        chunk = connection.recv(1)
        if not chunk:
            raise RuntimeError("server disconnected")
        if chunk == b"\n":
            return data.decode("utf-8")
        data.extend(chunk)

def main():
    parser = argparse.ArgumentParser(description="CinderDB localhost load generator")
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", required=True, type=int)
    parser.add_argument("--clients", required=True, type=int)
    parser.add_argument("--requests", required=True, type=int)
    parser.add_argument("--mode", choices=("get", "put"), required=True)
    args = parser.parse_args()
    if args.port < 1 or args.port > 65535 or args.clients < 1 or args.requests < 1:
        parser.error("port, clients, and requests must be positive")

    failures = 0
    failures_lock = threading.Lock()
    gate = threading.Barrier(args.clients)

    def client(client_id):
        nonlocal failures
        local_failures = 0
        try:
            with socket.create_connection((args.host, args.port), timeout=5) as connection:
                connection.settimeout(10)
                gate.wait()
                for request in range(args.requests):
                    key = f"load_{client_id}_{request}"
                    wire = f"GET {key}\n" if args.mode == "get" else f"PUT {key} value_{request}\n"
                    connection.sendall(wire.encode("utf-8"))
                    reply = readline(connection)
                    if args.mode == "get":
                        if reply not in ("NOT_FOUND",) and not reply.startswith("VALUE "):
                            local_failures += 1
                    elif reply != "OK":
                        local_failures += 1
        except Exception:
            local_failures += args.requests
        with failures_lock:
            failures += local_failures

    threads = [threading.Thread(target=client, args=(index,)) for index in range(args.clients)]
    started = time.perf_counter()
    for thread in threads:
        thread.start()
    for thread in threads:
        thread.join()
    elapsed = time.perf_counter() - started
    total = args.clients * args.requests
    print(f"mode={args.mode}")
    print(f"clients={args.clients}")
    print(f"operations={total}")
    print(f"elapsed_seconds={elapsed:.6f}")
    print(f"operations_per_second={total / elapsed:.2f}")
    print(f"errors={failures}")
    raise SystemExit(0 if failures == 0 else 1)

if __name__ == "__main__":
    main()
