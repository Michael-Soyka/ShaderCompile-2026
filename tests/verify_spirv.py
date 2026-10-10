# Checks a ShaderCompile .vcs holding SPIR-V: combo count, SPIR-V magic, GLCompute entry point,
# or (vs/ps mode) a .vcs holding an SM3 shell wrapping SPIR-V for OTM DXVK
import lzma
import struct
import sys

LZMA_ID = 0x414D5A4C
SPIRV_MAGIC = 0x07230203
OP_ENTRY_POINT = 15
EXEC_MODEL_GLCOMPUTE = 5

OP_NAME = 5
OP_MEMBER_NAME = 6
OP_DECORATE = 71
OP_MEMBER_DECORATE = 72
OP_VARIABLE = 59
OP_TYPE_POINTER = 32
OP_TYPE_SAMPLER = 26
OP_DECORATE_STRING = 5632
DEC_BUILTIN = 11
DEC_LOCATION = 30
DEC_BINDING = 33
DEC_OFFSET = 35
DEC_USER_SEMANTIC = 5635
STORAGE_INPUT = 1
STORAGE_OUTPUT = 3
EXEC_MODEL = { 'vs': 0, 'ps': 4 }
VERSION_TOKEN = { 'vs': 0xFFFE0300, 'ps': 0xFFFF0300 }
SHELL_MARKER = b'OTM_DXVK_SHADER\0'
SHELL_MARKER_STAGES = b'OTM_DXVK_STAGES\0'
STAGE_EXEC_MODEL = { 0: 0, 1: 1, 2: 2 }   # VS Vertex, HS TessellationControl, DS TessellationEvaluation


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


def shell_payload( code, stage ):
    words = struct.unpack( f'<{len( code ) // 4}I', code )
    if words[0] != VERSION_TOKEN[stage]:
        raise ValueError( f'bad version token {words[0]:#x}' )

    payload = []
    pos = 1
    while ( words[pos] & 0xFFFF ) == 0xFFFE:
        count = words[pos] >> 16
        payload += words[pos + 1:pos + 1 + count]
        pos += 1 + count

    if words[pos] != 0x0000FFFF or pos + 1 != len( words ):
        raise ValueError( 'no end token after the comments' )

    return payload


# [( stage, spirv bytes )], one VS entry for a v1 shell
def unwrap_stages( code, stage ):
    payload = shell_payload( code, stage )
    marker = struct.pack( '<4I', *payload[:4] )
    if marker == SHELL_MARKER:
        return [ ( 0, unwrap_shell( code, stage ) ) ]

    if marker != SHELL_MARKER_STAGES:
        raise ValueError( 'no OTM_DXVK_SHADER / OTM_DXVK_STAGES marker' )

    count = payload[4]
    table = [ ( payload[5 + ( i * 2 )], payload[6 + ( i * 2 )] ) for i in range( count ) ]
    pos = 5 + ( count * 2 )
    stages = []
    for kind, size in table:
        stages.append( ( kind, struct.pack( f'<{size}I', *payload[pos:pos + size] ) ) )
        pos += size

    if pos != len( payload ):
        raise ValueError( f'stage sizes {pos} != payload {len( payload )}' )

    return stages


def main_stages( path, expected, expected_tess, bindings ):
    data = open( path, 'rb' ).read()
    combos = read_vcs_combos( data )
    if len( combos ) != expected:
        print( f'{path}: {len( combos )} combos, expected {expected}' )
        return 1

    tess = 0
    for combo_id, code in combos:
        stages = unwrap_stages( code, 'vs' )
        kinds = [ kind for kind, _ in stages ]
        if kinds not in ( [ 0 ], [ 0, 1, 2 ] ):
            print( f'{path}: combo {combo_id} stages {kinds}, expected [0] or [0, 1, 2]' )
            return 1

        tess += 1 if len( kinds ) == 3 else 0
        for kind, spirv in stages:
            info = reflect( spirv )
            # DXVK creates every stage with the entry point name "main"
            if ( STAGE_EXEC_MODEL[kind], 'main' ) not in info['entries']:
                print( f'{path}: combo {combo_id} stage {kind} has no entry "main" with execution model {STAGE_EXEC_MODEL[kind]}: {info["entries"]}' )
                return 1

            if not info['bindings'] <= bindings:
                print( f'{path}: combo {combo_id} stage {kind} bindings {sorted( info["bindings"] )} not in {sorted( bindings )}' )
                return 1

            for var in info['io'] - info['builtins']:
                if var not in info['semantics']:
                    print( f'{path}: combo {combo_id} stage {kind} variable %{var} has no UserSemantic' )
                    return 1

    if tess != expected_tess:
        print( f'{path}: {tess} combos with hull and domain stages, expected {expected_tess}' )
        return 1

    print( f'{path}: {len( combos )} combos ok, {tess} with hull and domain stages' )
    return 0


def unwrap_shell( code, stage ):
    words = struct.unpack( f'<{len( code ) // 4}I', code )
    if words[0] != VERSION_TOKEN[stage]:
        raise ValueError( f'bad version token {words[0]:#x}' )

    chunks = []
    pos = 1
    while ( words[pos] & 0xFFFF ) == 0xFFFE:
        count = words[pos] >> 16
        chunks.append( words[pos + 1:pos + 1 + count] )
        pos += 1 + count

    if words[pos] != 0x0000FFFF or pos + 1 != len( words ):
        raise ValueError( 'no end token after the comments' )

    if not chunks or struct.pack( '<4I', *chunks[0][:4] ) != SHELL_MARKER:
        raise ValueError( 'no OTM_DXVK_SHADER marker' )

    total = chunks[0][4]
    spirv = list( chunks[0][5:] )
    for chunk in chunks[1:]:
        spirv += chunk

    if len( spirv ) != total:
        raise ValueError( f'SPIR-V size {len( spirv )} != header {total}' )

    return struct.pack( f'<{total}I', *spirv )


def reflect( code ):
    words = struct.unpack( f'<{len( code ) // 4}I', code )
    if words[0] != SPIRV_MAGIC:
        raise ValueError( f'bad SPIR-V magic {words[0]:#x}' )

    info = { 'bindings': set(), 'builtins': set(), 'semantics': {}, 'io': set(), 'member_names': {}, 'offsets': {}, 'entries': [] }
    i = 5
    while i < len( words ):
        count = words[i] >> 16
        opcode = words[i] & 0xFFFF
        args = words[i + 1:i + count]

        if opcode == OP_ENTRY_POINT:
            name_bytes = struct.pack( f'<{count - 3}I', *words[i + 3:i + count] )
            info['entries'].append( ( args[0], name_bytes.split( b'\0' )[0].decode() ) )
        elif opcode == OP_DECORATE and args[1] == DEC_BINDING:
            info['bindings'].add( args[2] )
        elif opcode == OP_DECORATE and args[1] == DEC_BUILTIN:
            info['builtins'].add( args[0] )
        elif opcode == OP_DECORATE_STRING and args[1] == DEC_USER_SEMANTIC:
            text = struct.pack( f'<{len( args ) - 2}I', *args[2:] ).split( b'\0' )[0].decode()
            info['semantics'][args[0]] = text
        elif opcode == OP_VARIABLE and args[2] in ( STORAGE_INPUT, STORAGE_OUTPUT ):
            info['io'].add( args[1] )
        elif opcode == OP_MEMBER_NAME:
            text = struct.pack( f'<{len( args ) - 2}I', *args[2:] ).split( b'\0' )[0].decode()
            info['member_names'][( args[0], args[1] )] = text
        elif opcode == OP_MEMBER_DECORATE and args[2] == DEC_OFFSET:
            info['offsets'][( args[0], args[1] )] = args[3]
        elif opcode == OP_TYPE_SAMPLER:
            # Our DXVK binds only combined texture + sampler pairs
            raise ValueError( 'separate sampler type: [[vk::combinedImageSampler]] did not combine' )

        i += count

    return info


def main_shell( path, expected, stage, bindings, offsets ):
    data = open( path, 'rb' ).read()
    combos = read_vcs_combos( data )
    if len( combos ) != expected:
        print( f'{path}: {len( combos )} combos, expected {expected}' )
        return 1

    for combo_id, code in combos:
        info = reflect( unwrap_shell( code, stage ) )

        if ( EXEC_MODEL[stage], 'main' ) not in info['entries']:
            print( f'{path}: combo {combo_id} has no {stage} entry "main": {info["entries"]}' )
            return 1

        if info['bindings'] != bindings:
            print( f'{path}: combo {combo_id} bindings {sorted( info["bindings"] )}, expected {sorted( bindings )}' )
            return 1

        for var in info['io'] - info['builtins']:
            if var not in info['semantics']:
                print( f'{path}: combo {combo_id} variable %{var} has no UserSemantic' )
                return 1

        named = { name: info['offsets'].get( key ) for key, name in info['member_names'].items() }
        for name, offset in offsets.items():
            if named.get( name ) != offset:
                print( f'{path}: combo {combo_id} member {name} offset {named.get( name )}, expected {offset}' )
                return 1

    print( f'{path}: {len( combos )} shelled SPIR-V combos ok' )
    return 0


def read_vcs_combos( data ):
    num_static = struct.unpack_from( '<I', data, 20 )[0]
    records = [ struct.unpack_from( '<II', data, 28 + ( i * 8 ) ) for i in range( num_static ) ]
    alias_pos = 28 + ( num_static * 8 )
    num_alias = struct.unpack_from( '<I', data, alias_pos )[0]
    aliases = [ struct.unpack_from( '<II', data, alias_pos + 4 + ( i * 8 ) ) for i in range( num_alias ) ]

    per_static = {}
    for static_id, offset in records:
        if static_id != 0xFFFFFFFF:
            per_static[static_id] = read_combos( data, offset )

    combos = [ c for cs in per_static.values() for c in cs ]
    for _, source in aliases:
        combos += per_static[source]

    return combos


def main():
    path = sys.argv[1]
    expected = int( sys.argv[2] )
    mode = sys.argv[3] if len( sys.argv ) > 3 else 'main'

    if mode == 'vs-stages':
        # verify_spirv.py <vcs> <combos> vs-stages <combos with HS/DS> <binding,binding,...>
        bindings = { int( b ) for b in sys.argv[5].split( ',' ) }
        return main_stages( path, expected, int( sys.argv[4] ), bindings )

    if mode in ( 'vs', 'ps' ):
        # verify_spirv.py <vcs> <combos> <vs|ps> <binding,binding,...> [member=offset ...]
        bindings = { int( b ) for b in sys.argv[4].split( ',' ) }
        offsets = { kv.split( '=' )[0]: int( kv.split( '=' )[1] ) for kv in sys.argv[5:] }
        return main_shell( path, expected, mode, bindings, offsets )

    entry = mode
    data = open( path, 'rb' ).read()
    combos = read_vcs_combos( data )
    if len( combos ) != expected:
        print( f'{path}: {len( combos )} combos, expected {expected}' )
        return 1

    for combo_id, code in combos:
        points = entry_points( code )
        if ( EXEC_MODEL_GLCOMPUTE, entry ) not in points:
            print( f'{path}: combo {combo_id} has no GLCompute entry "{entry}": {points}' )
            return 1

    print( f'{path}: {len( combos )} SPIR-V combos ok' )
    return 0


if __name__ == '__main__':
    sys.exit( main() )
