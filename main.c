#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <signal.h>
#include "SerialManager.h"

#define BAUDRATE 115200
#define PORT_TTYUSB 1
#define BUFFER_SIZE 1024

char PortSerialbuff[BUFFER_SIZE];
char TCPbuff[BUFFER_SIZE];
char msg_header[20];
char IpClient[32];

socklen_t addr_len;
struct sockaddr_in clientaddr;
struct sockaddr_in serveraddr;
pthread_t thread_TCP;
bool connected = false;
int bytesWrote;
int bytesRead;
int s;
int news;
int led1, led2, led3, led4;
int leds;
bool conected = false;

struct sigaction sign_action_1, sign_action_2;

void *task_TCP(void *param);
static void BlockSignals(void);
static void UnblockSignals(void);

static void SigInt_handler()
{
	write(1, "\nCtrl+c pressed!!\n", 18);
	if (0 != pthread_cancel(thread_TCP))
	{
		perror("Error");
	}
	exit(EXIT_SUCCESS);
}

static void SigTerm_handler()
{
	write(1, "Sigterm received!\n", 18);
	if (0 != pthread_cancel(thread_TCP))
	{
		perror("Error");
	}
	exit(EXIT_SUCCESS);
}

int main(void)
{

	sign_action_1.sa_handler = SigInt_handler;
	sign_action_1.sa_flags = 0; // SA_RESTART; //
	sigemptyset(&sign_action_1.sa_mask);

	sign_action_2.sa_handler = SigTerm_handler;
	sign_action_2.sa_flags = 0; // SA_RESTART; //
	sigemptyset(&sign_action_2.sa_mask);

	if (sigaction(SIGINT, &sign_action_1, NULL) == -1)
	{
		perror("sigaction");
		exit(1);
	}

	if (sigaction(SIGTERM, &sign_action_2, NULL) == -1)
	{
		perror("sigaction");
		exit(1);
	}

	//CREACION DEL SOCKET
	s = socket(AF_INET, SOCK_STREAM, 0);
	if (s == -1)
	{
		perror("NO SE PUDO CREAR EL SOCKET");
		exit(1);
	}
	printf("SE CREO EL SOCKET CORRECTAMENTE\n\n");
	//

	//LIBERAR EL SOCKET AL CERRAR SESION
	int Activado = 1;
	setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &Activado, sizeof(Activado));
	//

	//CARGAR DATOS DE IP:PORT DEL SERVER
	bzero((char *)&serveraddr, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_port = htons(10000);

	if (inet_pton(AF_INET, "127.0.0.1", &(serveraddr.sin_addr)) <= 0)
	{
		perror("ERROR IP DEL SERVIDOR NO VALIDO");
		exit(1);
	}
	printf("SERVIDOR VALIDADO\n\n");
	//

	//APERTURA DE PUERTO CON BIND
	if (bind(s, (struct sockaddr *)&serveraddr, sizeof(serveraddr)) == -1)
	{
		close(s);
		perror("ERROR DE BIND");
		exit(1);
	}
	printf("ENLAZANDO\n\n");
	//

	//SETEO DE SOCKET EN MODO LISTENINIG
	if (listen(s, 10) == -1)
	{
		perror("ERROR DE LISTEN");
		exit(1);
	}
	printf("ESCUCHO\n\n");
	//

	//APERTURA DEL PEURT SERIAL
	if (serial_open(PORT_TTYUSB, BAUDRATE))
	{
		perror("NO SE LOGRO ABRIR EL PUERTO SERIAL");
		exit(1);
	}
	printf("PUERTO SERIE ABIERTO\n\n");
	//

	//CREACION DE UN HILO
	int ret = pthread_create(&thread_TCP, NULL, task_TCP, NULL);
	if (ret == 1)
	{
		perror("ERROR AL CREAR TASKSERIALPORT");
		exit(1);
	}
	printf("SE CREO CORRECTAMENTE TASKTCP\n\n");
	//

	for (;;)
	{
		while (conected)
		{
			int SerialRX;
			int led;

			if (SerialRX = serial_receive(PortSerialbuff, BUFFER_SIZE))
			{
				printf("SE RECIBIO MENSAJE DEL SERIALPORT\n");

				sscanf(PortSerialbuff, "%[>TOGGLE STATE:]%d", msg_header, &led);
				if (strcmp(msg_header, ">TOGGLE STATE:") != 0)
				{
					perror("MENSAJE INCORRECTO");
				}
				else
				{
					sprintf(PortSerialbuff, ":LINE%dTG\n", led);
					memset(msg_header, 0, strlen(msg_header));
					//printf("%s\n\n",msg_header);
					if ((bytesWrote = write(news, PortSerialbuff, strlen(PortSerialbuff))) == -1)
					{
						perror("ERROR DE WRITE");
					}
				}
			}
		}
	}
}

void *task_TCP(void *param)
{
	for (;;)
	{
		addr_len = sizeof(struct sockaddr_in);
		if ((news = accept(s, (struct sockaddr *)&clientaddr, &addr_len)) == -1)
		{
			perror("ERROR EN ACEPT");
			exit(1);
		}

		inet_ntop(AF_INET, &(clientaddr.sin_addr), IpClient, sizeof(IpClient));
		printf("CONEXION CON IP: %s\n", IpClient);
		conected = true;
		do
		{
			if ((bytesRead = read(news, TCPbuff, BUFFER_SIZE)) == -1)
			{
				perror("ERROR DE READ");
			}
			else
			{
				TCPbuff[bytesRead] = '\0';
				sscanf(TCPbuff, "%[:STATES]%d", msg_header, &leds);

				if (strcmp(msg_header, ":STATES") != 0)
				{
					perror("MENSAJE INVALIDO");
				}

				else
				{
					memset(msg_header, 0, strlen(msg_header));
					led1 = leds / 1000;
					led2 = (leds % 1000) / 100;
					led3 = ((leds % 1000) % 100) / 10;
					led4 = (((leds % 1000) % 100) % 10) / 1;
					sprintf(TCPbuff, ">OUTS:%d,%d,%d,%d\r\n", led1, led2, led3, led4);
					printf("%s", TCPbuff);
					serial_send(TCPbuff, strlen(TCPbuff));
				}
			}
		} while (bytesRead > 0);
		conected = false;
		close(news);
	}
	return 0;
}

static void BlockSignals(void)
{
	sigset_t set;
	sigemptyset(&set);
	sigfillset(&set);
	int err = pthread_sigmask(SIG_BLOCK, &set, NULL);
	if (0 != err)
	{
		perror("Error blocking signals");
		exit(1);
	}
}

static void UnblockSignals(void)
{
	sigset_t set;
	sigemptyset(&set);
	sigfillset(&set);
	int err = pthread_sigmask(SIG_UNBLOCK, &set, NULL);
	if (0 != err)
	{
		perror("Error unblocking signals");
		exit(1);
	}
}
