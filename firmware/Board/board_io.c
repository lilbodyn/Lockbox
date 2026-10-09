#include "board_io.h"
#include "board_pins.h"
extern I2C_HandleTypeDef hi2c1;
static ADC_HandleTypeDef light_adc;

static void byte_settle(void)
{
    /* A wire byte takes 9 / 100 kHz = 90 us. The simulator exposes TXIS
       before its queued transfer completes. Allow one byte plus 20 us.
       This is board adaptation, not a change to the sensor driver. */
    uint32_t before = SysTick->VAL, cycles = 0;
    while (cycles < 110U * 48U) {
        uint32_t after = SysTick->VAL;
        cycles += before >= after ? before - after : before + SysTick->LOAD + 1U - after;
        before = after;
    }
}
static bus_result_t wait_flag(uint32_t flag, uint32_t started)
{
    for (;;) {
        uint32_t flags = I2C1->ISR;
        if ((flags & I2C_ISR_NACKF) != 0) return BUS_NACK;
        if ((flags & (I2C_ISR_BERR | I2C_ISR_ARLO)) != 0) return BUS_IO_ERROR;
        if ((flags & flag) != 0) return BUS_OK;
        if (HAL_GetTick() - started >= 10) return BUS_TIMEOUT;
    }
}
static bus_result_t transfer(uint8_t address, uint8_t *data, size_t length, bool reading)
{
    if (length == 0 || length > 255) return BUS_IO_ERROR;
    I2C1->ICR = I2C_ICR_NACKCF | I2C_ICR_STOPCF | I2C_ICR_BERRCF | I2C_ICR_ARLOCF;
    uint32_t started = HAL_GetTick();
    I2C1->CR2 = ((uint32_t)address << 1) | ((uint32_t)length << 16) |
        I2C_CR2_AUTOEND | I2C_CR2_START | (reading ? I2C_CR2_RD_WRN : 0U);
    bus_result_t result = BUS_OK;
    if (!reading) byte_settle();
    for (size_t i = 0; i < length; ++i) {
        result = wait_flag(reading ? I2C_ISR_RXNE : I2C_ISR_TXIS, started);
        if (result != BUS_OK) break;
        if (reading) data[i] = (uint8_t)I2C1->RXDR;
        else { I2C1->TXDR = data[i]; byte_settle(); }
    }
    if (result == BUS_OK) result = wait_flag(I2C_ISR_STOPF, started);
    if (result != BUS_OK) I2C1->CR2 |= I2C_CR2_STOP;
    I2C1->ICR = I2C_ICR_NACKCF | I2C_ICR_STOPCF | I2C_ICR_BERRCF | I2C_ICR_ARLOCF;
    I2C1->CR2 = 0;
    return result;
}
static bus_result_t register_read(void *context, uint8_t address, uint8_t reg,
    uint8_t *data, size_t length)
{
    (void)context;
    bus_result_t result = transfer(address, &reg, 1, false);
    return result == BUS_OK ? transfer(address, data, length, true) : result;
}
static bus_result_t register_write(void *context, uint8_t address, uint8_t reg,
    const uint8_t *data, size_t length)
{
    (void)context;
    if (length != 1) return BUS_IO_ERROR;
    uint8_t frame[2] = {reg, data[0]};
    return transfer(address, frame, sizeof frame, false);
}
register_bus_t board_register_bus(void)
{
    register_bus_t bus = {NULL, register_read, register_write};
    return bus;
}
bool board_io_start(void)
{
    GPIO_InitTypeDef pin = {0};
    __HAL_RCC_ADC_CLK_ENABLE();
    pin.Pin = LB_LDR_PIN;
    pin.Mode = GPIO_MODE_ANALOG;
    pin.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(LB_LDR_PORT, &pin);
    pin.Pin = LB_BUTTON_PIN;
    pin.Mode = GPIO_MODE_INPUT;
    pin.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(LB_BUTTON_PORT, &pin);
    pin.Pin = LB_PIR_PIN;
    pin.Pull = GPIO_PULLDOWN;
    HAL_GPIO_Init(LB_PIR_PORT, &pin);
    pin.Pin = LB_ADC_MARK_PIN;
    pin.Mode = GPIO_MODE_OUTPUT_PP;
    pin.Pull = GPIO_NOPULL;
    pin.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(LB_ADC_MARK_PORT, &pin);
    pin.Pin = LB_ALARM_PIN;
    HAL_GPIO_Init(LB_ALARM_PORT, &pin);
    board_alarm(false);
    light_adc.Instance = ADC1;
    light_adc.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
    light_adc.Init.Resolution = ADC_RESOLUTION_12B;
    light_adc.Init.DataAlign = ADC_DATAALIGN_RIGHT;
    light_adc.Init.ScanConvMode = ADC_SCAN_DISABLE;
    light_adc.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
    light_adc.Init.NbrOfConversion = 1;
    light_adc.Init.ExternalTrigConv = ADC_SOFTWARE_START;
    light_adc.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
    light_adc.Init.Overrun = ADC_OVR_DATA_OVERWRITTEN;
    light_adc.Init.SamplingTimeCommon1 = ADC_SAMPLETIME_160CYCLES_5;
    light_adc.Init.SamplingTimeCommon2 = ADC_SAMPLETIME_160CYCLES_5;
    light_adc.Init.TriggerFrequencyMode = ADC_TRIGGER_FREQ_LOW;
    if (HAL_ADC_Init(&light_adc) != HAL_OK) return false;
    ADC_ChannelConfTypeDef channel = {0};
    channel.Channel = ADC_CHANNEL_0;
    channel.Rank = ADC_REGULAR_RANK_1;
    channel.SamplingTime = ADC_SAMPLINGTIME_COMMON_1;
    if (HAL_ADC_ConfigChannel(&light_adc, &channel) != HAL_OK) return false;
    return HAL_ADCEx_Calibration_Start(&light_adc) == HAL_OK;
}
bool board_adc_read(uint16_t *counts)
{
    HAL_GPIO_WritePin(LB_ADC_MARK_PORT, LB_ADC_MARK_PIN, GPIO_PIN_SET);
    bool good = HAL_ADC_Start(&light_adc) == HAL_OK;
    if (good) good = HAL_ADC_PollForConversion(&light_adc, 2) == HAL_OK;
    if (good) *counts = (uint16_t)HAL_ADC_GetValue(&light_adc);
    HAL_ADC_Stop(&light_adc);
    HAL_GPIO_WritePin(LB_ADC_MARK_PORT, LB_ADC_MARK_PIN, GPIO_PIN_RESET);
    return good;
}
bool board_button_pressed(void)
{
    return HAL_GPIO_ReadPin(LB_BUTTON_PORT, LB_BUTTON_PIN) == GPIO_PIN_RESET;
}
bool board_pir_level(void)
{
    return HAL_GPIO_ReadPin(LB_PIR_PORT, LB_PIR_PIN) == GPIO_PIN_SET;
}
void board_alarm(bool active)
{
    HAL_GPIO_WritePin(LB_ALARM_PORT, LB_ALARM_PIN, active ? GPIO_PIN_SET : GPIO_PIN_RESET);
}
