# cs2-esp-linux

> [!WARNING]
> **ПРОЕКТ ЗАБРОШЕН / UNMAINTAINED**
> Обновлять я это не буду, мне поебать. Оффсеты под новые патчи CS2 ищите и обновляйте сами в `config/offsets.json`. Код выложен как есть (AS-IS). PR и Issue чекать никто не собирается.

Внешний (external) ESP, Aimbot, Triggerbot и RCS для CS2 на Linux: демон `cs2espd` читает память запущенной
игры через `/proc/<pid>/mem` и рисует боксы игроков в прозрачном X11-оверлее
поверх XWayland. Управление — через CLI-утилиту `espctl` и горячую клавишу F8.

Код в процесс игры не инжектится, ядро не трогается, ptrace для записи не используется.

## Архитектура

```
cs2espd
  ├─ game     — attach к процессу, обход entity list (чанки), view matrix
  ├─ esp      — world-to-screen, вычисление экранных боксов
  ├─ trigger  — чтение m_iIDEntIndex под прицелом и выстрел через XTest
  ├─ overlay  — X11 ARGB окно: override_redirect, XShape input, текст через Xft
  └─ ipc      — Unix socket /tmp/cs2esp.sock + XGrabKey(F8) + SIGUSR1

espctl — CLI: on / off / toggle / trigger [on|off|toggle] / status / quit
common — memory (pread64 + process_vm_readv fallback), config (json + валидация),
         json-парсер без внешних зависимостей, IPC-протокол, throttled logger
```

## Зависимости

- CMake >= 3.16
- Компилятор C++20 (g++ >= 11 или clang >= 15)
- libX11, libXext (XShape), libXfixes, libXrender, libXft (тянет freetype/fontconfig), libXtst (XTest для bhop)
- pthread

### Debian / Ubuntu

```bash
sudo apt-get install -y build-essential cmake pkg-config \
    libx11-dev libxext-dev libxfixes-dev libxrender-dev libxrandr-dev \
    libxft-dev libfontconfig1-dev libfreetype6-dev libxtst-dev
```

### Arch

```bash
sudo pacman -S --needed base-devel cmake pkgconf \
    libx11 libxext libxfixes libxrender libxrandr libxft fontconfig freetype2 libxtst
```

## Быстрый старт (в 1 команду)

```bash
# 1. Автоматическая проверка зависимостей и сборка
./build.sh

# 2. Запуск чита
./start.sh
```

В игре:
- **`[INSERT]`** — Открыть / закрыть интерактивное меню (вкладки: ESP, AIMBOT, TRIGGERBOT, RCS, BINDS).
- **`[F8]`** — Быстрое включение / выключение ESP.
- Стрелки вверх/вниз — навигация по пунктам меню.
- Стрелки влево/вправо или Enter — изменение значений.

## Ручная сборка (если не хочется скриптом)

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

Бинарники: `build/cs2espd`, `build/espctl`.

## Запуск

```bash
./build/cs2espd                    # демон: оверлей + F8 + меню INS + IPC-сокет
./build/espctl status              # OK ESP:ON AIM:ON TRIGGER:ON | ERR <текст>
./build/espctl on|off|toggle       # управление ESP
./build/espctl aim [on|off|toggle] # управление Aimbot
./build/espctl trigger [on|off|toggle] # управление Triggerbot
./build/espctl quit                # выход
kill -USR1 $(pidof cs2espd)        # toggle ESP по сигналу
```

## Интерактивное меню (клавиша INSERT / INS)

В оверлей встроено GUI-меню с поддержкой клавиатуры и мыши:
- **Вкладка [1] ESP**: общий тумблер ESP, скелеты игроков (bone joints), боксы, имена игроков, дистанция, полоски здоровья, team check (только враги), максимальная дистанция отрисовки.
- **Вкладка [2] AIMBOT**: тумблер Aimbot, выбор кости (HEAD кость 7 / CHEST кость 5), клавиша удержания (`always`, `alt`, `shift`, `ctrl`, `caps`, `c`, `v`, `x`, `mouse1`, `mouse2`), радиус FOV (в пикселях), фактор сглаживания (smooth), team check.
- **Вкладка [3] TRIGGERBOT**: тумблер Triggerbot, team check, выбор клавиши удержания (`always`, `alt`, `shift`, `ctrl`, `caps`, `x`), задержка перед выстрелом (pre-delay), длительность клика (post-delay), кулдаун между выстрелами.
- **Вкладка [4] BINDS & MISC**: отображение биндов (Menu: INSERT, ESP: F8, Aimbot, Trigger), ограничение FPS оверлея, кнопка сохранения настроек в `config.json`.

### Навигация в меню:
- **`Insert` / `Escape`** — открыть / закрыть меню;
- **`1`, `2`, `3`, `4` или `Tab`** (или стрелки `Влево / Вправо` на строке `Tab Category`) — переключение вкладок;
- **`Стрелки Вверх / Вниз`** — перемещение по пунктам;
- **`Стрелки Влево / Вправо`** — циклическое изменение значений и слайдеров;
- **`Enter` или `Пробел`** — переключение чекбоксов / сохранение настроек;
- **Мышь** — интерактивные клики по вкладкам, пунктам и кнопкам (пока меню открыто, клики внутри меню перехватываются, клики вне меню проходят в игру).

`cs2espd` работает в foreground, лог — в stdout/stderr. Останов: `espctl quit`,
SIGINT/SIGTERM или Ctrl+C. Порядок запуска относительно игры не важен: пока CS2
не запущена, демон печатает `waiting for cs2` (один раз в 5 секунд) и продолжает
работать; как только процесс появится — аттачится сам.

Сокет занят другим демоном — запуск завершится с понятной ошибкой, мёртвый
stale-файл от прошлого запуска корректно поднимается заново.

## Ограничения XWayland

- Оверлей рассчитан на **XWayland** (или обычный X-сервер): окно создаётся через
  Xlib с 32-битным ARGB visual. Без композитора прозрачность работать не будет —
  на XWayland композитор есть всегда, под голым X11 нужен picom или аналог.
- **Нативный Wayland-оверлей не поддерживается** (layer-shell не реализован):
  если сессия без XWayland, демон остановится с ошибкой «cannot open display».
- Для совпадения боксов с кадром игры запускай CS2 в **borderless / windowed**
  режиме: проекция считается на весь X-экран оверлея.
- Горячая клавиша: игра (или другой клиент) может переграбить F8 через XGrabKey —
  тогда демон залогирует предупреждение и продолжит работать, ESP останется
  управляемым через `espctl`.

## Конфиги

### `config/config.json`

| Поле         | Значение            | Что значит |
|--------------|---------------------|------------|
| `process_name` | `cs2`             | имя процесса игры (по `comm` / `cmdline`) |
| `game_module`  | `libclient.so`    | клиентский модуль, база ищется в `/proc/<pid>/maps` |
| `socket_path`  | `/tmp/cs2esp.sock`| файл управляющего сокета |
| `hotkey`       | `F8`              | клавиша toggle ESP (KeySym, XGrabKey) |
| `fps_limit`    | `144`             | частота цикла чтения и оверлея |
| `team_check`   | `true`            | `false` — рисовать и союзников (цветом `colors.teammate`) |
| `max_distance` | `0`               | лимит дистанции в игровых юнитах; 0 — без лимита |
| `colors.*`     | `#RRGGBBAA`       | цвет enemy / teammate / local |
| `esp.*`        | —                 | размеры боксов, hp-полоски, включение имён/дистанции/статуса, шрифт Xft |
| `esp.show_skeleton` | `true`        | рисовать скелеты игроков (18 костных связок) |
| `esp.monitor`  | `-1`              | `-1` — авто: оверлей ложится на монитор, где окно игры (детект по `_NET_WM_PID` → active window → курсор); `0`/`1` — индекс XRR-монитора |
| `aimbot.enabled` | `false`         | включение модуля aimbot |
| `aimbot.hotkey`  | `"always"`      | клавиша удержания (`always`, `alt`, `shift`, `ctrl`, `mouse1`, `mouse2`, `x`, `c`, `v`) |
| `aimbot.bone`    | `"head"`        | целевая кость (`"head"` кость 7, `"chest"` кость 5) |
| `aimbot.fov`     | `120.0`         | радиус захвата FOV в пикселях |
| `aimbot.smooth`  | `4.0`           | сглаживание доводки (1.0 = instant snap, 4.0 = smooth assist) |
| `aimbot.team_check` | `true`       | целиться только во врагов |
| `triggerbot.enabled` | `false`      | включение модуля triggerbot |
| `triggerbot.hotkey`  | `"always"`       | клавиша удержания (`always`, `ALT`, `SHIFT`, `CTRL`, `X`) |
| `triggerbot.pre_delay_ms` | `10`        | задержка перед выстрелом (мс) |
| `triggerbot.post_delay_ms` | `20`       | длительность клика (мс) |
| `triggerbot.cooldown_ms` | `120`        | задержка между выстрелами (мс) |

### Мульти-монитор

На двух мониторах 1920x1080 XWayland отдаёт root 3840x1080, но оверлей заранее
сужен до одной головы: демон находит окно игры (`_NET_WM_PID`) и рисует
только на её мониторе, перебазируя W2S под геометрию этой головы. Переезд игры
на другой монитор отлавливается каждые ~20 кадров — окно пересоздаётся
на новом head. Если `_NET_WM_PID` у окна игры нет, работают fallback'и
(active window → позиция курсора → индекс 0), либо задай `esp.monitor` явно.

### `config/offsets.json`

Уже заполнены из свежего дампа **linux-ветки** `a2x/cs2-dumper` (нативный
Linux-клиент, RVAs для `libclient.so`; схема — из `output/libclient_so.json`
той же ветки, поэтому поля C++-классов совпадают с твоим билдом):

```json
rvas.dwEntityList      = 74199040   # libclient.so
rvas.dwViewMatrix      = 76839680
rvas.dwLocalPlayerPawn = 76809432
pawn.m_pGameSceneNode  = 1184
pawn.m_iHealth         = 1212
pawn.m_lifeState       = 1220
pawn.m_iTeamNum        = 1367
pawn.m_hController     = 5188
game_scene_node.m_vecOrigin = 128
controller.m_iszPlayerName  = 2172
```

Поле `controller.m_iszPlayerName` опционально: если Valve поменяет его
семантику, демон просто перестаёт рисовать имена (боксы работают и без него).
`entity_list.*` — стандартная схема чанков (8/16/120/9/0x7FFF), трогать не надо.

Валидация: не-числа в offsets, кривые цвета (`#RRGGBBAA`), не-числовой
`fps_limit` и прочее — ошибка с именем файла и поля. Примеры:

```
config.json: field 'process_name' must be a number
config.json: JSON parse error: line 1, column 1: invalid literal
offsets.json: field 'dwEntityList' must be a non-negative number or null
```

## Как обновлять offsets

После патча игры оффсеты меняются; RVAs — только с Linux-дампа.

1. Возьми свежий дамп **linux-ветки** `a2x/cs2-dumper`:
   ```bash
   curl -sL https://raw.githubusercontent.com/a2x/cs2-dumper/linux/output/offsets.json
   curl -sL -o /tmp/libclient_so.json https://raw.githubusercontent.com/a2x/cs2-dumper/linux/output/libclient_so.json
   ```
2. Обнови в `config/offsets.json`:
   - `rvas.*` — прямо из `offsets.json` (секция `libclient.so`).
   - `pawn.*`, `game_scene_node.*`, `controller.*` — из `libclient_so.json`:
     `C_BaseEntity` (`m_pGameSceneNode`, `m_iHealth`, `m_lifeState`, `m_iTeamNum`),
     `CBasePlayerPawn::m_hController`, `CGameSceneNode::m_vecOrigin`,
     `CBasePlayerController::m_iszPlayerName`.
   - `entity_list.*` не трогай, пока Valve не поменяет разметку чанков.
3. Перезапусти `cs2espd`. Пустые поля видны сразу: каждый нулевой offset логируется
   как ERROR с именем файла и пути поля.

```
[INFO ] config loaded: config/config.json + config/offsets.json
[INFO ] attached to cs2 (pid 12345)
[INFO ] module base: 0x7f12ab000000
```

## Структура проекта

```
cs2-esp-linux/
├── CMakeLists.txt
├── README.md
├── config/
│   ├── config.json
│   └── offsets.json
├── include/common/
│   ├── config.hpp
│   ├── ipc.hpp
│   ├── json.hpp
│   ├── log.hpp
│   └── memory.hpp
└── src/
    ├── common/
    │   ├── config.cpp
    │   ├── ipc.cpp
    │   ├── json.cpp
    │   ├── log.cpp
    │   └── memory.cpp
    ├── cs2espd/
    │   ├── main.cpp
    │   ├── game.hpp / game.cpp
    │   ├── esp.hpp / esp.cpp
    │   ├── triggerbot.hpp / triggerbot.cpp
    │   ├── menu.hpp / menu.cpp
    │   └── overlay.hpp / overlay.cpp
    └── espctl/
        └── main.cpp
```

## Траблшутинг

| Симптом | Причина |
|---------|---------|
| `X11: cannot open display` | в сессии нет XWayland / X-сервера |
| `32-битный ARGB TrueColor visual не найден` | нет композитора (голый X11 без picom) |
| `socket /tmp/cs2esp.sock is busy` | уже работает другой cs2espd |
| `top-level must be an object` и т.п. | битый JSON в конфиге |
| `offsets.json: rvas.* == 0 ...` | offsets не заполнены — см. раздел выше |
| `waiting for cs2` | игра не запущена; это штатное сообщение |
