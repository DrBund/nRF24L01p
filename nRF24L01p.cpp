/* nRF24L01p.h - Library for nRF24L01p Radio
	Created by: Steve Lammers, 2/21/2015
	Released to the public domain.

 SPI Settings
	2MHz
	Mode 0
	MSB First
  SPI is started in the Arduino loop, Settings changed in this file, search for "SPI"
	
 Pins
	CE   9
	CSN  10
	MOSI 11
	MISO 12
	SCK  13
	IRQ  2
*/ 

#include "nRF24L01p.h"
#include "nRF24L01_define_map.h"
#include "string.h"
#include "SPI.h"

// Used to check the status of a given bit in a variable
#define CHECK_BIT(var,pos) ((var & (1 << pos)) == (1 << pos))


void NRF24L01pClass::init(int _cepin, int _csnpin)
{
	ce_pin = _cepin;
	csn_pin = _csnpin;
}

int NRF24L01pClass::get_ce_pin(void) 
{ 
	return ce_pin; 
}
	
void NRF24L01pClass::setDebugVal(int tmp_debug_val)
{
	debug_val = tmp_debug_val;
}
	
int NRF24L01pClass::getDebugVal(void)
{
	return debug_val;
}
/*SET BIT
Used to more easily set or clear bits in registers etc
@param byteIn is the byte to be worked on
@param bitNum is the bit to change, 0-7
@param setClear is the boolean value to set the bit 1 or 0
*/
unsigned char NRF24L01pClass::setBit(unsigned char byteIn, int bitNum, boolean setClear)
{
	if(setClear == 1)
		byteIn |= (1<<bitNum);
	else
		byteIn &= ~(1<<bitNum);
		
	return byteIn;
}
	

void NRF24L01pClass::begin(void)
{
	// Initialize pins
	pinMode(ce_pin, OUTPUT);
	pinMode(csn_pin, OUTPUT);
	
	SPI.setBitOrder(MSBFIRST);
	SPI.setDataMode(SPI_MODE0);
	// Arduino Uno operates at 16MHz, we want SPI to run at 2MHz
	SPI.setClockDivider(SPI_CLOCK_DIV8);

}

void NRF24L01pClass::setup_data_pipes(unsigned char pipesOn [], unsigned char fixedPayloadWidth [])
{
	writeRegister(EN_RXADDR, pipesOn, 1);
	writeRegister(RX_PW_P0, fixedPayloadWidth, 1);
}



void NRF24L01pClass::writeRegister(unsigned char thisRegister, unsigned char thisValue [], int byteNum)
{
	// Must start with CSN pin high, then bring CSN pin low for the transfer
	// Transmit the command byte
	// Bring CSN pin back to high
	thisRegister = 0x20 | thisRegister;
	digitalWrite(csn_pin, LOW);
	
	SPI.transfer(thisRegister); // This is the register that is being written to
	int ind=0;
	while (ind < byteNum)
	{
		SPI.transfer(thisValue[ind]);
		ind = ind+1;
	}
	
	digitalWrite(csn_pin, HIGH);
}




unsigned char * NRF24L01pClass::readRegister(unsigned char thisRegister, int byteNum)
{
	// Must start with CSN pin high, then bring CSN pin low for the transfer
	// Transmit the command byte and the same number of dummy bytes as expected to receive from the register
	// Read the same number of bytes from radio plus the STATUS register as the first byte returned
	// Bring CSN pin back to high
	digitalWrite(csn_pin, LOW);
	
	SPI.transfer(thisRegister); // This is the register that is being read from
	int ind = 0;
	
	while (ind <= byteNum)
	{
		register_value[ind] = SPI.transfer(0x00); // First byte returned is the status, subsequent bytes are from register
		//Serial.print("Register byte ");
		//Serial.print(ind);
		//Serial.print(" value = ");
		//Serial.println(register_value[ind], BIN);
		ind = ind+1;
	}
	
	digitalWrite(csn_pin, HIGH);
	
	return register_value;
	
}


/* CONFIG
Configure the nRF24L01p and startup
@param RXTX sets the radio into 1:Receive 0:Transmit
@param PWRUP_PWRDOWN 1:Power Up 0:Power Down
*/
void NRF24L01pClass::configRadio(boolean RXTX, boolean PWRUP_PWRDOWN)
{
	// CRC is enabled with a 2-byte encoding scheme
	unsigned char configByte = 0b00001111;
	// Set RXTX bit
	configByte = setBit(configByte,PRIM_RX,RXTX);
	// Set PWRUP_PWRDOWN bit
	configByte = setBit(configByte,PWR_UP,PWRUP_PWRDOWN);
	
	// Write array
	unsigned char writeBytes [5];
	writeBytes[0]=configByte;
	// Write to config register
	writeRegister(CONFIG, writeBytes, 1);
	
}


/* txMode Transmit Mode
Put radio into transmission mode
*/
void NRF24L01pClass::txMode(void)
{
	configRadio(0,1);
	// CE is held LOW unless a packet is being actively transmitted, In which case it is toggled high for >10us
	digitalWrite(ce_pin, LOW);
}
	
/* rMode Receive Mode
Put radio into receiving mode
*/
void NRF24L01pClass::rMode(void)
{
	configRadio(1,1);
	// CE HIGH monitors air and receives packets while in receive mode
	digitalWrite(ce_pin, HIGH);
	// CE LOW puts the chip in standby and it no longer monitors the air
}


/* txData Transmit Data
Transmit data
@param DATA is the data to transmit
@param BYTE_NUM is the number of bytes to transmit 1-5
*/
void NRF24L01pClass::txData(unsigned char DATA [], int BYTE_NUM)
{
	// First the command byte (0xA0, W_TX_PAYLOAD) is sent and then the payload. 
	// The number of payload bytes sent must match the payload length of the receiver you are sending the payload to
	
	// Must start with CSN pin high, then bring CSN pin low for the transfer
	// Transmit the command byte
	// Bring CSN pin back to high
	
	digitalWrite(csn_pin, LOW);
	SPI.transfer(W_TX_PAYLOAD); // This is the register that is being written to
	int ind=0;
	//Serial.println("DATA"); //DEBUG
	while (ind < BYTE_NUM)
	{
		//DEBUG
		//Serial.print("Element ");
		//Serial.print(ind);
		//Serial.print(": ");
		//Serial.println(DATA[ind]);
		
		SPI.transfer(DATA[ind]);
		ind = ind+1;
	}
	digitalWrite(csn_pin, HIGH);
	
	// When sending packets, the CE pin (which is normally held low in TX operation) is set to high for a minimum of 10us to send the packet.
	digitalWrite(ce_pin, HIGH);
	delay(1); 
	digitalWrite(ce_pin, LOW);
	
	// Once the packet was sent, a TX_DS interrupt will occur
	// If auto-ack is enabled on the pipe, then TX_DS flag will only be sent if the packet actually gets through
	/*
	If the maximum amount of retries is hit, then the MAX_RT interrupt will
	become active. At this point, you should clear the interrupts and continue based on
	which interrupt was asserted. Also remember that, like the RX FIFO, the TX FIFO is
	three levels deep. This means that you can load up to three packets into the 24L01�s TX
	FIFO before you do the CE toggle to send them on their way.
	*/
	
	
}


/* rData Receive Data
Receive data
@param DATA is the data to transmit
@param BYTE_NUM is the number of bytes to transmit 1-5
register values are read into NRF24L01Class.register_value array
*/
unsigned char * NRF24L01pClass::rData(int byteNum)
{

	// Bring CE low to disable the receiver
	digitalWrite(ce_pin, LOW);
	
	// Execute R_RX_PAYLOAD operation
	// First the command byte (0x61, R_RX_PAYLOAD) is sent and then the payload.
	// The number of payload bytes sent must match the payload length of the receiver you are sending the payload to
	
	digitalWrite(csn_pin, LOW);
	
	SPI.transfer(R_RX_PAYLOAD); // This is the register that is being read from
	int ind = 0;
	
	while (ind <= byteNum)
	{
		register_value[ind] = SPI.transfer(0x00); // First byte returned is the status, subsequent bytes are from register
		//Serial.print("Register byte ");
		//Serial.print(ind);
		//Serial.print(" value = ");
		//Serial.println(register_value[ind]);
		ind = ind+1;
	}
	
	digitalWrite(csn_pin, HIGH);

	// Bring CE high to re-enable the receiver
	digitalWrite(ce_pin, HIGH);
	
	return register_value;
	
}


/* flushTX Flush TX FIFO

*/
void NRF24L01pClass::flushTX(void)
{
	// Must start with CSN pin high, then bring CSN pin low for the transfer
	// Transmit the command byte
	// Bring CSN pin back to high
	digitalWrite(csn_pin, LOW);
	SPI.transfer(FLUSH_TX); // This is the register that is being written to
	digitalWrite(csn_pin, HIGH);
}

/* flushTX Flush TX FIFO

*/
void NRF24L01pClass::flushRX(void)
{
	// Must start with CSN pin high, then bring CSN pin low for the transfer
	// Transmit the command byte
	// Bring CSN pin back to high
	digitalWrite(csn_pin, LOW);
	SPI.transfer(FLUSH_RX); // This is the register that is being written to
	digitalWrite(csn_pin, HIGH);
}



//NRF24L01pClass NRF24L01p;



