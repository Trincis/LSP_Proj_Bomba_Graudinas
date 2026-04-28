#include "network.h"
#include <unistd.h>
#include <string.h>
// Vienkāršas funkcijas ziņu sūtīšanai un saņemšanai, izmantojot TCP socketus
int send_msg(int sock, uint8_t type, uint8_t sender, uint8_t target,
             const void *payload, size_t len)
{
    msg_header_t h;
    h.msg_type = type;
    h.sender_id = sender;
    h.target_id = target;
    h.payload_len = len;
    // Sūta ziņas galveni
    if (write(sock, &h, sizeof(h)) != sizeof(h))
        return -1;
    // Sūta ziņas saturu, ja tāds ir
    if (len > 0 && write(sock, payload, len) != (ssize_t)len)
        return -1;

    return 0;
}
// Saņem ziņu no socketa, nolasot vispirms galveni, un pēc tam saturu
int recv_msg(int sock, msg_header_t *h, uint8_t *buf, size_t bufsize)
{
    ssize_t r = read(sock, h, sizeof(*h));
    if (r <= 0) return r;
    // Ja ziņas galvenē norādītais payload garums pārsniedz bufera izmēru, atgriež kļūdu
    if (h->payload_len > 0) {
        if (!buf || bufsize < h->payload_len)
            return -1;

        ssize_t p = read(sock, buf, h->payload_len);
        // Ja nolasītais payload garums nesakrīt ar galvenē norādīto, atgriež kļūdu
        if (p != h->payload_len)
            return -1;
    }

    return 1;
}