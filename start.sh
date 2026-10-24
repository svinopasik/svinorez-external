#!/usr/bin/env bash
set -e

cd "$(dirname "$0")"

# Цвета
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m'

if [ ! -f "build/cs2espd" ]; then
    echo -e "${YELLOW}[!] Бинарник build/cs2espd не найден. Запускаю сборку...${NC}"
    ./build.sh
fi

# Проверка запущена ли CS2
if ! pgrep -x "cs2" >/dev/null 2>&1; then
    echo -e "${YELLOW}[!] Внимание: процесс cs2 сейчас не запущен.${NC}"
    echo -e "${YELLOW}[*] Демон запустится и будет ожидать запуск игры.${NC}"
fi

# Завершаем старый экземпляр если висит
if pgrep -x "cs2espd" >/dev/null 2>&1; then
    echo -e "${YELLOW}[*] Перезапуск существующего экземпляра cs2espd...${NC}"
    pkill -9 -x "cs2espd" 2>/dev/null || true
    sleep 0.5
fi

rm -f /tmp/cs2esp.sock

echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}   Запуск svinorez-external Daemon      ${NC}"
echo -e "${BLUE}========================================${NC}"
echo -e "Горячие клавиши в игре:"
echo -e "  - ${GREEN}[INSERT]${NC} : Открыть / закрыть интерактивное GUI меню"
echo -e "  - ${GREEN}[F8]${NC}     : Включить / выключить ESP"
echo -e "\nУправление из консоли:"
echo -e "  - ${YELLOW}./build/espctl status${NC}  : статус всех модулей"
echo -e "  - ${YELLOW}./build/espctl menu${NC}    : открыть меню"
echo -e "  - ${YELLOW}./build/espctl quit${NC}    : остановить чит"
echo -e "${BLUE}========================================${NC}\n"

exec ./build/cs2espd
