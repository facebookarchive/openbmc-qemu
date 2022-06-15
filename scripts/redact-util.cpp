#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cassert>
#include <cstdint>

constexpr int FRUID_EEPROM_SIZE = 512;
constexpr int AREA_COUNT = 5;
constexpr int AREA_ARRAY_MAX_COUNT = 10;
constexpr int AREA_ARRAY_OFFSET[AREA_COUNT] = {0, 3, 6, 3, 0};
constexpr bool REDACTED[AREA_COUNT][AREA_ARRAY_MAX_COUNT] = {
    {},
    {},
    {1, 0, 1, 1, 0, 1, 1},
    {1, 0, 1, 0, 1, 1, 0, 1},
    {},
};

struct CommonHeader {
    uint8_t version; // 0x01
    // Offsets encoded as multiples of 8 bytes.
    uint8_t area_offset[AREA_COUNT];
    uint8_t pad; // 0x00
    uint8_t checksum;
};

static uint8_t checksum(const uint8_t *data, int len)
{
    uint8_t sum = 0;
    for (int i = 0; i < len; i++) {
        sum += data[i];
    }
    return sum;
}

static void print_usage_and_exit() {
    printf("USAGE: redact-util FILE\n");
    exit(1);
}

int main(int argc, char **argv) {
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0) {
            print_usage_and_exit();
        }
    }
    if (argc < 2) {
        print_usage_and_exit();
    }

    uint8_t buf[FRUID_EEPROM_SIZE];
    auto f = fopen(argv[1], "r");
    fread(buf, sizeof(buf), 1, f);
    fclose(f);

    const auto &common = *reinterpret_cast<const CommonHeader*>(buf);

    assert(common.version == 0x01);
    assert(common.pad == 0x00);
    assert(checksum(buf, sizeof(CommonHeader)) == 0);

    for (int i = 1; i < 5; i++) {
        uint8_t offset = common.area_offset[i] * 8;
        if (offset == 0) {
            continue;
        }
        uint8_t *area = &buf[offset];
        uint32_t len = static_cast<uint32_t>(area[1] & 0b111111) * 8;

        uint8_t *p = &area[AREA_ARRAY_OFFSET[i]];
        for (int j = 0; *p != 0xC1; j++) {
            assert(j < AREA_ARRAY_MAX_COUNT);
            uint32_t elem_len = static_cast<uint32_t>(*p & 0b111111);
            *p = elem_len | 0b11000000;
            p++;
            if (REDACTED[i][j]) {
                memset(p, 'X', elem_len);
            }
            printf("%d %d %.*s\n", i, j, (int)elem_len, (const char *)p);
            p = &p[elem_len];
        }

        area[len - 1] = -checksum(area, len - 1);
        assert(checksum(area, len) == 0);
    }

    char path[256];
    snprintf(path, sizeof(path), "%s.redacted", argv[1]);
    f = fopen(path, "w");
    fwrite(buf, sizeof(buf), 1, f);
    fclose(f);
}
