#!/bin/bash

echo "======================================"
echo "  SMTP Client - Тестовый скрипт"
echo "======================================"
echo ""

if [ ! -f "./smtp_client" ]; then
    echo "❌ Ошибка: smtp_client не найден"
    echo "Запустите: make"
    exit 1
fi

echo "✅ Исполняемый файл найден"
echo ""

# Проверка системного Python3
PYTHON_CMD="/usr/bin/python3"
if [ ! -f "$PYTHON_CMD" ]; then
    PYTHON_CMD="python3"
fi

echo "✅ Используется Python: $PYTHON_CMD"
echo ""

echo "======================================"
echo "Для тестирования выполните:"
echo "======================================"
echo ""
echo "1. В ПЕРВОМ терминале запустите SMTP-сервер:"
echo "   $PYTHON_CMD -m smtpd -n -c DebuggingServer 127.0.0.1:2525"
echo ""
echo "   (Порт 2525 не требует прав администратора)"
echo ""
echo "2. Во ВТОРОМ терминале запустите клиент:"
echo "   ./smtp_client localhost test@example.com recipient@test.com \"Test Subject\" \"Hello World\""
echo ""
echo "======================================"
echo ""

read -p "Запустить тестовый SMTP-сервер сейчас? (y/n): " choice

if [ "$choice" = "y" ] || [ "$choice" = "Y" ]; then
    echo ""
    echo "Запуск тестового SMTP-сервера на localhost:2525..."
    echo "Нажмите Ctrl+C для остановки"
    echo ""
    
    $PYTHON_CMD -m smtpd -n -c DebuggingServer 127.0.0.1:2525
else
    echo ""
    echo "Вы можете запустить сервер вручную командой выше."
fi
