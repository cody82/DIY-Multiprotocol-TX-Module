//*************************************
// FrSky Telemetry serial code        *
// By Midelic  on RCGroups                 *
//*************************************

#if defined TELEMETRY

#if defined SPORT_TELEMETRY	
	#define SPORT_TIME 12000
	#define FRSKY_SPORT_PACKET_SIZE   8
	uint32_t last = 0;
	uint8_t sport_counter=0;
	uint8_t RxBt = 0;
	uint8_t rssi;
	uint8_t sport = 0;
#endif
#if defined HUB_TELEMETRY
	#define USER_MAX_BYTES 6
	uint8_t prev_index;
#endif

#define START_STOP              0x7e
#define BYTESTUFF               0x7d
#define STUFF_MASK              0x20
#define MAX_PKTX 10
uint8_t pktx[MAX_PKTX];
uint8_t pktx1[MAX_PKTX];
uint8_t index;
uint8_t pass = 0;
uint8_t frame[18];

#ifdef BASH_SERIAL
// For bit-bashed serial output

struct t_serial_bash
{
	uint8_t head ;
	uint8_t tail ;
	uint8_t data[64] ;
	uint8_t busy ;
	uint8_t speed ;
} SerialControl ;
#endif

#if defined DSM_TELEMETRY
void DSM2_frame()
{
	Serial_write(0xAA);					// Start
	for (uint8_t i = 0; i < 17; i++)	// RSSI value followed by 16 bytes of telemetry data
		Serial_write(pkt[i]);
}
#endif

void frskySendStuffed()
{
	Serial_write(START_STOP);
	for (uint8_t i = 0; i < 9; i++)
	{
		if ((frame[i] == START_STOP) || (frame[i] == BYTESTUFF))
		{
			Serial_write(BYTESTUFF);	    	  
			frame[i] ^= STUFF_MASK;	
		}
		Serial_write(frame[i]);
	}
	Serial_write(START_STOP);
}

void compute_RSSIdbm()
{
	
	RSSI_dBm = (((uint16_t)(pktt[len-2])*18)>>4);
	if(pktt[len-2] >=128)
		RSSI_dBm -= 164;
	else
		RSSI_dBm += 130;
}

void frsky_check_telemetry(uint8_t *pkt,uint8_t len)
{
	if(pkt[1] == rx_tx_addr[3] && pkt[2] == rx_tx_addr[2] && len ==(pkt[0] + 3))
	{	   
		for (uint8_t i=3;i<len;i++)
			pktt[i]=pkt[i];				 
		telemetry_link=1;
		if(pktt[6])
			telemetry_counter=(telemetry_counter+1)%32;
		//
#if defined FRSKYX_CC2500_INO
		if ((cur_protocol[0]&0x1F)==MODE_FRSKYX)
		{
			if ((pktt[5] >> 4 & 0x0f) == 0x08)
			{  
				seq_last_sent = 8;
				seq_last_rcvd = 0;
				pass=0;
			} 
			else
			{
				if ((pktt[5] >> 4 & 0x03) == (seq_last_rcvd + 1) % 4)
					seq_last_rcvd = (seq_last_rcvd + 1) % 4;
				else
					pass=0;//reset if sequence wrong
			}
		}
#endif
	}
}

void frsky_link_frame()
{
	frame[0] = 0xFE;
	if ((cur_protocol[0]&0x1F)==MODE_FRSKY)
	{		
		compute_RSSIdbm();				
		frame[1] = pktt[3];
		frame[2] = pktt[4];
		frame[3] = pktt[5];
		frame[4] = (uint8_t)RSSI_dBm;
	}
	else
		if ((cur_protocol[0]&0x1F)==MODE_HUBSAN)
		{	
			frame[1] = v_lipo*2; //v_lipo; common 0x2A=42/10=4.2V
			frame[2] = frame[1];			
			frame[3] = 0x00;
			frame[4] = (uint8_t)RSSI_dBm;
		}
	frame[5] = frame[6] = frame[7] = frame[8] = 0;			
	frskySendStuffed();
}

#if defined HUB_TELEMETRY
void frsky_user_frame()
{
	uint8_t indexx = 0, c=0, j=8, n=0, i;
	
	if(pktt[6]>0 && pktt[6]<=10)
	{//only valid hub frames	  
		frame[0] = 0xFD;
		frame[2] = pktt[7];		
		switch(pass)
		{
			case 0:
				indexx=pktt[6];
				for(i=0;i<indexx;i++)
				{
//					if(pktt[j]==0x5E)
//					{
//						if(c++)
//						{
//							c=0;
//							n++;
//							j++;
//						}
//					}
					pktx[i]=pktt[j++];
				}	
//				indexx = indexx-n;
				pass=1;
				
			case 1:
				index=indexx;
				prev_index = indexx; 
				if(index<USER_MAX_BYTES)
				{   			
					for(i=0;i<index;i++)
						frame[i+3]=pktx[i];
					pktt[6]=0;
					pass=0;
				}
				else
				{
					index = USER_MAX_BYTES;
					for(i=0;i<index;i++)
						frame[i+3]=pktx[i];
					pass=2;
				}			
				break;
			case 2:		
				index = prev_index - index;
				prev_index=0;
				if(index<=(MAX_PKTX-USER_MAX_BYTES))	//10-6=4
					for(i=0;i<index;i++)
						frame[i+3]=pktx[USER_MAX_BYTES+i];
				pass=0;
				pktt[6]=0; 
				break;
			default:
				break;
		}
		if(!index)
			return;
		frame[1] = index;
		frskySendStuffed();
	}
	else
		pass=0;
}	   
#endif

/*
HuB RX packets.
pkt[6]|(counter++)|00 01 02 03 04 05 06 07 08 09 
        %32        
01     08          5E 28 12 00 5E 5E 3A 06 00 5E
0A     09          28 12 00 5E 5E 3A 06 00 5E 5E  
09     0A          3B 09 00 5E 5E 06 36 7D 5E 5E 
03     0B          5E 28 11 00 5E 5E 06 06 6C 5E
0A     0C          00 5E 5E 3A 06 00 5E 5E 3B 09
07     0D          00 5E 5E 06 06 6C 5E 16 72 5E
05     0E          5E 28 11 00 5E 5E 3A 06 00 5E
0A     0F          5E 3A 06 00 5E 5E 3B 09 00 5E
05     10          5E 06 16 72 5E 5E 3A 06 00 5E
*/

#if defined SPORT_TELEMETRY
/* SPORT details serial
			100K 8E2 normal-multiprotocol
			-every 12ms-or multiple of 12; %36
			1  2  3  4  5  6  7  8  9  CRC DESCR
			7E 98 10 05 F1 20 23 0F 00 A6 SWR_ID 
			7E 98 10 01 F1 33 00 00 00 C9 RSSI_ID 
			7E 98 10 04 F1 58 00 00 00 A1 BATT_ID 
			7E BA 10 03 F1 E2 00 00 00 18 ADC2_ID 
			7E BA 10 03 F1 E2 00 00 00 18 ADC2_ID 
			7E BA 10 03 F1 E2 00 00 00 18 ADC2_ID 
			7E BA 10 03 F1 E2 00 00 00 18 ADC2_ID 
			7E BA 10 03 F1 E2 00 00 00 18 ADC2_ID 
			7E BA 10 03 F1 E2 00 00 00 18 ADC2_ID 	
			
			
			Telemetry frames(RF) SPORT info 
			15 bytes payload
			SPORT frame valid 6+3 bytes
			[00] PKLEN  0E 0E 0E 0E 
			[01] TXID1  DD DD DD DD 
			[02] TXID2  6D 6D 6D 6D 
			[03] CONST  02 02 02 02 
			[04] RS/RB  2C D0 2C CE	//D0;CE=2*RSSI;....2C = RX battery voltage(5V from Bec)
			[05] HD-SK  03 10 21 32	//TX/RX telemetry hand-shake bytes
			[06] NO.BT  00 00 06 03	//No.of valid SPORT frame bytes in the frame		
			[07] STRM1  00 00 7E 00 
			[08] STRM2  00 00 1A 00 
			[09] STRM3  00 00 10 00 
			[10] STRM4  03 03 03 03  
			[11] STRM5  F1 F1 F1 F1 
			[12] STRM6  D1 D1 D0 D0
			[13] CHKSUM1 --|2 CRC bytes sent by RX (calculated on RX side crc16/table)
			[14] CHKSUM2 --|
			+2	appended bytes automatically  RSSI and LQI/CRC bytes(len=0x0E+3);
			
0x06	0x06	0x06	0x06	0x06

0x7E	0x00	0x03	0x7E	0x00
0x1A	0x00	0xF1	0x1A	0x00
0x10	0x00	0xD7	0x10	0x00
0x03	0x7E	0x00	0x03	0x7E
0xF1	0x1A	0x00	0xF1	0x1A
0xD7	0x10	0x00	0xD7	0x10

0xE1	0x1C	0xD0	0xEE	0x33
0x34	0x0A	0xC3	0x56	0xF3
				
		*/
void sportSend(uint8_t *p)
{
	uint16_t crc_s = 0;
	Serial_write(START_STOP);//+9
	Serial_write(p[0]) ;
	for (uint8_t i = 1; i < 9; i++)
	{
		if (i == 8)
			p[i] = 0xff - crc_s;
		
		if ((p[i] == START_STOP) || (p[i] == BYTESTUFF))
		{
			Serial_write(BYTESTUFF);//stuff again
			Serial_write(STUFF_MASK ^ p[i]);
		} 
		else			
			Serial_write(p[i]);					
		
		if (i>0)
		{
			crc_s += p[i]; //0-1FF
			crc_s += crc_s >> 8; //0-100
			crc_s &= 0x00ff;
		}
	}
}

void sportIdle()
{
	Serial_write(START_STOP);
}	

void sportSendFrame()
{
	uint8_t i;
	sport_counter = (sport_counter + 1) %36;
	
	if(sport_counter<6)
	{
		frame[0] = 0x98;
		frame[1] = 0x10;
		for (i=5;i<8;i++)
		frame[i]=0;
	}
	switch (sport_counter)
	{
		case 0:
			frame[2] = 0x05;
			frame[3] = 0xf1;
			frame[4] = 0x02 ;//dummy values if swr 20230f00
			frame[5] = 0x23;
			frame[6] = 0x0F;
			break;
		case 2: // RSSI
			frame[2] = 0x01;
			frame[3] = 0xf1;
			frame[4] = rssi;
			break;
		case 4: //BATT
			frame[2] = 0x04;
			frame[3] = 0xf1;
			frame[4] = RxBt;//a1;
			break;								
		default:
			if(sport)
			{	
				for (i=0;i<FRSKY_SPORT_PACKET_SIZE;i++)
				frame[i]=pktx1[i];
				sport=0;
				break;
			}
			else
			{
				sportIdle();
				return;
			}		
	}
	sportSend(frame);
}	

void proces_sport_data(uint8_t data)
{
	switch (pass)
	{
		case 0:
			if (data == START_STOP)
			{//waiting for 0x7e
				index = 0;
				pass = 1;
			}
			break;		
		case 1:
			if (data == START_STOP)	// Happens if missed packet
			{//waiting for 0x7e
				index = 0;
				pass = 1;
				break;		
			}
			if(data == BYTESTUFF)//if they are stuffed
				pass=2;
			else
				if (index < MAX_PKTX)		
					pktx[index++] = data;		
			break;
		case 2:	
			if (index < MAX_PKTX)	
				pktx[index++] = data ^ STUFF_MASK;	//unstuff bytes	
			pass=1;
			break;	
	} // end switch
	if (index >= FRSKY_SPORT_PACKET_SIZE)
	{//8 bytes no crc 
		if ( sport )
		{
			// overrun!
		}
		else
		{
			uint8_t i ;
			for ( i = 0 ; i < FRSKY_SPORT_PACKET_SIZE ; i += 1 )
			{
				pktx1[i] = pktx[i] ;	// Double buffer
			}
			sport = 1;//ok to send
		}
		pass = 0;//reset
	}
}

#endif

void frskyUpdate()
{
#if defined SPORT_TELEMETRY
	if ((cur_protocol[0]&0x1F)==MODE_FRSKYX)
	{	// FrSkyX
		if(telemetry_link)
		{		
			if(pktt[4] & 0x80)
				rssi=pktt[4] & 0x7F ;
			else 
				RxBt = (pktt[4]<<1) + 1 ;
			for (uint8_t i=0; i < pktt[6]; i++)
			proces_sport_data(pktt[7+i]);
			telemetry_link=0;
		}
	}
#endif					

	// check for space in tx buffer

#ifdef BASH_SERIAL
	uint8_t h ;
	uint8_t t ;
	h = SerialControl.head ;
	t = SerialControl.tail ;
	if ( h >= t )
	{
		t += 64 - h ;
	}
	else
	{
		t -= h ;
	}
	if ( t < 32 )
	{
		return ;
	}

#else
	uint8_t h ;
	uint8_t t ;
	h = tx_head ;
	t = tx_tail ;
	if ( h >= t )
	{
		t += TXBUFFER_SIZE - h ;
	}
	else
	{
		t -= h ;
	}
	if ( t < 16 )
	{
		return ;
	}
#endif
	 
	#if defined DSM_TELEMETRY
	if(telemetry_link && (cur_protocol[0]&0x1F) == MODE_DSM2 )
	{	// DSM2
		DSM2_frame();
		telemetry_link=0;
		return;
	}
	#endif
	if(telemetry_link && (cur_protocol[0]&0x1F) != MODE_FRSKYX )
	{	// FrSky + Hubsan
		frsky_link_frame();
		telemetry_link=0;
		return;
	}
	#if defined HUB_TELEMETRY
	if(!telemetry_link && (cur_protocol[0]&0x1F) == MODE_FRSKY)
	{	// FrSky
		frsky_user_frame();
		return;
	}
	#endif
	#if defined SPORT_TELEMETRY
	if ((cur_protocol[0]&0x1F)==MODE_FRSKYX)
	{	// FrSkyX
		uint32_t now = micros();
		if ((now - last) > SPORT_TIME)
		{
			sportSendFrame();
			last += SPORT_TIME ;
		}
	}
	#endif					
}


/**************************/
/**************************/
/**  Serial TX routines  **/
/**************************/
/**************************/

#ifndef BASH_SERIAL
// Routines for normal serial output
void Serial_write(uint8_t data)
{
	cli();	// disable global int
	if(++tx_head>=TXBUFFER_SIZE)
		tx_head=0;
	tx_buff[tx_head]=data;
#ifdef XMEGA
	USARTC0.CTRLA = (USARTC0.CTRLA & 0xFC) | 0x01 ;
#else	
	UCSR0B |= (1<<UDRIE0);//enable UDRE interrupt
#endif
	sei();	// enable global int
}

// Speed is 0 for 100K and 1 for 9600
void initTXSerial( uint8_t speed)
{
	if(speed==SPEED_9600)
	{ // 9600
	#ifdef XMEGA
		USARTC0.BAUDCTRLA = 207 ;
		USARTC0.BAUDCTRLB = 0 ;
		
		USARTC0.CTRLB = 0x18 ;
		USARTC0.CTRLA = (USARTC0.CTRLA & 0xCF) | 0x10 ;
		USARTC0.CTRLC = 0x03 ;
	#else
		//9600 bauds
		UBRR0H = 0x00;
		UBRR0L = 0x67;
		UCSR0A = 0 ;	// Clear X2 bit
		//Set frame format to 8 data bits, none, 1 stop bit
		UCSR0C = (1<<UCSZ01)|(1<<UCSZ00);
		UCSR0B = (1<<TXEN0);//tx enable
	#endif
	}
	else
		UCSR0B |= (1<<TXEN0);//tx enable
}

#ifdef XMEGA
ISR(USARTC0_DRE_vect)
#else
ISR(USART_UDRE_vect)
#endif
{	// Transmit interrupt
	if(tx_head!=tx_tail)
	{
		if(++tx_tail>=TXBUFFER_SIZE)//head 
			tx_tail=0;
#ifdef XMEGA
		USARTC0.DATA = tx_buff[tx_tail] ;
#else
		UDR0=tx_buff[tx_tail];
#endif
	}
	if (tx_tail == tx_head)
#ifdef XMEGA
		USARTC0.CTRLA &= ~0x03 ;
#else
		UCSR0B &= ~(1<<UDRIE0); // Check if all data is transmitted . if yes disable transmitter UDRE interrupt
#endif
}

#else	//BASH_SERIAL
// Routines for bit-bashed serial output

// Speed is 0 for 100K and 1 for 9600
void initTXSerial( uint8_t speed)
{
	TIMSK0 = 0 ;	// Stop all timer 0 interrupts
#ifdef INVERT_SERIAL
	PORTD &= ~2 ;
#else
	PORTD |= 2 ;
#endif
	DDRD |= 2 ;	// TxD pin is an output
	UCSR0B &= ~(1<<TXEN0) ;

	SerialControl.speed = speed ;
	if ( speed == SPEED_9600 )
	{
		OCR0A = 207 ;	// 104uS period
		TCCR0A = 3 ;
		TCCR0B = 0x0A ; // Fast PMM, 2MHz
	}
	else	// 100K
	{
		TCCR0A = 0 ;
		TCCR0B = 2 ;	// Clock/8 (0.5uS)
	}
}

void Serial_write( uint8_t byte )
{
	uint8_t temp ;
	uint8_t temp1 ;
	uint8_t byteLo ;

#ifdef INVERT_SERIAL
	byte = ~byte ;
#endif

	byteLo = byte ;
	byteLo >>= 7 ;		// Top bit
	if ( SerialControl.speed == SPEED_100K )
	{
#ifdef INVERT_SERIAL
		byteLo |= 0x02 ;	// Parity bit
#else
		byteLo |= 0xFC ;	// Stop bits
#endif
	// calc parity
		temp = byte ;
		temp >>= 4 ;
		temp = byte ^ temp ;
		temp1 = temp ;
		temp1 >>= 2 ;
		temp = temp ^ temp1 ;
		temp1 = temp ;
		temp1 <<= 1 ;
		temp ^= temp1 ;
		temp &= 0x02 ;
#ifdef INVERT_SERIAL
		byteLo ^= temp ;
#else	
		byteLo |= temp ;
#endif
	}
	else
	{
		byteLo |= 0xFE ;	// Stop bit
	}	
	byte <<= 1 ;
#ifdef INVERT_SERIAL
	byte |= 1 ;		// Start bit
#endif
  uint8_t next = (SerialControl.head + 2) & 0x3f ;
	if ( next != SerialControl.tail )
	{
		SerialControl.data[SerialControl.head] = byte ;
		SerialControl.data[SerialControl.head+1] = byteLo ;
		SerialControl.head = next ;
	}
	cli() ;
	if ( SerialControl.busy == 0 )
	{
		sei() ;
		// Start the transmission here
#ifdef INVERT_SERIAL
		GPIOR2 = 0 ;
#else
		GPIOR2 = 0x01 ;
#endif
		if ( SerialControl.speed == SPEED_100K )
		{
			GPIOR1 = 1 ;
			OCR0B = TCNT0 + 40 ;
			OCR0A = OCR0B + 210 ;
			TIFR0 = (1<<OCF0A) | (1<<OCF0B) ;
			TIMSK0 |= (1<<OCIE0B) ;
			SerialControl.busy = 1 ;
		}
		else
		{
			GPIOR1 = 1 ;
			TIFR0 = (1<<TOV0) ;
			TIMSK0 |= (1<<TOIE0) ;
			SerialControl.busy = 1 ;
		}
	}
	else
	{
		sei() ;
	}
}

// Assume timer0 at 0.5uS clock

ISR(TIMER0_COMPA_vect)
{
	uint8_t byte ;
	byte = GPIOR0 ;
	if ( byte & 0x01 )
	{
		PORTD |= 0x02 ;
	}
	else
	{
		PORTD &= ~0x02 ;
	}
	byte /= 2 ;		// Generates shorter code than byte >>= 1
	GPIOR0 = byte ;
	if ( --GPIOR1 == 0 )
	{
		TIMSK0 &= ~(1<<OCIE0A) ;
		GPIOR1 = 3 ;
	}
	else
	{
		OCR0A += 20 ;
	}
}

ISR(TIMER0_COMPB_vect)
{
	uint8_t byte ;
	byte = GPIOR2 ;
	if ( byte & 0x01 )
	{
		PORTD |= 0x02 ;
	}
	else
	{
		PORTD &= ~0x02 ;
	}
	byte /= 2 ;		// Generates shorter code than byte >>= 1
	GPIOR2 = byte ;
	if ( --GPIOR1 == 0 )
	{
		// prepare next byte and allow for 2 stop bits
		struct t_serial_bash *ptr = &SerialControl ;
		if ( ptr->head != ptr->tail )
		{
			GPIOR0 = ptr->data[ptr->tail] ;
			GPIOR2 = ptr->data[ptr->tail+1] ;
			ptr->tail = ( ptr->tail + 2 ) & 0x3F ;
			GPIOR1 = 8 ;
			OCR0A = OCR0B + 40 ;
			OCR0B = OCR0A + 8 * 20 ;
			TIMSK0 |= (1<<OCIE0A) ;
		}
		else
		{
			SerialControl.busy = 0 ;
			TIMSK0 &= ~(1<<OCIE0B) ;
		}
	}
	else
	{
		OCR0B += 20 ;
	}
}

ISR(TIMER0_OVF_vect)
{
	uint8_t byte ;
	if ( GPIOR1 > 2 )
	{
		byte = GPIOR0 ;
	}
	else
	{
		byte = GPIOR2 ;
	}
	if ( byte & 0x01 )
	{
		PORTD |= 0x02 ;
	}
	else
	{
		PORTD &= ~0x02 ;
	}
	byte /= 2 ;		// Generates shorter code than byte >>= 1
	if ( GPIOR1 > 2 )
	{
		GPIOR0 = byte ;
	}
	else
	{
		GPIOR2 = byte ;
	}
	if ( --GPIOR1 == 0 )
	{
		// prepare next byte
		struct t_serial_bash *ptr = &SerialControl ;
		if ( ptr->head != ptr->tail )
		{
			GPIOR0 = ptr->data[ptr->tail] ;
			GPIOR2 = ptr->data[ptr->tail+1] ;
			ptr->tail = ( ptr->tail + 2 ) & 0x3F ;
			GPIOR1 = 10 ;
		}
		else
		{
			SerialControl.busy = 0 ;
			TIMSK0 &= ~(1<<TOIE0) ;
		}
	}
}


#endif // BASH_SERIAL

#endif // TELEMETRY

