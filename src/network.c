#include "network.h"
#include <unistd.h>
#include <string.h>

int send_msg(int sock, uint8_t type, uint8_t sender, uint8_t target,
             const void *payload, size_t len)
{
    msg_header_t h = { type, sender, target };

    if (write(sock, &h, sizeof(h)) != sizeof(h))
        return -1;

    if (len > 0 && write(sock, payload, len) != (ssize_t)len)
        return -1;

    return 0;
}

int recv_msg(int sock, msg_header_t *h, uint8_t *buf, size_t bufsize)
{
    ssize_t r = read(sock, h, sizeof(*h));
    if (r == 0) return 0;
    if (r < 0) return -1;
    if (r != sizeof(*h)) return -1;

    // Šeit vēlāk lasīsi payload atkarībā no msg_type
    return 1;
}
