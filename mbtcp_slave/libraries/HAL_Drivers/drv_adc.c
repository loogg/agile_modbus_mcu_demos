/*
 * Copyright (c) 2006-2022, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2018-12-05     zylx         first version
 * 2018-12-12     greedyhao    Porting for stm32f7xx
 * 2019-02-01     yuneizhilin  fix the stm32_adc_init function initialization issue
 * 2020-06-17     thread-liu   Porting for stm32mp1xx
 * 2020-10-14     Dozingfiretruck   Porting for stm32wbxx
 * 2022-05-22     Stanley Lwin Add stm32_adc_get_vref
 */

#include <board.h>

#if defined(BSP_USING_ADC1) || defined(BSP_USING_ADC2) || defined(BSP_USING_ADC3)
#include "drv_config.h"

//#define DRV_DEBUG
#define LOG_TAG             "drv.adc"
#include <drv_log.h>

static ADC_HandleTypeDef adc_config[] =
{
#ifdef BSP_USING_ADC1
    ADC1_CONFIG,
#endif

#ifdef BSP_USING_ADC2
    ADC2_CONFIG,
#endif

#ifdef BSP_USING_ADC3
    ADC3_CONFIG,
#endif
};

struct stm32_adc
{
    ADC_HandleTypeDef ADC_Handler;
    struct rt_adc_device stm32_adc_device;
};

static struct stm32_adc stm32_adc_obj[sizeof(adc_config) / sizeof(adc_config[0])];

static rt_err_t stm32_adc_enabled(struct rt_adc_device *device, rt_uint32_t channel, rt_bool_t enabled)
{
    ADC_HandleTypeDef *stm32_adc_handler;
    RT_ASSERT(device != RT_NULL);
    stm32_adc_handler = device->parent.user_data;

    if (enabled)
    {
#if defined(SOC_SERIES_STM32L4) || defined(SOC_SERIES_STM32G0) || defined (SOC_SERIES_STM32MP1) || defined(SOC_SERIES_STM32H7) || defined (SOC_SERIES_STM32WB)
        ADC_Enable(stm32_adc_handler);
#else
        __HAL_ADC_ENABLE(stm32_adc_handler);
#endif
    }
    else
    {
#if defined(SOC_SERIES_STM32L4) || defined(SOC_SERIES_STM32G0) || defined (SOC_SERIES_STM32MP1) || defined(SOC_SERIES_STM32H7) || defined (SOC_SERIES_STM32WB)
        ADC_Disable(stm32_adc_handler);
#else
        __HAL_ADC_DISABLE(stm32_adc_handler);
#endif
    }

    return RT_EOK;
}

static rt_uint8_t stm32_adc_get_resolution(struct rt_adc_device *device)
{
#if defined(SOC_SERIES_STM32F1) || defined(SOC_SERIES_STM32F3)
    return 12;
#else
    ADC_HandleTypeDef *stm32_adc_handler = device->parent.user_data;

    RT_ASSERT(device != RT_NULL);

    switch(stm32_adc_handler->Init.Resolution)
    {
        case ADC_RESOLUTION_12B:
            return 12;
        case ADC_RESOLUTION_10B:
            return 10;
        case ADC_RESOLUTION_8B:
            return 8;
        case ADC_RESOLUTION_6B:
            return 6;
        default:
            return 0;
    }
#endif /* defined(SOC_SERIES_STM32F1) || defined(SOC_SERIES_STM32F3) */
}

static rt_uint32_t stm32_adc_get_channel(rt_uint32_t channel)
{
    rt_uint32_t stm32_channel = 0;

    switch (channel)
    {
    case  0:
        stm32_channel = ADC_CHANNEL_0;
        break;
    case  1:
        stm32_channel = ADC_CHANNEL_1;
        break;
    case  2:
        stm32_channel = ADC_CHANNEL_2;
        break;
    case  3:
        stm32_channel = ADC_CHANNEL_3;
        break;
    case  4:
        stm32_channel = ADC_CHANNEL_4;
        break;
    case  5:
        stm32_channel = ADC_CHANNEL_5;
        break;
    case  6:
        stm32_channel = ADC_CHANNEL_6;
        break;
    case  7:
        stm32_channel = ADC_CHANNEL_7;
        break;
    case  8:
        stm32_channel = ADC_CHANNEL_8;
        break;
    case  9:
        stm32_channel = ADC_CHANNEL_9;
        break;
    case 10:
        stm32_channel = ADC_CHANNEL_10;
        break;
    case 11:
        stm32_channel = ADC_CHANNEL_11;
        break;
    case 12:
        stm32_channel = ADC_CHANNEL_12;
        break;
    case 13:
        stm32_channel = ADC_CHANNEL_13;
        break;
    case 14:
        stm32_channel = ADC_CHANNEL_14;
        break;
    case 15:
        stm32_channel = ADC_CHANNEL_15;
        break;
#ifdef ADC_CHANNEL_16
    case 16:
        stm32_channel = ADC_CHANNEL_16;
        break;
#endif
    case 17:
        stm32_channel = ADC_CHANNEL_17;
        break;
#ifdef ADC_CHANNEL_18
    case 18:
        stm32_channel = ADC_CHANNEL_18;
        break;
#endif
#ifdef ADC_CHANNEL_19
    case 19:
        stm32_channel = ADC_CHANNEL_19;
        break;
#endif
    }

    return stm32_channel;
}

static rt_int16_t stm32_adc_get_vref (struct rt_adc_device *device)
{
    RT_ASSERT(device);
    return 3300;
}

static rt_err_t stm32_adc_get_value(struct rt_adc_device *device, rt_uint32_t channel, rt_uint32_t *value)
{
    ADC_ChannelConfTypeDef ADC_ChanConf;
    ADC_HandleTypeDef *stm32_adc_handler;

    RT_ASSERT(device != RT_NULL);
    RT_ASSERT(value != RT_NULL);

    stm32_adc_handler = device->parent.user_data;

    rt_memset(&ADC_ChanConf, 0, sizeof(ADC_ChanConf));

#ifndef ADC_CHANNEL_16
    if (channel == 16)
    {
        LOG_E("ADC channel must not be 16.");
        return -RT_ERROR;
    }
#endif

/* ADC channel number is up to 17 */
#if !defined(ADC_CHANNEL_18)
    if (channel <= 17)
/* ADC channel number is up to 19 */
#elif defined(ADC_CHANNEL_19)
    if (channel <= 19)
/* ADC channel number is up to 18 */
#else
    if (channel <= 18)
#endif
    {
        /* set stm32 ADC channel */
        ADC_ChanConf.Channel =  stm32_adc_get_channel(channel);
    }
    else
    {
#if !defined(ADC_CHANNEL_18)
        LOG_E("ADC channel must be between 0 and 17.");
#elif defined(ADC_CHANNEL_19)
        LOG_E("ADC channel must be between 0 and 19.");
#else
        LOG_E("ADC channel must be between 0 and 18.");
#endif
        return -RT_ERROR;
    }

#if defined(SOC_SERIES_STM32MP1) || defined (SOC_SERIES_STM32H7) || defined (SOC_SERIES_STM32WB)
    ADC_ChanConf.Rank = ADC_REGULAR_RANK_1;
#else
    ADC_ChanConf.Rank = 1;
#endif

#if defined(SOC_SERIES_STM32F0)
    ADC_ChanConf.SamplingTime = ADC_SAMPLETIME_71CYCLES_5;
#elif defined(SOC_SERIES_STM32F1)
    ADC_ChanConf.SamplingTime = ADC_SAMPLETIME_55CYCLES_5;
#elif defined(SOC_SERIES_STM32F2) || defined(SOC_SERIES_STM32F4) || defined(SOC_SERIES_STM32F7)
    ADC_ChanConf.SamplingTime = ADC_SAMPLETIME_112CYCLES;
#elif defined(SOC_SERIES_STM32L4)
    ADC_ChanConf.SamplingTime = ADC_SAMPLETIME_247CYCLES_5;
#elif defined(SOC_SERIES_STM32MP1)
    ADC_ChanConf.SamplingTime = ADC_SAMPLETIME_810CYCLES_5;
#elif defined(SOC_SERIES_STM32H7)
    ADC_ChanConf.SamplingTime = ADC_SAMPLETIME_64CYCLES_5;
    #elif defined (SOC_SERIES_STM32WB)
    ADC_ChanConf.SamplingTime = ADC_SAMPLETIME_2CYCLES_5;
#endif

#if defined(SOC_SERIES_STM32F2) || defined(SOC_SERIES_STM32F4) || defined(SOC_SERIES_STM32F7) || defined(SOC_SERIES_STM32L4) || defined (SOC_SERIES_STM32WB)
    ADC_ChanConf.Offset = 0;
#endif

#if defined(SOC_SERIES_STM32L4)
    ADC_ChanConf.OffsetNumber = ADC_OFFSET_NONE;
    ADC_ChanConf.SingleDiff = LL_ADC_SINGLE_ENDED;
#elif defined(SOC_SERIES_STM32MP1) || defined(SOC_SERIES_STM32H7) || defined (SOC_SERIES_STM32WB)
    ADC_ChanConf.OffsetNumber = ADC_OFFSET_NONE;  /* ADC channel affected to offset number */
    ADC_ChanConf.Offset       = 0;
    ADC_ChanConf.SingleDiff   = ADC_SINGLE_ENDED; /* ADC channel differential mode */
#endif
    HAL_ADC_ConfigChannel(stm32_adc_handler, &ADC_ChanConf);

    /* perform an automatic ADC calibration to improve the conversion accuracy */
#if defined(SOC_SERIES_STM32L4) || defined (SOC_SERIES_STM32WB)
    if (HAL_ADCEx_Calibration_Start(stm32_adc_handler, ADC_ChanConf.SingleDiff) != HAL_OK)
    {
        LOG_E("ADC calibration error!\n");
        return -RT_ERROR;
    }
#elif defined(SOC_SERIES_STM32MP1) || defined(SOC_SERIES_STM32H7)
    /* Run the ADC linear calibration in single-ended mode */
    if (HAL_ADCEx_Calibration_Start(stm32_adc_handler, ADC_CALIB_OFFSET_LINEARITY, ADC_ChanConf.SingleDiff) != HAL_OK)
    {
        LOG_E("ADC open linear calibration error!\n");
        /* Calibration Error */
        return -RT_ERROR;
    }
#endif
    /* start ADC */
    HAL_ADC_Start(stm32_adc_handler);

    /* Wait for the ADC to convert */
    HAL_ADC_PollForConversion(stm32_adc_handler, 100);

    /* get ADC value */
    *value = (rt_uint32_t)HAL_ADC_GetValue(stm32_adc_handler);

    return RT_EOK;
}

static const struct rt_adc_ops stm_adc_ops =
{
    .enabled = stm32_adc_enabled,
    .convert = stm32_adc_get_value,
    .get_resolution = stm32_adc_get_resolution,
    .get_vref = stm32_adc_get_vref,
};

static int stm32_adc_init(void)
{
    int result = RT_EOK;
    /* save adc name */
    char name_buf[5] = {'a', 'd', 'c', '0', 0};
    int i = 0;

    for (i = 0; i < sizeof(adc_config) / sizeof(adc_config[0]); i++)
    {
        /* ADC init */
        name_buf[3] = '0';
        stm32_adc_obj[i].ADC_Handler = adc_config[i];
#if defined(ADC1)
        if (stm32_adc_obj[i].ADC_Handler.Instance == ADC1)
        {
            name_buf[3] = '1';
        }
#endif
#if defined(ADC2)
        if (stm32_adc_obj[i].ADC_Handler.Instance == ADC2)
        {
            name_buf[3] = '2';
        }
#endif
#if defined(ADC3)
        if (stm32_adc_obj[i].ADC_Handler.Instance == ADC3)
        {
            name_buf[3] = '3';
        }
#endif
        if (HAL_ADC_Init(&stm32_adc_obj[i].ADC_Handler) != HAL_OK)
        {
            LOG_E("%s init failed", name_buf);
            result = -RT_ERROR;
        }
        else
        {
            /* register ADC device */
            if (rt_hw_adc_register(&stm32_adc_obj[i].stm32_adc_device, name_buf, &stm_adc_ops, &stm32_adc_obj[i].ADC_Handler) == RT_EOK)
            {
                LOG_D("%s init success", name_buf);
            }
            else
            {
                LOG_E("%s register failed", name_buf);
                result = -RT_ERROR;
            }
        }
    }

    return result;
}
INIT_BOARD_EXPORT(stm32_adc_init);

#endif /* BSP_USING_ADC */
