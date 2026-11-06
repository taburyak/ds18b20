/**
 * |----------------------------------------------------------------------
 * | Copyright (C) Tilen Majerle, 2014
 * |
 * | This program is free software: you can redistribute it and/or modify
 * | it under the terms of the GNU General Public License as published by
 * | the Free Software Foundation, either version 3 of the License, or
 * | any later version.
 * |
 * | This program is distributed in the hope that it will be useful,
 * | but WITHOUT ANY WARRANTY; without even the implied warranty of
 * | MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * | GNU General Public License for more details.
 * |
 * | You should have received a copy of the GNU General Public License
 * | along with this program.  If not, see <http://www.gnu.org/licenses/>.
 * |----------------------------------------------------------------------
 */
#include "main.h"
#include "onewire.h"
#include "ds18b20Config.h"

void ONEWIRE_DELAY(uint16_t time_us)
{
    /* Delta-based delay on 32-bit TIM5 (never zeroes CNT, preserving FreeRTOS RunTimeStats) */
    uint32_t start = _DS18B20_TIMER.Instance->CNT;
    uint32_t ticks = (uint32_t)time_us * _DS18B20_TIMER_TICKS_PER_US;
    while ((_DS18B20_TIMER.Instance->CNT - start) < ticks)
    {
    }
}

void ONEWIRE_LOW(OneWire_t *gp)
{
    gp->GPIOx->BSRR = (uint32_t)gp->GPIO_Pin << 16U;
}

void ONEWIRE_HIGH(OneWire_t *gp)
{
    gp->GPIOx->BSRR = gp->GPIO_Pin;
}

void ONEWIRE_INPUT(OneWire_t *gp)
{
    /* In Open-Drain mode with internal pull-up, releasing the line high is sufficient to read IDR */
    gp->GPIOx->BSRR = gp->GPIO_Pin;
}

void ONEWIRE_OUTPUT(OneWire_t *gp)
{
    (void)gp;
}

void OneWire_Init(OneWire_t* OneWireStruct, GPIO_TypeDef* GPIOx, uint16_t GPIO_Pin) 
{
    HAL_TIM_Base_Start(&_DS18B20_TIMER);

    OneWireStruct->GPIOx = GPIOx;
    OneWireStruct->GPIO_Pin = GPIO_Pin;

    GPIO_InitTypeDef gpinit = {0};
    gpinit.Mode  = GPIO_MODE_OUTPUT_OD;
    gpinit.Pull  = GPIO_PULLUP;
    gpinit.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    gpinit.Pin   = GPIO_Pin;
    HAL_GPIO_Init(GPIOx, &gpinit);

    /* Release line high and wait at least 2 ms so DS18B20 exits any prior reset state */
    ONEWIRE_HIGH(OneWireStruct);
    ONEWIRE_DELAY(2000);
}

uint8_t OneWire_Reset(OneWire_t* OneWireStruct)
{
    uint8_t i = 1U;

    /* Line low, and wait 480us */
    ONEWIRE_LOW(OneWireStruct);
    ONEWIRE_DELAY(480);

    /* Release line and poll for low presence pulse across the 20..120 us window */
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    ONEWIRE_HIGH(OneWireStruct);
    ONEWIRE_DELAY(20);
    for (uint8_t k = 0U; k < 20U; k++)
    {
        if (HAL_GPIO_ReadPin(OneWireStruct->GPIOx, OneWireStruct->GPIO_Pin) == GPIO_PIN_RESET)
        {
            i = 0U;
            break;
        }
        ONEWIRE_DELAY(5);
    }
    if (primask == 0U)
    {
        __enable_irq();
    }

    /* Delay for 410 us */
    ONEWIRE_DELAY(410);
    /* Return value of presence pulse, 0 = OK, 1 = ERROR */
    return i;
}

void OneWire_WriteBit(OneWire_t* OneWireStruct, uint8_t bit)
{
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    if (bit)
    {
        /* Set line low for 6 us, release for 64 us */
        ONEWIRE_LOW(OneWireStruct);
        ONEWIRE_DELAY(6);
        ONEWIRE_HIGH(OneWireStruct);
        ONEWIRE_DELAY(64);
    }
    else
    {
        /* Set line low for 60 us, release for 10 us */
        ONEWIRE_LOW(OneWireStruct);
        ONEWIRE_DELAY(60);
        ONEWIRE_HIGH(OneWireStruct);
        ONEWIRE_DELAY(10);
    }
    if (primask == 0U)
    {
        __enable_irq();
    }
}

uint8_t OneWire_ReadBit(OneWire_t* OneWireStruct)
{
    uint8_t bit = 0;
    uint32_t primask = __get_PRIMASK();
    __disable_irq();

    /* Line low for 3 us, release and sample at ~12 us from slot start */
    ONEWIRE_LOW(OneWireStruct);
    ONEWIRE_DELAY(3);
    ONEWIRE_HIGH(OneWireStruct);
    ONEWIRE_DELAY(10);

    if (HAL_GPIO_ReadPin(OneWireStruct->GPIOx, OneWireStruct->GPIO_Pin))
    {
        bit = 1;
    }
    if (primask == 0U)
    {
        __enable_irq();
    }

    /* Wait 50us to complete 60us period */
    ONEWIRE_DELAY(50);
    return bit;
}

void OneWire_WriteByte(OneWire_t* OneWireStruct, uint8_t byte) {
    uint8_t i = 8;
    /* Write 8 bits */
    while (i--) {
        /* LSB bit is first */
        OneWire_WriteBit(OneWireStruct, byte & 0x01);
        byte >>= 1;
    }
}

uint8_t OneWire_ReadByte(OneWire_t* OneWireStruct) {
    uint8_t i = 8, byte = 0;
    while (i--) {
        byte >>= 1;
        byte |= (OneWire_ReadBit(OneWireStruct) << 7);
    }

    return byte;
}

uint8_t OneWire_First(OneWire_t* OneWireStruct) {
    /* Reset search values */
    OneWire_ResetSearch(OneWireStruct);

    /* Start with searching */
    return OneWire_Search(OneWireStruct, ONEWIRE_CMD_SEARCHROM);
}

uint8_t OneWire_Next(OneWire_t* OneWireStruct) {
   /* Leave the search state alone */
   return OneWire_Search(OneWireStruct, ONEWIRE_CMD_SEARCHROM);
}

void OneWire_ResetSearch(OneWire_t* OneWireStruct) {
    /* Reset the search state */
    OneWireStruct->LastDiscrepancy = 0;
    OneWireStruct->LastDeviceFlag = 0;
    OneWireStruct->LastFamilyDiscrepancy = 0;
}

uint8_t OneWire_Search(OneWire_t* OneWireStruct, uint8_t command) {
    uint8_t id_bit_number;
    uint8_t last_zero, rom_byte_number, search_result;
    uint8_t id_bit, cmp_id_bit;
    uint8_t rom_byte_mask, search_direction;

    /* Initialize for search */
    id_bit_number = 1;
    last_zero = 0;
    rom_byte_number = 0;
    rom_byte_mask = 1;
    search_result = 0;

    // if the last call was not the last one
    if (!OneWireStruct->LastDeviceFlag)
    {
        // 1-Wire reset
        if (OneWire_Reset(OneWireStruct))
        {
            /* Reset the search */
            OneWireStruct->LastDiscrepancy = 0;
            OneWireStruct->LastDeviceFlag = 0;
            OneWireStruct->LastFamilyDiscrepancy = 0;
            return 0;
        }

        // issue the search command
        OneWire_WriteByte(OneWireStruct, command);

        // loop to do the search
        do {
            // read a bit and its complement
            id_bit = OneWire_ReadBit(OneWireStruct);
            cmp_id_bit = OneWire_ReadBit(OneWireStruct);

            // check for no devices on 1-wire
            if ((id_bit == 1) && (cmp_id_bit == 1)) {
                break;
            } else {
                // all devices coupled have 0 or 1
                if (id_bit != cmp_id_bit) {
                    search_direction = id_bit;  // bit write value for search
                } else {
                    // if this discrepancy if before the Last Discrepancy
                    // on a previous next then pick the same as last time
                    if (id_bit_number < OneWireStruct->LastDiscrepancy) {
                        search_direction = ((OneWireStruct->ROM_NO[rom_byte_number] & rom_byte_mask) > 0);
                    } else {
                        // if equal to last pick 1, if not then pick 0
                        search_direction = (id_bit_number == OneWireStruct->LastDiscrepancy);
                    }

                    // if 0 was picked then record its position in LastZero
                    if (search_direction == 0) {
                        last_zero = id_bit_number;

                        // check for Last discrepancy in family
                        if (last_zero < 9) {
                            OneWireStruct->LastFamilyDiscrepancy = last_zero;
                        }
                    }
                }

                // set or clear the bit in the ROM byte rom_byte_number
                // with mask rom_byte_mask
                if (search_direction == 1) {
                    OneWireStruct->ROM_NO[rom_byte_number] |= rom_byte_mask;
                } else {
                    OneWireStruct->ROM_NO[rom_byte_number] &= ~rom_byte_mask;
                }

                // serial number search direction write bit
                OneWire_WriteBit(OneWireStruct, search_direction);

                // increment the byte counter id_bit_number
                // and shift the mask rom_byte_mask
                id_bit_number++;
                rom_byte_mask <<= 1;

                // if the mask is 0 then go to new SerialNum byte rom_byte_number and reset mask
                if (rom_byte_mask == 0) {
                    //docrc8(ROM_NO[rom_byte_number]);  // accumulate the CRC
                    rom_byte_number++;
                    rom_byte_mask = 1;
                }
            }
        } while (rom_byte_number < 8);  // loop until through all ROM bytes 0-7

        // if the search was successful then
        if (!(id_bit_number < 65)) {
            // search successful so set LastDiscrepancy,LastDeviceFlag,search_result
            OneWireStruct->LastDiscrepancy = last_zero;

            // check for last device
            if (OneWireStruct->LastDiscrepancy == 0) {
                OneWireStruct->LastDeviceFlag = 1;
            }

            search_result = 1;
        }
    }

    // if no device found then reset counters so next 'search' will be like a first
    if (!search_result || !OneWireStruct->ROM_NO[0]) {
        OneWireStruct->LastDiscrepancy = 0;
        OneWireStruct->LastDeviceFlag = 0;
        OneWireStruct->LastFamilyDiscrepancy = 0;
        search_result = 0;
    }

    return search_result;
}

int OneWire_Verify(OneWire_t* OneWireStruct) {
    unsigned char rom_backup[8];
    int i,rslt,ld_backup,ldf_backup,lfd_backup;

    // keep a backup copy of the current state
    for (i = 0; i < 8; i++)
    rom_backup[i] = OneWireStruct->ROM_NO[i];
    ld_backup = OneWireStruct->LastDiscrepancy;
    ldf_backup = OneWireStruct->LastDeviceFlag;
    lfd_backup = OneWireStruct->LastFamilyDiscrepancy;

    // set search to find the same device
    OneWireStruct->LastDiscrepancy = 64;
    OneWireStruct->LastDeviceFlag = 0;

    if (OneWire_Search(OneWireStruct, ONEWIRE_CMD_SEARCHROM)) {
        // check if same device found
        rslt = 1;
        for (i = 0; i < 8; i++) {
            if (rom_backup[i] != OneWireStruct->ROM_NO[i]) {
                rslt = 1;
                break;
            }
        }
    } else {
        rslt = 0;
    }

    // restore the search state
    for (i = 0; i < 8; i++) {
        OneWireStruct->ROM_NO[i] = rom_backup[i];
    }
    OneWireStruct->LastDiscrepancy = ld_backup;
    OneWireStruct->LastDeviceFlag = ldf_backup;
    OneWireStruct->LastFamilyDiscrepancy = lfd_backup;

    // return the result of the verify
    return rslt;
}

void OneWire_TargetSetup(OneWire_t* OneWireStruct, uint8_t family_code) {
   uint8_t i;

    // set the search state to find SearchFamily type devices
    OneWireStruct->ROM_NO[0] = family_code;
    for (i = 1; i < 8; i++) {
        OneWireStruct->ROM_NO[i] = 0;
    }

    OneWireStruct->LastDiscrepancy = 64;
    OneWireStruct->LastFamilyDiscrepancy = 0;
    OneWireStruct->LastDeviceFlag = 0;
}

void OneWire_FamilySkipSetup(OneWire_t* OneWireStruct) {
    // set the Last discrepancy to last family discrepancy
    OneWireStruct->LastDiscrepancy = OneWireStruct->LastFamilyDiscrepancy;
    OneWireStruct->LastFamilyDiscrepancy = 0;

    // check for end of list
    if (OneWireStruct->LastDiscrepancy == 0) {
        OneWireStruct->LastDeviceFlag = 1;
    }
}

uint8_t OneWire_GetROM(OneWire_t* OneWireStruct, uint8_t index) {
    return OneWireStruct->ROM_NO[index];
}

void OneWire_Select(OneWire_t* OneWireStruct, uint8_t* addr) {
    uint8_t i;
    OneWire_WriteByte(OneWireStruct, ONEWIRE_CMD_MATCHROM);

    for (i = 0; i < 8; i++) {
        OneWire_WriteByte(OneWireStruct, *(addr + i));
    }
}

void OneWire_SelectWithPointer(OneWire_t* OneWireStruct, uint8_t *ROM) {
    uint8_t i;
    OneWire_WriteByte(OneWireStruct, ONEWIRE_CMD_MATCHROM);

    for (i = 0; i < 8; i++) {
        OneWire_WriteByte(OneWireStruct, *(ROM + i));
    }
}

void OneWire_GetFullROM(OneWire_t* OneWireStruct, uint8_t *firstIndex) {
    uint8_t i;
    for (i = 0; i < 8; i++) {
        *(firstIndex + i) = OneWireStruct->ROM_NO[i];
    }
}

uint8_t OneWire_CRC8(uint8_t *addr, uint8_t len) {
    uint8_t crc = 0, inbyte, i, mix;

    while (len--) {
        inbyte = *addr++;
        for (i = 8; i; i--) {
            mix = (crc ^ inbyte) & 0x01;
            crc >>= 1;
            if (mix) {
                crc ^= 0x8C;
            }
            inbyte >>= 1;
        }
    }

    /* Return calculated CRC */
    return crc;
}

