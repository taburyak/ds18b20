#ifndef    _DS18B20CONFIG_H
#define    _DS18B20CONFIG_H

/*
 * Configured for STM32F407ZGT6 Console:
 *   - Uses 32-bit TIM5 running at 2 MHz (Prescaler = 83 on 84 MHz APB1 timer clock -> 0.5 us/tick)
 *   - Pin: PA15 (DS18B20_GPIO_Port / DS18B20_Pin)
 */
#define _DS18B20_USE_FREERTOS                       0
#define _DS18B20_MAX_SENSORS                        1
#define _DS18B20_GPIO                               DS18B20_GPIO_Port
#define _DS18B20_PIN                                DS18B20_Pin

#define _DS18B20_CONVERT_TIMEOUT_MS                 1000
#define DS18B20_USE_CRC                             1

#define _DS18B20_TIMER                              htim5
#define _DS18B20_TIMER_TICKS_PER_US                 2U

extern TIM_HandleTypeDef _DS18B20_TIMER;

#endif


