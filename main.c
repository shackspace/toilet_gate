#include <avr/io.h>
#include <util/delay.h>
#include <avr/power.h>
#include <avr/interrupt.h>

#define RELAY_TIME	500		// time in milliseconds the relays will be enabled
					// max 65536
					
#define OPEN_TIME	60000		// time in milliseconds between button was pressed and we are locking the door
					// max 4294967296
					
#define DEBUG_INTERVAL	2000		// time in milliseconds between door toggle in debug mode
					// max 65536
					
#define DEBUG_THRESHOLD	5000		// time in milliseconds the boot/debug button must be held until we go into debug mode
					// max 65536

// default: active high
// PB0 := relay 0
// PB1 := relay 1
// PB2 := button (active high)
// PD2 := red LED
// PD3 := green LED
// PE2 := debug button (active low) (boot button on microcontroller pcb)
// to unlock: set PB0, unset PB1 (PB0 & /PB1)
// to lock: set PB1, unset PB0 (PB1 & /PB0)

#define LED_RED_ON	PORTD |= (1 << PD2)
#define LED_RED_OFF	PORTD &= ~(1 << PD2)
#define LED_GREEN_ON	PORTD |= (1 << PD3)
#define LED_GREEN_OFF	PORTD &= ~(1 << PD3)

#define RELAY_OPEN_ON	PORTB |= (1 << PB0)
#define RELAY_OPEN_OFF	PORTB &= ~(1 << PB0)
#define RELAY_CLOSE_ON	PORTB |= (1 << PB1)
#define RELAY_CLOSE_OFF	PORTB &= ~(1 << PB1)

#define BUTTON		(PINB & (1 << PB2))
#define BUTTON_DEBUG	(PINE & (1 << PE2))

void init(void){
	// deactivate clock divider -> 16 MHz
	clock_prescale_set(clock_div_1);
	
	// prepare data direction registers
	// set PB0, PB1, PD2, PD3 to output
	DDRB |= (1 << PB0) | (1 << PB1);
	DDRD |= (1 << PD2) | (1 << PD3);
}

int main(void) {
	
	// initialize the hardware
	init();
	uint8_t state = 2;	// we start in open state
#if OPEN_TIME > 65536
	uint32_t timer = 0;	// no time set, so we will immediately start with locking the door
#else
	uint16_t timer = 0;
#endif
	uint16_t led_clock = 0;
	uint16_t debug_counter = 0;
	while(1) {
		switch(state){
			// closed
			case 0:
				// blink red led
				if(led_clock > 100)
					LED_RED_OFF;
				else
					LED_RED_ON;
				if(led_clock > 0)
					led_clock--;
				else
					led_clock = 1000;
				
				if(BUTTON){
					state = 1;		// now opening
					RELAY_OPEN_ON;		// triggering the open-relay
					timer = RELAY_TIME;	// opening-state has to wait 500ms
				}
				break;
				
			// opening
			case 1:	
				LED_RED_OFF;
				if(timer > 0)
					timer--;		// still waiting until opening has finished
				else {
					RELAY_OPEN_OFF;		// stopping the open-relay
					state = 2;		// now open
					timer = OPEN_TIME;	// open-state has to wait 60.000ms (1 min)
				}
				break;
				
			//open
			case 2:
				// blink green led
				if(led_clock > 500)
					LED_GREEN_OFF;
				else
					LED_GREEN_ON;
				if(led_clock > 0)
					led_clock--;
				else led_clock = 1000;
				
				if(timer > 0)
					timer--;		// still waiting until we can lock the door
				else{
					state = 3;		// now closing
					RELAY_CLOSE_ON;		// triggering the close-relay
					timer = RELAY_TIME;	// closing-state has to wait 500ms
				}
				if(BUTTON)
					timer = OPEN_TIME;	// reset the timer
				break;
				
			// closing
			case 3:
				LED_GREEN_OFF;
				if(timer > 0)
					timer--;		// still waiting until closing has finished
				else{
					RELAY_CLOSE_OFF;	// stopping the close-relay
					state = 0;		// now closed
				}
				break;

			// debugging
			default:
				LED_RED_ON;				// signal that we are in debugging mode
				while(1){
					RELAY_OPEN_ON;			//
					_delay_ms(RELAY_TIME);		//  unlock the door
					RELAY_OPEN_OFF;			//
					
					LED_GREEN_ON;			// signal that the door is unlocked
					
					_delay_ms(DEBUG_INTERVAL);	// wait
					
					RELAY_CLOSE_ON;			//
					_delay_ms(RELAY_TIME);		//  lock the door
					RELAY_CLOSE_OFF;		//
					
					LED_GREEN_OFF;			// signal that the door is locked
					
					_delay_ms(DEBUG_INTERVAL);	// wait
				}
				break;

		}
		_delay_ms(1);
		
		if(BUTTON_DEBUG == 0){				// The boot/debug button is pressed
			debug_counter++;			// count the time the button is pressed
			if(debug_counter > DEBUG_THRESHOLD){	// we have counted 5 seconds
				state = 100;			// we go to the debugging state
			}
		}
		else if (debug_counter > 0){			// The boot/debug button is not pressed, but the counter is not zero
			debug_counter--;			// so we decrement the counter. If we would reset it, we could get problems with bouncing.
		}
		
	}
	return 0;
}
