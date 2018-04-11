/*!
 * \file ftdi_spi.c
 *
 * \author FTDI
 * \date 20110523
 *
 * Copyright � 2000-2014 Future Technology Devices International Limited
 *
 *
 * THIS SOFTWARE IS PROVIDED BY FUTURE TECHNOLOGY DEVICES INTERNATIONAL LIMITED ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL FUTURE TECHNOLOGY DEVICES INTERNATIONAL LIMITED
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Project: libMPSSE
 * Module: SPI
 *
 * Rivision History:
 * 0.1  - initial version
 * 0.2  - 20110708 - added function SPI_ChangeCS, moved SPI_Read/WriteGPIO to middle layer
 * 0.3  - 20111103 - bugfix: sizeTransferred 0 when SPI_TRANSFER_OPTIONS_SIZE_IN_BYTE
 *				  ENABLE_MULTI_BYTE_TRANSFER - transfer multiple bytes per USB frame
 *				  added function SPI_ReadWrite
 * 0.41 - 20140903 - fixed compile warnings
 */


/******************************************************************************/
/*								Include files					  			  */
/******************************************************************************/
#include "ftdi_infra.h"		/*Common prrtable infrastructure(datatypes, libraries, etc)*/
#include "ftdi_common.h"	/*Common across I2C, SPI, JTAG modules*/
#include "ftdi_spi.h"		/*SPI specific*/
#include "ftdi_mid.h"		/*Middle layer*/


/******************************************************************************/
/*								Macro and type defines					  		  */
/******************************************************************************/
/*Comment the following line to disable parameter checking at the start of each function*/
#define ENABLE_PARAMETER_CHECKING	1

/*The following macros will be used only during development/debugging phase*/

/*Enable to allow only one chip select(DBUS3)*/
/* #define DEVELOPMENT_FIXED_CS		1 */

/*Enable to for no linked list implimentation(only 1 fixed channel)*/
/* #define NO_LINKED_LIST		1 */

/* The early implimentation allowed only one byte data transfer per USB frame.
This macro enables the code to transfer multiple bytes per frame when
SPI_TRANSFER_OPTIONS_SIZE_IN_BYTES bit is set in the transferOptions parameter when
calling SPI_Read or SPI_Write */
#define ENABLE_MULTI_BYTE_TRANSFER	1


/******************************************************************************/
/*								Local function declarations					  */
/******************************************************************************/
/* List management functions */
FT_STATUS SPI_AddChannelConfig(FT_HANDLE handle);
FT_STATUS SPI_DelChannelConfig(FT_HANDLE handle);
FT_STATUS SPI_SaveChannelConfig(FT_HANDLE handle, ChannelConfig *config);
FT_STATUS SPI_GetChannelConfig(FT_HANDLE handle, ChannelConfig **config);
FT_STATUS SPI_DisplayList(void);
/* Read/Write functions */
FT_STATUS SPI_Write8bits(FT_HANDLE handle,uint8 byte, uint8 len);
FT_STATUS SPI_Read8bits(FT_HANDLE handle,uint8 *byte, uint8 len);
//FT_STATUS SPI_ToggleCS(FT_HANDLE handle, bool state);



/******************************************************************************/
/*								Global variables							  */
/******************************************************************************/

#ifdef NO_LINKED_LIST
	ChannelContext channelContext;
#else
/*Root of the linked list that holds channel configurations*/
	ChannelContext *ListHead=NULL;
#endif


/******************************************************************************/
/*						Public function definitions						  */
/******************************************************************************/

/*!
 * \brief Gets the number of SPI channels connected to the host
 *
 * This function gets the number of SPI channels that are connected to the host system
 *
 * \param[out] *numChannels Pointer to variable in which the no of channels will be returned
 * \return Returns status code of type FT_STATUS(see D2XX Programmer's Guide)
 * \sa
 * \note This function doesn't return the number of FTDI chips connected to the host system
 * \note FT2232D has 1 MPSSE port
 * \note FT2232H has 2 MPSSE ports
 * \note FT4232H has 4 ports but only 2 of them have MPSSEs
 * so call to this function will return 2 if a FT4232 is connected to it.
 * \warning
 */
FTDI_API FT_STATUS SPI_GetNumChannels(uint32 *numChannels)
{
	FT_STATUS status;

	FN_ENTER;
#ifdef ENABLE_PARAMETER_CHECKING
	CHECK_NULL_RET(numChannels);
#endif
	status = FT_GetNumChannels(SPI, numChannels);
	CHECK_STATUS(status);
	FN_EXIT;
	return status;
}

/*!
 * \brief Provides information about channel
 *
 * This function takes a channel index (valid values are from 0 to the value returned by
 * I2C_GetNumChannels -1) and provides information about the channel in the form of a
 * populated ChannelInfo structure.
 *
 * \param[in] index Index of the channel
 * \param[out] chanInfo Pointer to FT_DEVICE_LIST_INFO_NODE structure(see D2XX \
Programmer's Guide)
 * \return Returns status code of type FT_STATUS(see D2XX Programmer's Guide)
 * \note  The channel ID can be determined by the user from the last digit of the location ID
 * \sa
 * \warning
 */
FTDI_API FT_STATUS SPI_GetChannelInfo(uint32 index,
					FT_DEVICE_LIST_INFO_NODE *chanInfo)
{
	FT_STATUS status;
	FN_ENTER;
#ifdef ENABLE_PARAMETER_CHECKING
	CHECK_NULL_RET(chanInfo);
#endif
	status = FT_GetChannelInfo(SPI,index+1,chanInfo);
	CHECK_STATUS(status);
	FN_EXIT;
	return status;
}

/*!
 * \brief Opens a channel and returns a handle to it
 *
 * This function opens the indexed channel and returns a handle to it
 *
 * \param[in] index Index of the channel
 * \param[out] handle Pointer to the handle of the opened channel
 * \return Returns status code of type FT_STATUS(see D2XX Programmer's Guide)
 * \sa
 * \note Trying to open an already open channel will return an error code
 * \warning
 */
FTDI_API FT_STATUS SPI_OpenChannel(uint32 index, FT_HANDLE *handle)
{
	FT_STATUS status;
	FN_ENTER;
#ifdef ENABLE_PARAMETER_CHECKING
	CHECK_NULL_RET(handle);
#endif
	/* 1 is added to index because mid layer accepts indices starting from 1*/
	status = FT_OpenChannel(SPI,index+1,handle);
	DBG(MSG_DEBUG,"index=%u handle=%u\n",(unsigned)index,(unsigned)*handle);
	CHECK_STATUS(status);
	if(FT_OK == status)
	{
		status=SPI_AddChannelConfig(*handle);
		CHECK_STATUS(status);
	}
	FN_EXIT;
	return status;
}


/*!
 * \brief Initializes a channel
 *
 * This function initializes the channel and the communication parameters associated with it
 *
 * \param[in] handle Handle of the channel
 * \param[out] config Pointer to ChannelConfig structure(memory to be allocated by caller)
 * \return Returns status code of type FT_STATUS(see D2XX Programmer's Guide)
 * \sa
 * \note
 * \warning
 */
FTDI_API FT_STATUS SPI_InitChannel(FT_HANDLE handle, ChannelConfig *config)
{
	FT_STATUS status;
	uint8 buffer[6];
	uint32 noOfBytes=0;
	uint32 noOfBytesTransferred;
	uint8 mode;
	FN_ENTER;
#ifdef ENABLE_PARAMETER_CHECKING
	CHECK_NULL_RET(config);
	CHECK_NULL_RET(handle);
#endif
	/*This is to ensure that the library puts the lines to correct directions
	even if wrong values are passed by the user */
	/* Set initial direction of line SCLK  as OUT */
	config->initialPinDir |= 0x0001;/*Note: Direction is out if bit is 1!!! */
	/* Set initial direction of  MOSI line as OUT */
	config->initialPinDir |= 0x0002;
	/* Set initial direction of MISO line as IN */
	config->initialPinDir &= 0xFFFB;
	/* Set initial direction of CS line as OUT */
	config->initialPinDir |= \
		((1<<((config->configOptions & SPI_CONFIG_OPTION_CS_MASK)>>2))<<3);

	/*Set initial state of clock line*/
	mode = (config->configOptions & SPI_CONFIG_OPTION_MODE_MASK);
	switch(mode)
	{
		case 0:
		case 1:
			/* clock idle low */
			config->initialPinState &= 0xFFFE;
			break;

		case 2:
		case 3:
			/* clock idle high */
			config->initialPinState |= 0x0001;
			break;
		default:
			DBG(MSG_DEBUG,"invalid mode(%u)\n",(unsigned)mode);
	}

	/*Set initial state of CS line*/
	if (config->configOptions & SPI_CONFIG_OPTION_CS_ACTIVELOW)
	{
		config->initialPinState |= ((1<<((config->configOptions & SPI_CONFIG_OPTION_CS_MASK)>>2))<<3);
	}
	else
	{
		config->initialPinState &= ~((1<<((config->configOptions & SPI_CONFIG_OPTION_CS_MASK)>>2))<<3);
	}

	/* Copy initial state values to present state variable */
	config->currentPinState = (uint16)config->initialPinState;
	config->currentPinDir   = (uint16)config->initialPinDir;

	DBG(MSG_DEBUG,"handle=0x%x ClockRate=%u LatencyTimer=%u Options=0x%x\n",\
		(unsigned)handle,(unsigned)config->ClockRate,	\
		(unsigned)config->LatencyTimer,(unsigned)config->configOptions);

	status = FT_InitChannel(SPI,handle,(uint32)config->ClockRate,	\
		(uint32)config->LatencyTimer,(uint32)config->configOptions);
	CHECK_STATUS(status);
	if(FT_OK == status)
	{
		/* Set the directions and values to the lines */
		buffer[noOfBytes++] = MPSSE_CMD_SET_DATA_BITS_HIGHBYTE;
		buffer[noOfBytes++] = (uint8)((config->currentPinState & 0xFF00)>>8);
		buffer[noOfBytes++] = (uint8)((config->currentPinDir & 0xFF00)>>8); /*Dir*/
		buffer[noOfBytes++] = MPSSE_CMD_SET_DATA_BITS_LOWBYTE;/*MPSSE command*/
		buffer[noOfBytes++] = (uint8)(config->currentPinState & 0x00FF);
		buffer[noOfBytes++] = (uint8)(config->currentPinDir & 0x00FF); /*Dir*/
		status = FT_Channel_Write(SPI,handle,noOfBytes,buffer,\
			&noOfBytesTransferred);
		CHECK_STATUS(status);

		if(FT_OK == status)
		{
			DBG(MSG_DEBUG,"line %u handle=0x%x\n",__LINE__,(unsigned)handle);
			status=SPI_SaveChannelConfig(handle,config);
			CHECK_STATUS(status);
		}
	}
	FN_EXIT;
	return status;
}

/*!
 * \brief Closes a channel
 *
 * Closes a channel and frees all resources that were used by it
 *
 * \param[in] handle Handle of the channel
 * \param[out] none
 * \return Returns status code of type FT_STATUS(see D2XX Programmer's Guide)
 * \sa
 * \note
 * \warning
 */
FTDI_API FT_STATUS SPI_CloseChannel(FT_HANDLE handle)
{
	FT_STATUS status;
	ChannelConfig *config=NULL;
	uint8 buffer[6];
	uint32 noOfBytes=0;
	uint32 noOfBytesTransferred;
	FN_ENTER;
#ifdef ENABLE_PARAMETER_CHECKING
		CHECK_NULL_RET(handle);
#endif
	/* Retrieve final state values for the lines */
	status = SPI_GetChannelConfig(handle,&config);
	CHECK_STATUS(status);

	/* Set lines to final state */
	buffer[noOfBytes++] = MPSSE_CMD_SET_DATA_BITS_HIGHBYTE;/* MPSSE command */
	buffer[noOfBytes++] = ((config->finalPinState & 0xFF00)>>8); /*Value*/
	buffer[noOfBytes++] = ((uint8)(config->finalPinDir & 0xFF00)>>8); /*Direction*/
	buffer[noOfBytes++] = MPSSE_CMD_SET_DATA_BITS_LOWBYTE;/* MPSSE command */
	buffer[noOfBytes++] = (config->finalPinState & 0x00FF); /*Value*/
	buffer[noOfBytes++] = (uint8)(config->finalPinDir & 0x00FF); /*Direction*/
	status = FT_Channel_Write(SPI,handle,noOfBytes,buffer,\
		&noOfBytesTransferred);
	CHECK_STATUS(status);

	status = FT_CloseChannel(SPI,handle);
	CHECK_STATUS(status);
	if(FT_OK == status)
	{
		status=SPI_DelChannelConfig(handle);
		CHECK_STATUS(status);
	}
	FN_EXIT;
	return status;
}

/*!
 * \brief Reads data from a SPI slave device
 *
 * This function reads the specified number of bits or bytes from the SPI device
 *
 * \param[in] handle Handle of the channel
 * \param[in] *buffer Pointer to buffer to where data will be read to
 * \param[in] sizeToTransfer Size of data to be transfered
 * \param[out] sizeTransfered Pointer to variable containing the size of data
 *			that got transferred
 * \param[in] transferOptions This parameter specifies data transfer options
 *				if BIT0 is 0 then size is in bytes, otherwise in bits
 *				if BIT1 is 1 then CHIP_SELECT line will be enables at start of transfer
 *				if BIT2 is 1 then CHIP_SELECT line will be disabled at end of transfer
 *
 * \return Returns status code of type FT_STATUS(see D2XX Programmer's Guide)
 * \sa
 * \note
 * \warning
 */
FTDI_API FT_STATUS SPI_Read(FT_HANDLE handle, uint8 *buffer,
	uint32 sizeToTransfer, uint32 *sizeTransferred, uint32 transferOptions)
{
	FT_STATUS status;
	//uint32 i;
	uint8 byte = 0;
	uint8 bitsToTransfer=0;
	FN_ENTER;
#ifdef ENABLE_PARAMETER_CHECKING
	CHECK_NULL_RET(handle);
	CHECK_NULL_RET(buffer);
	CHECK_NULL_RET(sizeTransferred);
#endif
	LOCK_CHANNEL(handle);

	if(transferOptions & SPI_TRANSFER_OPTIONS_CHIPSELECT_ENABLE)
	{
		/* Enable CHIPSELECT line for the channel */
		status = SPI_ToggleCS(handle,TRUE);
		CHECK_STATUS(status);
	}

	if(transferOptions & SPI_TRANSFER_OPTIONS_SIZE_IN_BITS)
	{/*sizeToTransfer is in bits*/
		*sizeTransferred=0;
		while(*sizeTransferred<sizeToTransfer)
		{
			if((sizeToTransfer - *sizeTransferred)>=8)
				bitsToTransfer = 8;
			else
				bitsToTransfer = (uint8)(sizeToTransfer - *sizeTransferred);
			status = SPI_Read8bits(handle,&byte, bitsToTransfer);
			buffer[(*sizeTransferred+1)/8] = byte;
			CHECK_STATUS(status);
			if(FT_OK == status)
				*sizeTransferred += bitsToTransfer;
		}
	}
	else
	{/*sizeToTransfer is in bytes*/
		uint32 noOfBytes=0,noOfBytesTransferred=0;
		uint8 cmdBuffer[10];
		ChannelConfig *config=NULL;
		uint8 mode;

		status = SPI_GetChannelConfig(handle,&config);
		CHECK_STATUS(status);
		/*mode is given by bit1-bit0 of ChannelConfig.Options*/
		mode = (config->configOptions & SPI_CONFIG_OPTION_MODE_MASK);
		/* Command to write 8bits */
		switch(mode)
		{
			case 0:
				cmdBuffer[noOfBytes++] = MPSSE_CMD_DATA_IN_BYTES_POS_EDGE;
				break;
			case 1:
				cmdBuffer[noOfBytes++] = MPSSE_CMD_DATA_IN_BYTES_NEG_EDGE;
				break;
			case 2:
				cmdBuffer[noOfBytes++] = MPSSE_CMD_DATA_IN_BYTES_NEG_EDGE;
				break;
			case 3:
				cmdBuffer[noOfBytes++] = MPSSE_CMD_DATA_IN_BYTES_POS_EDGE;
				break;
			default:
				DBG(MSG_DEBUG,"invalid mode(%u)\n",(unsigned)mode);
		}
		/* length LSB */
		cmdBuffer[noOfBytes++] = (uint8)((sizeToTransfer-1) & 0x000000FF) ;
		/* length MSB */
		cmdBuffer[noOfBytes++] = (uint8)(((sizeToTransfer-1) & 0x0000FF00)>>8);

		/*Command MPSSE to send data to PC immediately */
		cmdBuffer[noOfBytes++] = MPSSE_CMD_SEND_IMMEDIATE;
		status = FT_Channel_Write(SPI,handle,noOfBytes,cmdBuffer,
			&noOfBytesTransferred);
		CHECK_STATUS(status);

		status = FT_Channel_Read(SPI,handle,sizeToTransfer,buffer,
			sizeTransferred);
		CHECK_STATUS(status);
		DBG(MSG_DEBUG,"sizeToTransfer=%u sizeTransferred=%u cmdBuffer[0]=0x%x  \
			cmdBuffer[1]=0x%x cmdBuffer[2]=0x%x buffer[0]=0x%x buffer[1]=0x%x\n"
			,sizeToTransfer,sizeTransferred,cmdBuffer[0],cmdBuffer[1],
			cmdBuffer[2],buffer[0],buffer[1]);
	}

	if(transferOptions & SPI_TRANSFER_OPTIONS_CHIPSELECT_DISABLE)
	{
		/* Disable CHIPSELECT line for the channel */
		status = SPI_ToggleCS(handle,FALSE);
		CHECK_STATUS(status);
	}
	UNLOCK_CHANNEL(handle);
	DBG(MSG_DEBUG,"sizeToTransfer=%u  sizeTransferred=%u BitMode=%u \
		CS_Enable=%u CS_Disable=%u\n", sizeToTransfer,*sizeTransferred,
		(unsigned)(transferOptions & SPI_TRANSFER_OPTIONS_SIZE_IN_BITS),
		(unsigned)(transferOptions & SPI_TRANSFER_OPTIONS_CHIPSELECT_ENABLE),
		(unsigned)(transferOptions & SPI_TRANSFER_OPTIONS_CHIPSELECT_DISABLE));
	FN_EXIT;
	return status;
}

/*!
 * \brief Writes data to a SPI slave device
 *
 * This function writes the specified number of bits or bytes to the SPI device
 *
 * \param[in] handle Handle of the channel
 * \param[in] *buffer Pointer to buffer from containing the data
 * \param[in] sizeToTransfer Size of data to be transfered
 * \param[out] sizeTransfered Pointer to variable containing the size of data
 *			that got transferred
 * \param[in] transferOptions This parameter specifies data transfer options
 *				if BIT0 is 0 then size is in bytes, otherwise in bits
 *				if BIT1 is 1 then CHIP_SELECT line will be enables at start of transfer
 *				if BIT2 is 1 then CHIP_SELECT line will be disabled at end of transfer
 *
 * \return Returns status code of type FT_STATUS(see D2XX Programmer's Guide)
 * \sa
 * \note
 * \warning
 */
FTDI_API FT_STATUS SPI_Write(FT_HANDLE handle, uint8 *buffer,
	uint32 sizeToTransfer, uint32 *sizeTransferred, uint32 transferOptions)
{
	FT_STATUS status;
	ChannelConfig *config=NULL;
	uint8 byte;
	uint8 bitsToTransfer=0;
	FN_ENTER;

#ifdef ENABLE_PARAMETER_CHECKING
	CHECK_NULL_RET(handle);
	CHECK_NULL_RET(buffer);
	CHECK_NULL_RET(sizeTransferred);
#endif
	LOCK_CHANNEL(handle);
	status = SPI_GetChannelConfig(handle,&config);
	CHECK_STATUS(status);
	/* Mode is given by bit1-bit0 of ChannelConfig.Options */
	DBG(MSG_DEBUG,"configOptions=0x%x\n",(unsigned)config->configOptions);
	DBG(MSG_DEBUG,"LatencyTimer=%u\n",(unsigned)config->LatencyTimer);
	if(transferOptions & SPI_TRANSFER_OPTIONS_CHIPSELECT_ENABLE)
	{
		/* enable CHIPSELECT line for the channel */
		status = SPI_ToggleCS(handle,TRUE);
		CHECK_STATUS(status);
	}

	if(transferOptions & SPI_TRANSFER_OPTIONS_SIZE_IN_BITS)
	{/* sizeToTransfer is in bits */
		*sizeTransferred=0;
		/* loop until all the bits are transferred */
		while(*sizeTransferred<sizeToTransfer)
		{
			if((sizeToTransfer - *sizeTransferred)>=8)
				bitsToTransfer = 8;
			else
				bitsToTransfer = (uint8)(sizeToTransfer - *sizeTransferred);
			byte = buffer[(*sizeTransferred+1)/8];
			status = SPI_Write8bits(handle,byte,bitsToTransfer);
			CHECK_STATUS(status);
			if(FT_OK == status)
				*sizeTransferred += bitsToTransfer;
		}
	}
	else
	{/* sizeToTransfer is in bytes */
		uint32 noOfBytes=0,noOfBytesTransferred=0;
		uint8 cmdBuffer[3];
		ChannelConfig *config=NULL;
		uint8 mode;

		status = SPI_GetChannelConfig(handle,&config);
		CHECK_STATUS(status);
		/*mode is given by bit1-bit0 of ChannelConfig.Options*/
		mode = (config->configOptions & SPI_CONFIG_OPTION_MODE_MASK);
		/* Command to write 8bits */
		switch(mode)
		{
			case SPI_CONFIG_OPTION_MODE0:
				cmdBuffer[noOfBytes++] = MPSSE_CMD_DATA_OUT_BYTES_NEG_EDGE;
				break;
			case SPI_CONFIG_OPTION_MODE1:
				cmdBuffer[noOfBytes++] = MPSSE_CMD_DATA_OUT_BYTES_POS_EDGE;
				break;
			case SPI_CONFIG_OPTION_MODE2:
				cmdBuffer[noOfBytes++] = MPSSE_CMD_DATA_OUT_BYTES_POS_EDGE;
				break;
			case SPI_CONFIG_OPTION_MODE3:
				cmdBuffer[noOfBytes++] = MPSSE_CMD_DATA_OUT_BYTES_NEG_EDGE;
				break;
			default:
				DBG(MSG_DEBUG,"invalid mode(%u)\n",(unsigned)mode);
		}
		/* length low byte */
		cmdBuffer[noOfBytes++] = (uint8)((sizeToTransfer-1) & 0x000000FF);
		/* length high byte */
		cmdBuffer[noOfBytes++] = (uint8)(((sizeToTransfer-1) & 0x0000FF00)>>8);
		/* write command */
		status = FT_Channel_Write(SPI,handle,noOfBytes,cmdBuffer,\
			&noOfBytesTransferred);
		CHECK_STATUS(status);
		/* write data */
		status = FT_Channel_Write(SPI,handle,sizeToTransfer,buffer,\
			sizeTransferred);
		CHECK_STATUS(status);
	}

	if(transferOptions & SPI_TRANSFER_OPTIONS_CHIPSELECT_DISABLE)
	{
		/* disable CHIPSELECT line for the channel */
		status = SPI_ToggleCS(handle,FALSE);
		CHECK_STATUS(status);
	}
	UNLOCK_CHANNEL(handle);
	DBG(MSG_DEBUG,"sizeToTransfer=%u  sizeTransferred=%u BitMode=%u \
		CS_Enable=%u CS_Disable=%u\n",sizeToTransfer,*sizeTransferred,		\
		(unsigned)(transferOptions & SPI_TRANSFER_OPTIONS_SIZE_IN_BITS),	\
		(unsigned)(transferOptions & SPI_TRANSFER_OPTIONS_CHIPSELECT_ENABLE),\
		(unsigned)(transferOptions & SPI_TRANSFER_OPTIONS_CHIPSELECT_DISABLE));
	FN_EXIT;
	return status;
}


/*!
 * \brief Reads and writes data from/to a SPI slave device
 *
 * This function transfers data in both directions between a SPI master and a slave. One bit is
 * clocked out and one bit is clocked in during every clock.
 *
 * \param[in] handle Handle of the channel
 * \param[in] *inBuffer Pointer to buffer to which data read will be stored
 * \param[in] *outBuffer Pointer to buffer that contains data to be transferred to the slave
 * \param[in] sizeToTransfer Size of data to be transferred
 * \param[out] sizeTransfered Pointer to variable containing the size of data
 *			that got transferred
 * \param[in] transferOptions This parameter specifies data transfer options
 *				if BIT0 is 0 then size is in bytes, otherwise in bits
 *				if BIT1 is 1 then CHIP_SELECT line will be enables at start of transfer
 *				if BIT2 is 1 then CHIP_SELECT line will be disabled at end of transfer
 *
 * \return Returns status code of type FT_STATUS(see D2XX Programmer's Guide)
 * \sa
 * \note
 * \warning
 */
FTDI_API FT_STATUS SPI_ReadWrite(FT_HANDLE handle, uint8 *inBuffer,
	uint8 *outBuffer, uint32 sizeToTransfer, uint32 *sizeTransferred,
	uint32 transferOptions)
{
	FT_STATUS status;
	ChannelConfig *config=NULL;
	uint8 mode;
	uint8 bitsToTransfer=0;
	uint32 noOfBytes=0,noOfBytesTransferred=0;
	uint8 cmdBuffer[10];
	FN_ENTER;

#ifdef ENABLE_PARAMETER_CHECKING
	CHECK_NULL_RET(handle);
	CHECK_NULL_RET(inBuffer);
	CHECK_NULL_RET(outBuffer);
	CHECK_NULL_RET(sizeTransferred);
#endif

	LOCK_CHANNEL(handle);
	status = SPI_GetChannelConfig(handle,&config);
	CHECK_STATUS(status);

	/*mode is given by bit1-bit0 of ChannelConfig.Options*/
	mode = (config->configOptions & SPI_CONFIG_OPTION_MODE_MASK);

	if(transferOptions & SPI_TRANSFER_OPTIONS_CHIPSELECT_ENABLE)
	{
		/* enable CHIPSELECT line for the channel */
		status = SPI_ToggleCS(handle,TRUE);
		CHECK_STATUS(status);
	}

	/* start of transfer */
	if(transferOptions & SPI_TRANSFER_OPTIONS_SIZE_IN_BITS)
	{/* sizeToTransfer is in bits */
		*sizeTransferred=0;
		/* Command to write 8bits */
		switch(mode)
		{
			case SPI_CONFIG_OPTION_MODE0:
				cmdBuffer[0] = MPSSE_CMD_DATA_BITS_IN_POS_OUT_NEG_EDGE;
				break;
			case SPI_CONFIG_OPTION_MODE1:
				cmdBuffer[0] = MPSSE_CMD_DATA_BITS_IN_NEG_OUT_POS_EDGE;
				break;
			case SPI_CONFIG_OPTION_MODE2:
				cmdBuffer[0] = MPSSE_CMD_DATA_BITS_IN_NEG_OUT_POS_EDGE;
				break;
			case SPI_CONFIG_OPTION_MODE3:
				cmdBuffer[0] = MPSSE_CMD_DATA_BITS_IN_POS_OUT_NEG_EDGE;
				break;
			default:
				DBG(MSG_DEBUG,"invalid mode(%u)\n",(unsigned)mode);
		}
		while(*sizeTransferred < sizeToTransfer)
		{
			if((sizeToTransfer - *sizeTransferred)>=8)
				bitsToTransfer = 8;
			else
				bitsToTransfer = (uint8)(sizeToTransfer - *sizeTransferred);
			cmdBuffer[1] = bitsToTransfer - 1; /*takes value 0 for 1 bit; 7 for 8 bits*/
			cmdBuffer[2] = outBuffer[(*sizeTransferred+1)/8];

			/*Write command and data*/
			status = FT_Channel_Write(SPI,handle,3,cmdBuffer,\
				&noOfBytesTransferred);
			CHECK_STATUS(status);
			if(3 > noOfBytesTransferred)
			{/*timeout occured if FT_OK is returned but transferred length is requested len*/
				DBG(MSG_ERR,"Timeout occured. RequestedTxLen=3 TxLen=%u \n",\
					(unsigned)noOfBytesTransferred);
			}

			/*Read from buffer*/
			status = FT_Channel_Read(SPI,handle,1,\
				inBuffer+((*sizeTransferred+1)/8),&noOfBytesTransferred);
			CHECK_STATUS(status);
			if(1 > noOfBytesTransferred)
			{/*timeout occured if FT_OK is returned but transferred length is requested len*/
				DBG(MSG_ERR,"Timeout occured. RequestedTxLen=1 TxLen=%u \n",\
					(unsigned)noOfBytesTransferred);
			}

			if(FT_OK == status)
				*sizeTransferred += bitsToTransfer;
		}
	}
	else
	{/*sizeToTransfer is in bytes*/
		*sizeTransferred=0;
		/* Command to write 8bits */
		switch(mode)
		{
			case SPI_CONFIG_OPTION_MODE0:
				cmdBuffer[0] = MPSSE_CMD_DATA_BYTES_IN_POS_OUT_NEG_EDGE;
				break;
			case SPI_CONFIG_OPTION_MODE1:
				cmdBuffer[0] = MPSSE_CMD_DATA_BYTES_IN_NEG_OUT_POS_EDGE;
				break;
			case SPI_CONFIG_OPTION_MODE2:
				cmdBuffer[0] = MPSSE_CMD_DATA_BYTES_IN_NEG_OUT_POS_EDGE;
				break;
			case SPI_CONFIG_OPTION_MODE3:
				cmdBuffer[0] = MPSSE_CMD_DATA_BYTES_IN_POS_OUT_NEG_EDGE;
				break;
			default:
				DBG(MSG_DEBUG,"invalid mode(%u)\n",(unsigned)mode);
		}
		cmdBuffer[1] = (uint8)((sizeToTransfer-1) & 0x000000FF);/* lengthL */
		cmdBuffer[2] = (uint8)(((sizeToTransfer-1) & 0x0000FF00)>>8);/*lenghtH*/

		/*Write command*/
		status = FT_Channel_Write(SPI,handle,3,cmdBuffer,&noOfBytesTransferred);
		CHECK_STATUS(status);

		/*Write data*/
		noOfBytes = sizeToTransfer;
		status = FT_Channel_Write(SPI,handle,noOfBytes,outBuffer,\
			&noOfBytesTransferred);
		CHECK_STATUS(status);
		#if 0
		{//for debugging
			int i;
			printf("\nnoOfBytes=%d noOfBytesTransferred=%d data=",noOfBytes,noOfBytesTransferred);
			for(i=0;i<noOfBytes;i++)
			{
				printf(" 0x%x",outBuffer[i]);
			}
			printf("\n");
		}
		#endif

		/*Read from buffer*/
		status = FT_Channel_Read(SPI,handle,sizeToTransfer,inBuffer,\
			sizeTransferred);
		CHECK_STATUS(status);
		#if 0
		{//for debugging
			int i;
			printf("\nsizeToTransfer=%d sizeTransferred=%d data=",sizeToTransfer,*sizeTransferred);
			for(i=0;i<*sizeTransferred;i++)
			{
				printf(" 0x%x",outBuffer[i]);
			}
			printf("\n");
		}
		#endif
	}
	/* end of transfer */

	if(transferOptions & SPI_TRANSFER_OPTIONS_CHIPSELECT_DISABLE)
	{
		/* disable CHIPSELECT line for the channel */
		status = SPI_ToggleCS(handle,FALSE);
		CHECK_STATUS(status);
	}
	UNLOCK_CHANNEL(handle);

	FN_EXIT;
	return status;
}


/*!
 * \brief Read the state of SPI MISO line
 *
 * Reads the logic state of the SPI MISO line without clocking the bus
 *
 * \param[in] handle Handle of the channel
 * \param[out] *state State of the line
 * \return Returns status code of type FT_STATUS(see D2XX Programmer's Guide)
 * \sa
 * \note This function may be used for applications that needs to poll the state of
 *		the MISO line to check if the device is busy
 * \warning
 */
FTDI_API FT_STATUS SPI_IsBusy(FT_HANDLE handle, bool *state)
{
	FT_STATUS status=FT_OTHER_ERROR;
	uint32 noOfBytes=0,noOfBytesTransferred=0;
	uint8 buffer[10];

	FN_ENTER;
	/*Enable CS*/
	SPI_ToggleCS(handle, TRUE);
	/*Send command to read*/
	buffer[noOfBytes++]=MPSSE_CMD_GET_DATA_BITS_LOWBYTE;
	buffer[noOfBytes++]=MPSSE_CMD_SEND_IMMEDIATE;
	status = FT_Channel_Write(SPI,handle,noOfBytes,buffer,\
		&noOfBytesTransferred);
	CHECK_STATUS(status);

	/*Read*/
	noOfBytes=1;
	noOfBytesTransferred=0;
	status = FT_Channel_Read(SPI,handle,noOfBytes,buffer,&noOfBytesTransferred);
	CHECK_STATUS(status);
	DBG(MSG_DEBUG,"Low byte read = 0x%x\n",buffer[0]);
	if(0 == (buffer[0] && 0x04))
		*state=FALSE;
	else
		*state=TRUE;

	/*Disable CS*/
	SPI_ToggleCS(handle, FALSE);
	FN_EXIT;
	return status;
}

/*!
 * \brief Changes the chip select line
 *
 * This function changes the chip select line that is to be used to communicate to the SPI slave
 *
 * \param[in] handle Handle of the channel
 * \param[in] configOptions Provides a way to select the chip select line & SPI mode
 *	BIT1-0=CPOL-CPHA:	00 - MODE0 - data captured on rising edge, propagated on falling
 *						01 - MODE1 - data captured on falling edge, propagated on rising
 *						10 - MODE2 - data captured on falling edge, propagated on rising
 *						11 - MODE3 - data captured on rising edge, propagated on falling
 *	BIT4-BIT2: 000 - A/B/C/D_DBUS3=ChipSelect
 *			 : 001 - A/B/C/D_DBUS4=ChipSelect
 * 			 : 010 - A/B/C/D_DBUS5=ChipSelect
 *			 : 011 - A/B/C/D_DBUS6=ChipSelect
 *			 : 100 - A/B/C/D_DBUS7=ChipSelect
 *	BIT5: ChipSelect is active high if this bit is 0
 *	BIT6 -BIT31		: Reserved
 * \return Returns status code of type FT_STATUS(see D2XX Programmer's Guide)
 * \sa
 * \note This function should only be called after SPI_Init has been called
 * \warning
 */
FTDI_API FT_STATUS SPI_ChangeCS(FT_HANDLE handle, uint32 configOptions)
{
	FT_STATUS status=FT_OTHER_ERROR;
	uint8 mode;
#if 1
	uint8 buffer[5];
	uint32 noOfBytes=0;
	uint32 noOfBytesTransferred;
#endif
	ChannelConfig *config=NULL;
	FN_ENTER;
#ifdef ENABLE_PARAMETER_CHECKING
	CHECK_NULL_RET(handle);
#endif

	status = SPI_GetChannelConfig(handle,&config);
	CHECK_STATUS(status);
	/* Replace config options with new values */
	config->configOptions = configOptions;
	/* Ensure new CS lins is set as OUT */
	config->currentPinState |= \
		((1<<((config->configOptions & SPI_CONFIG_OPTION_CS_MASK)>>2))<<3);

	DBG(MSG_DEBUG,"handle=0x%x configOptions=0x%x \n",\
		(unsigned)handle,(unsigned)configOptions);

	/*Set initial state of clock line*/
	mode = (config->configOptions & SPI_CONFIG_OPTION_MODE_MASK);
	switch(mode)
	{
		case 0:
		case 1:
			/* clock idle low */
			config->currentPinState &= 0xFFFE;
			break;

		case 2:
		case 3:
			/* clock idle high */
			config->currentPinState |= 0x0001;
			break;
		default:
			DBG(MSG_DEBUG,"invalid mode(%u)\n",(unsigned)mode);
	}

	buffer[noOfBytes++] = MPSSE_CMD_SET_DATA_BITS_LOWBYTE;/* MPSSE command */
	buffer[noOfBytes++] = (uint8)(config->currentPinState & 0x00FF);/*Val*/
	buffer[noOfBytes++] = (uint8)(config->currentPinDir & 0x00FF); /*Dir*/

	status = FT_Channel_Write(SPI,handle,noOfBytes,buffer,\
			&noOfBytesTransferred);
	CHECK_STATUS(status);

	status=SPI_SaveChannelConfig(handle,config);
	CHECK_STATUS(status);

	FN_EXIT;
	return status;
}

/*!
 * \brief Writes to the available GPIO lines
 *
 * Writes to the GPIO lines not associated with the SPI pins of the MPSSE channel
 *
 * \param[in] handle Handle of the channel
 * \param[in] dir The direction of the lines. 0 for in and 1 for out
 * \param[in] value Output state of the GPIO lines
 * \return status
 * \sa
 * \note
 * \warning
 */
FTDI_API FT_STATUS SPI_WriteGPIO(FT_HANDLE handle, uint16 dir, uint16 value)
{
	FT_STATUS status=FT_OTHER_ERROR;
	ChannelConfig *config=NULL;
	uint16 currentValue;

	FN_ENTER;
#ifdef ENABLE_PARAMETER_CHECKING
	CHECK_NULL_RET(handle);
#endif

	status = SPI_GetChannelConfig(handle, &config);
	CHECK_STATUS(status);

	if (NULL == config)
	{
		return FT_OTHER_ERROR;
	}

	// Dir should always be output for SCLK, MOSI and CS, input for MISO.
	dir |= (1 << 0) | (1 << 1) | ((1<<((config->configOptions & SPI_CONFIG_OPTION_CS_MASK)>>2))<<3);
	dir &= ~(1 << 2);

	// Value should not modify SPI pins and CS.
	currentValue = config->currentPinState;
	CHECK_STATUS(status);

	value &= ~(0x07 | ((1<<((config->configOptions & SPI_CONFIG_OPTION_CS_MASK)>>2))<<3)); // Clear SPI pins in new value.
	value |= (currentValue & 0x07) | (currentValue & ((1<<((config->configOptions & SPI_CONFIG_OPTION_CS_MASK)>>2))<<3)); // OR in any high SPI pins to new value.

	status = FT_WriteGPIO(handle, dir, value);
	CHECK_STATUS(status);

	// Save value of pins into config so it does not get overriden by other functions.
	config->currentPinState = value;
	config->currentPinDir = dir;

	FN_EXIT;
	return status;
}

/*!
 * \brief Reads from the 16 GPIO lines
 *
 * This function reads the GPIO lines of the MPSSE channel
 *
 * \param[in] handle Handle of the channel
 * \param[out] *value Input state of the 16 GPIO lines(1=high)
 * \return status
 * \sa
 * \note The directions of the GPIO pins have to be first set to input mode using FT_WriteGPIO
 * \warning
 */
FTDI_API FT_STATUS SPI_ReadGPIO(FT_HANDLE handle,uint16 *value)
{
	FT_STATUS status=FT_OTHER_ERROR;

	FN_ENTER;
#ifdef ENABLE_PARAMETER_CHECKING
	CHECK_NULL_RET(handle);
	CHECK_NULL_RET(value);
#endif
	status = FT_ReadGPIO(handle, value);
	CHECK_STATUS(status);

	FN_EXIT;
	return status;
}

/*!
 * \brief Sets the SPI clock rate.
 *
 * This function sets the SPI clock rate of the MPSSE channel
 *
 * \param[in] handle Handle of the channel
 * \param[in] clock SPI clock rate to set for this channel
 * \return status
 * \sa
 * \note
 * \warning
 */
FTDI_API FT_STATUS SPI_SetClock(FT_HANDLE handle, uint32 clock)
{
	FT_STATUS status=FT_OTHER_ERROR;
	FT_DEVICE device;

	FN_ENTER;
#ifdef ENABLE_PARAMETER_CHECKING
	CHECK_NULL_RET(handle);
#endif
	status = FT_GetFtDeviceType(handle, &device);
	CHECK_STATUS(status);

	status = FT_SetClock(handle, device, clock);
	CHECK_STATUS(status);

	FN_EXIT;
	return status;
}

/*!
 * \brief Sets the SPI mode.
 *
 * This function sets the SPI mode of the MPSSE channel
 *
 * \param[in] handle Handle of the channel
 * \param[in] mode SPI mode to set for this channel
 * \return status
 * \sa
 * \note
 * \warning
 */
FTDI_API FT_STATUS SPI_SetMode(FT_HANDLE handle, uint8 mode)
{
	FT_STATUS status=FT_OTHER_ERROR;
	ChannelConfig *config=NULL;

	FN_ENTER;
#ifdef ENABLE_PARAMETER_CHECKING
	CHECK_NULL_RET(handle);
#endif
	// Check mode is valid.
	if (SPI_CONFIG_OPTION_MODE3 < mode)
	{
		return FT_INVALID_PARAMETER;
	}

	status = SPI_GetChannelConfig(handle, &config);
	CHECK_STATUS(status);

	if (NULL == config)
	{
		return FT_OTHER_ERROR;
	}

	config->configOptions &= ~(SPI_CONFIG_OPTION_MODE_MASK);
	config->configOptions |= mode & SPI_CONFIG_OPTION_MODE_MASK;

	/*Set initial state of clock line*/
	switch(mode)
	{
		case 0:
		case 1:
			/* clock idle low */
			config->initialPinState &= 0xFFFE;
			config->currentPinState &= 0xFFFE;
			break;

		case 2:
		case 3:
			/* clock idle high */
			config->initialPinState |= 0x0001;
			config->currentPinState |= 0x0001;
			break;
		default:
			DBG(MSG_DEBUG,"invalid mode(%u)\n",(unsigned)mode);
	}

	status = FT_WriteGPIO(handle, config->currentPinDir, config->currentPinState);
	CHECK_STATUS(status);

	FN_EXIT;
	return status;
}

/******************************************************************************/
/*						Local function definations						  */
/******************************************************************************/

/*!
 * \brief Allocates storage in the system to store channel configuration data
 *
 * This function adds a link in a linked list to store channel configuration
 * information when they are passed by the user in SPI_OpenChannel.
 *
 * \param[in] handle Handle of the channel
 * \return Returns status code of type FT_STATUS(see D2XX Programmer's Guide)
 * \sa
 * \note This function should be called after a channel has been successfully
 *	     opened and a handle to it has been assigned
 * \warning
 */
FT_STATUS SPI_AddChannelConfig(FT_HANDLE handle)
{
	FT_STATUS status=FT_OTHER_ERROR;
	ChannelContext *tempNode=NULL;
	ChannelContext *lastNode=NULL;
	FN_ENTER;
	DBG(MSG_DEBUG,"line %u handle=0x%x\n",__LINE__,(unsigned)handle);

#ifdef NO_LINKED_LIST
	status = FT_OK;
#else
	if(NULL == ListHead)
	{/* Add first node */
		ListHead = (ChannelContext *) INFRA_MALLOC(sizeof(ChannelContext));
		if(NULL == ListHead)
		{
			status = FT_INSUFFICIENT_RESOURCES;
			DBG(MSG_ERR,"Failed allocating memory\n");
		}
		else
		{
			ListHead->handle = handle;
			ListHead->next = NULL;
			status = FT_OK;
		}
	}
	else
	{/* Add subsequent nodes */
		/* Traverse list */
		for(tempNode=ListHead; NULL != tempNode; tempNode=tempNode->next)
		{
			lastNode=tempNode;
		}
		tempNode = (ChannelContext *) INFRA_MALLOC(sizeof(ChannelContext));
		if(NULL == tempNode)
		{
			status = FT_INSUFFICIENT_RESOURCES;
			DBG(MSG_ERR,"Failed allocating memory\n");
		}
		else
		{
			tempNode->handle = handle;
			tempNode->next = NULL;
			lastNode->next = tempNode;
			status = FT_OK;
		}
	}
#endif
	FN_EXIT;
#ifdef INFRA_DEBUG_ENABLE
	SPI_DisplayList();
#endif
	return status;
}

/*!
 * \brief Deletes storage allocated for channel configuration data
 *
 * This function traverses the channel configuration data linked list,
 * finds the channel with the given handle and then deletes it
 *
 * \param[in] handle Handle of the channel
 * \return Returns status code of type FT_STATUS(see D2XX Programmer's Guide)
 * \sa
 * \note
 * \warning
 */
FT_STATUS SPI_DelChannelConfig(FT_HANDLE handle)
{
	FT_STATUS status=FT_OTHER_ERROR;
	ChannelContext *tempNode;
	ChannelContext *lastNode=NULL;
	FN_ENTER;

#ifdef NO_LINKED_LIST
	status = FT_OK;
#else
	if(NULL == ListHead)
	{
		DBG(MSG_NOTICE,"List is empty\n");
	}
	else
	{
		for(tempNode=ListHead; NULL != tempNode;
			lastNode=tempNode, tempNode=tempNode->next)
		{
			if(tempNode->handle == handle)
			{/*Node found*/
				if(tempNode == ListHead)
				{/* Is the first node */
					INFRA_FREE(ListHead);
					ListHead = NULL;
				}
				else if(NULL == tempNode->next)
				{/*Last node*/
					lastNode->next = NULL;
					INFRA_FREE(tempNode);
				}
				else
				{/* Middle node */
					lastNode->next = tempNode->next;
					INFRA_FREE(tempNode);
				}
				status = FT_OK;
				break;
			}
		}
	}
#endif
	status=FT_OK;
	FN_EXIT;
#ifdef INFRA_DEBUG_ENABLE
	SPI_DisplayList();
#endif
	return status;
}


/*!
 * \brief Saves the channel's configuration data
 *
 * This function traverses the channel configuration data linked list
 * and if the node with the provided handle is founed then saves the
 * channel configuration data that is provided into the memory locations
 * that were previously allocated using SPI_AddChannelConfig
 *
 * \param[in] handle Handle of the channel
 * \param[in] config Pointer to ChannelConfig structure
 * \return Returns status code of type FT_STATUS(see D2XX Programmer's Guide)
 * \sa
 * \note
 * \warning
 */
FT_STATUS SPI_SaveChannelConfig(FT_HANDLE handle, ChannelConfig *config)
{
	FT_STATUS status=FT_OTHER_ERROR;
	ChannelContext *tempNode=NULL;
	FN_ENTER;

#ifdef NO_LINKED_LIST
		memcpy(&channelContext.config,config,sizeof(ChannelConfig));
		channelContext.handle = handle;
		status = FT_OK;
#else
	if(NULL == ListHead)
	{
		DBG(MSG_NOTICE,"List is empty\n");
	}
	else
	{
		tempNode = ListHead;
		for(tempNode = ListHead; 0 != tempNode; tempNode=tempNode->next)
		{
			DBG(MSG_DEBUG,"line=%d tempNode->handle=0x%x handle=0x%x tempNode->\
				next=0x%x\n",__LINE__,(unsigned)tempNode->handle,
				(unsigned)handle, (unsigned)tempNode->next);
			if(tempNode->handle == handle)
			{/*Node found*/
				INFRA_MEMCPY(&(tempNode->config),config,sizeof(ChannelConfig));
				status = FT_OK;
			}
		}
	}
#endif

	FN_EXIT;
#ifdef INFRA_DEBUG_ENABLE
		SPI_DisplayList();
#endif
	return status;
}

/*!
 * \brief Retrieves the pointer to the channel's configuration data
 *
 * This function traverses the channel configuration data linked list
 * and if the node with the provided handle is founed then it provides
 * the address of the associated channel configuration data structure
 * in the pointer provided in the second parameter of the function
 *
 * \param[in] handle Handle of the channel
 * \param[in] config Pointer to ChannelConfig structure
 * \return Returns status code of type FT_STATUS(see D2XX Programmer's Guide)
 * \sa
 * \note
 * \warning
 */
FT_STATUS SPI_GetChannelConfig(FT_HANDLE handle, ChannelConfig **config)
{
	FT_STATUS status=FT_OTHER_ERROR;
	ChannelContext *tempNode=NULL;
	FN_ENTER;

#ifdef NO_LINKED_LIST
		if(handle == channelContext.handle)
		{
			*config= &(channelContext.config);
			status = FT_OK;
		}
		else
			DBG(MSG_DEBUG,"handle not found in channel config list\n");
#else
	if(NULL == ListHead)
	{
		DBG(MSG_NOTICE,"List is empty\n");
	}
	else
	{
		for(tempNode=ListHead; NULL != tempNode; tempNode=tempNode->next)
		{
			if(tempNode->handle == handle)
			{/*Node found*/
				*config = &(tempNode->config);
				status = FT_OK;
			}
		}
	}
#endif

	FN_EXIT;
#ifdef INFRA_DEBUG_ENABLE
	SPI_DisplayList();
#endif
	return status;
}

/*!
 * \brief Display the contents of linked list
 *
 * This function traverses the channel configuration data linked list
 * and prints the data at each node.
 *
 * \return Returns status code of type FT_STATUS(see D2XX Programmer's Guide)
 * \sa
 * \note	Useful for debuging
 * \warning
 */
FT_STATUS SPI_DisplayList(void)
{
	FT_STATUS status=FT_OTHER_ERROR;
#ifdef INFRA_DEBUG_ENABLE
	ChannelContext *tempNode=NULL;
#endif
	FN_ENTER;
#ifdef INFRA_DEBUG_ENABLE
	printf("%s:%d:%s():\n",__FILE__, __LINE__, __FUNCTION__);
	for(tempNode = ListHead; 0 != tempNode; tempNode=tempNode->next)
	{
		//if(currentDebugLevel>=MSG_DEBUG)
		{
			printf("\ttempNode->handle=0x%x\n",(unsigned)tempNode->handle);
			printf("\ttempNode-->next=0x%x\n",(unsigned)tempNode->next);
			printf("\ttempNode->config->ClockRate=%u\n",
				(unsigned)tempNode->config.ClockRate);
		}
	}
	printf("------------------------------------------------------\n");
#endif
	FN_EXIT;
	return status;
}

/*!
 * \brief Toggles the state of the CS line
 *
 * This function turns ON/OFF the chip select line associated with the channel
 *
 * \param[in] handle Handle of the channel
 * \param[in] state TRUE if CS needs to be set, false otherwise
 * \return Returns status code of type FT_STATUS(see D2XX Programmer's Guide)
 * \sa
 * \note
 * \warning
 */
FT_STATUS SPI_ToggleCS(FT_HANDLE handle, bool state)
{
	ChannelConfig *config=NULL;
	bool activeLow;
	FT_STATUS status=FT_OTHER_ERROR;
	uint8 buffer[5];
	uint32 i=0;
	uint32 noOfBytesTransferred;
	uint16 value, oldValue, direction;

	FN_ENTER;
	if(!state)
	{
		INFRA_SLEEP(2);
	}
#ifdef DEVELOPMENT_FIXED_CS
//#if 1
	/* For initial development only - assuming only ADBUS0 will be used for CS*/
	buffer[i++]=MPSSE_CMD_SET_DATA_BITS_LOWBYTE;
	if(TRUE==state)
	{
		//buffer[i++]=0x08;		/*value*/
		buffer[i++]=0x09;		/*value - mode2,3 clock idle high*/
	}
	else
	{
		//buffer[i++]=0x00;		/*value*/
		buffer[i++]=0x01;		/*value - mode2,3 clock idle high*/
	}
	buffer[i++]=0x0B;//direction;	/*direction*/
	status = FT_Channel_Write(SPI,handle,i,buffer,&noOfBytesTransferred);
	CHECK_STATUS(status);
#else
	/*Get a pointer to the channel's configuration data and manipulate there directly*/
	status = SPI_GetChannelConfig(handle,&config);
	CHECK_STATUS(status);
	activeLow = (config->configOptions & \
		SPI_CONFIG_OPTION_CS_ACTIVELOW)?TRUE:FALSE;

	DBG(MSG_DEBUG,"config->configOptions=0x%x activeLow=0x%x\n",
		(unsigned)config->configOptions,(unsigned)activeLow);

	//direction = (uint8)config->currentPinState;/*get current state*/
	direction = config->currentPinDir;
	direction |= \
		((1<<((config->configOptions & SPI_CONFIG_OPTION_CS_MASK)>>2))<<3);
	DBG(MSG_DEBUG,"config->currentPinState=0x%x direction=0x%x\n",
		(unsigned)config->currentPinState,(unsigned)direction);

	//oldValue = (uint8)(8>>config->currentPinState);
	oldValue = config->currentPinState;
	value = ((1<<((config->configOptions & SPI_CONFIG_OPTION_CS_MASK)>>2))<<3);

	DBG(MSG_DEBUG,"oldValue=0x%x value=0x%x\n",oldValue,value);

	if((TRUE==state && FALSE==activeLow) || (FALSE==state && TRUE==activeLow))
		value = oldValue | value; /* set the CS line high */
	if((TRUE==state && TRUE==activeLow) || (FALSE==state && FALSE==activeLow))
		value = oldValue & ~value;/* set the CS line low */

	//save  dirn & value
	config->currentPinState = value;
	config->currentPinDir = direction;
	DBG(MSG_DEBUG,"config->currentPinState=0x%x\n",
		(unsigned)config->currentPinState);

	/*MPSSE command to set low bytes*/
	buffer[i++]=MPSSE_CMD_SET_DATA_BITS_LOWBYTE;
	buffer[i++]=value;		/*value*/
	buffer[i++]=direction;	/*direction*/
	DBG(MSG_DEBUG,"direction=0x%x value=0x%x\n",direction,value);
	status = FT_Channel_Write(SPI,handle,i,buffer,&noOfBytesTransferred);
	CHECK_STATUS(status);
#endif
	FN_EXIT;
	return status;
}

/*!
 * \brief Writes 8 or less bits to the SPI device
 *
 * This function is called by SPI_Write to write 8 or few number of bits
 *
 * \param[in] handle Handle of the channel
 * \param[in] byte Data of length 8 or less bits
 * \param[in] len Length of data in bits(maximum 8)
 * \return Returns status code of type FT_STATUS(see D2XX Programmer's Guide)
 * \sa
 * \note
 * \warning
 */
FT_STATUS SPI_Write8bits(FT_HANDLE handle,uint8 byte, uint8 len)
{
	FT_STATUS status=FT_OTHER_ERROR;
	uint32 noOfBytes=0,noOfBytesTransferred=0;
	uint8 buffer[10];
	ChannelConfig *config=NULL;
	uint8 mode;
	FN_ENTER;

	status = SPI_GetChannelConfig(handle,&config);
	CHECK_STATUS(status);
	/*mode is given by bit1-bit0 of ChannelConfig.Options*/
	mode = (config->configOptions & SPI_CONFIG_OPTION_MODE_MASK);
	/* Command to write 8bits */
	switch(mode)
	{
		case 0:
			buffer[noOfBytes++] = MPSSE_CMD_DATA_OUT_BITS_NEG_EDGE;
			break;
		case 1:
			buffer[noOfBytes++] = MPSSE_CMD_DATA_OUT_BITS_POS_EDGE;
			break;
		case 2:
			buffer[noOfBytes++] = MPSSE_CMD_DATA_OUT_BITS_POS_EDGE;
			break;
		case 3:
			buffer[noOfBytes++] = MPSSE_CMD_DATA_OUT_BITS_NEG_EDGE;
			break;
		default:
			DBG(MSG_DEBUG,"invalid mode(%u)\n",(unsigned)mode);
	}
	buffer[noOfBytes++] = len-1;/* 1bit->arg=0, for 8bits->arg=7 */
	buffer[noOfBytes++] = byte;
	DBG(MSG_DEBUG,"SPIMODE=%u buffer[0]=0x%x writing data=0x%x len=%u\n",
		(unsigned)mode,(unsigned)buffer[0],(unsigned)byte,(unsigned)len);
	status = FT_Channel_Write(SPI,handle,noOfBytes,buffer,\
		&noOfBytesTransferred);
	CHECK_STATUS(status);

	FN_EXIT;
	return status;
}


/*!
 * \brief Reads 8 or less bits to the SPI device
 *
 * This function is called by SPI_Read to read 8 or few number of bits
 *
 * \param[in] handle Handle of the channel
 * \param[in] byte Data of length 8 or less bits
 * \param[in] len Length of data in bits(maximum 8)
 * \param[in] mode SPI mode
 * \return Returns status code of type FT_STATUS(see D2XX Programmer's Guide)
 * \sa
 * \note
 * \warning
 */
FT_STATUS SPI_Read8bits(FT_HANDLE handle,uint8 *byte, uint8 len)
{
	FT_STATUS status=FT_OTHER_ERROR;
	uint32 noOfBytes=0,noOfBytesTransferred=0;
	uint8 buffer[10];
	ChannelConfig *config=NULL;
	uint8 mode;

	FN_ENTER;
	status = SPI_GetChannelConfig(handle,&config);
	CHECK_STATUS(status);
	/*mode is given by bit1-bit0 of ChannelConfig.Options*/
	mode = (config->configOptions & SPI_CONFIG_OPTION_MODE_MASK);
	/* Command to write 8bits */
	switch(mode)
	{
		case 0:
			buffer[noOfBytes++] = MPSSE_CMD_DATA_IN_BITS_POS_EDGE;
			break;
		case 1:
			buffer[noOfBytes++] = MPSSE_CMD_DATA_IN_BITS_NEG_EDGE;
			break;
		case 2:
			buffer[noOfBytes++] = MPSSE_CMD_DATA_IN_BITS_NEG_EDGE;
			break;
		case 3:
			buffer[noOfBytes++] = MPSSE_CMD_DATA_IN_BITS_POS_EDGE;
			break;
		default:
			DBG(MSG_DEBUG,"invalid mode(%u)\n",(unsigned)mode);
	}
	buffer[noOfBytes++] = len-1;/* 1bit->arg=0, for 8bits->arg=7 */

	/*Command MPSSE to send data to PC immediately */
	buffer[noOfBytes++] = MPSSE_CMD_SEND_IMMEDIATE;

	DBG(MSG_DEBUG,"writing data=0x%x len=%u\n",(unsigned)byte,(unsigned)len);
	status = FT_Channel_Write(SPI,handle,noOfBytes,buffer,\
		&noOfBytesTransferred);
	CHECK_STATUS(status);

	noOfBytes=1;
	noOfBytesTransferred=0;
	status = FT_Channel_Read(SPI,handle,noOfBytes,buffer,&noOfBytesTransferred);
	CHECK_STATUS(status);

	*byte = buffer[0];
	DBG(MSG_DEBUG,"SPI_Read8bits len=%u(in bits) byte=0x%x mode=%u\n",\
		(unsigned)len,*byte,(unsigned)mode);

	FN_EXIT;
	return status;
}



