#pragma once
#include <Socket.h>
#include <Folder.h>
#include "Message.h"

namespace MessageExchanger {
    Message awaitMessage(Socket* socket, int msg_size=SIZE_MESSAGE_TEXT, MessageType type=MessageType::text); // await message on the given socket
    void sendMessage(Socket* socket, Message &&m); // send message on the given socket
}

namespace FileExchanger {
    bool receiveFile(Socket *socket, Folder* f); // receive file from the given socket and write in the given folder
    void sendFile(Socket *socket, Folder* f, const filesystem::path& path, FileStatus status); // send from given folder to given socket
}
