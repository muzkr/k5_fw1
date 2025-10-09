/* Copyright 2023 muzkr
 * https://github.com/muzkr
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 *     Unless required by applicable law or agreed to in writing, software
 *     distributed under the License is distributed on an "AS IS" BASIS,
 *     WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *     See the License for the specific language governing permissions and
 *     limitations under the License.
 */

#include "driver/board.h"
#include "driver/st7565.h"
#include "driver/bk1080.h"
#include "driver/crc.h"
#include "driver/system.h"
#include "driver/systick.h"

#include "py32f0xx_ll_bus.h"
#include "py32f0xx_ll_rcc.h"
#include "py32f0xx_ll_gpio.h"
#include "py32f0xx_ll_flash.h"
#include "py32f0xx_ll_utils.h"
#include "py32f0xx_ll_system.h"
#include "py32f0xx_ll_adc.h"

static void APP_SystemClockConfig(void);
static void BOARD_GPIO_Init(void);
static void BOARD_ADC_Init(void);

void BOARD_ADC_GetBatteryInfo(uint16_t *pVoltage, uint16_t *pCurrent)
{
    static const uint32_t CHANNELS[2] = {LL_ADC_CHANNEL_4, LL_ADC_CHANNEL_9};

    uint16_t Data[2];
    for (int i = 0; i < 2; i++)
    {
        LL_ADC_REG_SetSequencerChannels(ADC1, CHANNELS[i]);
        LL_ADC_Enable(ADC1);
        SYSTEM_DelayMs(1);
        LL_ADC_REG_StartConversion(ADC1);
        while (!LL_ADC_IsActiveFlag_EOC(ADC1))
            ;
        LL_ADC_ClearFlag_EOC(ADC1);
        Data[i] = LL_ADC_REG_ReadConversionData12(ADC1);
    }

    *pVoltage = Data[0];
    *pCurrent = Data[1];
}

void BOARD_Init(void)
{

    /* Enable SYSCFG and PWR clocks */
    LL_APB1_GRP2_EnableClock(LL_APB1_GRP2_PERIPH_SYSCFG);
    LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_PWR);

    /* Configure system clock */
    APP_SystemClockConfig();

    SYSTICK_Init();

    BOARD_GPIO_Init();
    BOARD_ADC_Init();
    ST7565_Init();
#if defined(ENABLE_FMRADIO)
    BK1080_Init(0, false);
#endif
#if defined(ENABLE_AIRCOPY) || defined(ENABLE_UART)
    CRC_Init();
#endif
}

/**
 * @brief  System Clock Configuration
 * @param  None
 * @retval None
 */
static void APP_SystemClockConfig(void)
{
    /* Enable HSI */
    LL_RCC_HSI_Enable();
    LL_RCC_HSI_SetCalibFreq(LL_RCC_HSICALIBRATION_24MHz);
    while (LL_RCC_HSI_IsReady() != 1)
    {
    }

    /* Set AHB prescaler */
    LL_RCC_SetAHBPrescaler(LL_RCC_SYSCLK_DIV_1);

    /* Configure HSISYS as system clock source */
    LL_RCC_SetSysClkSource(LL_RCC_SYS_CLKSOURCE_HSISYS);
    while (LL_RCC_GetSysClkSource() != LL_RCC_SYS_CLKSOURCE_STATUS_HSISYS)
    {
    }

    LL_FLASH_SetLatency(LL_FLASH_LATENCY_0);

    /* Set APB1 prescaler*/
    LL_RCC_SetAPB1Prescaler(LL_RCC_APB1_DIV_1);
    // LL_Init1msTick(24000000);

    /* Update system clock global variable SystemCoreClock (can also be updated by calling SystemCoreClockUpdate function) */
    LL_SetSystemCoreClock(24000000);
}

static void BOARD_GPIO_Init(void)
{
    LL_IOP_GRP1_EnableClock(LL_IOP_GRP1_PERIPH_GPIOA | LL_IOP_GRP1_PERIPH_GPIOB | LL_IOP_GRP1_PERIPH_GPIOF);

    LL_GPIO_InitTypeDef GPIO_InitStruct;
    LL_GPIO_StructInit(&GPIO_InitStruct);
    GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
    GPIO_InitStruct.Speed = LL_GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Pull = LL_GPIO_PULL_UP;

    // ------------------

    GPIO_InitStruct.Mode = LL_GPIO_MODE_OUTPUT;
    GPIO_InitStruct.Pin = LL_GPIO_PIN_15   //
                          | LL_GPIO_PIN_13 //
                          | LL_GPIO_PIN_12 //
                          | LL_GPIO_PIN_11 //
                          | LL_GPIO_PIN_10 //
                          | LL_GPIO_PIN_9  //
                          | LL_GPIO_PIN_8  //
                          | LL_GPIO_PIN_7  //
                          | LL_GPIO_PIN_6  //
                          | LL_GPIO_PIN_5  //
        ;

    LL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    // ------------------

    GPIO_InitStruct.Mode = LL_GPIO_MODE_OUTPUT;
    GPIO_InitStruct.Pin = LL_GPIO_PIN_7   //
                          | LL_GPIO_PIN_6 //
                          | LL_GPIO_PIN_5 //
                          | LL_GPIO_PIN_4 //
                          | LL_GPIO_PIN_3 //
                          | LL_GPIO_PIN_0 //
        ;

    LL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    // ------------------

    GPIO_InitStruct.Mode = LL_GPIO_MODE_INPUT;
    GPIO_InitStruct.Pin = LL_GPIO_PIN_1   //
                          | LL_GPIO_PIN_0 //
        ;

    LL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    // ------------------

    GPIO_InitStruct.Mode = LL_GPIO_MODE_INPUT;
    GPIO_InitStruct.Pin = LL_GPIO_PIN_1   //
                          | LL_GPIO_PIN_0 //
        ;

    LL_GPIO_Init(GPIOF, &GPIO_InitStruct);

    // ------------------

    LL_GPIO_SetOutputPin(GPIOA, LL_GPIO_PIN_9 | LL_GPIO_PIN_10);

#if defined(ENABLE_FMRADIO)
    // GPIO_SetBit(&GPIOB->DATA, GPIOB_PIN_BK1080);
    // TODO:
#endif
}

static void BOARD_ADC_Init(void)
{

    LL_ADC_Reset(ADC1);
    LL_APB1_GRP2_EnableClock(LL_APB1_GRP2_PERIPH_ADC1);

    LL_ADC_StartCalibration(ADC1);
    while (LL_ADC_IsCalibrationOnGoing(ADC1))
    {
        SYSTEM_DelayMs(10);
    }

    LL_GPIO_SetPinMode(GPIOA, LL_GPIO_PIN_4, LL_GPIO_MODE_ANALOG);
    LL_GPIO_SetPinMode(GPIOB, LL_GPIO_PIN_1, LL_GPIO_MODE_ANALOG);

    LL_ADC_SetCommonPathInternalCh(__LL_ADC_COMMON_INSTANCE(ADC1), LL_ADC_PATH_INTERNAL_NONE);

    LL_ADC_InitTypeDef ADC_InitStruct;
    ADC_InitStruct.Clock = LL_ADC_CLOCK_SYNC_PCLK_DIV4;
    ADC_InitStruct.Resolution = LL_ADC_RESOLUTION_12B;
    ADC_InitStruct.DataAlignment = LL_ADC_DATA_ALIGN_RIGHT;
    ADC_InitStruct.LowPowerMode = LL_ADC_LP_AUTOWAIT;
    LL_ADC_Init(ADC1, &ADC_InitStruct);

    LL_ADC_SetSamplingTimeCommonChannels(ADC1, LL_ADC_SAMPLINGTIME_28CYCLES_5);
    LL_ADC_REG_SetTriggerSource(ADC1, LL_ADC_REG_TRIG_SOFTWARE);
    LL_ADC_REG_SetOverrun(ADC1, LL_ADC_REG_OVR_DATA_OVERWRITTEN);
    LL_ADC_REG_SetSequencerDiscont(ADC1, LL_ADC_REG_SEQ_DISCONT_DISABLE);
    LL_ADC_REG_SetContinuousMode(ADC1, LL_ADC_REG_CONV_SINGLE);
}
