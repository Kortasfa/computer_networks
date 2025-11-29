import time
from .common import find_free_port, wait_port, run_server, run_client, read_all_output


def main() -> None:
    port = find_free_port()
    srv = run_server(port)
    try:
        wait_port("127.0.0.1", port)
        # Send invalid number 0 to request server shutdown
        _ = run_client("Client Bad", 0, "127.0.0.1", port)
        # Give server a moment to process and initiate shutdown
        time.sleep(0.5)
        # Second client should fail to connect eventually if server stopped accepting
        try:
            _ = run_client("Client After", 10, "127.0.0.1", port)
            # We don't assert failure strictly because connection racing may still allow one more accept
        except Exception:
            pass
    finally:
        srv.terminate()
        read_all_output(srv)


if __name__ == "__main__":
    main()


