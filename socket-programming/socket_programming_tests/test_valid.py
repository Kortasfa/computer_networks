from .common import find_free_port, wait_port, run_server, run_client, read_all_output


def main() -> None:
    port = find_free_port()
    srv = run_server(port)
    try:
        wait_port("127.0.0.1", port)
        name = "Client of Test"
        number = 7
        res = run_client(name, number, "127.0.0.1", port)
        out = res.stdout.decode() if isinstance(res.stdout, (bytes, bytearray)) else res.stdout

        assert "[client] Client name: " in out
        assert "[client] Server name: Server of Computer Networks" in out
        assert "[client] Client number: 7" in out
        assert "[client] Server number: 50" in out
        assert "[client] Sum: 57" in out
    finally:
        srv.terminate()
        read_all_output(srv)


if __name__ == "__main__":
    main()


