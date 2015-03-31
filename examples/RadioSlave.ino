/* BY: Steve Lammers, www.stevelammers.com
	
	NOTES: This code is designed to work with the SimpleRadio Labview program that
	is currently being developed by Steve Lammers. Master radio is connected
	to the LabView computer. It queries the slave radio for data.
	
	Data and commands are transmitted between LabView and the radio as 2 bytes
	the first is a command byte and the second is a data byte.
	
	TODO: Split code into radio_master and radio_slave
		  Add capability such that Labview can ask remote sensor for data
		  Change addresses
		  Payload width as a easier function
		  Add Labview connection for main receiver
		  New code that works with handshake for mesh network
		  JTAG debugger for AVR
		  
	
	// DEBUG NOTES !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
		if reading RX Data starts acting weird, you may need to myRadio.flushRX();
		This seems to happen when timing gets off. RX FIFO may just fill up and not
		get cleared correctly sometimes? I just needed to use it often when getting
		the transmitter running. Otherwise, each byte int he FIFO would be the same
		as the MSByte or LSByte.
		
	// STATUS println for DEBUG
	unsigned char * ptmp_array;
	Serial.println("----------------------------------");
	Serial.print("STATUS: ");
	ptmp_array = myRadio.readRegister(STATUS,1);
	Serial.println(*ptmp_array, BIN);

	// >> DEBUG >> DEBUG >> DEBUG >> DEBUG for main LOOP-------
	unsigned char * tmpRxData = myRadio.rData(5);
	Serial.println("RX Data: ");
	for (int x=0; x<5; x++)
	{
		Serial.print("Element ");
		Serial.print(x);
		Serial.print(": ");
		Serial.println(*(tmpRxData+x));
	}
	
	myRadio.flushRX();
	// << DEBUG << DEBUG << DEBUG << DEBUG for main LOOP-------

*/

#include <nRF24L01_define_map.h>
#include <nRF24L01p.h>
#include <SPI.h>

//NRF24L01pClass * myRadio;

// Interrupt variable
volatile unsigned char IRQ_state = 0x00;

// Used to check the status of a given bit in a variable
#define CHECK_BIT(var,pos) ((var & (1 << pos)) == (1 << pos))

// GLOBALS >> GLOBALS  >> GLOBALS  >> GLOBALS  >> GLOBALS 
// Set if the radio is transmitter (TX) or receiver (RX)
int radioMode = 1; // radioMode = 1 for RX, 0 for TX
int rxDataFLAG = 0; // Indicates that there is data in the RX FIFO buffer

int CE_pin = 9;
int CSN_pin = 10;
// int airDataRate = 250; //kBps, can be 250, 1000 or 2000 section 6.3.2
//int rfChannelFreq = 0x02; // = 2400 + RF_CH(MHz) section 6.3.3 0x02 is the default
//  RF_CH can be set from 0-83. Any channel higher than 83 is off limits in US by FCC law
//SETUP_AW: AW=11-5 byte address width
//myRadio = new NRF24L01p;
NRF24L01p myRadio(CE_pin,CSN_pin);
//myRadio.init(CE_pin,CSN_pin);

// GLOBALS << GLOBALS  << GLOBALS  << GLOBALS  << GLOBALS 
int signalVal = 0x05; // Dummy signal value for testing

void setup()
{
	/* add setup code here */
	Serial.begin(9600);
	//Serial.println("Begin startup");
	
	// Begin SPI communication
	SPI.begin();
	
	
	
	// Start radio
	myRadio.begin();
	
	// Setup data pipes, addresses etc
	//
	// Use default addresses for now _ CHANGE ADDRESSES HERE IN FUTURE
	unsigned char pipesOn [] = {0x03}; // which pipes to turn on for receiving
	unsigned char fixedPayloadWidth [] = {0x02}; // number of bytes for payload width
	myRadio.setup_data_pipes(pipesOn, fixedPayloadWidth);
	
	//DEBUG - change RX_ADDR_P0 to see if I am reading the right value
	unsigned char tmpArr [] = {0xE7,0xE7,0xE7,0xE7,0xE7};
	myRadio.writeRegister(RX_ADDR_P0,tmpArr, 5);
	
	
	// Configure radio to be TX (transmitter) or RX (receiver)
	if (radioMode)
	{myRadio.rMode();}// Configure radio to be a receiver
	else
	{myRadio.txMode();}// Configure radio to be a receiver

	// Clear any interrupts
	myRadio.clear_interrupts();
	
	// Attach interrupt 0 (digital pin 2 on the Arduino Uno)
	//	This interrupt monitors the interrupt pin (IRQ) from the nRF24L01 Radio
	//  The IRQ is normally high, and active low
	//  The IRQ is triggered at:
	delay(100); // Make sure all the configuration is completed before attaching the interrupt
	attachInterrupt(0, IRQ_resolve, FALLING);
}




void loop()
{
	/* add main program code here */
	int serialCommand = 0; // Serial commands: 0-do nothing
						   //                  1-query radioSlave for data
	int serialData	  = 0; // Data byte
	
	if (IRQ_state == 1)
	{
		unsigned char tmp_status = myRadio.IRQ_reset_and_respond();
		
		if CHECK_BIT(tmp_status,0) // TX_FIFO full
		{
			//Serial.println("TX_FIFO Full");
		}
		if (CHECK_BIT(tmp_status,1)|CHECK_BIT(tmp_status,2)|CHECK_BIT(tmp_status,3)) // TX_FIFO full
		{
			//Serial.println("Pipe Number Changed");
		}
		if CHECK_BIT(tmp_status,4) // Maximum number of TX retries interrupt
		{
			//Serial.println("Max TX retries IRQ");
			myRadio.flushTX();
		}
		if CHECK_BIT(tmp_status,5) // Data sent TX FIFO interrupt
		{
			//Serial.println("Data Sent TX FIFO IRQ");
		}
		if CHECK_BIT(tmp_status,6) // Data ready RX FIFO interrupt
		{
			//Serial.println("Data ready RX FIFO IRQ");
			// Read the data from the R_RX_PAYLOAD
			// RX_P_NO bits 3:1 tell what pipe number the payload is available in 000-101: Data Pipe Number, 110: Not Used, 111: RX_FIFO Empty
			// Get bits 3:1 and right shift to get pipe number
			//pipeNumber = (tmp_status & 0xE) >> 1;
			rxDataFLAG = 1; //Set Rx Data FLAG
		}
		
		IRQ_state = 0; //reset IRQ_state
	}
	
	
	// Receive transmission from master
	if (rxDataFLAG == 1)
	{
		// Get package
		unsigned char * tmpRxData = myRadio.rData(2);
		serialCommand = *(tmpRxData+0);
		serialData    = *(tmpRxData+1);
		// Check Command byte
		if(serialCommand == 0X01) // Read signal and return data to master
		{
			signalVal++; // Dummy signal value for testing
			// Turn Master to transmitter
			myRadio.txMode();
			// MISO Command byte for return signal value: 0x02
			unsigned char tmpData [] = {0x02, signalVal}; // Data needs to be the same size as the fixedDataWidth set in setup
			myRadio.txData(tmpData, 2); // This is currently sending data to pipe 0 at the default address. Change this once the radio is working
			//
			// Turn Master to receiver
			myRadio.rMode();
		}
		
		myRadio.flushRX();
		rxDataFLAG = 0; // reset rxDataFLAG
	}
	
	delay(5); // Short delay to keep everything running well. Make sure the IRQ's get cleared before next loop. etc...
	
}






unsigned char setBit(unsigned char byteIn, int bitNum, boolean setClear)
{
	if(setClear == 1)
	byteIn |= (1<<bitNum);
	else
	byteIn &= ~(1<<bitNum);
	
	return byteIn;
}



//******* INTERRUPTS **************** INTERRUPTS ***************** INTERRUPTS ****************************

/* IRQ_resolve
Resolve the attachInterrupt function quickly
*/
void IRQ_resolve()
{
	// Get the IRQ code from the receiver and assign it to IRQ_state variable
	//unsigned char * p_tmp;
	//Serial.println("IRQ");
	//IRQ_state = * myRadio.readRegister(STATUS,1); // this returns a pointer, so I dereferenced it to the unsigned char for IRQ_state
	IRQ_state = 1;
}



/* IRQ_reset_and_respond
Reset the IRQ in the radio STATUS register
Also resolve the condition which triggered the interrupt
*/
/*
void IRQ_reset_and_respond(void)
{
	//Serial.println(" ------------------ RESPOND TO IRQ --------------------- ");
	unsigned char tmp_state [] = {0x00};
	unsigned char tmp_status = * myRadio.readRegister(STATUS,1);
	
	if CHECK_BIT(tmp_status,0) // TX_FIFO full
	{
		//Serial.println("TX_FIFO Full");
	}
	if (CHECK_BIT(tmp_status,1)|CHECK_BIT(tmp_status,2)|CHECK_BIT(tmp_status,3)) // TX_FIFO full
	{
		//Serial.println("Pipe Number Changed");
	}
	if CHECK_BIT(tmp_status,4) // Maximum number of TX retries interrupt
	{
		//Serial.println("Max TX retries IRQ");
		myRadio.flushTX();
	}
	if CHECK_BIT(tmp_status,5) // Data sent TX FIFO interrupt
	{
		//Serial.println("Data Sent TX FIFO IRQ");
	}
	if CHECK_BIT(tmp_status,6) // Data ready RX FIFO interrupt
	{
		//Serial.println("Data ready RX FIFO IRQ");
		// Read the data from the R_RX_PAYLOAD
		// RX_P_NO bits 3:1 tell what pipe number the payload is available in 000-101: Data Pipe Number, 110: Not Used, 111: RX_FIFO Empty
		// Get bits 3:1 and right shift to get pipe number
		//pipeNumber = (tmp_status & 0xE) >> 1;
		rxDataFLAG = 1; //Set Rx Data FLAG
	}
	
	myRadio.clear_interrupts();
	IRQ_state = 0; //reset IRQ_state
	
}

void clear_interrupts(void)
{
	// Clear any interrupts
	unsigned char tmp_state [] = {1<<RX_DR};
	myRadio.writeRegister(STATUS, tmp_state, 1);
	tmp_state [0] = 1<<TX_DS;
	myRadio.writeRegister(STATUS, tmp_state, 1);
	tmp_state [0] = 1<<MAX_RT;
	myRadio.writeRegister(STATUS, tmp_state, 1);
	// Flush the TX register
	myRadio.flushTX();
}
*/


