/*
 * FdxUdpServer.c
 *
 *  Created on: Jun 5, 2020
 *      Author: midoj
 */

/* standard libraries */
#include "stdio.h"
#include "stdlib.h"
#include "stdint.h"

/* UDP libraries */
#include "unistd.h"
#include "string.h"
#include "sys/types.h"
#include "sys/socket.h"
#include "sys/time.h"
#include "arpa/inet.h"
#include "netinet/in.h"
#include "netdb.h"


/* fdx and udp header files */
#include "udp.h"
#include "FDX.h"
#include "FDX_Server.h"


/*GPIO library*/
#if RPI_HOST

#include "wiringPi.h"
#include "wiringPiSPI.h"

#endif


#define TOTAL_DIGITAL_INPUT_CHANNEL					8
#define TOTAL_DIGITAL_OUTPUT_CHANNEL				8
#define TOTAL_ADC_CHANNEL							8


#define SEQ_NUM_INDX_START							0
#define SEQ_NUM_INDX_STOP							1
#define SEQ_NUM_INDX_DATA_EXCHANGE					2
#define SEQ_NUM_INDX_STATUS							3

#define IN_CHANNELS_NUM  							8U
#define OUT_CHANNELS_NUM  							8U
#define PWM_IN_Channels_NUM							2U
#define PWM_OUT_Channels_NUM						2U
#define ADC_NUM_OF_CHANNLES							8U



#define GPIO_17   0U
#define GPIO_27   2U
#define GPIO_22   3U
#define GPIO_05   21U
#define GPIO_06   22U
#define GPIO_13   23U
#define GPIO_19   24U
#define GPIO_26   25U

#define GPIO_18   1U
#define GPIO_23   4U
#define GPIO_24   5U
#define GPIO_25   6U
#define GPIO_12   26U
#define GPIO_16   27U
#define GPIO_20   28U
#define GPIO_21   29U

#define PWM_OUTPUT_CHANNEL 18U

static UDP_Server server;
static uint16_t Session_Status = 0;

/*
 * Sequence number array elements:
 * 		1. Cmd_Start
 * 		2. Cmd_Stop
 *
 */
static uint16_t Sequance_Number[4];

#if RPI_HOST
static uint8_t InputChannelTable[IN_CHANNELS_NUM] = {
		GPIO_17,
		GPIO_27,
		GPIO_22,
		GPIO_05,
		GPIO_06,
		GPIO_13,
		GPIO_19,
		GPIO_26,
};

static uint8_t OutputChannelTable[OUT_CHANNELS_NUM] = {
		GPIO_18,
		GPIO_23,
		GPIO_24,
		GPIO_25,
		GPIO_12,
		GPIO_16,
		GPIO_20,
		GPIO_21
};

static uint8_t PWM_InputChannelTable[PWM_IN_Channels_NUM] = {

};

static uint8_t PWM_OutputChannelTable[PWM_OUT_Channels_NUM] = {

};

static uint8_t ADCChannelTable[ADC_NUM_OF_CHANNLES] = {0};


static void ADC_AllChannelsRead(uint8_t *pData_arr, uint8_t data_size)
{
	uint32_t indx = 0;
	uint8_t recData = 0xAA;			// CMD to get all channels value

	wiringPiSPIDataRW(0, &recData, 1);

	delay(1);

	if(recData == 0xBB)
	{
		for (indx = 0; indx < data_size; indx++)
		{
			wiringPiSPIDataRW(0, (pData_arr + indx), 1);
			delay(1);
		}

#if DEBUG_START
		for (indx = 0; indx < data_size ; indx++)
		{
			printf("ADC_ALL Channel Number %d = %d\n", indx, ADCChannelTable[indx]);
			fflush(stdout);
		}
#endif

	}
}
#endif

static Std_ReturnType DataExchange_Handler(uint16_t groupId, Server_Config_t *server)
{
	Std_ReturnType ApiStatus = E_OK;
	uint16_t periph_id = 0;
	uint16_t indx = 0;
	uint16_t ADC_ArrIndx;

#if DEBUG_START
	uint32_t indxDebug = 0;
#endif

	FDX_DataExchange_t *pFDX_DataExchange = (FDX_DataExchange_t *) &server->recv_msg[CMD_SIZE_OFFSET];

	if(Session_Status == SESSION_OPEN)
	{
#if DEBUG_START
		printf("Session status check passed\n");
		fflush(stdout);
#endif

		if(GET_DIR(groupId) == DIR_APP_TO_GOLDENBOX)
		{
#if DEBUG_START
			printf("Direction check passed\n");
			fflush(stdout);
#endif
			periph_id = GET_PERIPH_ID(groupId);

			switch(periph_id)
			{

			case PERIPH_ID_DIGITAL_INPUT:

#if DEBUG_START
				printf("Digital Peripheral ID check passed\n");
				fflush(stdout);
#endif

				/* Data size must  be even number (2 bytes for each channel "channel number, Status" ) */
				for(indx = 0; indx < pFDX_DataExchange->DataSize; indx += 2)
				{
					if(server->recv_msg[DATAEX_BYTES_OFFSET + indx] >= TOTAL_DIGITAL_INPUT_CHANNEL)
					{
						ApiStatus = E_NOT_OK;
						break;
					}

					server->sent_msg[DATAEX_BYTES_OFFSET + indx] = server->recv_msg[DATAEX_BYTES_OFFSET + indx];

#if RPI_HOST
					server->sent_msg[DATAEX_SIZE_OFFSET + indx + 1] =
							digitalRead(InputChannelTable[server->recv_msg[DATAEX_BYTES_OFFSET + indx]]);
#endif
				}



				if(ApiStatus == E_OK)
				{
					/* Create header frame with incremental sequence number */
					FDX_CreateFrameHeader(server->sent_msg, Sequance_Number[SEQ_NUM_INDX_STATUS], 1);
					Sequance_Number[SEQ_NUM_INDX_DATA_EXCHANGE] += 1;

					/* Assign data to be sent */
					FDX_CreateDataExchangeCmd(server->sent_msg,
							(DIR_GOLDENBOX_TO_APP | PERIPH_ID_DIGITAL_INPUT),
							pFDX_DataExchange->DataSize,
							&server->sent_msg[DATAEX_BYTES_OFFSET]);
					server->total_bytes_to_send = HEADER_SIZE + CMD_DATA_EXCHANGE_SIZE + pFDX_DataExchange->DataSize;
				}
				else
				{
					/* Create header frame with incremental sequence number */
					FDX_CreateFrameHeader(server->sent_msg, Sequance_Number[SEQ_NUM_INDX_STATUS], 1);
					Sequance_Number[SEQ_NUM_INDX_STATUS] += 1;


					/* Create status command */
					FDX_CreateStatusCmd(server->sent_msg, (SESSION_OPEN | RESPOND_NOK), 0);
					server->total_bytes_to_send = HEADER_SIZE + CMD_STATUS_SIZE;
				}

				break;


			case PERIPH_ID_DIGITAL_OUTPUT:

#if DEBUG_START
				printf("Digital output Peripheral ID check passed\n");
				fflush(stdout);
#endif
				/* Apply Changes to HW */
				for(indx = 0; indx < pFDX_DataExchange->DataSize; indx += 2)
				{

#if DEBUG_START
					printf("Channel number: %d\n", server->recv_msg[DATAEX_BYTES_OFFSET + indx]);
					fflush(stdout);
#endif
					if(server->recv_msg[DATAEX_BYTES_OFFSET + indx] >= TOTAL_DIGITAL_OUTPUT_CHANNEL)
					{
						ApiStatus = E_NOT_OK;
						break;
					}
					server->sent_msg[DATAEX_SIZE_OFFSET + indx] = server->recv_msg[DATAEX_BYTES_OFFSET + indx];

#if DEBUG_START
					printf("Digital Output Channel number = %d\n", server->recv_msg[DATAEX_BYTES_OFFSET + indx]);
					printf("Digital Output Channel value  = %d\n", server->recv_msg[DATAEX_BYTES_OFFSET + indx + 1]);
#endif

#if RPI_HOST
					digitalWrite(OutputChannelTable[server->recv_msg[DATAEX_BYTES_OFFSET + indx]],
							server->recv_msg[DATAEX_BYTES_OFFSET + indx + 1]);
#endif
				}


				/* Create header frame with incremental sequence number */
				FDX_CreateFrameHeader(server->sent_msg, Sequance_Number[SEQ_NUM_INDX_STATUS], 1);
				Sequance_Number[SEQ_NUM_INDX_STATUS] += 1;


				if(ApiStatus == E_OK)
				{
					/* Create status command */
					FDX_CreateStatusCmd(server->sent_msg, (SESSION_OPEN | RESPOND_OK), 0);
					server->total_bytes_to_send = HEADER_SIZE + CMD_STATUS_SIZE;
				}
				else
				{
					/* Create status command */
					FDX_CreateStatusCmd(server->sent_msg, (SESSION_OPEN | RESPOND_NOK), 0);
					server->total_bytes_to_send = HEADER_SIZE + CMD_STATUS_SIZE;
				}
				break;



			case PERIPH_ID_ADC:

#if DEBUG_START
				printf("ADC Peripheral ID check passed\n");
				fflush(stdout);
#endif
				/* Apply Changes to HW */
				for(indx = 0, ADC_ArrIndx = 0;
						indx < pFDX_DataExchange->DataSize && ADC_ArrIndx < ADC_NUM_OF_CHANNLES;
						indx += 2, ADC_ArrIndx += 1)
				{

#if DEBUG_START
					printf("ADC Channel number: %d\n", server->recv_msg[DATAEX_BYTES_OFFSET + indx]);
					fflush(stdout);
#endif
					if(server->recv_msg[DATAEX_BYTES_OFFSET + indx] >= TOTAL_ADC_CHANNEL)
					{
						ApiStatus = E_NOT_OK;
						break;
					}
					server->sent_msg[DATAEX_BYTES_OFFSET + indx] = server->recv_msg[DATAEX_BYTES_OFFSET + indx];

#if RPI_HOST
					/* Get ADC channels value */
					ADC_AllChannelsRead(ADCChannelTable, ADC_NUM_OF_CHANNLES);

					/* Send message access by server */
					server->sent_msg[DATAEX_BYTES_OFFSET + indx + 1] = ADCChannelTable[ADC_ArrIndx];
#endif

#if DEBUG_START

					for (indxDebug = 0; indxDebug < ADC_NUM_OF_CHANNLES ; ++indxDebug)
					{
						printf("ADC Channel Number %d = %d\n", indxDebug, ADCChannelTable[indxDebug]);
						fflush(stdout);
					}
#endif

				}


				if(ApiStatus == E_OK)
				{
					/* Create header frame with incremental sequence number */
					FDX_CreateFrameHeader(server->sent_msg, Sequance_Number[SEQ_NUM_INDX_STATUS], 1);
					Sequance_Number[SEQ_NUM_INDX_DATA_EXCHANGE] += 1;

					/* Assign data to be sent */
					FDX_CreateDataExchangeCmd(server->sent_msg,
							(DIR_GOLDENBOX_TO_APP | PERIPH_ID_DIGITAL_INPUT),
							pFDX_DataExchange->DataSize,
							&server->sent_msg[DATAEX_BYTES_OFFSET]);
					server->total_bytes_to_send = HEADER_SIZE + CMD_DATA_EXCHANGE_SIZE + pFDX_DataExchange->DataSize;
				}
				else
				{
					/* Create header frame with incremental sequence number */
					FDX_CreateFrameHeader(server->sent_msg, Sequance_Number[SEQ_NUM_INDX_STATUS], 1);
					Sequance_Number[SEQ_NUM_INDX_STATUS] += 1;


					/* Create status command */
					FDX_CreateStatusCmd(server->sent_msg, (SESSION_OPEN | RESPOND_NOK), 0);
					server->total_bytes_to_send = HEADER_SIZE + CMD_STATUS_SIZE;
				}

				break;




			case PERIPH_ID_UART:

				break;


			case PERIPH_ID_PWM_INPUT:

#if DEBUG_START
				printf("PWM INPUT ID check passed\n");
				fflush(stdout);
#endif
				/* Apply Changes to HW */
				for(indx = 0; indx < pFDX_DataExchange->DataSize; indx += 3)
				{

#if DEBUG_START
					printf("Channel number: %d\n", server->recv_msg[DATAEX_BYTES_OFFSET + indx]);
					fflush(stdout);
#endif
					if(server->recv_msg[DATAEX_BYTES_OFFSET + indx] >= PWM_IN_Channels_NUM)
					{
						ApiStatus = E_NOT_OK;
						break;

					}
					server->sent_msg[DATAEX_SIZE_OFFSET + indx] = server->recv_msg[DATAEX_BYTES_OFFSET + indx];

					/*get FREQ and Duty cycle from Hardware*/
					/*Send the FREQUENCY*/
					//server->sent_msg[DATAEX_SIZE_OFFSET + indx + 1] =
					/*Send Duty Cycle*/
					//server->sent_msg[DATAEX_SIZE_OFFSET + indx + 2] =
#if DEBUG_START
					printf("PWM INPUT Channel number = %d\n", server->recv_msg[DATAEX_BYTES_OFFSET + indx]);
					printf("PWM INPUT Frequency  = %d\n", server->recv_msg[DATAEX_BYTES_OFFSET + indx + 1]);
					printf("PWM INPUT Duty Cycle  = %d\n", server->recv_msg[DATAEX_BYTES_OFFSET + indx + 2]);


#endif


				}



				if(ApiStatus == E_OK)
				{
					/* Create header frame with incremental sequence number */
					FDX_CreateFrameHeader(server->sent_msg, Sequance_Number[SEQ_NUM_INDX_STATUS], 1);
					Sequance_Number[SEQ_NUM_INDX_DATA_EXCHANGE] += 1;

					/* Assign data to be sent */
					FDX_CreateDataExchangeCmd(server->sent_msg,
							(DIR_GOLDENBOX_TO_APP | PERIPH_ID_DIGITAL_INPUT),
							pFDX_DataExchange->DataSize,
							&server->sent_msg[DATAEX_BYTES_OFFSET]);
					server->total_bytes_to_send = HEADER_SIZE + CMD_DATA_EXCHANGE_SIZE + pFDX_DataExchange->DataSize;
				}
				else
				{
					/* Create header frame with incremental sequence number */
					FDX_CreateFrameHeader(server->sent_msg, Sequance_Number[SEQ_NUM_INDX_STATUS], 1);
					Sequance_Number[SEQ_NUM_INDX_STATUS] += 1;


					/* Create status command */
					FDX_CreateStatusCmd(server->sent_msg, (SESSION_OPEN | RESPOND_NOK), 0);
					server->total_bytes_to_send = HEADER_SIZE + CMD_STATUS_SIZE;
				}

				break;



			case PERIPH_ID_PWM_OUTPUT:

#if DEBUG_START
				printf("PWM OUTPUT ID check passed\n");
				fflush(stdout);
#endif
				/* Apply Changes to HW */
				for(indx = 0; indx < pFDX_DataExchange->DataSize; indx += 2)
				{

#if DEBUG_START
					printf("Channel number: %d\n", server->recv_msg[DATAEX_BYTES_OFFSET + indx]);
					fflush(stdout);
#endif
					if(server->recv_msg[DATAEX_BYTES_OFFSET + indx] >= TOTAL_DIGITAL_OUTPUT_CHANNEL)
					{
						ApiStatus = E_NOT_OK;
						break;
					}
					server->sent_msg[DATAEX_SIZE_OFFSET + indx] = server->recv_msg[DATAEX_BYTES_OFFSET + indx];

#if DEBUG_START
					printf("PWM OUTPUT Channel number = %d\n", server->recv_msg[DATAEX_BYTES_OFFSET + indx]);
					printf("PWM OUTPUT Channel value  = %d\n", server->recv_msg[DATAEX_BYTES_OFFSET + indx + 1]);
#endif
					/*digitalWrite(PWM_OutputChannelTable[server->recv_msg[DATAEX_BYTES_OFFSET + indx]],
							server->recv_msg[DATAEX_BYTES_OFFSET + indx + 1]);*/
				}


				/* Create header frame with incremental sequence number */
				FDX_CreateFrameHeader(server->sent_msg, Sequance_Number[SEQ_NUM_INDX_STATUS], 1);
				Sequance_Number[SEQ_NUM_INDX_STATUS] += 1;


				if(ApiStatus == E_OK)
				{
					/* Create status command */
					FDX_CreateStatusCmd(server->sent_msg, (SESSION_OPEN | RESPOND_OK), 0);
					server->total_bytes_to_send = HEADER_SIZE + CMD_STATUS_SIZE;
				}
				else
				{
					/* Create status command */
					FDX_CreateStatusCmd(server->sent_msg, (SESSION_OPEN | RESPOND_NOK), 0);
					server->total_bytes_to_send = HEADER_SIZE + CMD_STATUS_SIZE;
				}
				break;










			default:
				ApiStatus = E_NOT_OK;

				/* Create header frame with incremental sequence number */
				FDX_CreateFrameHeader(server->sent_msg, Sequance_Number[SEQ_NUM_INDX_STATUS], 1);
				Sequance_Number[SEQ_NUM_INDX_STATUS] += 1;

				/* Create status command */
				FDX_CreateStatusCmd(server->sent_msg, (SESSION_OPEN | RESPOND_NOK), 0);
				server->total_bytes_to_send = HEADER_SIZE + CMD_STATUS_SIZE;
				break;
			}
		}
		else
		{
			ApiStatus = E_NOT_OK;
			/* Create header frame with incremental sequence number */
			FDX_CreateFrameHeader(server->sent_msg, Sequance_Number[SEQ_NUM_INDX_STATUS], 1);
			Sequance_Number[SEQ_NUM_INDX_STATUS] += 1;

			/* Create status command */
			FDX_CreateStatusCmd(server->sent_msg, (SESSION_OPEN | RESPOND_NOK), 0);
			server->total_bytes_to_send = HEADER_SIZE + CMD_STATUS_SIZE;
		}
	}
	else
	{
		/* Create header frame with incremental sequence number */
		FDX_CreateFrameHeader(server->sent_msg, Sequance_Number[SEQ_NUM_INDX_STATUS], 1);
		Sequance_Number[SEQ_NUM_INDX_STATUS] += 1;

		/* Create status command */
		FDX_CreateStatusCmd(server->sent_msg, (SESSION_CLOSE | RESPOND_NOK), 0);
		server->total_bytes_to_send = HEADER_SIZE + CMD_STATUS_SIZE;

#if DEBUG_START
		printf("No session opened\n");
		fflush(stdout);
#endif
	}

	return ApiStatus;
}


static void FdxServer_Handler(Server_Config_t *server)
{
	Std_ReturnType result = E_NOT_OK;

	uint16_t frameType = 0;
	uint16_t dataSize = 0;
	uint16_t groupID = 0;


	result = FDX_ParsingFrame(server->recv_msg, &frameType, &dataSize, &groupID);

	if(result == E_OK)
	{
		switch(frameType)
		{
		case Cmd_Start:
			Session_Status = SESSION_OPEN;
			memset(Sequance_Number, 0, (sizeof(uint16_t) * 4));
			/* Create header frame with incremental sequence number */
			FDX_CreateFrameHeader(server->sent_msg, Sequance_Number[SEQ_NUM_INDX_STATUS], 1);
			Sequance_Number[SEQ_NUM_INDX_STATUS] += 1;

			/* Create status command */
			FDX_CreateStatusCmd(server->sent_msg, (SESSION_OPEN | RESPOND_OK), 0);
			server->total_bytes_to_send = HEADER_SIZE + CMD_STATUS_SIZE;

#if DEBUG_START
			printf("OK...............Start command received\n");
			fflush(stdout);
#endif
			break;

		case Cmd_Stop:
			Session_Status = SESSION_CLOSE;
			/* Create header frame with incremental sequence number */
			FDX_CreateFrameHeader(server->sent_msg, Sequance_Number[SEQ_NUM_INDX_STATUS], 1);
			Sequance_Number[SEQ_NUM_INDX_STATUS] += 1;

			/* Create status command */
			FDX_CreateStatusCmd(server->sent_msg, (SESSION_CLOSE | RESPOND_OK), 0);
			server->total_bytes_to_send = HEADER_SIZE + CMD_STATUS_SIZE;

#if DEBUG_START
			printf("OK...............Stop command received\n");
			fflush(stdout);
#endif
			break;

		case Cmd_DataExchange:
			DataExchange_Handler(groupID, server);

#if DEBUG_START
			printf("OK...............Data exchange command received\n");
			fflush(stdout);
#endif
			break;

		default:
			if(Session_Status == SESSION_OPEN)
			{
				/* Create header frame with incremental sequence number */
				FDX_CreateFrameHeader(server->sent_msg, Sequance_Number[SEQ_NUM_INDX_STATUS], 1);
				Sequance_Number[SEQ_NUM_INDX_STATUS] += 1;

				FDX_CreateStatusCmd(server->sent_msg, (SESSION_OPEN | RESPOND_NOK), 0);
				server->total_bytes_to_send = HEADER_SIZE + CMD_STATUS_SIZE;

#if DEBUG_START
				printf("NOK...............Unknown command\n");
				fflush(stdout);
#endif
			}
			else
			{
				/* Create header frame with incremental sequence number */
				FDX_CreateFrameHeader(server->sent_msg, Sequance_Number[SEQ_NUM_INDX_STATUS], 1);
				Sequance_Number[SEQ_NUM_INDX_STATUS] += 1;

				FDX_CreateStatusCmd(server->sent_msg, (SESSION_CLOSE | RESPOND_NOK), 0);
				server->total_bytes_to_send = HEADER_SIZE + CMD_STATUS_SIZE;

#if DEBUG_START
				printf("NOK...............No session opened\n");
				fflush(stdout);
#endif
			}
			break;
		}
	}
	else
	{
#if DEBUG_START
		printf("Invalid Message\n");
		fflush(stdout);
#endif
	}
}




void FdxServer_Init(char *ip, uint16_t port)
{
#if RPI_HOST
	uint8_t indx;
	int SPI_fd;
#endif

	server.Config.ip 	= (char *)ip;
	server.Config.port 	= port;
	server.Handler 		= FdxServer_Handler;

#if RPI_HOST
	/*using BCM numbering*/
	wiringPiSetup();

	SPI_fd = wiringPiSPISetupMode(0, SPI_SPEED, SPI_MODE);

	for(indx = 0; indx < IN_CHANNELS_NUM ; ++indx)
	{
		pinMode(InputChannelTable[indx], INPUT);
		pinMode(OutputChannelTable[indx], OUTPUT);
	}
#endif


}




void FdxServer_StartForever(void)
{
	Server_Lunch(&server);
}


