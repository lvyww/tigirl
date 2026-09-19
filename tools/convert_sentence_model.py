"""Losslessly unpack TigerClaw TCSKNM02 into Tigirl's TCSKNM01 layout."""
import argparse
import mmap
from pathlib import Path
import struct

HEADER = struct.Struct('<8sIIQIIIIQIIQQQIIQQ')


def convert(source: Path, destination: Path):
    with source.open('rb') as stream, mmap.mmap(stream.fileno(), 0, access=mmap.ACCESS_READ) as data:
        fields = HEADER.unpack_from(data)
        magic, version, header_size, size, stride, reserved, unigrams, reserved2, unigram_at, *rest = fields
        if (magic, version, header_size, size) != (b'TCSKNM02', 1, HEADER.size, len(data)) or not stride or reserved or reserved2:
            raise ValueError('Invalid TCSKNM02 header')
        big_count, big_pages, big_at, big_index, tri_count, tri_pages, reserved3, tri_at, tri_index = rest
        if reserved3 or unigram_at != HEADER.size or big_at != unigram_at + unigrams * 8 or tri_at != big_index + big_pages * 16 or len(data) != tri_index + tri_pages * 16:
            raise ValueError('Invalid section offsets')

        def blocks(count, pages, start, end, wide):
            entries, contexts = bytearray(), bytearray()
            at, previous = start, -1
            if pages != (count + stride - 1) // stride:
                raise ValueError('Invalid page count')
            for i in range(count):
                if at + 16 > end:
                    raise ValueError('Truncated context')
                key, probability, successors = struct.unpack_from('<QII', data, at)
                if key <= previous or key >= (1 << (42 if wide else 21)):
                    raise ValueError('Invalid context key')
                if i % stride == 0 and struct.unpack_from('<QQ', data, end + i // stride * 16) != (key, at):
                    raise ValueError('Invalid sparse index')
                previous = key
                contexts.extend(struct.pack('<QI' if wide else '<II', key, probability))
                at += 16
                if at + successors * 8 > end:
                    raise ValueError('Truncated successors')
                last = -1
                for j in range(successors):
                    token, bits = struct.unpack_from('<II', data, at + j * 8)
                    if token <= last or token >= (1 << 21):
                        raise ValueError('Invalid successor key')
                    last = token
                    entries.extend(struct.pack('<QI', (key << 21) | token, bits))
                at += successors * 8
            if at != end:
                raise ValueError('Trailing context data')
            return entries, contexts

        bigrams, big_contexts = blocks(big_count, big_pages, big_at, big_index, False)
        trigrams, tri_contexts = blocks(tri_count, tri_pages, tri_at, tri_index, True)
        destination.parent.mkdir(parents=True, exist_ok=True)
        with destination.open('xb') as output:
            output.write(b'TCSKNM01' + struct.pack('<II', 1, unigrams))
            output.write(data[unigram_at:big_at])
            for section, count, fmt in [(bigrams, len(bigrams)//12, '<Q'), (big_contexts, big_count, '<I'), (trigrams, len(trigrams)//12, '<Q'), (tri_contexts, tri_count, '<Q')]:
                output.write(struct.pack(fmt, count))
                output.write(section)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    parser.add_argument('destination', type=Path)
    args = parser.parse_args()
    convert(args.source, args.destination)
