#include <avr/io.h>
#include <util/delay.h>
#include <avr/power.h>
#include <avr/interrupt.h>

int main() {
	// internen clock teiler deaktivieren
	clock_prescale_set(clock_div_1);
	sei();	// set interrupts

	unsigned char timer = 0;
	unsigned short timer2 = 0;

	DDRB |= (1 << PB0) | (1 << PB1);
	DDRD |= (1 << PD2) | (1 << PB3);

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

	while(1) {
		if(PINB & (1 << PINB2))
		{
			PORTD &= ~(1 << PD2);
			PORTD |= (1 << PD3);
				PORTB |= (1 << PB0);
				_delay_ms(500);
				PORTB &= ~(1 << PB0);
				_delay_ms(500);

			for(timer = 0; timer < 150; timer++)
			{
				PORTD &= ~(1 << PD3);
				for(timer2 = 0; timer2 < 3 * timer + 100; timer2++)
				{
					_delay_ms(1);
					if(PINB & (1 << PINB2))
						timer = 0;
				}
				PORTD |= (1 << PD3);
				for(timer2 = 0; timer2 < timer + 100; timer2++)
				{
					_delay_ms(1);
					if(PINB & (1 << PINB2))
						timer = 0;
				}
			}

			PORTD &= ~(1 << PD3);
			PORTD |= (1 << PD2);
			PORTB |= (1 << PB1);
				_delay_ms(500);
				PORTB &= ~(1 << PB1);
				_delay_ms(500);
		}
		else
		{
			_delay_ms(10);
			timer++;
			if(timer < 200)
			{
				PORTD &= ~(1 << PD2);
			}
			else
			{
				PORTD |= (1 << PD2);
			}
		}
	}
	return 0;
}
