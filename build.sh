#!/usr/bin/env bash
set -e

# Цвета для вывода
RED='\033[0;31m'
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
NC='\033[0m'

echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}   svinorez-external — Автосборка       ${NC}"
echo -e "${BLUE}========================================${NC}"

# Переходим в корень репозитория
cd "$(dirname "$0")"

# Проверка и установка зависимостей
check_deps() {
    local missing=0
    for cmd in cmake g++ pkg-config; do
        if ! command -v "$cmd" >/dev/null 2>&1; then
            missing=1
            break
        fi
    done

    # Проверка заголовков X11 через pkg-config
    if [ $missing -eq 0 ]; then
        for pkg in x11 xext xfixes xrender xrandr xft xtst; do
            if ! pkg-config --exists "$pkg" 2>/dev/null; then
                missing=1
                break
            fi
        done
    fi

    if [ $missing -eq 1 ]; then
        echo -e "${YELLOW}[!] Не найдены необходимые зависимости для сборки.${NC}"
        if command -v pacman >/dev/null 2>&1; then
            echo -e "${YELLOW}[*] Обнаружен Arch Linux. Устанавливаем пакеты через pacman...${NC}"
            sudo pacman -S --needed --noconfirm base-devel cmake pkgconf \
                libx11 libxext libxfixes libxrender libxrandr libxft fontconfig freetype2 libxtst
        elif command -v apt-get >/dev/null 2>&1; then
            echo -e "${YELLOW}[*] Обнаружен Debian/Ubuntu. Устанавливаем пакеты через apt...${NC}"
            sudo apt-get update && sudo apt-get install -y build-essential cmake pkg-config \
                libx11-dev libxext-dev libxfixes-dev libxrender-dev libxrandr-dev \
                libxft-dev libfontconfig1-dev libfreetype6-dev libxtst-dev
        elif command -v dnf >/dev/null 2>&1; then
            echo -e "${YELLOW}[*] Обнаружен Fedora/RHEL. Устанавливаем пакеты через dnf...${NC}"
            sudo dnf install -y gcc-c++ cmake pkgconf-pkg-config \
                libX11-devel libXext-devel libXfixes-devel libXrender-devel libXrandr-devel \
                libXft-devel fontconfig-devel freetype-devel libXtst-devel
        else
            echo -e "${RED}[X] Не удалось автоматически установить зависимости.${NC}"
            echo -e "Установите вручную: cmake, g++, libx11, libxext, libxrandr, libxft, libxtst"
            exit 1
        fi
    else
        echo -e "${GREEN}[✓] Все необходимые зависимости установлены.${NC}"
    fi
}

check_deps

echo -e "\n${BLUE}[*] Конфигурирование CMake...${NC}"
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release

echo -e "\n${BLUE}[*] Компиляция проекта ($(nproc) потоков)...${NC}"
cmake --build build -j"$(nproc)"

echo -e "\n${GREEN}========================================${NC}"
echo -e "${GREEN}   Сборка успешно завершена!            ${NC}"
echo -e "${GREEN}========================================${NC}"
echo -e "Исполняемые файлы в папке build/:"
echo -e "  - ${GREEN}build/cs2espd${NC} : основной демон чита (ESP, Aimbot, Trigger, RCS, Menu)"
echo -e "  - ${GREEN}build/espctl${NC}  : консольная утилита управления (espctl status / toggle / aim / rcs)"
echo -e "\nЗапуск:"
echo -e "  ${YELLOW}./start.sh${NC}   или   ${YELLOW}./build/cs2espd${NC}"
echo -e "Открыть меню в игре: клавиша ${YELLOW}[INSERT]${NC}"
