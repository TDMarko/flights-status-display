#!/usr/bin/env python3
"""Stitch the preview PPMs into one PNG grid (2x scale). Pure stdlib."""
import glob, os, struct, sys, zlib

def read_ppm(path):
    data = open(path, 'rb').read()
    parts, i = [], 0
    while len(parts) < 4:
        while data[i:i+1].isspace(): i += 1
        if data[i:i+1] == b'#':
            while data[i:i+1] != b'\n': i += 1
            continue
        j = i
        while not data[j:j+1].isspace(): j += 1
        parts.append(data[i:j]); i = j
    i += 1
    w, h = int(parts[1]), int(parts[2])
    return w, h, data[i:i + w*h*3]

def write_png(path, w, h, rgb):
    raw = b''.join(b'\x00' + rgb[y*w*3:(y+1)*w*3] for y in range(h))
    def chunk(t, d):
        c = t + d
        return struct.pack('>I', len(d)) + c + struct.pack('>I', zlib.crc32(c) & 0xffffffff)
    png = (b'\x89PNG\r\n\x1a\n'
           + chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 2, 0, 0, 0))
           + chunk(b'IDAT', zlib.compress(raw, 9))
           + chunk(b'IEND', b''))
    open(path, 'wb').write(png)

def main():
    out_dir = sys.argv[1] if len(sys.argv) > 1 else '.preview'
    files = sorted(glob.glob(os.path.join(out_dir, '*.ppm')))
    if not files: sys.exit('no ppm files in ' + out_dir)
    scale, gap, cols = 2, 10, 2
    tiles = [read_ppm(f) for f in files]
    tw, th = tiles[0][0]*scale, tiles[0][1]*scale
    rows = (len(tiles) + cols - 1) // cols
    W = cols*tw + (cols+1)*gap
    H = rows*th + (rows+1)*gap
    canvas = bytearray(b'\x22' * (W*H*3))
    for idx, (w, h, px) in enumerate(tiles):
        ox = gap + (idx % cols) * (tw + gap)
        oy = gap + (idx // cols) * (th + gap)
        for y in range(h):
            row = px[y*w*3:(y+1)*w*3]
            big = bytearray()
            for x in range(w):
                big += row[x*3:x*3+3] * scale
            for sy in range(scale):
                start = ((oy + y*scale + sy) * W + ox) * 3
                canvas[start:start+len(big)] = big
    write_png(os.path.join(out_dir, 'montage.png'), W, H, bytes(canvas))
    print('wrote', os.path.join(out_dir, 'montage.png'), W, 'x', H)
    for f in files: print(' ', os.path.basename(f))

main()
