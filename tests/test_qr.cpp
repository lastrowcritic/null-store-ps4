#define PORTAL_HOST
#include "../src/qr.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

// Two independent checks:
//  1. An exact matrix match against an external encoder (segno) for a payload
//     whose bitstream fills the data codewords, so no padding convention can
//     differ. That pins down row/column order, format bits and mask handling.
//  2. A byte-mode read-back of the real setup URL straight out of the drawn
//     matrix, which proves the address survives encoding regardless of padding.

static std::string readTextFile(const char* path) {
    std::string text;
    FILE* file = fopen(path, "rb");
    if (!file) return text;
    char buffer[4096];
    size_t got;
    while ((got = fread(buffer, 1, sizeof buffer, file))) text.append(buffer, got);
    fclose(file);
    return text;
}

static std::vector<std::string> rowsOf(const QrSymbol& symbol) {
    std::vector<std::string> rows;
    for (int y = 0; y < symbol.size; ++y) {
        std::string row;
        for (int x = 0; x < symbol.size; ++x) row += symbol.dark(x, y) ? '1' : '0';
        rows.push_back(row);
    }
    return rows;
}

// Version 1..6 function patterns, sufficient for a 29x29 (version 3) symbol.
static std::vector<std::vector<bool>> functionMap(int size, int version) {
    std::vector<std::vector<bool>> map((size_t)size, std::vector<bool>((size_t)size, false));
    auto fill = [&](int row, int col, int height, int width) {
        for (int y = row; y < row + height; ++y)
            for (int x = col; x < col + width; ++x)
                if (y >= 0 && y < size && x >= 0 && x < size) map[(size_t)y][(size_t)x] = true;
    };
    fill(0, 0, 9, 9);
    fill(0, size - 8, 9, 8);
    fill(size - 8, 0, 8, 9);
    for (int i = 0; i < size; ++i) { map[6][(size_t)i] = true; map[(size_t)i][6] = true; }
    for (int i = 0; i < 9; ++i) { map[8][(size_t)i] = true; map[(size_t)i][8] = true; }
    for (int i = 0; i < 8; ++i) { map[8][(size_t)(size - 1 - i)] = true; map[(size_t)(size - 1 - i)][8] = true; }
    map[(size_t)(size - 8)][8] = true;
    if (version >= 2) {
        int center = size - 7;
        fill(center - 2, center - 2, 5, 5);
    }
    return map;
}

static bool maskBit(int mask, int x, int y) {
    switch (mask) {
        case 0: return (x + y) % 2 == 0;
        case 1: return y % 2 == 0;
        case 2: return x % 3 == 0;
        case 3: return (x + y) % 3 == 0;
        case 4: return ((y / 2) + (x / 3)) % 2 == 0;
        case 5: return (x * y) % 2 + (x * y) % 3 == 0;
        case 6: return ((x * y) % 2 + (x * y) % 3) % 2 == 0;
        default: return ((x + y) % 2 + (x * y) % 3) % 2 == 0;
    }
}

static std::string readBytePayload(const std::vector<std::string>& rows, int mask) {
    int size = (int)rows.size();
    int version = (size - 17) / 4;
    auto functions = functionMap(size, version);
    std::vector<int> bits;
    int right = size - 1;
    bool upward = true;
    while (right > 0) {
        if (right == 6) --right;
        for (int i = 0; i < size; ++i) {
            int y = upward ? size - 1 - i : i;
            for (int column = 0; column < 2; ++column) {
                int x = right - column;
                if (x < 0 || functions[(size_t)y][(size_t)x]) continue;
                int value = rows[(size_t)y][(size_t)x] - '0';
                if (maskBit(mask, x, y)) value ^= 1;
                bits.push_back(value);
            }
        }
        upward = !upward;
        right -= 2;
    }
    std::vector<int> codewords;
    for (size_t i = 0; i + 7 < bits.size(); i += 8) {
        int value = 0;
        for (int k = 0; k < 8; ++k) value = (value << 1) | bits[i + k];
        codewords.push_back(value);
    }
    if (codewords.empty()) return "";
    std::string bitstream;
    for (int code : codewords)
        for (int k = 7; k >= 0; --k) bitstream += ((code >> k) & 1) ? '1' : '0';
    if (bitstream.compare(0, 4, "0100") != 0) return "";
    int length = 0;
    for (int i = 4; i < 12; ++i) length = (length << 1) | (bitstream[(size_t)i] - '0');
    std::string payload;
    for (int i = 0; i < length; ++i) {
        int value = 0;
        for (int k = 0; k < 8; ++k) value = (value << 1) | (bitstream[(size_t)(12 + i * 8 + k)] - '0');
        payload += (char)value;
    }
    return payload;
}

int main() {
    // 1. Exact matrix match, no padding ambiguity.
    QrSymbol reference;
    assert(makeQrSymbol("HELLO WORLD", reference, 0));
    assert(reference.size == 21);

    std::string expected = readTextFile("tests/fixtures/qr-reference.txt");
    size_t junk = expected.find_first_not_of("01\n");
    if (junk != std::string::npos) expected.resize(junk);
    assert(!expected.empty() && "missing tests/fixtures/qr-reference.txt");
    auto expectedRows = std::vector<std::string>();
    {
        size_t at = 0;
        while (at < expected.size()) {
            size_t end = expected.find('\n', at);
            if (end == std::string::npos) end = expected.size();
            std::string row = expected.substr(at, end - at);
            if (!row.empty()) expectedRows.push_back(row);
            at = end + 1;
        }
    }
    auto rows = rowsOf(reference);
    assert(rows.size() == expectedRows.size());
    for (size_t i = 0; i < rows.size(); ++i) assert(rows[i] == expectedRows[i]);

    // 2. The real setup address reads back out of the matrix.
    const std::string url = "http://192.168.1.40:8090/418302";
    QrSymbol symbol;
    assert(makeQrSymbol(url, symbol, 3));
    assert(symbol.size == 29);
    auto urlRows = rowsOf(symbol);
    assert(readBytePayload(urlRows, 3) == url);

    // Every mask produces a symbol that still reads back.
    for (int mask = 0; mask < 8; ++mask) {
        QrSymbol candidate;
        assert(makeQrSymbol(url, candidate, mask));
        assert(readBytePayload(rowsOf(candidate), mask) == url);
    }

    // Quiet zone and scaling helpers used by the panel.
    assert(symbol.scaleFor(300) == 300 / (29 + 8));
    assert(symbol.pixelsFor(300) == (29 + 8) * symbol.scaleFor(300));
    assert(symbol.scaleFor(10) == 1);
    assert(!symbol.dark(-1, 0) && !symbol.dark(0, -1) && !symbol.dark(symbol.size, 0));

    // Too much data for version 10 is refused instead of truncated.
    QrSymbol huge;
    assert(!makeQrSymbol(std::string(900, 'a'), huge));

    std::cout << "QR: reference matrix match for HELLO WORLD, address round trip for all 8 masks, scaling and bounds passed.\n";
}
