/*
 * Copyright (C) 2019 Maciej Suminski <orson@orson.net.pl>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>

#include "delay_timer.h"
#include "servo.h"
#include "motor.h"
#include "serial.h"
#include "nrf24l.h"
#include "link.h"

int main(void)
{
    unsigned int last_packet_cnt = 0;
    struct packet radio_data;
    rcc_clock_setup_in_hsi_out_48mhz();
    /*rcc_clock_setup_in_hse_8mhz_out_72mhz();*/

    /* LED pin */
    rcc_periph_clock_enable(RCC_GPIOC);
    gpio_set_mode(GPIOC, GPIO_MODE_OUTPUT_2_MHZ,
              GPIO_CNF_OUTPUT_PUSHPULL, GPIO13);
    gpio_clear(GPIOC, GPIO13);

    delay_init();
    delay_ms(50);   // perhaps nrf24l needs more time to complete reset?

    serial_init(9600);
    servo_init();
    motor_init();
    nrf24l_init();

    while (1)
    {
        if (nrf24l_copy_buffer((uint8_t*) &radio_data, PACKET_TOTAL_SIZE))
        {
            unsigned int speed = 2500 - 2500 * radio_data.adc2 / 4096;

            if (speed > 5)  // prevent high pitch noise on low PWM duty factor
                speed += 1500;

            if ((radio_data.buttons & 0x10) == 0x00)
            {
                // reverse
                motor_run(REVERSE, speed);
            }
            else
            {
                // forward
                motor_run(FORWARD, speed);
            }

            servo_set(MAX_POSITION - (radio_data.adc1 * (MAX_POSITION - MIN_POSITION)) / 4096);
            last_packet_cnt = 0;
        }
        else
        {
            ++last_packet_cnt;

            if (last_packet_cnt == 100000)
            {
                // disable the main motor when packets do not arrive for some time
                motor_stop();
            }
            else if (last_packet_cnt == 1000000)
            {
                nrf24l_init();
                last_packet_cnt = 0;
            }
        }
    }
}
