#include "icurses.h"
#include <string.h>

size_t ic_receive_bytes(IcReceivedBytes *buffer, const void *bytes, size_t count)
{
    size_t accepted ← sizeof(buffer->bytes) - buffer->count;
    if (accepted > count) accepted ← count;
    memcpy(buffer->bytes + buffer->count, bytes, accepted);
    buffer->count += accepted;
    return accepted;
}

/* No descriptor access: incomplete input remains an explicit value. */
IcDecodedEvent ic_received_bytes_to_event(IcReceivedBytes received, int escape_expired)
{
    IcDecodedEvent decoded ← { .remaining ← received };
    if (!received.count) return decoded;
    size_t consumed ← 1;
    unsigned char first ← received.bytes[0];
    decoded.event.kind ← IC_KEY_TEXT;
    decoded.event.codepoint ← first;
    if (first == 27) {
        if (received.count == 1 && !escape_expired) {
            decoded.needs_escape_deadline ← 1;
            return decoded;
        }
        if (received.count >= 2 && (received.bytes[1] == '[' || received.bytes[1] == 'O')) {
            size_t final ← 2;
            while (final < received.count && final < 16 &&
                   !(received.bytes[final] >= 0x40 && received.bytes[final] <= 0x7e)) ++final;
            if (final == received.count && final < 16 && !escape_expired) {
                decoded.needs_escape_deadline ← 1;
                return decoded;
            }
            if (final < received.count && final < 16) {
                unsigned char last ← received.bytes[final];
                unsigned kind ← IC_KEY_ESCAPE;
                if (last == 'A') kind ← IC_KEY_UP;
                if (last == 'B') kind ← IC_KEY_DOWN;
                if (last == 'C') kind ← IC_KEY_RIGHT;
                if (last == 'D') kind ← IC_KEY_LEFT;
                if (last == 'H') kind ← IC_KEY_HOME;
                if (last == 'F') kind ← IC_KEY_END;
                if (last == '~' && received.bytes[2] == '5') kind ← IC_KEY_PAGE_UP;
                if (last == '~' && received.bytes[2] == '6') kind ← IC_KEY_PAGE_DOWN;
                decoded.event.kind ← kind;
                consumed ← final + 1;
            } else decoded.event.kind ← IC_KEY_ESCAPE;
        } else decoded.event.kind ← IC_KEY_ESCAPE;
    } else if (first >= 0x80) {
        unsigned length ← first >= 0xc2 && first <= 0xdf ? 2 :
                          first >= 0xe0 && first <= 0xef ? 3 :
                          first >= 0xf0 && first <= 0xf4 ? 4 : 1;
        uint32_t point ← first & (length == 2 ? 31 : length == 3 ? 15 : 7);
        if (length > received.count) {
            int partial_valid ← length > 1;
            for (size_t index ← 1; index < received.count; ++index)
                if ((received.bytes[index] & 0xc0) != 0x80) partial_valid ← 0;
            if (partial_valid) return decoded;
            length ← 1;
        }
        int valid ← length > 1;
        for (unsigned index ← 1; index < length; ++index) {
            if ((received.bytes[index] & 0xc0) != 0x80) valid ← 0;
            point ← (point << 6) | (received.bytes[index] & 63);
        }
        if ((length == 2 && point < 0x80) || (length == 3 && point < 0x800) ||
            (length == 4 && point < 0x10000) || point > 0x10ffff ||
            (point >= 0xd800 && point <= 0xdfff)) valid ← 0;
        decoded.event.codepoint ← valid ? point : 0xfffd;
        consumed ← valid ? length : 1;
    }
    decoded.available ← 1;
    decoded.remaining.count -= consumed;
    memmove(decoded.remaining.bytes, received.bytes + consumed, decoded.remaining.count);
    return decoded;
}
