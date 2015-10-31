#include <avr/io.h>
#include <util/delay.h>
#include <avr/power.h>
#include <avr/interrupt.h>

// default: active high
// PB0 := relay 0
// PB1 := relay 1
// PB2 := button
// PD2 := red LED
// PD3 := green LED
// to unlock: set PB0, unset PB1 (PB0 & /PB1)
// to lock: set PB1, unset PB0 (PB1 & /PB0)

#define LED_RED_ON	PORTD |= (1 << PD2)
#define LED_RED_OFF	PORTD &= ~(1 << PD2)
#define LED_GREEN_ON	PORTD |= (1 << PD3)
#define LED_GREEN_OFF	PORTD &= ~(1 << PD3)
#define BUTTON		PINB & (1 << PINB2)

void delay(uint16_t t){
	for(uint16_t i = 0; i < t; i++){
		_delay_ms(1);
	}
}

void lock(void){
	PORTB |= (1 << PB1);	// set relay PB1
	delay(500);
	PORTB &= ~(1 << PB1);	// unset relay PB1
}

void unlock(void){
	PORTB |= (1 << PB0);	// set relay PB0
	delay(500);
	PORTB &= ~(1 << PB0);	// unset relay PB0
}

void init(void){
	// deactivate clock divider -> 16 MHz
	clock_prescale_set(clock_div_1);
	
	// prepare data direction registers
	// set PB0, PB1, PD2, PD3 to output
	DDRB |= (1 << PB0) | (1 << PB1);
	DDRD |= (1 << PD2) | (1 << PD3);
}

void debug(void){	// TODO
/*
	// debugging: open and close with a frequency of 1Hz
	while(!(PINB & (1 << PINB2)))
	{
		PORTB |= (1 << PB0);
		_delay_ms(1000);
		PORTB &= ~(1 << PB0);
		_delay_ms(1000);
		PORTB |= (1 << PB1);
		_delay_ms(1000);
		PORTB &= ~(1 << PB1);
		_delay_ms(1000);
	}
*/
}


int main(void) {
	
	// initialize the hardware
	init();
	uint8_t state = 0;
	uint16_t timer = 0;
	uint16_t led_clock = 0;
	while(1) {
		switch(state){
			// closed
			case 0:
				// blink red led
				if(led_clock > 100) LED_RED_OFF;
				else LED_RED_ON;
				if(led_clock > 0) led_clock--;
				else led_clock = 1000;
				
				if(BUTTON){
					state = 1;		// now opening
					PORTB |= (1 << PB0);	// triggering the open-relay
					timer = 500;		// opening-state has to wait 500ms
				}
				break;
				
				
				
			// opening
			case 1:	
				LED_RED_OFF;
				if(timer > 0) timer--;		// still waiting until opening has finished
				else{
					PORTB &= ~(1 << PB0);	// stopping the open-relay
					state = 2;		// now open
					timer = 60000;		// open-state has to wait 60.000ms (1 min)
				}
				break;
				
				
				
			//open
			case 2:
				// blink green led
				if(led_clock > 500) LED_GREEN_OFF;
				else LED_GREEN_ON;
				if(led_clock > 0) led_clock--;
				else led_clock = 1000;
				
				if(timer > 0) timer--;		// still waiting until we can lock the door
				else{
					state = 3;		// now closing
					PORTB |= (1 << PB1);	// triggering the close-relay
					timer = 500;		// closing-state has to wait 500ms
				}
				if(BUTTON) timer = 60000;	// reset the timer
				break;
				
				
				
			// closing
			case 3:
				LED_GREEN_OFF;
				if(timer > 0) timer--;		// still waiting until opening has finished
				else{
					PORTB &= ~(1 << PB1);	// stopping the close-relay
					state = 0;		// now closed
				}
				break;
			
		}
		delay(1);
		
	}
	return 0;
}
