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

int main(void) {
	// deactivate internal clock divider
	clock_prescale_set(clock_div_1);
	// set interrupts
	sei();

	unsigned char timer = 0;
	unsigned short timer2 = 0;

	// prepare data direction registers
	// set PB0, PB1, PD2, PD3 to output
	DDRB |= (1 << PB0) | (1 << PB1);
	DDRD |= (1 << PD2) | (1 << PD3);

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

	while(1) {
		if(PINB & (1 << PB2))
		{
			/* unlock door */

			// unset red LED
			PORTD &= ~(1 << PD2);

			// set green LED
			PORTD |= (1 << PD3);

			// set relay PB0
			PORTB |= (1 << PB0);

			_delay_ms(500);

			// unset relay PB0
			PORTB &= ~(1 << PB0);

			//_delay_ms(500);

			// wait for timeout
			for(timer = 0; timer < 150; timer++)
			{
				// unset green LED
				PORTD &= ~(1 << PD3);

				// compute waiting time for green LED (off)
				for(timer2 = 0; timer2 < 3 * timer + 100; timer2++)
				{
					_delay_ms(1);
					// reset timer if button pushed
					if(PINB & (1 << PINB2))
						timer = 0;
				}
				// set green LED
				PORTD |= (1 << PD3);

				// compute waiting time for green LED (on)
				for(timer2 = 0; timer2 < timer + 100; timer2++)
				{
					_delay_ms(1);
					// reset timer if button pushed
					if(PINB & (1 << PINB2))
						timer = 0;
				}
			}

			/* lock door */

			// unset LED
			PORTD &= ~(1 << PD3);

			// set relay PD2
			PORTD |= (1 << PD2);

			// set relay PB1
			PORTB |= (1 << PB1);
			_delay_ms(500);

			// unset relay PB1
			PORTB &= ~(1 << PB1);
			//_delay_ms(500);

		}
		else
		{
			/* idle mode: red LED blinking */
			_delay_ms(10);
			timer++;
			if(timer < 200)
			{
				// unset red LED
				PORTD &= ~(1 << PD2);
			}
			else
			{
				// set red LED
				PORTD |= (1 << PD2);
			}
		}
	}
	return 0;
}
