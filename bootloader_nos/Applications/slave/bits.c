#include <stdint.h>
#include <string.h>
#include "agile_modbus.h"
#include "agile_modbus_slave_util.h"

static uint8_t _tab_bits[10] = {0, 1, 0, 1, 0, 1, 0, 1, 0, 1};

static int get_map_buf(void *buf, int bufsz)
{
    uint8_t *ptr = (uint8_t *)buf;

    for (int i = 0; i < sizeof(_tab_bits); i++) {
        ptr[i] = _tab_bits[i];
    }

    return 0;
}

static int set_map_buf(int index, int len, void *buf, int bufsz)
{
    uint8_t *ptr = (uint8_t *)buf;

    for (int i = 0; i < len; i++) {
        _tab_bits[index + i] = ptr[index + i];
    }

    return 0;
}

const agile_modbus_slave_util_map_t bit_maps[1] = {
    {0x041A, 0x0423, get_map_buf, set_map_buf}};
