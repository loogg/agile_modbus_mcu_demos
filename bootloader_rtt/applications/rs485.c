#include "rs485.h"
#include <board.h>

#define DBG_TAG "rs485"
#define DBG_LVL DBG_INFO
#include <rtdbg.h>

#define RS485_RE_PIN GET_PIN(G, 8)
#define RS485_TX_EN() rt_pin_write(RS485_RE_PIN, PIN_HIGH)
#define RS485_RX_EN() rt_pin_write(RS485_RE_PIN, PIN_LOW)

static rt_device_t _dev = RT_NULL;
static rt_sem_t _rx_sem = RT_NULL;

static rt_err_t rs485_ind_cb(rt_device_t dev, rt_size_t size)
{
    rt_sem_release(_rx_sem);

    return RT_EOK;
}

int rs485_send(uint8_t *buf, int len)
{
    RS485_TX_EN();
    rt_device_write(_dev, 0, buf, len);
    RS485_RX_EN();

    return len;
}

int rs485_receive(uint8_t *buf, int bufsz, int timeout, int bytes_timeout)
{
    int len = 0;

    while(1)
    {
        rt_sem_control(_rx_sem, RT_IPC_CMD_RESET, RT_NULL);

        int rc = rt_device_read(_dev, 0, buf + len, bufsz);
        if(rc > 0)
        {
            timeout = bytes_timeout;
            len += rc;
            bufsz -= rc;
            if(bufsz == 0)
                break;

            continue;
        }

        if(rt_sem_take(_rx_sem, rt_tick_from_millisecond(timeout)) != RT_EOK)
            break;
        timeout = bytes_timeout;
    }

    return len;
}

int rs485_init(void)
{
    rt_pin_mode(RS485_RE_PIN, PIN_MODE_OUTPUT);
    RS485_RX_EN();

    _rx_sem = rt_sem_create("rs485", 0, RT_IPC_FLAG_FIFO);
    if(_rx_sem == RT_NULL)
    {
        LOG_E("create rx_sem failed.");
        return -RT_ERROR;
    }

    _dev = rt_device_find("uart2");
    if (_dev == RT_NULL)
    {
        LOG_E("can't find device uart2.");
        rt_sem_delete(_rx_sem);
        return -RT_ERROR;
    }

    rt_device_set_rx_indicate(_dev, rs485_ind_cb);
    rt_device_open(_dev, RT_DEVICE_OFLAG_RDWR | RT_DEVICE_FLAG_INT_RX);

    return RT_EOK;
}
