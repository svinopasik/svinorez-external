# svinorez-external

> [!NOTE]
> Основное окружение разработки и тестирования — **Arch Linux + Hyprland (XWayland)**. Для корректной работы прозрачного оверлея требуется X11/XWayland с композитором, поддерживающим 32-битный ARGB TrueColor visual.

Внешний (external) ESP, Aimbot и Triggerbot для Counter-Strike 2 на Linux.
Демон `cs2espd` читает память процесса игры через прямой системный вызов `process_vm_readv` и рисует боксы, скелеты и информацию об игроках в прозрачном X11-оверлее поверх XWayland. Управление осуществляется через интерактивное GUI-меню (клавиша `Insert`), CLI-утилиту `espctl` и горячую клавишу `F8`.

Код в процесс игры не внедряется (no injection), ядро не модифицируется, ptrace для записи не используется.

## Архитектура

```
svinorez-external
  ├─ cs2espd
  │    ├─ game       — поиск процесса, чтение памяти (process_vm_readv), обход entity list (чанки), view matrix
  │    ├─ esp        — world-to-screen, скелеты (24 кости / 18 связок), боксы, дистанция, здоровье
  │    ├─ aimbot     — выбор кости (голова/грудь), FOV, smooth-доводка, visible check, humanize-джиттер
  │    ├─ triggerbot — чтение crosshair ID (m_iIDEntIndex), team check, эмуляция клика через XTest
  │    ├─ overlay    — X11 ARGB окно (override_redirect), XShape input clipping, шрифт Xft, Hyprland socket IPC
  │    ├─ menu       — интерактивное 4-вкладочное GUI-меню (Insert): ESP, Aimbot, Triggerbot, Binds
  │    └─ ipc        — абстрактный Unix-сокет @.cs_pipe_stream, XGrabKey(F8), SIGUSR1
  ├─ espctl          — CLI-управление: on / off / toggle / aim / trigger / status / quit
  └─ common          — memory (process_vm_readv), keymap (PC105 сканкоды + Keysym), config (JSON), логгер
```

## Зависимости

- CMake >= 3.16
- Компилятор C++20 (GCC >= 11 или Clang >= 15)
- `libX11` — подключение к X-серверу / XWayland
- `libXext` — расширение XShape (прозрачность кликов и захват мыши в меню)
- `libXfixes` — интеграция с оконной системой X11
- `libXrender` — альфа-композитинг и визуализаторы
- `libXrandr` — определение геометрии мониторов и авто-привязка к монитору с игрой
- `libXft` — рендеринг шрифтов через FreeType2 и Fontconfig
- `libXtst` — расширение XTest для эмуляции ввода (доводка аимбота, выстрел триггербота)
- `pthread`

### Debian / Ubuntu

```bash
sudo apt-get install -y build-essential cmake pkg-config \
    libx11-dev libxext-dev libxfixes-dev libxrender-dev libxrandr-dev \
    libxft-dev libfontconfig1-dev libfreetype6-dev libxtst-dev
```

### Arch Linux

```bash
sudo pacman -S --needed base-devel cmake pkgconf \
    libx11 libxext libxfixes libxrender libxrandr libxft fontconfig freetype2 libxtst
```

## Быстрый старт

```bash
# 1. Автоматическая проверка зависимостей и сборка
./build.sh

# 2. Запуск демона
./start.sh
```

В игре:
- **`[INSERT]`** — открыть / закрыть интерактивное меню (вкладки: ESP, AIMBOT, TRIGGERBOT, BINDS & MISC).
- **`[F8]`** — быстрое включение / выключение ESP.
- `Стрелки вверх/вниз` — навигация по строкам меню.
- `Стрелки влево/вправо` — переключение вкладок или изменение значений слайдеров.
- `Enter` / `Пробел` — переключение тумблеров или сохранение настроек на диск.
- `Мышь` — интерактивные клики по вкладкам, кнопкам и чекбоксам при открытом меню.

## Ручная сборка

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

Бинарники: `build/cs2espd`, `build/espctl`.

## Запуск и управление

```bash
# Запуск демона оверлея в foreground
./build/cs2espd

# Запрос текущего статуса
./build/espctl status
# Вывод: OK ESP:ON AIM:OFF TRIGGER:OFF MENU:CLOSED

# Управление модулем ESP
./build/espctl on
./build/espctl off
./build/espctl toggle

# Управление модулем Aimbot (по умолчанию toggle)
./build/espctl aim
./build/espctl aim on
./build/espctl aim off

# Управление модулем Triggerbot (по умолчанию toggle)
./build/espctl trigger
./build/espctl trigger on
./build/espctl trigger off

# Завершение работы демона
./build/espctl quit

# Переключение ESP через POSIX-сигнал
kill -USR1 $(pidof cs2espd)
```

`cs2espd` работает в foreground, логирование ведётся в `stdout`/`stderr`. Останов выполняется через `./build/espctl quit`, `SIGINT`/`SIGTERM` или комбинацию `Ctrl+C`.

Порядок запуска относительно игры не имеет значения: если CS2 ещё не запущена, демон периодически логирует `waiting for cs2` и ожидает появления процесса. При запуске игры демон автоматически аттачится и находит базу модуля `libclient.so`.

## Интерактивное меню (клавиша INSERT / INS)

В оверлей встроено GUI-меню с поддержкой мыши и клавиатуры, разделенное на 4 вкладки:

- **Вкладка [1] ESP**:
  - `ESP Master Switch`: глобальный тумблер ESP (ON/OFF).
  - `Player Skeletons`: отрисовка костей скелета (24 сустава / 18 связок).
  - `Bounding Boxes`: прямоугольные 2D-боксы игроков.
  - `Player Names`: отображение никнеймов игроков.
  - `Distance Display`: расстояние до цели в игровых юнитах.
  - `Health Bars`: вертикальная полоса здоровья игрока.
  - `Team Check (Enemies Only)`: фильтр своих игроков (отображать только врагов).
  - `Max Render Distance`: лимит дистанции отрисовки (Unlimited / 1000..8000 юнитов).

- **Вкладка [2] AIMBOT**:
  - `Aimbot Master Switch`: включение/выключение аимбота.
  - `Target Bone`: выбор целевой кости (`head` кость 7, `chest` кость 5).
  - `Activation Hotkey`: клавиша удержания (`always`, `alt`, `shift`, `ctrl`, `caps`, `c`, `v`, `x`, `z`, `mouse1`, `mouse2`).
  - `FOV Radius`: радиус захвата цели в пикселях от прицела (20..250 px).
  - `Smooth Factor`: плавность доводки курсора (2..30).
  - `Visible Check (No Walls)`: проверка видимости цели через `m_entitySpottedState`.
  - `Team Check (No Teammates)`: игнорирование союзников.

- **Вкладка [3] TRIGGERBOT**:
  - `Triggerbot Master`: включение/выключение триггербота.
  - `Team Check (No Teammates)`: стрельба исключительно по врагам.
  - `Activation Hotkey`: клавиша удержания (`always`, `alt`, `shift`, `ctrl`, `caps`, `x`, `c`, `v`, `z`, `mouse1`, `mouse2`).
  - `Reaction Pre-delay`: задержка перед первым выстрелом (0..300 мс).
  - `Mouse Click Duration`: длительность зажатия ЛКМ (20..80 мс).
  - `Cooldown Between Shots`: интервал между сериями выстрелов (100..450 мс).

- **Вкладка [4] BINDS & MISC**:
  - `Menu Toggle Key`: отображение клавиши открытия меню (`[ INSERT ]`).
  - `ESP Toggle Hotkey`: горячая клавиша переключения ESP (`F7`..`F12`).
  - `Aimbot Hotkey`: горячая клавиша Aimbot.
  - `Triggerbot Hotkey`: горячая клавиша Triggerbot.
  - `Overlay Target FPS`: ограничение частоты кадров оверлея (60..360 FPS).
  - `Save Settings To Disk`: сохранение всех изменённых параметров в `config/config.json`.

### Навигация в меню:
- **`Insert` / `Escape`** — открыть / закрыть меню.
- **`1`, `2`, `3`, `4` или `Tab`** — быстрое переключение между вкладками.
- **`Стрелки Вверх / Вниз`** — навигация по строкам параметров.
- **`Стрелки Влево / Вправо`** — циклическое переключение значений и слайдеров.
- **`Enter` или `Пробел`** — переключение чекбоксов / сохранение настроек.
- **Мышь** — интерактивные клики по вкладкам, параметрам и кнопкам (пока меню открыто, клики внутри меню обрабатываются интерфейсом, клики вне меню проходят в игру).

## Ограничения XWayland и Hyprland

- Оверлей рассчитан на **XWayland** (или стандартную X11-сессию): окно создаётся через Xlib с 32-битным ARGB visual. Без работающего композитора прозрачность недоступна (в XWayland композитинг есть по умолчанию, под чистым X11 требуется `picom` или аналог).
- **Нативный Wayland layer-shell не используется**: если запустить демон в сессии без XWayland, запуск прервётся с ошибкой `cannot open display`.
- При запуске в Hyprland демон автоматически применяет правила к окну оверлея через сокет IPC (`hl.window_rule` / `windowrulev2`), отключая размытие фона (`no_blur = true`) и увод фокуса (`no_focus = true`, `no_initial_focus = true`).
- Для точного совпадения экранных боксов с моделями игроков CS2 необходимо запускать в режиме **полноэкранного окна без рамок (borderless windowed)** или в обычном оконном режиме.
- Системный вызов `process_vm_readv`: в системах с активным модулем безопасности Yama LSM ядро может блокировать чтение памяти неродственных процессов. Для разрешения выполните:
  ```bash
  echo 0 | sudo tee /proc/sys/kernel/yama/ptrace_scope
  ```
  Либо назначьте бинарнику права:
  ```bash
  sudo setcap cap_sys_ptrace=eip build/cs2espd
  ```

## Конфигурация

### `config/config.json`

| Поле | Значение по умолчанию | Описание |
|------|------------------------|----------|
| `process_name` | `"cs2"` | Имя процесса игры (поиск в `/proc/<pid>/comm` и `/proc/<pid>/cmdline`) |
| `game_module` | `"libclient.so"` | Имя клиентской библиотеки игры в `/proc/<pid>/maps` |
| `socket_path` | `"@.cs_pipe_stream"` | Путь к абстрактному управляющему Unix-сокету |
| `hotkey` | `"F8"` | Клавиша быстрого переключения ESP (KeySym через XGrabKey) |
| `fps_limit` | `144` | Частота цикла обновления оверлея и чтения памяти |
| `team_check` | `true` | Фильтр союзников (`false` — рисовать союзников цветом `colors.teammate`) |
| `max_distance` | `0` | Максимальная дистанция отрисовки в юнитах (0 — без ограничений) |
| `stealth.spoof_process_name` | `"pipewire-sys"` | Подмена имени процесса в `/proc/<pid>/comm` и `/proc/<pid>/cmdline` |
| `stealth.randomize_window_class` | `false` | Случайная генерация WM_CLASS окна оверлея |
| `colors.local` | `"#00FF00FF"` | Цвет локального игрока (`#RRGGBBAA`) |
| `colors.enemy` | `"#FF4040FF"` | Цвет противников (`#RRGGBBAA`) |
| `colors.teammate` | `"#40A0FFFF"` | Цвет союзников (`#RRGGBBAA`) |
| `esp.box_height_units` | `70` | Высота бокса игрока в игровых юнитах |
| `esp.box_width_ratio` | `0.45` | Отношение ширины бокса к его высоте |
| `esp.health_bar_width` | `6` | Ширина вертикального бара здоровья в пикселях |
| `esp.show_names` | `true` | Отображение никнеймов игроков |
| `esp.show_distance` | `true` | Отображение дистанции до игрока |
| `esp.show_status_text` | `true` | Отображение служебных статусов в углу экрана |
| `esp.show_skeleton` | `true` | Отрисовка скелета (18 связок костей) |
| `esp.font` | `"DejaVu Sans:size=10"` | Шрифт Xft для текста оверлея |
| `esp.monitor` | `-1` | Индекс монитора: `-1` — автодетект по окну игры, `0`/`1` — явный индекс |
| `aimbot.enabled` | `false` | Включение модуля Aimbot |
| `aimbot.hotkey` | `"c"` | Клавиша удержания для доводки прицела |
| `aimbot.bone` | `"chest"` | Целевая кость: `"head"` (кость 7) или `"chest"` (кость 5) |
| `aimbot.fov` | `35` | Радиус захвата целей в пикселях от прицела |
| `aimbot.smooth` | `20` | Коэффициент сглаживания доводки |
| `aimbot.team_check` | `true` | Целиться только во вражеских игроков |
| `aimbot.visible_check` | `true` | Проверка видимости (игнорировать цели за препятствиями) |
| `aimbot.humanize` | `true` | Микро-джиттер для имитации естественного движения мыши |
| `triggerbot.enabled` | `false` | Включение модуля Triggerbot |
| `triggerbot.hotkey` | `"alt"` | Клавиша активации автоматического выстрела |
| `triggerbot.pre_delay_ms` | `190` | Задержка перед кликом (мс) |
| `triggerbot.post_delay_ms` | `55` | Длительность зажатия ЛКМ (мс) |
| `triggerbot.cooldown_ms` | `350` | Задержка между повторными выстрелами (мс) |
| `triggerbot.humanize` | `true` | Рандомизация задержек реакции по нормальному распределению |

### Мульти-монитор

В многомониторных конфигурациях XWayland создает единый root-экран (например, 3840x1080 для двух FullHD мониторов). Демон определяет монитор с окном игры по `_NET_WM_PID` (с фолбэками на активное окно, позицию курсора и монитор 0) и ограничивает оверлей размерами нужного дисплея, пересчитывая проекцию World-to-Screen.

При перемещении окна CS2 на соседний монитор демон пересоздает окно оверлея на целевом дисплее. Также монитор можно зафиксировать параметром `esp.monitor`.

### `config/offsets.json`

Файл содержит RVA-смещения клиентского модуля `libclient.so` и смещения внутри структур движка Source 2:

```json
{
  "rvas": {
    "dwLocalPlayerController": 76586936,
    "dwEntityList": 74206208,
    "dwViewMatrix": 76846848
  },
  "entity_list": {
    "pointer_size": 8,
    "chunk_stride": 0,
    "entity_stride": 112,
    "chunk_shift": 9,
    "index_mask": 32767
  },
  "pawn": {
    "m_pGameSceneNode": 1184,
    "m_iHealth": 1212,
    "m_lifeState": 1220,
    "m_iTeamNum": 1367,
    "m_hController": 5188,
    "m_iIDEntIndex": 17796,
    "m_entitySpottedState": 11544
  },
  "game_scene_node": {
    "m_vecAbsOrigin": 200,
    "m_modelState": 320,
    "modelState_origin": 0,
    "m_pBoneArray": 448
  },
  "controller": {
    "m_hPawn": 2108,
    "m_iszPlayerName": 2172
  }
}
```

Все поля проверяются при загрузке: если смещение отсутствует или равно нулю, демон выводит предупреждение с указанием имени отсутствующего поля.

### Как обновлять offsets

После каждого патча игры RVA-смещения меняются. Их можно извлечь из свежего дампа **linux-ветки** `a2x/cs2-dumper`:

1. Скачайте актуальные дампы для нативного Linux-клиента:
   ```bash
   curl -sL https://raw.githubusercontent.com/a2x/cs2-dumper/linux/output/offsets.json -o /tmp/offsets.json
   curl -sL https://raw.githubusercontent.com/a2x/cs2-dumper/linux/output/libclient_so.json -o /tmp/libclient_so.json
   ```
2. Обновите `config/offsets.json`:
   - `rvas.*` — берутся из секции `libclient.so` файла `offsets.json`.
   - Поля `pawn.*`, `game_scene_node.*`, `controller.*` — из `libclient_so.json`:
     - `C_BaseEntity` (`m_pGameSceneNode`, `m_iHealth`, `m_lifeState`, `m_iTeamNum`)
     - `C_CSPlayerPawn` (`m_iIDEntIndex`, `m_entitySpottedState`)
     - `CBasePlayerController` (`m_hPawn`, `m_iszPlayerName`)
     - `CGameSceneNode` (`m_vecAbsOrigin`, `m_pBoneArray`).
3. Перезапустите демон `cs2espd`.

## Структура проекта

```
svinorez-external/
├── CMakeLists.txt
├── README.md
├── build.sh
├── start.sh
├── config/
│   ├── config.json
│   └── offsets.json
├── include/
│   └── common/
│       ├── config.hpp
│       ├── ipc.hpp
│       ├── json.hpp
│       ├── keymap.hpp
│       ├── log.hpp
│       └── memory.hpp
└── src/
    ├── common/
    │   ├── config.cpp
    │   ├── ipc.cpp
    │   ├── json.cpp
    │   ├── keymap.cpp
    │   ├── log.cpp
    │   └── memory.cpp
    ├── cs2espd/
    │   ├── main.cpp
    │   ├── game.hpp / game.cpp
    │   ├── esp.hpp / esp.cpp
    │   ├── aimbot.hpp / aimbot.cpp
    │   ├── triggerbot.hpp / triggerbot.cpp
    │   ├── menu.hpp / menu.cpp
    │   └── overlay.hpp / overlay.cpp
    └── espctl/
        └── main.cpp
```

## Траблшутинг

| Симптом | Причина и решение |
|---------|-------------------|
| `X11: cannot open display` | В текущей сессии отсутствует XWayland / X-сервер. Проверьте переменную окружения `DISPLAY`. |
| `32-битный ARGB TrueColor visual не найден` | Отсутствует активный композитор (чистый X11 без `picom`). В XWayland композитинг работает всегда. |
| `process_vm_readv denied (EPERM)` | Модуль ядра Yama LSM запрещает ptrace: выполните `echo 0 \| sudo tee /proc/sys/kernel/yama/ptrace_scope` или `sudo setcap cap_sys_ptrace=eip build/cs2espd`. |
| `abstract socket ... is busy` | Уже запущен другой экземпляр демона `cs2espd`. Остановите его через `killall cs2espd` или `pkill -9 cs2espd`. |
| `JSON parse error` / `field must be...` | Ошибка валидации структуры `config/config.json` или `config/offsets.json`. Проверьте синтаксис JSON и типы полей. |
| `offsets.json: rvas.* == 0` | Оффсеты не заполнены или сброшены в 0. Обновите их по инструкции выше. |
| `waiting for cs2` | Процесс игры ещё не запущен. Демон продолжает ожидать появления процесса в системе. |
