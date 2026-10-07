# Checks a ShaderCompile .vcs holding SPIR-V: combo count, SPIR-V magic, GLCompute entry point
import lzma
import struct
import sys

LZMA_ID = 0x414D5A4C
SPIRV_MAGIC = 0x07230203
OP_ENTRY_POINT = 15
EXEC_MODEL_GLCOMPUTE = 5


def read_block( data, pos ):
    flags = struct.unpack_from( '<I', data, pos )[0]
    pos += 4
    kind = flags & 0xC0000000
    size = flags & 0x3FFFFFFF
    raw = data[pos:pos + size]
    pos += size

    if kind == 0x80000000:
        return raw, pos

    if kind == 0x40000000:
        ident, actual, packed = struct.unpack_from( '<III', raw, 0 )
        if ident != LZMA_ID:
            raise ValueError( 'bad LZMA id' )
        alone = raw[12:17] + struct.pack( '<Q', actual ) + raw[17:17 + packed]
        return lzma.decompress( alone, format = lzma.FORMAT_ALONE ), pos

    raise ValueError( f'unsupported block kind {kind:#x}' )


def read_combos( data, offset ):
    combos = []
    pos = offset
    while True:
        marker = struct.unpack_from( '<I', data, pos )[0]
        if marker == 0xFFFFFFFF:
            return combos

        block, pos = read_block( data, pos )
        inner = 0
        while inner < len( block ):
            combo_id, size = struct.unpack_from( '<II', block, inner )
            inner += 8
            combos.append( ( combo_id, block[inner:inner + size] ) )
            inner += size


def entry_points( code ):
    words = struct.unpack( f'<{len( code ) // 4}I', code )
    if words[0] != SPIRV_MAGIC:
        raise ValueError( f'bad SPIR-V magic {words[0]:#x}' )

    found = []
    i = 5
    while i < len( words ):
        count = words[i] >> 16
        opcode = words[i] & 0xFFFF
        if count == 0:
            raise ValueError( 'zero word count' )

        if opcode == OP_ENTRY_POINT:
            name_bytes = struct.pack( f'<{count - 3}I', *words[i + 3:i + count] )
            found.append( ( words[i + 1], name_bytes.split( b'\0' )[0].decode() ) )

        i += count

    return found


def main():
    path = sys.argv[1]
    expected = int( sys.argv[2] )
    entry = sys.argv[3] if len( sys.argv ) > 3 else 'main'
    data = open( path, 'rb' ).read()

    num_static = struct.unpack_from( '<I', data, 20 )[0]
    records = [ struct.unpack_from( '<II', data, 28 + ( i * 8 ) ) for i in range( num_static ) ]
    alias_pos = 28 + ( num_static * 8 )
    num_alias = struct.unpack_from( '<I', data, alias_pos )[0]
    aliases = [ struct.unpack_from( '<II', data, alias_pos + 4 + ( i * 8 ) ) for i in range( num_alias ) ]

    per_static = {}
    for static_id, offset in records:
        if static_id != 0xFFFFFFFF:
            per_static[static_id] = read_combos( data, offset )

    total = sum( len( c ) for c in per_static.values() )
    total += sum( len( per_static[source] ) for _, source in aliases )
    if total != expected:
        print( f'{path}: {total} combos, expected {expected}' )
        return 1

    for static_id, combos in per_static.items():
        for combo_id, code in combos:
            points = entry_points( code )
            if ( EXEC_MODEL_GLCOMPUTE, entry ) not in points:
                print( f'{path}: static {static_id} combo {combo_id} has no GLCompute entry "{entry}": {points}' )
                return 1

    print( f'{path}: {total} SPIR-V combos ok' )
    return 0


if __name__ == '__main__':
    sys.exit( main() )
