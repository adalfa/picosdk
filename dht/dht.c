/**
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 **/

#include <stdio.h>
#include <math.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"

#ifdef CYW43_WL_GPIO_LED_PIN
#include "pico/cyw43_arch.h"
#endif

#ifdef PICO_DEFAULT_LED_PIN
#define LED_PIN PICO_DEFAULT_LED_PIN
#endif
#define MIN_INTERVAL 2000 /**< min interval value */
#define TIMEOUT    UINT32_MAX
const uint DHT_PIN = 22;

const uint LEDB_PIN = 15;
const uint MAX_TIMINGS = 85;

typedef struct {
    float humidity;
    float temp_celsius;
} dht_reading;

// Perform initialisation
int pico_led_init(void) {
    #if defined(PICO_DEFAULT_LED_PIN)
        // A device like Pico that uses a GPIO for the LED will define PICO_DEFAULT_LED_PIN
        // so we can use normal GPIO functionality to turn the led on and off
        gpio_init(PICO_DEFAULT_LED_PIN);
        gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
        return PICO_OK;
    #elif defined(CYW43_WL_GPIO_LED_PIN)
        // For Pico W devices we need to initialise the driver etc
        return cyw43_arch_init();
    #endif
    }
    
    // Turn the led on or off
    void pico_set_led(bool led_on) {
    #if defined(PICO_DEFAULT_LED_PIN)
        // Just set the GPIO on or off
        gpio_put(PICO_DEFAULT_LED_PIN, led_on);
    #elif defined(CYW43_WL_GPIO_LED_PIN)
        // Ask the wifi "driver" to set the GPIO on or off
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, led_on);
    #endif
    }

void read_from_dht(uint gpio,dht_reading *result);

int main() {
    stdio_init_all();
    gpio_init(DHT_PIN);
    gpio_init(LEDB_PIN);
    gpio_set_dir(LEDB_PIN, GPIO_OUT);
    gpio_put(LEDB_PIN,true);
#ifdef CYW43_WL_GPIO_LED_PIN
    int rc=pico_led_init();
    hard_assert(rc== PICO_OK);
    
#endif
pico_set_led(1);
gpio_set_dir(DHT_PIN, GPIO_OUT);
gpio_put(DHT_PIN, 1);    
while (1) {
        dht_reading reading;
        read_from_dht(DHT_PIN,&reading);
        float fahrenheit = (reading.temp_celsius * 9 / 5) + 32;
        printf("Humidity = %.1f%%, Temperature = %.1fC (%.1fF)\n",
               reading.humidity, reading.temp_celsius, fahrenheit);

        sleep_ms(2000);
    }
}


void read_from_dht(uint gpio,dht_reading *result) {
    
    const int THRESHOLD = 7;

/**
 * @brief Maximum number of polling attempts during DHT11 data transmission.
 */
const int POLLING_LIMIT = 50;

/**
 * @brief Error value returned when there is a transmission error during DHT11 data reading.
 */
const int TRANSMISSION_ERROR = -999;
    
int count=0;
uint8_t raw[5];

gpio_set_dir(gpio, GPIO_OUT);
gpio_put(gpio,0);
sleep_ms(20);
gpio_put(gpio,1);
sleep_us(40);
gpio_set_dir(gpio, GPIO_IN);

//prima risposta da DHT a 0 
while(gpio_get(gpio)==0){
    count++;
    sleep_us(5);
    if(count==POLLING_LIMIT){
        printf("1 bad\n");
        return;
    }
}

count=0;
//seconda risposta da DHT a 1 
while(gpio_get(gpio)==1){
    count++;
    sleep_us(5);
    if(count==POLLING_LIMIT){
        printf("2 bad\n");
        return;
    }
}

//transmission start
for(int i=0;i<40;i++){
    count=0;
    //~50us
    while(gpio_get(gpio)==0){
        sleep_us(5);
    }
    //bit 0 or 1
    while(gpio_get(gpio)==1){
        sleep_us(5);
        count++;
    }
    raw[i / 8] <<= 1; 
    if(count>=THRESHOLD){
        raw[i / 8]|= 1;
           
    }
    
}

//check if raw data is valid
if(raw[4] != ((raw[0] + raw[1] + raw[2] + raw[3]) & 0xFF)){

    printf("Bad data\n");
}

printf("2:%04x\n",raw[2]);
printf("3:%04x\n",raw[3]);
float_t temp = raw[2];
     
      if (raw[3] & 0x80) {
        temp = -1 - temp;
      }
        
      temp += (raw[3] & 0x0f) * 0.1;
float_t h= raw[0] + raw[1] * 0.1; 
result->humidity=h;
result->temp_celsius=temp;

}


