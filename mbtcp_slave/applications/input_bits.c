#include <agile_modbus.h>
#include <agile_modbus_slave_util.h>
#include <string.h>

static uint8_t _tab_input_bits[10] = {0, 1, 1, 0, 0, 1, 1, 0, 0, 1};

static int get_map_buf(void *buf, int bufsz)
{
    uint8_t *ptr = (uint8_t *)buf;

    for (int i = 0; i < sizeof(_tab_input_bits); i++) {
        ptr[i] = _tab_input_bits[i];
    }

    return 0;
}

const agile_modbus_slave_util_map_t input_bit_maps[1] = {
    {0x041A, 0x0423, get_map_buf, NULL}};
