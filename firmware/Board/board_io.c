#include "board_io.h"
#include "board_pins.h"
extern I2C_HandleTypeDef hi2c1;
static ADC_HandleTypeDef light_adc;

static bus_result_t result_of(HAL_StatusTypeDef result)
{
    if (result == HAL_OK) return BUS_OK;
    if (result == HAL_TIMEOUT) return BUS_TIMEOUT;
    if ((HAL_I2C_GetError(&hi2c1) & HAL_I2C_ERROR_AF) != 0) return BUS_NACK;
    return BUS_IO_ERROR;
}
static bus_result_t register_read(void *context, uint8_t address, uint8_t reg,
    uint8_t *data, size_t length)
{
    (void)context;
    return result_of(HAL_I2C_Mem_Read(&hi2c1, (uint16_t)address << 1, reg,
        I2C_MEMADD_SIZE_8BIT, data, (uint16_t)length, 10));
}
static bus_result_t register_write(void *context, uint8_t address, uint8_t reg,
    const uint8_t *data, size_t length)
{
    (void)context;
    return result_of(HAL_I2C_Mem_Write(&hi2c1, (uint16_t)address << 1, reg,
        I2C_MEMADD_SIZE_8BIT, (uint8_t *)data, (uint16_t)length, 10));
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
