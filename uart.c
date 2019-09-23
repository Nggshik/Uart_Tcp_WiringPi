#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <stddef.h>

// #include "uart.h"
#include <wiringSerial.h>

#define UART_BUFFER_MAX_SIZE 400

enum
{
	UART1 = 0,
	UART2
};
typedef enum
{
	NONE = 0,
	MODBUS,
	PROFIBUS
} interface_type;
typedef enum
{
	UNDEFINED = 0,
	INIT = 1,
	INWORK = 2
} task_status;
typedef enum
{
	CLOSED = 0,
	OPENED
} status;

struct uart_control
{
	int fd;
	interface_type interface;
	status Status;
	int baud;
} uart_ctrl[2];

const char UART1_dev[] = "/dev/ttyS1";
const char UART2_dev[] = "/dev/ttyS2";

//Prototypes
int Uart_integer_data_receiver(int uart_num, int *buf);
int Uart_string_data_receiver(int uart_num, char *str);
int Uart_set_configuration(int uart_num, int baud);
int Uart_define_init(int uart_num);
int Uart_tasks(int uart_num, int baud, char *buf);
int Uart_data_send(int uart_num, char *str);

//----------------------------------------------------------------------------------------------------

int Uart_set_configuration(int uart_num, int baud)
{
	int fd;
	const char *dev_path;

	if (uart_ctrl[uart_num].Status == OPENED && uart_ctrl[uart_num].baud == baud)
	{
		printf("Uart_set_configuratoin(): This configuration is also set up");
		return 0;
	}
	else
	{
		serialClose(uart_ctrl[uart_num].fd);
	}

	if (uart_num > 1)
	{
		printf("Uart_init(): uart_num in wrong area %d\n", uart_num);
		return -1;
	}

	if (uart_num == 0)
	{
		dev_path = UART1_dev;
	}
	else
	{
		dev_path = UART2_dev;
	}

	if ((fd = serialOpen(dev_path, baud)) < 0)
	{
		fprintf(stderr, "Unable to open %s\n", strerror(errno));
		return -1;
	}
	uart_ctrl[uart_num].fd = fd;
	uart_ctrl[uart_num].baud = baud;
	uart_ctrl[uart_num].Status = OPENED;

	return 0;
}

int Uart_define_init(int uart_num)
{
	uart_ctrl[uart_num].fd = -1;
	uart_ctrl[uart_num].baud = 0;
	uart_ctrl[uart_num].Status = CLOSED;
	uart_ctrl[uart_num].interface = NONE;

	if (Uart_set_configuration(uart_num, 9600) < 0)
	{
		printf("Uart_define_init(): Can't open UART%d\n", uart_num + 1);
		return 1;
	}

	return 0;
}

int Uart_integer_data_receiver(int uart_num, int *buf)
{

	if (uart_ctrl[uart_num].Status == CLOSED)
		return 0;

	int num = 0;
	int *buf_ptr = buf;

	num = serialDataAvail(uart_ctrl[uart_num].fd);
	if (num > UART_BUFFER_MAX_SIZE)
	{
		printf("Uart_data_receiver(): Uart input message > UART_BUFFER_MAX_SIZE");
		return -1;
	}
	while (num-- > 0)
	{
		*(buf_ptr++) = serialGetchar(uart_ctrl[0].fd);
	}
	// if(uart_ctrl[uart_num].interface == NONE)
	// 	*(buf_ptr++) = '\0';

	return (buf_ptr - buf - 1);
}

int Uart_string_data_receiver(int uart_num, char *str)
{
	if (uart_ctrl[uart_num].Status == CLOSED)
		return 0;

	int num = 0;
	char *str_ptr = str;

	num = serialDataAvail(uart_ctrl[uart_num].fd);
	if (num > UART_BUFFER_MAX_SIZE)
	{
		printf("Uart_data_receiver(): Uart input message > UART_BUFFER_MAX_SIZE");
		return -1;
	}
	while (num-- > 0)
	{
		*(str_ptr++) = serialGetchar(uart_ctrl[uart_num].fd);
	}
	*str_ptr = '\0';

	return (str_ptr - str);
}

int Uart_tasks(int uart_num, int baud, char *buf)
{
	static task_status work_state = UNDEFINED;
	int num = 0;

	switch (work_state)
	{
	case UNDEFINED:
		if (Uart_set_configuration(uart_num, baud))
			return 1;
		work_state = INWORK;
		break;
	case INWORK:
		num = Uart_string_data_receiver(uart_num, buf);
		return num;
		break;
	default:
		return -1;
		break;
	}
	return 0;
}
int Uart_data_receiver(int *buf)
{
	int num = 0;
	if (uart_ctrl[UART1].Status == OPENED)
	{

		switch (uart_ctrl[UART1].interface)
		{
		case NONE:
			num = Uart_integer_data_receiver(UART1, buf);

			break;
		case MODBUS:
			num = Uart_integer_data_receiver(UART1, buf);
			break;
		case PROFIBUS:
			num = Uart_integer_data_receiver(UART1, buf);
		default:
			break;
		}
	}
}

int Uart_data_send(int uart_num, char *str)
{
	if (uart_num > 1)
		return -1;
	if (uart_ctrl[uart_num].Status == CLOSED)
		return 0;

	serialPuts(uart_ctrl[uart_num].fd, str);

	return strlen(str);
}
// int main(void)
// {
// 	int buf[UART_BUFFER_MAX_SIZE]={0};
// 	char str[UART_BUFFER_MAX_SIZE]={0};

// 	int num =0;

// 	// if(Uart_define_init(UART1))
// 	// 	return 1;
// 	for(;;)
// 	{
// 		if ((num = Uart_tasks(UART1, 9600,str)) > 0){
// 			printf("%s", str);
// 			printf("Num = %d\n",num);

// 			for(;num >= 0; num--){
// 				printf("str[%d] = %d\n",num,str[num]);
// 			}
// 		}
// 		fflush(stdout);

// 	}

// 	return 0;
// }
