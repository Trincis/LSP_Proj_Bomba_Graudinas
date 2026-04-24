#include "network.h"
#include <unistd.h>
#include <string.h>

int send_msg(int sock, uint8_t type, uint8_t sender, uint8_t target,
             const void *payload, size_t len)
{
    msg_header_t h;
    h.msg_type = type;
    h.sender_id = sender;
    h.target_id = target;
    h.payload_len = len;

    if (write(sock, &h, sizeof(h)) != sizeof(h))
        return -1;

    if (len > 0 && write(sock, payload, len) != (ssize_t)len)
        return -1;

    return 0;
}

int recv_msg(int sock, msg_header_t *h, uint8_t *buf, size_t bufsize)
{
    ssize_t r = read(sock, h, sizeof(*h));
    if (r <= 0) return r;

    if (h->payload_len > 0) {
        if (!buf || bufsize < h->payload_len)
            return -1;

        ssize_t p = read(sock, buf, h->payload_len);
        if (p != h->payload_len)
            return -1;
    }

    return 1;
}