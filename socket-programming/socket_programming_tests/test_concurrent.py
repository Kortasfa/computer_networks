import threading
from .common import find_free_port, wait_port, run_server, run_client, read_all_output


def client_job(results: list, idx: int, name: str, number: int, host: str, port: int) -> None:
    res = run_client(name, number, host, port)
    out = res.stdout.decode() if isinstance(res.stdout, (bytes, bytearray)) else res.stdout
    results[idx] = out


def main() -> None:
    port = find_free_port()
    srv = run_server(port)
    try:
        wait_port("127.0.0.1", port)
        results = [None, None]
        t1 = threading.Thread(target=client_job, args=(results, 0, "Client A", 20, "127.0.0.1", port))
        t2 = threading.Thread(target=client_job, args=(results, 1, "Client B", 30, "127.0.0.1", port))
        t1.start(); t2.start()
        t1.join(); t2.join()

        for out in results:
            assert out is not None
            assert "[client] Sum: " in out
    finally:
        srv.terminate()
        read_all_output(srv)


if __name__ == "__main__":
    main()


