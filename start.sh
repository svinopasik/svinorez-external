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
    echo -e "${YELLOW}[*] Демон запустится и будет ожидать запуск игры в фоне.${NC}"
fi

# Завершаем старый экземпляр если висит
if pgrep -f "cs2espd" >/dev/null 2>&1; then
    echo -e "${YELLOW}[*] Перезапуск существующего экземпляра...${NC}"
    pkill -9 -f "cs2espd" 2>/dev/null || true
    sleep 0.3
fi

# Очистка устаревших сокетов из /tmp (если использовался старый путь)
rm -f /tmp/cs2esp.sock

# Автодетект Hyprland: отключение размытия (blur) для прозрачного оверлея
if [ -n "$HYPRLAND_INSTANCE_SIGNATURE" ] || command -v hyprctl >/dev/null 2>&1; then
    echo -e "${YELLOW}[*] Обнаружен Hyprland. Применяю правила отключения блюра для оверлея...${NC}"
    # 1. Применение правил через hyprctl eval для hyprland.lua (Hyprland 0.56+)
    hyprctl eval 'hl.window_rule({ match = { class = "cs2esp" }, no_blur = true, no_focus = true, no_initial_focus = true })' >/dev/null 2>&1 || true
    hyprctl eval 'hl.window_rule({ match = { class = "^(wl_srv_.*)$" }, no_blur = true, no_focus = true, no_initial_focus = true })' >/dev/null 2>&1 || true

    # 2. Применение правил через hyprctl keyword для hyprland.conf (legacy)
    hyprctl keyword windowrulev2 "noblur,class:^(cs2esp)$" >/dev/null 2>&1 || true
    hyprctl keyword windowrulev2 "nofocus,class:^(cs2esp)$" >/dev/null 2>&1 || true
    hyprctl keyword windowrulev2 "noinitialfocus,class:^(cs2esp)$" >/dev/null 2>&1 || true
    echo -e "${GREEN}[✓] Правила Hyprland настроены (блюр отключён).${NC}\n"
fi

# Проверка прав доступа для прямого syscall process_vm_readv
if [ -f "/proc/sys/kernel/yama/ptrace_scope" ]; then
    SCOPE=$(cat /proc/sys/kernel/yama/ptrace_scope 2>/dev/null || echo "1")
    if [ "$SCOPE" != "0" ]; then
        if ! getcap build/cs2espd 2>/dev/null | grep -q "cap_sys_ptrace"; then
            sudo -n setcap cap_sys_ptrace=eip build/cs2espd 2>/dev/null || true
        fi
    fi
fi

echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}        External Service Launcher       ${NC}"
echo -e "${BLUE}========================================${NC}"
echo -e "Горячие клавиши в игре:"
echo -e "  - ${GREEN}[INSERT]${NC} : Открыть / закрыть интерактивное меню"
echo -e "  - ${GREEN}[F8]${NC}     : Включить / выключить ESP"
echo -e "\nУправление через терминал:"
echo -e "  - ${YELLOW}./build/espctl status${NC}  : статус модулей"
echo -e "  - ${YELLOW}./build/espctl quit${NC}    : остановка сервиса"
echo -e "${BLUE}========================================${NC}\n"

exec ./build/cs2espd
