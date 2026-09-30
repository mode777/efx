// Deterministic ZIP writer used by the gallery build and the release sample
// packer. Entries are stored (uncompressed) with a fixed 1980-01-01 timestamp
// and a hand-rolled CRC32, so the same inputs always produce byte-identical
// archives regardless of the Node version, host clock, or filesystem order.
// No third-party dependency (design D2).

// CRC32 (IEEE 802.3) lookup table, built once.
const CRC_TABLE = (() => {
    const table = new Uint32Array(256);
    for (let n = 0; n < 256; n++) {
        let c = n;
        for (let k = 0; k < 8; k++) {
            c = c & 1 ? 0xedb88320 ^ (c >>> 1) : c >>> 1;
        }
        table[n] = c >>> 0;
    }
    return table;
})();

function crc32(buf) {
    let c = 0xffffffff;
    for (let i = 0; i < buf.length; i++) {
        c = CRC_TABLE[(c ^ buf[i]) & 0xff] ^ (c >>> 8);
    }
    return (c ^ 0xffffffff) >>> 0;
}

// MS-DOS date/time for 1980-01-01 00:00:00 (the earliest ZIP timestamp).
const DOS_DATE = ((1980 - 1980) << 9) | (1 << 5) | 1; // 0x0021
const DOS_TIME = 0;
// external_attr: regular file, mode 0644, in the high 16 bits.
const EXTERNAL_ATTR = (0o644 << 16) >>> 0;

/**
 * Build a ZIP archive from `[{ name, data }]`. Names are sorted and use
 * forward slashes; no directory records are emitted.
 */
export function makeZip(entries) {
    const files = entries
        .map((e) => ({
            name: String(e.name).replace(/\\/g, '/'),
            data: Buffer.isBuffer(e.data) ? e.data : Buffer.from(e.data),
        }))
        .sort((a, b) => (a.name < b.name ? -1 : a.name > b.name ? 1 : 0));

    const parts = [];
    const centrals = [];
    let offset = 0;

    for (const file of files) {
        const nameBuf = Buffer.from(file.name, 'utf8');
        const crc = crc32(file.data);
        const size = file.data.length;

        const local = Buffer.alloc(30 + nameBuf.length);
        local.writeUInt32LE(0x04034b50, 0); // local file header signature
        local.writeUInt16LE(20, 4); // version needed to extract
        local.writeUInt16LE(0, 6); // general purpose flags
        local.writeUInt16LE(0, 8); // compression method: stored
        local.writeUInt16LE(DOS_TIME, 10);
        local.writeUInt16LE(DOS_DATE, 12);
        local.writeUInt32LE(crc, 14);
        local.writeUInt32LE(size, 18); // compressed size
        local.writeUInt32LE(size, 22); // uncompressed size
        local.writeUInt16LE(nameBuf.length, 26);
        local.writeUInt16LE(0, 28); // extra field length
        nameBuf.copy(local, 30);
        parts.push(local, file.data);

        const central = Buffer.alloc(46 + nameBuf.length);
        central.writeUInt32LE(0x02014b50, 0); // central directory signature
        central.writeUInt16LE(20, 4); // version made by
        central.writeUInt16LE(20, 6); // version needed to extract
        central.writeUInt16LE(0, 8); // flags
        central.writeUInt16LE(0, 10); // method: stored
        central.writeUInt16LE(DOS_TIME, 12);
        central.writeUInt16LE(DOS_DATE, 14);
        central.writeUInt32LE(crc, 16);
        central.writeUInt32LE(size, 20);
        central.writeUInt32LE(size, 24);
        central.writeUInt16LE(nameBuf.length, 28);
        central.writeUInt16LE(0, 30); // extra
        central.writeUInt16LE(0, 32); // comment
        central.writeUInt16LE(0, 34); // disk number start
        central.writeUInt16LE(0, 36); // internal attributes
        central.writeUInt32LE(EXTERNAL_ATTR, 38);
        central.writeUInt32LE(offset, 42); // local header offset
        nameBuf.copy(central, 46);
        centrals.push(central);

        offset += local.length + file.data.length;
    }

    const centralSize = centrals.reduce((n, b) => n + b.length, 0);
    const end = Buffer.alloc(22);
    end.writeUInt32LE(0x06054b50, 0); // end of central directory signature
    end.writeUInt16LE(0, 4); // disk number
    end.writeUInt16LE(0, 6); // disk with central directory
    end.writeUInt16LE(files.length, 8); // entries on this disk
    end.writeUInt16LE(files.length, 10); // total entries
    end.writeUInt32LE(centralSize, 12);
    end.writeUInt32LE(offset, 16); // central directory offset
    end.writeUInt16LE(0, 20); // comment length

    return Buffer.concat([...parts, ...centrals, end]);
}
