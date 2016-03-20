#include <avr/io.h>
#include <util/delay.h>
#include <avr/power.h>
#include <avr/interrupt.h>

#define IN_RELAY_TIME		500	// time in milliseconds the relays will be enabled
					// max 65536

#define IN_OPEN_TIME		60000	// time in milliseconds between button was pressed and we are locking the door
					// max 4294967296

#define IN_DEBUG_INTERVAL	2000	// time in milliseconds between door toggle in debug mode
					// max 65536

#define IN_DEBUG_THRESHOLD	5000	// time in milliseconds the boot/debug button must be held until we go into debug mode
					// max 65536

#define LED_MAX_LIGHT		255	// maximum brightness of the LEDs
					// max 255

// default: active high
// PB0 := relay 0
// PB1 := relay 1
// PB2 := button (active high)
// PB7 := red LED	(OCR0A)
// PD0 := green LED	(OCR0B)
// PE2 := debug button (active low) (boot button on microcontroller pcb)
// to unlock: set PB0, unset PB1 (PB0 & /PB1)
// to lock: set PB1, unset PB0 (PB1 & /PB0)

#define LOOP_FACTOR		50			// speed of the main loop (runs this often per millisecond)
#define LOOP_DELAY		1000 / LOOP_FACTOR	// delay in microseconds after each run of the loop
#define RELAY_TIME		LOOP_FACTOR * IN_RELAY_TIME
#define OPEN_TIME		LOOP_FACTOR * IN_OPEN_TIME
#define DEBUG_INTERVAL		IN_DEBUG_INTERVAL
#define DEBUG_THRESHOLD		LOOP_FACTOR * IN_DEBUG_THRESHOLD

#if LED_MAX_LIGHT > 255
	#error LED_MAX_LIGHT must not be greater than 255
#endif

#define LED_RED		OCR0A
#define LED_GREEN	OCR0B

#define LED_RED_ON	OCR0A = 0xff
#define LED_RED_OFF	OCR0A = 0
#define LED_GREEN_ON	OCR0B = 0xff
#define LED_GREEN_OFF	OCR0B = 0

#define RELAY_OPEN_ON	PORTB |= (1 << PB0)
#define RELAY_OPEN_OFF	PORTB &= ~(1 << PB0)
#define RELAY_CLOSE_ON	PORTB |= (1 << PB1)
#define RELAY_CLOSE_OFF	PORTB &= ~(1 << PB1)

#define BUTTON		(PINB & (1 << PB2))
#define BUTTON_DEBUG	(PINE & (1 << PE2))

uint8_t fade_time = 4 * LOOP_FACTOR;		// 1 second
uint16_t dead_time = 1000;

void init(void){
	// deactivate clock divider -> 16 MHz
	clock_prescale_set(clock_div_1);
	
	// prepare data direction registers
	// set PB0, PB1, PB2, PD0 to output
	DDRB |= (1 << PB0) | (1 << PB1) | (1 << PB7);
	DDRD |= (1 << PD0);
	
	// setup Timer0 for fast PWM
//	TCCR0A |= (1 << COM0A1) | (1 << COM0B1) | (1 << WGM01) | (1 << WGM00);
	TCCR0A |= (1 << COM0A1) | (1 << COM0B1) | (1 << WGM00);
	TCCR0B |= (1 << CS00);
}

void fade(volatile uint8_t *led){
	static uint8_t state = 0;
	static uint16_t time = 0;
	switch(state){
		//fading up
		case 0:
			if(time == 0){				// we have reached the timeout
				(*led)++;			// increase brightness
				time = fade_time;		// reset the timer
				if((*led) == LED_MAX_LIGHT)	// we have reached the maximum brightness
					state = 1;		// now fading down
			}
			else					// still waiting for timeout
				time--;				// decrement timer
			break;
		
		//fading down
		case 1:
			if(time == 0){			// we have reached the timeout
				(*led)--;		// decrease brightness
				time = fade_time;	// reset the timer
				if((*led) == 0)		// LED is now off
					state = 2;	// now prepare dead time
			}
			else				// still waiting for timeout
				time--;			// decrement timer
			break;
		
		// prepare dead time
		case 2:
			time = dead_time * LOOP_FACTOR;		// set timeout for the dead time
			state = 3;
			break;
		
		// dead time
		case 3:
			if(time == 0){			// we have reached the timeout
				time = fade_time;	// prepare timeout for fading up
				state = 0;		// now fading up
			}
			else				// still waiting for timeout
				time--;			// decrement timer
	}
}


int main(void) {
	init();			// initialize the hardware
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
				if(LED_GREEN > 0)		// green LED is still on
					fade(&LED_GREEN);		// continue fading green LED until it is off
				else
					fade(&LED_RED);		// now we can fade red
				
				
				if(BUTTON){
					state = 1;		// now opening
					RELAY_OPEN_ON;		// triggering the open-relay
					timer = RELAY_TIME;	// opening-state has to wait
				}
				break;
				
			// opening
			case 1:	
				if(LED_RED > 0)			// red LED is still on
					fade(&LED_RED);		// continue fading red LED
				
				if(timer > 0)
					timer--;		// still waiting until opening has finished
				else {
					RELAY_OPEN_OFF;		// stopping the open-relay
					state = 2;		// now open
					timer = OPEN_TIME;	// open-state has to wait
				}
				break;
				
			//open
			case 2:
				if(LED_RED > 0)			// red LED is still on
					fade(&LED_RED);		// continue fading red LED until it is off
				else
					fade(&LED_GREEN);		// now we can fade green
				
				if(timer > 0)
					timer--;		// still waiting until we can lock the door
				else{
					state = 3;		// now closing
					RELAY_CLOSE_ON;		// triggering the close-relay
					timer = RELAY_TIME;	// closing-state has to wait
				}
				if(BUTTON)
					timer = OPEN_TIME;	// reset the timer
				break;
				
			// closing
			case 3:
				if(LED_GREEN > 0)		// green LED is still on
					fade(&LED_GREEN);		// continue fading green LED
				
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
					_delay_ms(IN_RELAY_TIME);		//  unlock the door
					RELAY_OPEN_OFF;			//
					
					LED_GREEN_ON;			// signal that the door is unlocked
					
					_delay_ms(IN_DEBUG_INTERVAL);	// wait
					
					RELAY_CLOSE_ON;			//
					_delay_ms(IN_RELAY_TIME);		//  lock the door
					RELAY_CLOSE_OFF;		//
					
					LED_GREEN_OFF;			// signal that the door is locked
					
					_delay_ms(IN_DEBUG_INTERVAL);	// wait
				}
				break;

		}
		_delay_us(LOOP_DELAY);
		
		if(BUTTON_DEBUG == 0){				// The boot/debug button is pressed
			debug_counter++;			// count the time the button is pressed
			if(debug_counter > DEBUG_THRESHOLD){	// we have reached the threshold
				state = 100;			// we go to the debugging state
			}
		}
		else if (debug_counter > 0){			// The boot/debug button is not pressed, but the counter is not zero
			debug_counter--;			// so we decrement the counter. If we would reset it, we could get problems with bouncing.
		}
		
	}
	return 0;
}
