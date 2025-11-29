import os
import socket
import subprocess
import sys
import time
from contextlib import closing


def find_free_port() -> int:
    with closing(socket.socket(socket.AF_INET, socket.SOCK_STREAM)) as s:
        s.bind(("127.0.0.1", 0))
        return s.getsockname()[1]


def wait_port(host: str, port: int, timeout: float = 5.0) -> None:
    deadline = time.time() + timeout
    while time.time() < deadline:
        with closing(socket.socket(socket.AF_INET, socket.SOCK_STREAM)) as s:
            s.settimeout(0.2)
            try:
                if s.connect_ex((host, port)) == 0:
                    return
            except OSError:
                pass
        time.sleep(0.05)
    raise RuntimeError(f"Port {host}:{port} did not open in time")


def run_server(port: int) -> subprocess.Popen:
    server_path = os.path.join(os.path.dirname(__file__), "..", "server")
    server_path = os.path.abspath(server_path)
    return subprocess.Popen([server_path, str(port)], stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)


def run_client(name: str, number: int, host: str, port: int) -> subprocess.CompletedProcess:
    client_path = os.path.join(os.path.dirname(__file__), "..", "client")
    client_path = os.path.abspath(client_path)
    inp = f"{number}\n".encode()
    return subprocess.run([client_path, name, host, str(port)], input=inp, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)


def read_all_output(proc: subprocess.Popen) -> str:
    try:
        out = proc.communicate(timeout=2.0)[0]
    except subprocess.TimeoutExpired:
        proc.kill()
        out = proc.communicate()[0]
    return out or ""


