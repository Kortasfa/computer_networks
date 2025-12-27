# RDTP (Reliable Data Transfer Protocol) — UDP + Go-Back-N

Реализация учебного протокола надежной передачи данных поверх UDP (эмуляция потерь/задержек делается внешними средствами ОС).

## Сборка

```bash
cd RDTP
make
```

Будут собраны два бинарника:
- `rdt_sender`
- `rdt_receiver`

## Запуск

### Receiver

```bash
./rdt_receiver <port> <output_file>
```

### Sender

```bash
./rdt_sender <host> <port> <file_to_send>
```

Опции sender:
- `-w <window_packets>`: размер окна в пакетах (по умолчанию 64)
- `-t <timeout_ms>`: тайм-аут ретрансляции (по умолчанию 200ms)
- `-m <mss_bytes>`: размер payload в DATA-пакете (по умолчанию 1000, максимум 1400)

Пример (localhost):

```bash
./rdt_receiver 9000 out.bin
./rdt_sender 127.0.0.1 9000 in.bin
diff in.bin out.bin
```

## Алгоритм надежности (базовый уровень)

Используется **Go-Back-N**:
- sender держит окно неподтвержденных сегментов и шлет новые сегменты, пока окно не заполнено;
- receiver принимает **только** сегмент с ожидаемым `seq` и сразу отправляет **cumulative ACK** (номер последнего принятого по порядку сегмента);
- при тайм-ауте sender ретранслирует **все** сегменты в окне.

Завершение передачи: sender посылает `FIN(seq = next_seq)` и ждет `ACK(seq = fin_seq)`.

## Формат пакета

Фиксированный заголовок 20 байт + payload.

- `magic` (4): `RDTP`
- `version` (1): 1
- `type` (1): `DATA=1`, `ACK=2`, `FIN=3`
- `reserved` (2): 0
- `seq` (4): номер сегмента (для ACK — ack number)
- `len` (2): длина payload
- `hdrLen` (2): 20
- `crc32` (4): CRC32(header(with crc32=0) + payload)

## Тестирование потерь/задержек

На Linux удобно использовать `tc netem`, например:

```bash
sudo tc qdisc add dev lo root netem loss 20% delay 50ms 10ms
# ... запуск sender/receiver ...
sudo tc qdisc del dev lo root
```
