#include <avr/io.h>
#include <util/delay.h>
#include <avr/power.h>
#include <avr/interrupt.h>
#include <stdbool.h>

#define RELAY_TIME			200		// time in milliseconds the relays will be enabled
#define RELAY_SLEEP			200		// time in milliseconds between each activation of the relays
#define RELAY_TICKS_OPEN	5		// how often the relays should be activated
#define RELAY_TICKS_CLOSE	1		// how often the relays should be activated
						// max 5
#define OPEN_TIME		60000	// time in milliseconds between button was pressed and we are locking the door


#define DEBUG_INTERVAL	2000	// time in milliseconds between door toggles in debug mode
					// max 65536

#define DEBUG_THRESHOLD	2000	// time in milliseconds the boot/debug button must be held until we go into debug mode
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

#define MAX_TASK_COUNT RELAY_TICKS_OPEN * 2 + RELAY_TICKS_CLOSE * 2 + 3 + 5	// theoretical minimum is RELAY_TICKS * 4 + 3

#if RELAY_TICKS_OPEN > 5
#error RELAY_TICKS_OPEN must not be higher than 5
#endif

#if RELAY_TICKS_CLOSE > 5
#error RELAY_TICKS_CLOSE must not be higher than 5
#endif

uint64_t system_millis = 0;

uint8_t state = 0;

typedef void (*VoidFnct) (void);

struct task{
	VoidFnct action;
	uint64_t time_to_run;
	uint8_t info;
	uint32_t repeat_time;
};

struct task tasklist[MAX_TASK_COUNT];

ISR(TIMER1_COMPA_vect){
	system_millis++;
}

uint8_t register_task(VoidFnct funcp, uint32_t first_run_in, uint8_t repeat, uint32_t repeat_millis){
	for(uint8_t i = 0; i < MAX_TASK_COUNT; i++){
		if(tasklist[i].info == 0){
			tasklist[i].action = funcp;
			tasklist[i].time_to_run = system_millis + first_run_in;
			tasklist[i].repeat_time = repeat_millis;
			tasklist[i].info = 1;
			if(repeat != 0){
				tasklist[i].info |= 2;
			}
			return i;
		}
	}
	return 0xff;
}

void deregister_task(uint8_t id){
	tasklist[id].info = 0;
}

void run_tasks(void){
	for(uint8_t i = 0; i < MAX_TASK_COUNT; i++){
		if(tasklist[i].info != 0){
			if(tasklist[i].time_to_run <= system_millis){
				tasklist[i].action();
				if(tasklist[i].info & 2){
					tasklist[i].time_to_run += tasklist[i].repeat_time;
				}
				else{
					tasklist[i].info = 0;
				}
			}
		}
	}
}

void enable_open(void){
	PORTB |= (1<<PB0);
}

void disable_open(void){
	PORTB &= ~(1<<PB0);
}

void enable_close(void){
	PORTB |= (1<<PB1);
}

void disable_close(void){
	PORTB &= ~(1<<PB1);
}

void state_closed(void){
	OCR0A = 0xff;
	state = 0;
}

void state_opening(void){
	OCR0A = 0;
	state = 1;
}

void state_open(void){
	OCR0B = 0xff;
	state = 2;
}

void state_closing(void){
	OCR0B = 0;
	state = 3;
}

void init(void){
	// deactivate clock divider -> 16 MHz
	clock_prescale_set(clock_div_1);
	
	// prepare data direction registers
	// set PB0, PB1, PB2, PD0 to output
	DDRB |= (1 << PB0) | (1 << PB1) | (1 << PB7);
	DDRD |= (1 << PD0);
	
	// setup Timer0 for fast PWM
	TCCR0A |= (1 << COM0A1) | (1 << COM0B1) | (1 << WGM00);
	TCCR0B |= (1 << CS00);

	// setup Timer1A to trigger every millisecond
	TCCR1B |= (1 << WGM12) | (1 << CS11) | (1 << CS10);
	OCR1A = 250;
	TIMSK1 |= (1 << OCIE1A);

	// enable interrupts
	sei();


	for(int i = 0; i < MAX_TASK_COUNT; i++){
		tasklist[i].info = 0;
	}

	DDRE |= (1<<PE6);
	PORTE |= (1<<PE2);
}

int main(void){
	init();

	uint8_t closing_tasks[((RELAY_TICKS_CLOSE * 2) + 2)];

	closing_tasks[0] = register_task(state_closing, 0, 0, 0);
	for(uint8_t i = 0; i < RELAY_TICKS_CLOSE; i++){
		closing_tasks[1 + i * 2] = register_task(enable_close, ((RELAY_TIME * i) + (RELAY_SLEEP * i)), 0, 0);
		closing_tasks[2 + i * 2] = register_task(disable_close, ((RELAY_TIME * (i + 1))  + (RELAY_SLEEP * i)), 0, 0);
	}
	closing_tasks[3 + (RELAY_TICKS_CLOSE - 1) * 2] = register_task(state_closed, (RELAY_TIME * RELAY_TICKS_CLOSE), 0, 0);

	uint8_t open_requested = 0;

	while(true){
		run_tasks();
		if((PINB & (1 << PB2)) || open_requested == 1){
			switch (state){
				case 0:	//closed
					open_requested = 0;
					state_opening();
					for(uint8_t i = 0; i < RELAY_TICKS_OPEN; i++){
						register_task(enable_open, ((RELAY_TIME * i) + (RELAY_SLEEP * i)), 0, 0);
						register_task(disable_open, ((RELAY_TIME * (i + 1))  + (RELAY_SLEEP * i)), 0, 0);
					}
					register_task(state_open, (RELAY_TIME * RELAY_TICKS_OPEN), 0, 0);

					closing_tasks[0] = register_task(state_closing, OPEN_TIME, 0, 0);
					for(uint8_t i = 0; i < RELAY_TICKS_CLOSE; i++){
						closing_tasks[1 + i * 2] = register_task(enable_close, ((RELAY_TIME * i) + (RELAY_SLEEP * i) + OPEN_TIME), 0, 0);
						closing_tasks[2 + i * 2] = register_task(disable_close, ((RELAY_TIME * (i + 1))  + (RELAY_SLEEP * i) + OPEN_TIME), 0, 0);
					}
					closing_tasks[3 + (RELAY_TICKS_CLOSE - 1) * 2] = register_task(state_closed, ((RELAY_TIME * RELAY_TICKS_CLOSE) + OPEN_TIME), 0, 0);

					break;

				case 1:	// opening
					// do nothing, we are already opening
					break;

				case 2:	// open
					for(uint8_t i = 0; i < ((RELAY_TICKS_CLOSE * 2) + 2); i++){
						deregister_task(closing_tasks[i]);
					}

					closing_tasks[0] = register_task(state_closing, OPEN_TIME, 0, 0);
					for(uint8_t i = 0; i < RELAY_TICKS_CLOSE; i++){
						closing_tasks[1 + i * 2] = register_task(enable_close, ((RELAY_TIME * i) + (RELAY_SLEEP * i) + OPEN_TIME), 0, 0);
						closing_tasks[2 + i * 2] = register_task(disable_close, ((RELAY_TIME * (i + 1))  + (RELAY_SLEEP * i) + OPEN_TIME), 0, 0);
					}
					closing_tasks[3 + (RELAY_TICKS_CLOSE - 1) * 2] = register_task(state_closed, ((RELAY_TIME * RELAY_TICKS_CLOSE) + OPEN_TIME), 0, 0);					
					break;

				case 3:	// closing
					open_requested = 1;
					break;
			}
		}
	}
}