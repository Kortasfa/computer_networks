## UDP Pinger (C++)

Файлы:
- `udp_pinger_client.cpp` — клиент (10 UDP ping сообщений, тайм-аут 1с, RTT + статистика).
- `udp_pinger_server.cpp` — UDP эхо-сервер (может имитировать потери/задержку, и печатать "client died" по heartbeat).

### Сборка

```bash
cd udtp-pinger
make
```

### Запуск

Сервер (эхо):

```bash
./udp_pinger_server 9000
```

Клиент (ping 10 раз, по умолчанию 1с тайм-аут и 1с интервал):

```bash
./udp_pinger_client 127.0.0.1 9000
```

Имитировать потери/задержку на сервере:

```bash
./udp_pinger_server 9000 --loss-percent 30 --delay-ms 50
```

Heartbeat мониторинг (сервер печатает сообщение, если от клиента нет пакетов > N секунд):

```bash
./udp_pinger_server 9000 --heartbeat-timeout-sec 3
./udp_pinger_client --heartbeat 127.0.0.1 9000 --count 10 --interval-ms 500
```

### Краткое описание алгоритма (RTT и тайм-аут)

- **Тайм-аут**: на UDP сокете выставляется `SO_RCVTIMEO` на 1 секунду. Если `recvfrom()` возвращает ошибку `EAGAIN/EWOULDBLOCK`, считаем пакет потерянным и печатаем `Request timed out`.
- **RTT**: клиент кладёт в пакет метку времени `timestamp_ms` (миллисекунды от `system_clock`). Сервер эхо-возвращает тот же payload. При получении ответа клиент парсит `timestamp_ms` из ответа и считает \( RTT = now\_ms - timestamp\_ms \).
- **Статистика**: после N попыток считаются потери (lost/N) и `min/avg/max` по собранным RTT.


