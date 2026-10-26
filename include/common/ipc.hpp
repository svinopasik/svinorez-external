#pragma once

#include <string>

namespace cs2esp {

// Дефолтный путь управляющего сокета, весь протокол — построчный текст:
//   клиент: "on\n" | "off\n" | "toggle\n" | "status\n" | "quit\n"
//   сервер: "OK ON\n" | "OK OFF\n" | "OK\n" | "ERR <текст>\n"
inline constexpr char kDefaultSocketPath[] = "@.cs_pipe_stream";

// Создаёт слушающий сокет. Если файл существует и на нём отвечает кто-то живой —
// ошибка «socket busy»; мёртвый файл корректно поднимается заново.
// Возвращает слушающий fd или -1 (текст ошибки — в error).
int  ipc_server_start(const std::string& path, std::string& error);

// Клиентское подключение. fd или -1 (текст ошибки — в error).
int  ipc_client_connect(const std::string& path, std::string& error);

// Послать строку с добавлением '\n' в конце. false при ошибке ( errno в error).
// Сокет без_SIGPIPE: send() с MSG_NOSIGNAL.
bool ipc_send_line(int fd, const std::string& line, std::string& error);

// Прочитать одну строку (без '\n' в out). false при ошибке/закрытии.
bool ipc_recv_line(int fd, std::string& out, std::string& error);

void ipc_close(int fd);

} // namespace cs2esp
