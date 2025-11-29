#!/bin/bash
# Скрипт для быстрого тестирования SMTP-клиента

echo "======================================"
echo "  SMTP Client - Тестовый скрипт"
echo "======================================"
echo ""

# Проверка наличия исполняемого файла
if [ ! -f "./smtp_client" ]; then
    echo "❌ Ошибка: smtp_client не найден"
    echo "Запустите: make"
    exit 1
fi

echo "✅ Исполняемый файл найден"
echo ""

# Проверка Python3
if ! command -v python3 &> /dev/null; then
    echo "❌ Ошибка: Python3 не установлен"
    exit 1
fi

echo "✅ Python3 найден"
echo ""

echo "======================================"
echo "Для тестирования выполните:"
echo "======================================"
echo ""
echo "1. В ПЕРВОМ терминале запустите SMTP-сервер:"
echo "   python3 -m smtpd -n -c DebuggingServer localhost:25"
echo ""
echo "   Если требуются права администратора:"
echo "   sudo python3 -m smtpd -n -c DebuggingServer localhost:25"
echo ""
echo "2. Во ВТОРОМ терминале запустите клиент:"
echo "   ./smtp_client localhost test@example.com recipient@test.com \"Test Subject\" \"Hello World\""
echo ""
echo "======================================"
echo ""

read -p "Запустить тестовый SMTP-сервер сейчас? (y/n): " choice

if [ "$choice" = "y" ] || [ "$choice" = "Y" ]; then
    echo ""
    echo "Запуск тестового SMTP-сервера на localhost:25..."
    echo "Нажмите Ctrl+C для остановки"
    echo ""
    
    # Попытка запуска без sudo
    python3 -m smtpd -n -c DebuggingServer localhost:25 2>/dev/null
    
    # Если не удалось, попробовать с sudo
    if [ $? -ne 0 ]; then
        echo ""
        echo "Требуются права администратора..."
        sudo python3 -m smtpd -n -c DebuggingServer localhost:25
    fi
else
    echo ""
    echo "Вы можете запустить сервер вручную командой выше."
fi
