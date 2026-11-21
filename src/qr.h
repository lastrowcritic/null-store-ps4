#pragma once
// Thin C++ wrapper over the vendored Nayuki QR encoder. The Sources screen draws
// the setup address from this so the phone can scan instead of type.
#include <string>
#include <vector>
#include <cstdint>
#include "qrcodegen.h"

struct QrSymbol {
    int size = 0;
    std::vector<uint8_t> modules;

    bool dark(int x, int y) const {
        if (x < 0 || y < 0 || x >= size || y >= size) return false;
        return modules[(size_t)y * size + x] != 0;
    }

    // Smallest pixel scale that still fits the symbol plus a four-module quiet
    // zone into a box of the given side.
    int scaleFor(int box) const {
        if (size <= 0) return 1;
        int total = size + 8;
        int scale = box / total;
        return scale < 1 ? 1 : scale;
    }

    int pixelsFor(int box) const { return (size + 8) * scaleFor(box); }
};

// Encodes ASCII text into a QR symbol. Medium error correction, automatic mask,
// smallest version that fits, up to version 10 (57 modules).
inline bool makeQrSymbol(const std::string& text, QrSymbol& out, int mask = -1) {
    const int maxVersion = 10;
    std::vector<uint8_t> buffer((size_t)qrcodegen_BUFFER_LEN_FOR_VERSION(maxVersion));
    std::vector<uint8_t> scratch((size_t)qrcodegen_BUFFER_LEN_FOR_VERSION(maxVersion));
    qrcodegen_Mask chosen = (mask >= 0 && mask <= 7) ? (qrcodegen_Mask)mask : qrcodegen_Mask_AUTO;
    if (!qrcodegen_encodeText(text.c_str(), scratch.data(), buffer.data(), qrcodegen_Ecc_MEDIUM, 1, maxVersion, chosen, false))
        return false;
    int size = qrcodegen_getSize(buffer.data());
    out.size = size;
    out.modules.assign((size_t)size * size, 0);
    for (int y = 0; y < size; ++y)
        for (int x = 0; x < size; ++x)
            out.modules[(size_t)y * size + x] = qrcodegen_getModule(buffer.data(), x, y) ? 1 : 0;
    return true;
}
