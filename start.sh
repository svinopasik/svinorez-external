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

# Автодетект Hyprland: отключение размытия (blur) для прозрачного оверлея
if [ -n "$HYPRLAND_INSTANCE_SIGNATURE" ] || command -v hyprctl >/dev/null 2>&1; then
    echo -e "${YELLOW}[*] Обнаружен Hyprland. Применяю правила отключения блюра для оверлея...${NC}"
    
    # 1. Применение правил на лету через hyprctl (Lua API и стандартный keyword)
    hyprctl eval 'hl.window_rule({ match = { class = "cs2esp" }, no_blur = true, no_focus = true, no_initial_focus = true })' >/dev/null 2>&1 || true
    hyprctl keyword windowrulev2 "noblur,class:^(cs2esp)$" >/dev/null 2>&1 || true
    hyprctl keyword windowrulev2 "nofocus,class:^(cs2esp)$" >/dev/null 2>&1 || true
    hyprctl keyword windowrulev2 "noinitialfocus,class:^(cs2esp)$" >/dev/null 2>&1 || true

    # 2. Добавление постоянного правила в пользовательский конфиг, если его там нет
    HYPR_LUA="$HOME/.config/hypr/hyprland.lua"
    HYPR_CONF="$HOME/.config/hypr/hyprland.conf"

    if [ -f "$HYPR_LUA" ]; then
        if ! grep -q "cs2esp" "$HYPR_LUA"; then
            echo -e "${GREEN}[+] Добавляю no_blur в $HYPR_LUA...${NC}"
            cat << 'EOF' >> "$HYPR_LUA"

-- svinorez-external: отключение блюра для оверлея
hl.window_rule({ match = { class = "cs2esp" }, no_blur = true, no_focus = true, no_initial_focus = true })
EOF
            hyprctl reload >/dev/null 2>&1 || true
        fi
    elif [ -f "$HYPR_CONF" ]; then
        if ! grep -q "cs2esp" "$HYPR_CONF"; then
            echo -e "${GREEN}[+] Добавляю noblur в $HYPR_CONF...${NC}"
            cat << 'EOF' >> "$HYPR_CONF"

# svinorez-external: отключение блюра для оверлея
windowrulev2 = noblur, class:^(cs2esp)$
windowrulev2 = nofocus, class:^(cs2esp)$
windowrulev2 = noinitialfocus, class:^(cs2esp)$
EOF
            hyprctl reload >/dev/null 2>&1 || true
        fi
    fi
    echo -e "${GREEN}[✓] Правила Hyprland настроены (блюр отключён).${NC}\n"
fi

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
