"""Preserve native glyphs and supplement cross-language names in bundled CFNTs."""
from pathlib import Path
from functools import lru_cache
import struct
import sys
import json

def blocks(data):
    if data[:6] != b"CFNT\xff\xfe":
        raise ValueError("not a little-endian CFNT")
    pos = struct.unpack_from("<H", data, 6)[0]
    for _ in range(struct.unpack_from("<H", data, 16)[0]):
        size = struct.unpack_from("<I", data, pos+4)[0]
        if size < 8 or pos+size > len(data):
            raise ValueError("invalid font block")
        yield data[pos:pos+4], pos, size
        pos += size

def character_map(data):
    result = {}
    for tag, pos, size in blocks(data):
        if tag != b"CMAP":
            continue
        first, last, method = struct.unpack_from("<3H", data, pos+8)
        if method == 0:
            start = struct.unpack_from("<H", data, pos+20)[0]
            pairs = ((c, start+c-first) for c in range(first, last+1))
        elif method == 1:
            pairs = ((c, struct.unpack_from("<H", data, pos+20+2*(c-first))[0])
                     for c in range(first, last+1))
        elif method == 2:
            count = struct.unpack_from("<H", data, pos+20)[0]
            pairs = (struct.unpack_from("<2H", data, pos+22+4*i) for i in range(count))
        else:
            raise ValueError("unsupported CMAP method")
        result.update((c, g) for c, g in pairs if g != 0xffff)
    return result

def character_widths(data):
    """CFNT CWDH advances, indexed by Unicode through its actual CMAP."""
    glyphs = {}
    for tag, pos, size in blocks(data):
        if tag == b"CWDH":
            first, last = struct.unpack_from("<2H", data, pos+8)
            if 16+3*(last-first+1) > size:
                raise ValueError("truncated CWDH")
            glyphs.update((i, data[pos+16+3*(i-first)+2]) for i in range(first,last+1))
    return {chr(c): glyphs[g] for c,g in character_map(data).items() if g in glyphs}

def glyph_metrics(data):
    result = {}
    for tag, pos, _ in blocks(data):
        if tag == b'CWDH':
            first, last = struct.unpack_from('<2H', data, pos+8)
            for index in range(first, last+1):
                result[index] = data[pos+16+3*(index-first):pos+19+3*(index-first)]
    return result

@lru_cache(maxsize=8)
def _sample_offsets(cell_w, cell_h, columns, rows, width, border=False):
    """GPU Morton-order sample locations; shared by every page of a font."""
    xbits = tuple(sum(((x >> bit)&1) << (2*bit) for bit in range(3)) for x in range(8))
    ybits = tuple(value*2 for value in xbits)
    result = []
    margin = 0 if border else 1
    for index in range(columns*rows):
        col, row = index % columns, index // columns
        result.append(tuple(((py//8)*(width//8)+px//8)*64+xbits[px&7]+ybits[py&7]
                            for y in range(cell_h+int(border)) for x in range(cell_w+int(border))
                            for px, py in [(margin+col*(cell_w+1)+x, margin+row*(cell_h+1)+y)]))
    return tuple(result)

class FontResource:
    """Read CFNT pixels without relying on human/OCR names for private glyphs."""
    def __init__(self, data):
        self.data = data
        self.mapping = character_map(data)
        self.metrics = glyph_metrics(data)
        self.texture = next(pos for tag, pos, _ in blocks(data) if tag == b'TGLP')
        self.cell_w, self.cell_h, self.baseline, self.max_width = data[self.texture+8:self.texture+12]
        self.pages, self.format, self.columns, self.rows, self.width, self.height = struct.unpack_from('<6H', data, self.texture+16)
        self.page_size = struct.unpack_from('<I', data, self.texture+12)[0]
        self.offset = struct.unpack_from('<I', data, self.texture+28)[0]
        self.samples = _sample_offsets(self.cell_w, self.cell_h, self.columns, self.rows, self.width)
        self.slot_samples = _sample_offsets(self.cell_w, self.cell_h, self.columns, self.rows, self.width, True)
        self.cache = {}

    def _pixels(self, index, border):
        key = index, border
        if key not in self.cache:
            page, glyph = divmod(index, self.columns*self.rows)
            if page >= self.pages or self.format not in (8, 9, 11):
                raise ValueError('unsupported glyph texture')
            pixels = (self.slot_samples if border else self.samples)[glyph]
            start = self.offset+page*self.page_size
            if self.format == 11:
                result = bytes(((self.data[start+p//2] >> (4*(p&1)))&15)*17 for p in pixels)
            elif self.format == 9:
                result = bytes((self.data[start+p] >> 4)*17 for p in pixels)
            else:
                result = bytes(self.data[start+p] for p in pixels)
            self.cache[key] = result
        return self.cache[key]

    def alpha(self, index):
        return self._pixels(index, False)

    def slot(self, index):
        return self._pixels(index, True)

    def signature(self, index):
        return self.metrics[index]+self.slot(index)

def required_name_codepoints():
    """Use name characters, never the viewer's human-decoded private glyph map."""
    root = Path(__file__).resolve().parents[2]
    from storage_messages import STORAGE_NOTICE_MESSAGES
    required = {ord(c) for c in '群组組盒子银行銀行'}
    required.update(ord(c) for texts in STORAGE_NOTICE_MESSAGES.values() for text in texts for c in text if c!='\n')
    resources = sorted((root/'ViewerForBankdata/resources').glob('species_*.json'))
    if len(resources) != 10:
        raise ValueError('missing native-language species name resources')
    for resource in resources:
        names = json.loads(resource.read_text(encoding='utf-8'))['names']
        if len(names) != 808 or not all(isinstance(name, str) for name in names):
            raise ValueError(f'invalid supported-species names: {resource.name}')
        required.update(ord(c) for name in names for c in name)
    return required

def _is_cjk(code):
    return 0x3400 <= code <= 0x9fff or 0xf900 <= code <= 0xfaff

def _extend_maps(parts, additions):
    """Fill existing TABLE holes before extending SCAN: a tail map is not enough.

    The native lookup stops on a matching TABLE even when its entry is FFFF.
    All supplied fonts have a terminal BMP SCAN, with sorted unique pairs.
    """
    tables = [part for part in parts if part[:4] == b'CMAP']
    scans = {}
    for code, glyph in sorted(additions.items()):
        for part in tables:
            first, last, method = struct.unpack_from('<3H', part, 8)
            if not first <= code <= last:
                continue
            if method == 0:
                raise ValueError('supplement overlaps an original direct mapping')
            if method == 1:
                offset = 20+2*(code-first)
                if struct.unpack_from('<H', part, offset)[0] != 0xffff:
                    raise ValueError('supplement overwrites an original table mapping')
                struct.pack_into('<H', part, offset, glyph)
            elif method == 2:
                index = id(part)
                if index not in scans:
                    count = struct.unpack_from('<H', part, 20)[0]
                    scans[index] = dict(struct.iter_unpack('<HH', part[22:22+4*count]))
                if code in scans[index]:
                    raise ValueError('supplement overwrites an original scan mapping')
                scans[index][code] = glyph
            else:
                raise ValueError('unsupported native character map')
            break
        else:
            raise ValueError(f'no native map can represent U+{code:04X}')
    for part in tables:
        if id(part) in scans:
            entries = sorted(scans[id(part)].items())
            if len(entries) > 0xffff:
                raise ValueError('CFNT scan entry count overflow')
            part[20:] = struct.pack('<H', len(entries))+b''.join(struct.pack('<HH', *entry) for entry in entries)
            part.extend(bytes((-len(part))%4))
            struct.pack_into('<I', part, 4, len(part))

def _write_alpha(destination, font, index, alpha, border=False):
    page, glyph = divmod(index, font.columns*font.rows)
    if page >= font.pages or len(alpha) != (font.cell_w+int(border))*(font.cell_h+int(border)):
        raise ValueError('glyph exceeds its new texture allocation')
    start = page*font.page_size
    samples = (font.slot_samples if border else font.samples)[glyph]
    for pixel, value in zip(samples, alpha):
        if font.format != 11 or value%17:
            raise ValueError('supplement requires lossless native A4 pixels')
        offset, shift = start+pixel//2, 4*(pixel&1)
        destination[offset] = (destination[offset] & ~(15 << shift)) | ((value//17) << shift)

def _rebuild_font(original, parts):
    """Relocate every native file offset after a texture/map block changes size."""
    header_size = struct.unpack_from('<H', original, 6)[0]
    output = bytearray(original[:header_size])
    positions = []
    for part in parts:
        if len(part)%4:
            raise ValueError('unaligned font block')
        struct.pack_into('<I', part, 4, len(part))
        positions.append(len(output))
        output.extend(part)
    groups = {tag: [pos for part, pos in zip(parts, positions) if part[:4] == tag]
              for tag in (b'FINF', b'TGLP', b'CWDH', b'CMAP')}
    if any(not nodes for nodes in groups.values()) or len(groups[b'FINF']) != 1 or len(groups[b'TGLP']) != 1:
        raise ValueError('unsupported native font block arrangement')
    finf = groups[b'FINF'][0]
    for offset, tag in ((16, b'TGLP'), (20, b'CWDH'), (24, b'CMAP')):
        struct.pack_into('<I', output, finf+offset, groups[tag][0]+8)
    for tag, offset in ((b'CWDH', 12), (b'CMAP', 16)):
        nodes = groups[tag]
        for index, pos in enumerate(nodes):
            struct.pack_into('<I', output, pos+offset, nodes[index+1]+8 if index+1 < len(nodes) else 0)
    old_texture = next(pos for tag, pos, _ in blocks(original) if tag == b'TGLP')
    relative_pixels = struct.unpack_from('<I', original, old_texture+28)[0]-old_texture
    texture = groups[b'TGLP'][0]
    struct.pack_into('<I', output, texture+28, texture+relative_pixels)
    if (texture+relative_pixels)%128:
        raise ValueError('font texture lost native 128-byte alignment')
    struct.pack_into('<II', output, 12, len(output), len(parts))
    return bytes(output)

def supplement_names(original, donors, required):
    """Supplement required names; retain all original indices, widths and pixels.

    Exact existing pixel+metric matches become aliases. New cells copy native
    donors without resizing. If the narrow-sheet atlas runs out, repack into
    power-of-two native texture dimensions only when that does not grow either
    the texture byte allocation or the number of draw sheets.
    """
    font = FontResource(original)
    compatible = [donor for donor in donors if
                  (donor.cell_w, donor.cell_h, donor.baseline, donor.format) ==
                  (font.cell_w, font.cell_h, font.baseline, font.format)]
    union = set().union(*(set(donor.mapping) for donor in compatible))
    unavailable = required-union-set(font.mapping)
    if unavailable:
        raise ValueError('no native donor glyphs for '+','.join(f'U+{code:04X}' for code in sorted(unavailable)))
    signatures = {}
    for index in sorted(font.metrics):
        signatures.setdefault(font.signature(index), index)
    mapping = dict(font.mapping)
    copied = []
    first = max(font.metrics)+1
    for code in sorted(required-set(mapping)):
        candidates = [(donor, donor.mapping[code]) for donor in compatible if code in donor.mapping]
        match = next((signatures[donor.signature(index)] for donor, index in candidates
                      if donor.signature(index) in signatures), None)
        if match is None:
            donor, index = candidates[0]
            match = first+len(copied)
            if match >= 0xffff:
                raise ValueError('CFNT glyph-index overflow')
            copied.append((donor, index))
            signatures[donor.signature(index)] = match
        mapping[code] = match
    # Extra cross-language mappings cost no texture cells. They are accepted
    # only with an exact native pixel+metric match, never OCR or resemblance.
    for code in sorted(union-set(mapping)):
        for donor in compatible:
            if code in donor.mapping and donor.signature(donor.mapping[code]) in signatures:
                mapping[code] = signatures[donor.signature(donor.mapping[code])]
                break
    additions = {code: index for code, index in mapping.items() if code not in font.mapping}
    if not additions:
        return original
    parts = [bytearray(original[pos:pos+size]) for _, pos, size in blocks(original)]
    texture_part = next(part for part in parts if part[:4] == b'TGLP')
    texture_prefix = font.offset-font.texture
    original_texture_bytes = font.pages*font.page_size
    geometry = font
    last = first+len(copied)
    if last > font.pages*font.columns*font.rows:
        options = []
        for width in (32, 64, 128, 256, 512, 1024):
            for height in (32, 64, 128, 256, 512, 1024):
                columns, rows = width//(font.cell_w+1), height//(font.cell_h+1)
                if not columns or not rows:
                    continue
                pages = (last+columns*rows-1)//(columns*rows)
                page_size = width*height//2
                if pages*page_size <= original_texture_bytes and pages <= font.pages:
                    options.append((pages*page_size, pages, width, height, page_size, columns, rows))
        if not options:
            raise ValueError(f'cross-language glyphs exceed native font memory: {first=}, '
                             f'new={len(copied)}; refusing unsafe expansion')
        _, pages, width, height, page_size, columns, rows = min(options)
        # Build the prospective header first so native sample coordinates can
        # be used unchanged for both the old and the new glyph images.
        probe = bytearray(original)
        struct.pack_into('<I6H', probe, font.texture+12, page_size, pages, font.format,
                         columns, rows, width, height)
        geometry = FontResource(bytes(probe))
        pixels = bytearray(pages*page_size)
        for index in sorted(font.metrics):
            _write_alpha(pixels, geometry, index, font.slot(index), True)
    else:
        pixels = bytearray(original[font.offset:font.offset+original_texture_bytes])
    for relative, (donor, index) in enumerate(copied):
        target = first+relative
        if target < font.pages*font.columns*font.rows and any(font.slot(target)):
            raise ValueError('supplement would replace nonempty unmapped native pixels')
        _write_alpha(pixels, geometry, target, donor.slot(index), True)
    trailer = texture_part[texture_prefix+original_texture_bytes:]
    texture_part[texture_prefix:] = pixels+trailer
    struct.pack_into('<I6H', texture_part, 12, geometry.page_size, geometry.pages, geometry.format,
                     geometry.columns, geometry.rows, geometry.width, geometry.height)
    if copied:
        widths = b''.join(donor.metrics[index] for donor, index in copied)
        size = (16+len(widths)+3)&~3
        part = bytearray(struct.pack('<4sIHHI', b'CWDH', size, first, last-1, 0)+widths+bytes(size-16-len(widths)))
        parts.insert(next(index for index, item in enumerate(parts) if item[:4] == b'CMAP'), part)
    _extend_maps(parts, additions)
    return _rebuild_font(original, parts)

def glyph_bitmap(data, index):
    pos = next(pos for tag, pos, _ in blocks(data) if tag == b"TGLP")
    cell_w, cell_h = data[pos+8:pos+10]
    sheet_size = struct.unpack_from("<I", data, pos+12)[0]
    count, fmt, cols, rows, width, height = struct.unpack_from("<6H", data, pos+16)
    offset = struct.unpack_from("<I", data, pos+28)[0]
    page, glyph = divmod(index, cols*rows)
    if page >= count or fmt not in (8, 9, 11):
        raise ValueError(f"unsupported glyph texture: {fmt=}, {page=}, {count=}")
    col, row = glyph % cols, glyph // cols
    result = []
    for y in range(cell_h):
        row_values = []
        for x in range(cell_w):
            px, py = 1+col*(cell_w+1)+x, 1+row*(cell_h+1)+y
            morton = sum(((px >> bit)&1) << (2*bit) |
                         ((py >> bit)&1) << (2*bit+1) for bit in range(3))
            pixel = ((py//8)*(width//8)+px//8)*64+morton
            location = offset+page*sheet_size
            value = data[location+pixel//2] if fmt == 11 else data[location+pixel]
            alpha = ((value >> (4*(pixel&1)))&15)*17 if fmt == 11 else \
                    (value >> 4)*17 if fmt == 9 else value
            row_values.append(alpha)
        result.append(row_values)
    return result

def glyph_pixels(data, index):
    from PIL import Image
    pixels = glyph_bitmap(data,index)
    image = Image.new("L", (len(pixels[0]),len(pixels)))
    image.putdata([alpha for row in pixels for alpha in row])
    return image

def supplement_arrows(original, donor):
    """Append two maps/widths and use unused sheet cells, preserving old glyphs.

    The small auxiliary font lacks U+2191/U+2193. Copy arrow pixels from the
    bundled standard font, trimming only transparent margins, without scaling.
    All original indices, metrics, maps and texture cells remain unchanged.
    """
    wanted = tuple(c for c in (0x2191,0x2193) if c not in character_map(original))
    if not wanted:
        return original
    parts = list(blocks(original)); out = bytearray(original)
    cwdh = [(p,s) for tag,p,s in parts if tag==b'CWDH']
    maps = [(p,s) for tag,p,s in parts if tag==b'CMAP']
    first = max(struct.unpack_from('<H',original,p+10)[0] for p,_ in cwdh)+1
    texture = next(p for tag,p,_ in parts if tag==b'TGLP')
    cell_w,cell_h = original[texture+8:texture+10]
    count,fmt,cols,rows,width,height = struct.unpack_from('<6H',original,texture+16)
    sheet_size = struct.unpack_from('<I',original,texture+12)[0]
    offset = struct.unpack_from('<I',original,texture+28)[0]
    if fmt!=9 or first+len(wanted)>count*cols*rows:
        raise ValueError('no unused auxiliary glyph cells for arrows')
    widths = bytearray()
    donor_map = character_map(donor)
    for n,code in enumerate(wanted):
        index = first+n
        if any(alpha for row in glyph_bitmap(original,index) for alpha in row):
            raise ValueError('arrow would replace nonempty unused glyph data')
        bitmap = glyph_bitmap(donor,donor_map[code])
        visible = [(x,y) for y,row in enumerate(bitmap) for x,alpha in enumerate(row) if alpha]
        left,top = min(x for x,y in visible),min(y for x,y in visible)
        right,bottom = max(x for x,y in visible)+1,max(y for x,y in visible)+1
        if right-left>cell_w or bottom-top>cell_h:
            raise ValueError('arrow pixels do not fit without resizing')
        page,glyph = divmod(index,cols*rows)
        col,row = glyph%cols,glyph//cols
        for y in range(bottom-top):
            for x in range(right-left):
                px,py = 1+col*(cell_w+1)+x,1+row*(cell_h+1)+y
                if px>=width or py>=height:
                    raise ValueError('glyph exceeds texture')
                morton = sum(((px>>b)&1)<<(2*b)|((py>>b)&1)<<(2*b+1) for b in range(3))
                pixel = ((py//8)*(width//8)+px//8)*64+morton
                alpha = bitmap[top+y][left+x]//17
                out[offset+page*sheet_size+pixel] = (alpha<<4)|15
        widths.extend((0,right-left,right-left+2))
    new_widths = len(out)
    size = (16+len(widths)+3)&~3
    out.extend(struct.pack('<4sIHHI',b'CWDH',size,first,first+len(wanted)-1,0)+widths+bytes(size-16-len(widths)))
    last_widths = cwdh[-1][0]
    if struct.unpack_from('<I',original,last_widths+12)[0]:
        raise ValueError('unexpected nonterminal CWDH')
    struct.pack_into('<I',out,last_widths+12,new_widths+8)
    new_map = len(out)
    size = (22+4*len(wanted)+3)&~3
    out.extend(struct.pack('<4sIHHHHIH',b'CMAP',size,min(wanted),max(wanted),2,0,0,len(wanted)))
    for n,code in enumerate(wanted):
        out.extend(struct.pack('<HH',code,first+n))
    out.extend(bytes(size-22-4*len(wanted)))
    last_map = maps[-1][0]
    if struct.unpack_from('<I',original,last_map+16)[0]:
        raise ValueError('unexpected nonterminal CMAP')
    struct.pack_into('<I',out,last_map+16,new_map+8)
    struct.pack_into('<I',out,12,len(out))
    struct.pack_into('<H',out,16,len(parts)+2)
    return bytes(out)

def patch_title_fonts(data):
    from tools.message_archive import read_garc,write_garc
    version,alignment,entries = read_garc(data)
    donors = [FontResource(entries[index].files[0]) for index in range(3)]
    required = required_name_codepoints()
    required.update(code for donor in donors for code in donor.mapping if not _is_cjk(code))
    for index in range(3):
        entries[index].files[0] = supplement_names(entries[index].files[0], donors, required)
    entries[3].files[0] = supplement_arrows(entries[3].files[0],entries[0].files[0])
    return write_garc(version,alignment,entries)

def build_title_fonts(source_romfs,output_romfs):
    relative=Path('a/0/2/3')
    target=output_romfs/relative
    target.parent.mkdir(parents=True,exist_ok=True)
    target.write_bytes(patch_title_fonts((source_romfs/relative).read_bytes()))

def main():
    root = Path(__file__).resolve().parents[2]
    sys.path.insert(0, str(root/"bank"))
    from tools.message_archive import read_garc
    from PIL import Image, ImageDraw
    tiles = []
    for title in ("bank", "mover"):
        entries = read_garc((root/title/"rom/romfs/a/0/2/3").read_bytes())[2]
        for font in range(4):
            data = entries[font].files[0]
            mapping = character_map(data)
            print(title, font, [(hex(c), mapping.get(c)) for c in (0x2191,0x2193,0xe004,0xe005,0xe07d)])
            if font:
                continue
            for code in sorted(mapping):
                if 0xe000 <= code <= 0xe00f or 0xe079 <= code <= 0xe07e:
                    tiles.append((title, code, glyph_pixels(data, mapping[code])))
    chart = Image.new("RGB", (480, ((len(tiles)+5)//6)*80), "white")
    draw = ImageDraw.Draw(chart)
    for i, (title, code, tile) in enumerate(tiles):
        x, y = (i%6)*80, (i//6)*80
        tile = tile.resize((tile.width*3, tile.height*3), Image.Resampling.NEAREST)
        chart.paste("black", (x+12, y, x+12+tile.width, y+tile.height), tile)
        draw.text((x+2, y+55), f"{title} {code:04X}", fill="black")
    output = root/"bank/build/title-button-glyphs.png"
    chart.save(output)
    print(output)

if __name__ == "__main__":
    main()
