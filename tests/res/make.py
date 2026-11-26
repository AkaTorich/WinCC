# Files for the resource test (tests\win_res.rc): an icon of two images (16x16 as a bitmap,
# 32x32 as PNG), a cursor with its hot spot at 5,7, a 24-bit picture 7x3, and some bytes.
#   python tests\res\make.py
import os, struct, zlib

here = os.path.dirname(os.path.abspath(__file__))


def dib(w, h, color):
    """BITMAPINFOHEADER + 32-bit pixels + the AND mask, as an image of an .ico / .cur."""
    head = struct.pack('<IiiHHIIiiII', 40, w, h * 2, 1, 32, 0, 0, 0, 0, 0, 0)
    pixels = bytes(color) * (w * h)
    mask_row = ((w + 31) // 32) * 4
    return head + pixels + bytes(mask_row * h)


def png(w, h, rgba):
    def chunk(kind, data):
        return struct.pack('>I', len(data)) + kind + data + struct.pack('>I', zlib.crc32(kind + data) & 0xFFFFFFFF)
    raw = b''.join(b'\0' + bytes(rgba) * w for _ in range(h))
    return (b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 6, 0, 0, 0)) +
            chunk(b'IDAT', zlib.compress(raw)) + chunk(b'IEND', b''))


def container(kind, images):
    """.ico (kind 1) or .cur (kind 2): images = (w, h, a, b, data); a, b — planes and bits or the hot spot."""
    out = struct.pack('<HHH', 0, kind, len(images))
    off = 6 + 16 * len(images)
    body = b''
    for w, h, a, b, data in images:
        out += struct.pack('<BBBBHHII', w % 256, h % 256, 0, 0, a, b, len(data), off + len(body))
        body += data
    return out + body


def bmp(w, h):
    row = (w * 3 + 3) & ~3
    pixels = b''
    for y in range(h):
        line = b''.join(bytes((x * 30, y * 80, 200)) for x in range(w))
        pixels += line + bytes(row - len(line))
    info = struct.pack('<IiiHHIIiiII', 40, w, h, 1, 24, 0, len(pixels), 2835, 2835, 0, 0)
    head = b'BM' + struct.pack('<IHHI', 14 + len(info) + len(pixels), 0, 0, 14 + len(info))
    return head + info + pixels


def write(name, data):
    with open(os.path.join(here, name), 'wb') as f:
        f.write(data)


write('test.ico', container(1, [(16, 16, 1, 32, dib(16, 16, (0, 0, 255, 255))),
                                (32, 32, 0, 0, png(32, 32, (0, 160, 0, 255)))]))
write('test.cur', container(2, [(32, 32, 5, 7, dib(32, 32, (255, 0, 0, 255)))]))
write('test.bmp', bmp(7, 3))
write('data.bin', bytes(range(256)) * 3 + b'WinCC')
print('ok')
