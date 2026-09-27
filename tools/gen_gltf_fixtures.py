#!/usr/bin/env python3
"""Generate the committed F6b glTF fixtures.

Deterministic, dependency-free: writes .gltf/.glb/.bin/.png fixtures under
tests/fixtures/resource/gltf and copies the golden scene asset. Run from the
repo root: python3 tools/gen_gltf_fixtures.py
"""
import json
import os
import struct
import zlib
import zipfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FIX = os.path.join(ROOT, "tests", "fixtures", "resource", "gltf")
GOLD = os.path.join(ROOT, "tests", "goldens", "gltf_import")

COMP_FLOAT = 5126
COMP_U8 = 5121
COMP_U16 = 5123
COMP_U32 = 5125

TARGET_ARRAY = 34962
TARGET_ELEMENT = 34963


class Bin:
    def __init__(self):
        self.data = bytearray()

    def add(self, blob, align=4):
        while len(self.data) % align != 0:
            self.data.append(0)
        off = len(self.data)
        self.data += blob
        return off

    def view(self, blob, target=None, align=4):
        off = self.add(blob, align=align)
        bv = {"buffer": 0, "byteOffset": off, "byteLength": len(blob)}
        if target is not None:
            bv["target"] = target
        return bv

    def size(self):
        return len(self.data)


def f32(values):
    return struct.pack("<%df" % len(values), *values)


def u16(values):
    return struct.pack("<%dH" % len(values), *values)


def u32(values):
    return struct.pack("<%dI" % len(values), *values)


def u8(values):
    return struct.pack("<%dB" % len(values), *values)


def png(w, h, rgba):
    raw = bytearray()
    for y in range(h):
        raw.append(0)
        raw += rgba[y * w * 4:(y + 1) * w * 4]

    def chunk(typ, data):
        out = struct.pack(">I", len(data)) + typ + data
        return out + struct.pack(">I", zlib.crc32(typ + data) & 0xFFFFFFFF)

    ihdr = struct.pack(">IIBBBBB", w, h, 8, 6, 0, 0, 0)
    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", ihdr)
            + chunk(b"IDAT", zlib.compress(bytes(raw), 9)) + chunk(b"IEND", b""))


def write(path, data):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    mode = "wb" if isinstance(data, (bytes, bytearray)) else "w"
    with open(path, mode) as f:
        f.write(data)


def base_asset():
    return {"version": "2.0", "generator": "efx-fixture-gen"}


def single_triangle(position_floats, name="Tri"):
    """A standalone .gltf + external .bin, one triangle, no material."""
    b = Bin()
    pv = b.view(f32(position_floats), TARGET_ARRAY)
    iv = b.view(u16([0, 1, 2]), TARGET_ELEMENT, align=2)
    gltf = {
        "asset": base_asset(),
        "scene": 0,
        "scenes": [{"nodes": [0]}],
        "nodes": [{"mesh": 0, "name": name}],
        "meshes": [{"name": "m", "primitives": [
            {"attributes": {"POSITION": 0}, "indices": 1}]}],
        "accessors": [
            {"bufferView": 0, "componentType": COMP_FLOAT, "count": 3,
             "type": "VEC3"},
            {"bufferView": 1, "componentType": COMP_U16, "count": 3,
             "type": "SCALAR"},
        ],
        "bufferViews": [pv, iv],
        "buffers": [{"byteLength": b.size()}],
    }
    return gltf, bytes(b.data)


def gen_triangle():
    gltf, bin = single_triangle([-1, 0, 0, 1, 0, 0, 0, 1, 0])
    gltf["buffers"][0]["uri"] = "triangle.bin"
    write(os.path.join(FIX, "triangle.gltf"),
          json.dumps(gltf, indent=2) + "\n")
    write(os.path.join(FIX, "triangle.bin"), bin)


def gen_transform():
    gltf, bin = single_triangle([-1, 0, 0, 1, 0, 0, 0, 1, 0])
    gltf["buffers"][0]["uri"] = "transform.bin"
    gltf["nodes"][0]["translation"] = [10, 20, 30]
    gltf["nodes"][0]["scale"] = [2, 2, 2]
    write(os.path.join(FIX, "transform.gltf"),
          json.dumps(gltf, indent=2) + "\n")
    write(os.path.join(FIX, "transform.bin"), bin)


def gen_materials():
    b = Bin()
    a = b.view(f32([-1, -1, 0, 1, -1, 0, 0, 1, 0]), TARGET_ARRAY)
    c = b.view(f32([2, -1, 0, 4, -1, 0, 3, 1, 0]), TARGET_ARRAY)
    gltf = {
        "asset": base_asset(),
        "scene": 0,
        "scenes": [{"nodes": [0]}],
        "nodes": [{"mesh": 0}],
        "meshes": [{"name": "m", "primitives": [
            {"attributes": {"POSITION": 0}, "material": 0},
            {"attributes": {"POSITION": 1}},
        ]}],
        "materials": [{
            "name": "factor",
            "pbrMetallicRoughness": {
                "baseColorFactor": [0.2, 0.4, 0.6, 1.0],
                "metallicFactor": 0.5,
                "roughnessFactor": 0.5,
            },
            "emissiveFactor": [0.1, 0.2, 0.3],
        }],
        "accessors": [
            {"bufferView": 0, "componentType": COMP_FLOAT, "count": 3,
             "type": "VEC3"},
            {"bufferView": 1, "componentType": COMP_FLOAT, "count": 3,
             "type": "VEC3"},
        ],
        "bufferViews": [a, c],
        "buffers": [{"uri": "materials.bin", "byteLength": b.size()}],
    }
    write(os.path.join(FIX, "materials.gltf"),
          json.dumps(gltf, indent=2) + "\n")
    write(os.path.join(FIX, "materials.bin"), bytes(b.data))


def gen_accessors():
    b = Bin()
    # surface 0: float positions + u16 indices
    p0 = b.view(f32([-1, 0, 0, 1, 0, 0, 0, 1, 0]), TARGET_ARRAY)
    i0 = b.view(u16([0, 1, 2]), TARGET_ELEMENT, align=2)
    # surface 1: u16 positions (not normalized)
    p1 = b.view(u16([0, 0, 0, 100, 0, 0, 0, 200, 0]), TARGET_ARRAY, align=2)
    # surface 2: u8 normalized positions
    p2 = b.view(u8([0, 0, 0, 255, 0, 0, 0, 128, 0]), TARGET_ARRAY, align=1)
    # surface 3: sparse float positions
    p3 = b.view(f32([0, 0, 0, 0, 0, 0, 0, 0, 0]), TARGET_ARRAY)
    si = b.view(u16([1, 2]), align=2)
    sv = b.view(f32([5, 0, 0, 0, 6, 0]))
    gltf = {
        "asset": base_asset(),
        "scene": 0,
        "scenes": [{"nodes": [0]}],
        "nodes": [{"mesh": 0}],
        "meshes": [{"name": "m", "primitives": [
            {"attributes": {"POSITION": 0}, "indices": 1},
            {"attributes": {"POSITION": 2}},
            {"attributes": {"POSITION": 3}},
            {"attributes": {"POSITION": 4}},
        ]}],
        "accessors": [
            {"bufferView": 0, "componentType": COMP_FLOAT, "count": 3,
             "type": "VEC3"},
            {"bufferView": 1, "componentType": COMP_U16, "count": 3,
             "type": "SCALAR"},
            {"bufferView": 2, "componentType": COMP_U16, "count": 3,
             "type": "VEC3"},
            {"bufferView": 3, "componentType": COMP_U8, "count": 3,
             "type": "VEC3", "normalized": True},
            {"bufferView": 4, "componentType": COMP_FLOAT, "count": 3,
             "type": "VEC3", "sparse": {
                 "count": 2,
                 "indices": {"bufferView": 5, "byteOffset": 0,
                             "componentType": COMP_U16},
                 "values": {"bufferView": 6, "byteOffset": 0},
             }},
        ],
        "bufferViews": [p0, i0, p1, p2, p3, si, sv],
        "buffers": [{"uri": "accessors.bin", "byteLength": b.size()}],
    }
    write(os.path.join(FIX, "accessors.gltf"),
          json.dumps(gltf, indent=2) + "\n")
    write(os.path.join(FIX, "accessors.bin"), bytes(b.data))


def gen_required_ext():
    gltf, _ = single_triangle([-1, 0, 0, 1, 0, 0, 0, 1, 0])
    gltf["buffers"][0]["uri"] = "triangle.bin"
    gltf["extensionsUsed"] = ["KHR_nonexistent"]
    gltf["extensionsRequired"] = ["KHR_nonexistent"]
    write(os.path.join(FIX, "required_ext.gltf"),
          json.dumps(gltf, indent=2) + "\n")


def gen_corrupt():
    write(os.path.join(FIX, "corrupt.gltf"), "{ this is not valid glTF json\n")


def gen_bad_image():
    b = Bin()
    b.view(f32([-1, -1, 0, 1, -1, 0, 0, 1, 0]), TARGET_ARRAY)
    b.view(f32([0, 0, 1, 0, 0, 1, 1, 1, 0]), TARGET_ARRAY)
    gltf = {
        "asset": base_asset(),
        "scene": 0,
        "scenes": [{"nodes": [0]}],
        "nodes": [{"mesh": 0}],
        "meshes": [{"name": "m", "primitives": [
            {"attributes": {"POSITION": 0, "TEXCOORD_0": 1}, "material": 0}]}],
        "materials": [{
            "pbrMetallicRoughness": {
                "baseColorTexture": {"index": 0},
            },
        }],
        "textures": [{"source": 0}],
        "images": [{"uri": "missing_texture.png"}],
        "accessors": [
            {"bufferView": 0, "componentType": COMP_FLOAT, "count": 3,
             "type": "VEC3"},
            {"bufferView": 1, "componentType": COMP_FLOAT, "count": 3,
             "type": "VEC2"},
        ],
        "bufferViews": [{"buffer": 0, "byteOffset": 0, "byteLength": 36,
                         "target": TARGET_ARRAY},
                        {"buffer": 0, "byteOffset": 36, "byteLength": 24,
                         "target": TARGET_ARRAY}],
        "buffers": [{"uri": "bad_image.bin", "byteLength": b.size()}],
    }
    write(os.path.join(FIX, "bad_image.gltf"),
          json.dumps(gltf, indent=2) + "\n")
    write(os.path.join(FIX, "bad_image.bin"), bytes(b.data))


def gen_textured_gltf():
    """External .bin + external .png (directory reference resolution)."""
    b = Bin()
    pv = b.view(f32([-1, -1, 0, 1, -1, 0, -1, 1, 0, 1, 1, 0]), TARGET_ARRAY)
    nv = b.view(f32([0, 0, 1] * 4), TARGET_ARRAY)
    uv = b.view(f32([0, 0, 1, 0, 0, 1, 1, 1]), TARGET_ARRAY)
    iv = b.view(u16([0, 1, 2, 2, 1, 3]), TARGET_ELEMENT, align=2)
    tex = png(2, 2, bytes([255, 0, 0, 255, 0, 255, 0, 255,
                           0, 0, 255, 255, 255, 255, 255, 255]))
    gltf = {
        "asset": base_asset(),
        "scene": 0,
        "scenes": [{"nodes": [0]}],
        "nodes": [{"mesh": 0}],
        "meshes": [{"name": "m", "primitives": [
            {"attributes": {"POSITION": 0, "NORMAL": 1, "TEXCOORD_0": 2},
             "indices": 3, "material": 0}]}],
        "materials": [{
            "pbrMetallicRoughness": {
                "baseColorFactor": [1, 1, 1, 1],
                "metallicFactor": 0.0,
                "roughnessFactor": 1.0,
                "baseColorTexture": {"index": 0},
            },
        }],
        "samplers": [{"magFilter": 9728, "minFilter": 9728,
                      "wrapS": 33071, "wrapT": 33071}],
        "textures": [{"source": 0, "sampler": 0}],
        "images": [{"uri": "tex.png"}],
        "accessors": [
            {"bufferView": 0, "componentType": COMP_FLOAT, "count": 4,
             "type": "VEC3"},
            {"bufferView": 1, "componentType": COMP_FLOAT, "count": 4,
             "type": "VEC3"},
            {"bufferView": 2, "componentType": COMP_FLOAT, "count": 4,
             "type": "VEC2"},
            {"bufferView": 3, "componentType": COMP_U16, "count": 6,
             "type": "SCALAR"},
        ],
        "bufferViews": [pv, nv, uv, iv],
        "buffers": [{"uri": "textured.bin", "byteLength": b.size()}],
    }
    write(os.path.join(FIX, "textured.gltf"),
          json.dumps(gltf, indent=2) + "\n")
    write(os.path.join(FIX, "textured.bin"), bytes(b.data))
    write(os.path.join(FIX, "tex.png"), tex)


def glb_from(gltf, bindata):
    jsonb = json.dumps(gltf, separators=(",", ":")).encode("utf-8")
    while len(jsonb) % 4 != 0:
        jsonb += b" "
    binb = bytearray(bindata)
    while len(binb) % 4 != 0:
        binb.append(0)
    total = 12 + 8 + len(jsonb) + (8 + len(binb) if binb else 0)
    out = bytearray()
    out += struct.pack("<4sII", b"glTF", 2, total)
    out += struct.pack("<II", len(jsonb), 0x4E4F534A) + jsonb
    if binb:
        out += struct.pack("<II", len(binb), 0x004E4942) + bytes(binb)
    return bytes(out)


def gen_quad_glb():
    """Self-contained .glb: two primitives, base-color texture (MASK) plus an
    emissive solid; sampler nearest/clamp. Drives the golden scene."""
    b = Bin()
    # textured quad (left), emissive quad (right)
    pos = f32([-1.5, -1, 0, -0.5, -1, 0, -1.5, 1, 0, -0.5, 1, 0,
               0.5, -1, 0, 1.5, -1, 0, 0.5, 1, 0, 1.5, 1, 0])
    nrm = f32([0, 0, 1] * 8)
    uv = f32([0, 0, 1, 0, 0, 1, 1, 1] * 2)
    idx = u16([0, 1, 2, 2, 1, 3, 4, 5, 6, 6, 5, 7])
    pv = b.view(pos, TARGET_ARRAY)
    nv = b.view(nrm, TARGET_ARRAY)
    uvv = b.view(uv, TARGET_ARRAY)
    iv = b.view(idx, TARGET_ELEMENT, align=2)
    tex = png(2, 2, bytes([255, 0, 0, 255, 0, 255, 0, 255,
                           0, 0, 255, 255, 255, 255, 255, 255]))
    tv = b.view(tex, align=4)
    gltf = {
        "asset": base_asset(),
        "scene": 0,
        "scenes": [{"nodes": [0]}],
        "nodes": [{"mesh": 0}],
        "meshes": [{"name": "m", "primitives": [
            {"attributes": {"POSITION": 0, "NORMAL": 1, "TEXCOORD_0": 2},
             "indices": 3, "material": 0},
            {"attributes": {"POSITION": 0, "NORMAL": 1, "TEXCOORD_0": 2},
             "indices": 3, "material": 1},
        ]}],
        "materials": [
            {
                "name": "textured-mask",
                "pbrMetallicRoughness": {
                    "baseColorFactor": [1, 1, 1, 1],
                    "metallicFactor": 0.0, "roughnessFactor": 1.0,
                    "baseColorTexture": {"index": 0},
                },
                "alphaMode": "MASK", "alphaCutoff": 0.5,
            },
            {
                "name": "emissive",
                "pbrMetallicRoughness": {
                    "baseColorFactor": [0, 0, 0, 1],
                    "metallicFactor": 0.0, "roughnessFactor": 1.0,
                },
                "emissiveFactor": [0, 1, 0],
            },
        ],
        "samplers": [{"magFilter": 9728, "minFilter": 9728,
                      "wrapS": 33071, "wrapT": 33071}],
        "textures": [{"source": 0, "sampler": 0}],
        "images": [{"bufferView": 4, "mimeType": "image/png"}],
        "accessors": [
            {"bufferView": 0, "componentType": COMP_FLOAT, "count": 8,
             "type": "VEC3"},
            {"bufferView": 1, "componentType": COMP_FLOAT, "count": 8,
             "type": "VEC3"},
            {"bufferView": 2, "componentType": COMP_FLOAT, "count": 8,
             "type": "VEC2"},
            {"bufferView": 3, "componentType": COMP_U16, "count": 12,
             "type": "SCALAR"},
        ],
        "bufferViews": [pv, nv, uvv, iv, tv],
        "buffers": [{"byteLength": b.size()}],
    }
    blob = glb_from(gltf, bytes(b.data))
    write(os.path.join(FIX, "quad.glb"), blob)
    write(os.path.join(GOLD, "quad.glb"), blob)


def gen_zip():
    path = os.path.join(ROOT, "tests", "fixtures", "resource",
                        "gltf_pack.zip")
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with zipfile.ZipFile(path, "w", zipfile.ZIP_DEFLATED) as z:
        z.write(os.path.join(FIX, "triangle.gltf"), "triangle.gltf")
        z.write(os.path.join(FIX, "triangle.bin"), "triangle.bin")
        z.write(os.path.join(FIX, "textured.gltf"), "textured.gltf")
        z.write(os.path.join(FIX, "textured.bin"), "textured.bin")
        z.write(os.path.join(FIX, "tex.png"), "tex.png")
        z.write(os.path.join(FIX, "quad.glb"), "quad.glb")


def gen_dedup():
    """Two surfaces share one (image, sampler) texture; a third uses the same
    image with a different sampler (distinct texture)."""
    b = Bin()
    a = b.view(f32([-1, -1, 0, 1, -1, 0, 0, 1, 0]), TARGET_ARRAY)
    c = b.view(f32([2, -1, 0, 4, -1, 0, 3, 1, 0]), TARGET_ARRAY)
    d = b.view(f32([5, -1, 0, 7, -1, 0, 6, 1, 0]), TARGET_ARRAY)
    uvs = b.view(f32([0, 0, 1, 0, 0, 1]), TARGET_ARRAY)
    tex = png(2, 2, bytes([255, 0, 0, 255, 0, 255, 0, 255,
                           0, 0, 255, 255, 255, 255, 255, 255]))
    gltf = {
        "asset": base_asset(),
        "scene": 0,
        "scenes": [{"nodes": [0]}],
        "nodes": [{"mesh": 0}],
        "meshes": [{"name": "m", "primitives": [
            {"attributes": {"POSITION": 0, "TEXCOORD_0": 3}, "material": 0},
            {"attributes": {"POSITION": 1, "TEXCOORD_0": 3}, "material": 0},
            {"attributes": {"POSITION": 2, "TEXCOORD_0": 3}, "material": 1},
        ]}],
        "materials": [
            {"pbrMetallicRoughness": {"baseColorTexture": {"index": 0}}},
            {"pbrMetallicRoughness": {"baseColorTexture": {"index": 1}}},
        ],
        "samplers": [{"magFilter": 9729, "minFilter": 9729},
                     {"magFilter": 9728, "minFilter": 9728}],
        "textures": [{"source": 0, "sampler": 0},
                     {"source": 0, "sampler": 1}],
        "images": [{"uri": "dedup.png"}],
        "accessors": [
            {"bufferView": 0, "componentType": COMP_FLOAT, "count": 3,
             "type": "VEC3"},
            {"bufferView": 1, "componentType": COMP_FLOAT, "count": 3,
             "type": "VEC3"},
            {"bufferView": 2, "componentType": COMP_FLOAT, "count": 3,
             "type": "VEC3"},
            {"bufferView": 3, "componentType": COMP_FLOAT, "count": 3,
             "type": "VEC2"},
        ],
        "bufferViews": [a, c, d, uvs],
        "buffers": [{"uri": "dedup.bin", "byteLength": b.size()}],
    }
    write(os.path.join(FIX, "dedup.gltf"),
          json.dumps(gltf, indent=2) + "\n")
    write(os.path.join(FIX, "dedup.bin"), bytes(b.data))
    write(os.path.join(FIX, "dedup.png"), tex)


def gen_cap():
    """One mesh with 17 primitives (over the fixed 16-surface cap)."""
    b = Bin()
    b.view(f32([-1, 0, 0, 1, 0, 0, 0, 1, 0]), TARGET_ARRAY)
    prims = [{"attributes": {"POSITION": 0}} for _ in range(17)]
    gltf = {
        "asset": base_asset(),
        "scene": 0,
        "scenes": [{"nodes": [0]}],
        "nodes": [{"mesh": 0}],
        "meshes": [{"name": "m", "primitives": prims}],
        "accessors": [{"bufferView": 0, "componentType": COMP_FLOAT,
                       "count": 3, "type": "VEC3"}],
        "bufferViews": [{"buffer": 0, "byteOffset": 0, "byteLength": 36,
                         "target": TARGET_ARRAY}],
        "buffers": [{"uri": "cap.bin", "byteLength": b.size()}],
    }
    write(os.path.join(FIX, "cap.gltf"), json.dumps(gltf, indent=2) + "\n")
    write(os.path.join(FIX, "cap.bin"), bytes(b.data))


def gen_script_probe():
    """Script-local probe assets (default --script resource root) and the
    matching web resource-root fixture that runs the same main.js body."""
    scripts = os.path.join(ROOT, "tests", "scripts")
    web = os.path.join(ROOT, "tests", "fixtures", "web", "gltf_probe")
    os.makedirs(scripts, exist_ok=True)
    os.makedirs(web, exist_ok=True)
    glb = open(os.path.join(FIX, "quad.glb"), "rb").read()
    corrupt = open(os.path.join(FIX, "corrupt.gltf"), "rb").read()
    write(os.path.join(scripts, "gltf_probe.glb"), glb)
    write(os.path.join(scripts, "gltf_corrupt.gltf"), corrupt)
    write(os.path.join(web, "gltf_probe.glb"), glb)
    write(os.path.join(web, "gltf_corrupt.gltf"), corrupt)
    script = os.path.join(scripts, "s_6b_gltf.js")
    if os.path.exists(script):
        with open(script) as f:
            write(os.path.join(web, "main.js"), f.read())


def main():
    os.makedirs(FIX, exist_ok=True)
    os.makedirs(GOLD, exist_ok=True)
    gen_triangle()
    gen_transform()
    gen_materials()
    gen_accessors()
    gen_required_ext()
    gen_corrupt()
    gen_bad_image()
    gen_textured_gltf()
    gen_dedup()
    gen_cap()
    gen_quad_glb()
    gen_zip()
    gen_script_probe()
    print("wrote F6b glTF fixtures under", os.path.relpath(FIX, ROOT))


if __name__ == "__main__":
    main()
