// digitime.cpp
//

#include "stdafx.h"

#define DEFAULT_PORT 0
#define DEFAULT_PROTO SOCK_STREAM // TCP

int send_to_scale(char data[], int len, char Buffer[]);

void Usage(char *progname) {
	fprintf(stderr,
		"Sync time tool for DIGI scales, verified on SM-100, SM-300, SM-500 models\n"\
		"Usage:\n\tdigitime -i host -p port [-d date] [-t time] [-v]\n"\
		"Format date and time: -d DDMMYY, -t HHMM.\nExample:\n"\
		"\tdigitime -i 10.10.10.22 -p 2022 -d 240517 -t 1053 -v\n"\
		"\nSources - https://github.com/ka1ash/digitime\n");
	WSACleanup();
	exit(1);
}

//Int to equiv Hex, example: 99(dec) -> 99(hex) = 153(return in dec)
int iteh(int v){
	return v+(v/10)*6;
}	

void str_hex(char *src, int len){
	for(int i=0; i<len; i++)
		printf("%02x ", src[i]);
	printf("\n");
}

SOCKET  conn_socket;
int V = 0; // Verbose flag

int _tmain(int argc, _TCHAR* argv[])
{
	char Buffer[256];
	char *server_name= "localhost";
	char tmp[2];
	int param_time = 0;
	unsigned short port = DEFAULT_PORT;
	int retval;
	unsigned int addr;
	int socket_type = DEFAULT_PROTO;
	struct sockaddr_in server;
	struct hostent *hp;
	WSADATA wsaData;

	time_t t = time(NULL);	
	tm* aTm = localtime(&t);
	int sm_100 = 0;
	//printf("Current datetime:\n%04d/%02d/%02d %02d:%02d:%02d \n",aTm->tm_year+1900, aTm->tm_mon+1, aTm->tm_mday, aTm->tm_hour, aTm->tm_min, aTm->tm_sec);

	//На весы время летит в таком формате: DDMMYY HHMM - 180517 1156
	//Формат команды сниффал Wireshark
	//f1 4e 00 00 00 ff 00       18 05 17 00 00 11 56 // так отправляет digiSync.exe на SM-100 не работает, SM-300 и SM-500 - ОК -- Len 14
	//f1 4e 00 00 00 ff 00 0e 00 18 05 17 00 00 11 56 // так шлет драйвер DigiSMLib.dll от http://scale-soft.com SM-100 - OK, SM-300 и SM-500 - не корректно -- Len  16

	char cmd_100[] = {0xf1, 0x4e, 0x00, 0x00, 0x00, 0xff, 0x00, 0x0e, 0x00, iteh(aTm->tm_mday), iteh(aTm->tm_mon+1), iteh(aTm->tm_year%100), 0, 0, iteh(aTm->tm_hour), iteh(aTm->tm_min)};
	char cmd_300[] = {0xf1, 0x4e, 0x00, 0x00, 0x00, 0xff, 0x00, iteh(aTm->tm_mday), iteh(aTm->tm_mon+1), iteh(aTm->tm_year%100), 0, 0, iteh(aTm->tm_hour), iteh(aTm->tm_min)};

	if (argc >1) {
		for(int i=1;i <argc;i++) {
			if ( (argv[i][0] == '-') || (argv[i][0] == '/') ) {
				switch(tolower(argv[i][1])) {
					case 'v': //Verbose, подробный вывод
						V = 1;
						break;
					case 'd': //date
						memcpy(tmp, argv[++i], 2); // DD
						cmd_300[7] = cmd_100[9] = iteh(atoi(tmp));
						memcpy(tmp, argv[i]+2, 2); // MM
						cmd_300[8] = cmd_100[10] = iteh(atoi(tmp));
						memcpy(tmp, argv[i]+4, 2); // YY
						cmd_300[9] = cmd_100[11] = iteh(atoi(tmp));
						break;

					case 't': //time
						memcpy(tmp, argv[++i], 2); // HH
						cmd_100[14] = iteh(atoi(tmp));
						memcpy(tmp, argv[i]+2, 2); // mm
						cmd_100[15] = iteh(atoi(tmp));
						break;

					case 'i': //host
						server_name = argv[++i];
						break;

					case 'p': //port
						port = atoi(argv[++i]);
						break;

					default:
						Usage(argv[0]);
						break;
				}
			}
			else
				Usage(argv[0]);
		}
	}
	
	if (WSAStartup(0x202,&wsaData) == SOCKET_ERROR) {
		fprintf(stderr,"WSAStartup failed with error %d\n",WSAGetLastError());
		WSACleanup();
		return -1;
	}
	
	if (port == 0){
		Usage(argv[0]);
	}

	// преобразование IP адреса из символьного в сетевой формат
    if (inet_addr(server_name)!=INADDR_NONE)
      server.sin_addr.s_addr=inet_addr(server_name);
    else
      // попытка получить IP адрес по доменному имени сервера
      if (hp=gethostbyname(server_name))
      // hst->h_addr_list содержит не массив адресов, а массив указателей на адреса
      ((unsigned long *)&server.sin_addr)[0]=
        ((unsigned long **)hp->h_addr_list)[0][0];
      else 
      {
        printf("Invalid address %s\n",server_name);
        closesocket(conn_socket);
        WSACleanup();
        return -1;
      }

	server.sin_family = AF_INET;
	server.sin_port = htons(port);

	conn_socket = socket(AF_INET, socket_type, 0); /* Open a socket */
	if (conn_socket <0 ) {
		fprintf(stderr,"Client: Error Opening socket: Error %d\n",
			WSAGetLastError());
		WSACleanup();
		return -1;
	}

	if(V) printf("Client connecting to: %s\n", server_name);
	if (connect(conn_socket,(struct sockaddr*)&server,sizeof(server))
		== SOCKET_ERROR) {
		fprintf(stderr,"connect() failed: %d\n",WSAGetLastError());
		WSACleanup();
		return -1;
	}
	if(V) printf("Connect OK\n");

	//Получаем данные о весах: 55.	SCALE DATA FILE - (4FH)
	char cmd[] = {0xf7, 0x4f, 0x00, 0x00, 0x00, 0x00};
	retval = send_to_scale(cmd, sizeof(cmd), Buffer);
	if(retval>0){
		int model = Buffer[85];
		if(V) printf("Model: 0x%x\n", model);
		//По весам которые были в наличии проверил
		// 0x11 - SM-100 -- для этих нужен другой формат команды
		// 0x05 - SM-300
		// 0x01 - SM-500
		/* А так написано в доке "FileStructure SM-101":
			* SCALE MAIN BOARD TYPE:
 				0 – SM90 TWB1600;				1 – SM90 TWB2150/2151;
				2 – SM500;						3 – SM90 TOUCH SCREEN;
 				4 – SM500 TOUCH SCREEN;			5– SM300
				90- DP90						91- DPS90
				92- DI90
				10- SM100						11- SM101(+)
		*/
		// Отправляем команду весам и смотрим ответ 06,07- ок
		if(model == 0x11 || model == 0x10){ //SM-100
			retval = send_to_scale(cmd_100, sizeof(cmd_100), Buffer);
		}
		else{ //все остальные: SM-300, SM-500 etc
			retval = send_to_scale(cmd_300, sizeof(cmd_300), Buffer);
		}
		if(retval>0){
			if(Buffer[0] == 0x06 || Buffer[0] == 0x07)
			if(V) printf("time sync OK!\n");
		}
		else{
			closesocket(conn_socket);
			WSACleanup();
			return -2;
		}
		
	}
	else
	{
		closesocket(conn_socket);
		WSACleanup();
		return -1;
	}


	
	closesocket(conn_socket);
	WSACleanup();
	return 0;
}

int send_to_scale(char data[], int len_data, char Buffer[256])
{
	if(V) printf("Send command to scales(len: %d):\n", len_data);
	char cmd[] = {0xf7, 0x4f, 0x00, 0x00, 0x00, 0x00};
	if(V) str_hex(data, len_data);

	int retval = send(conn_socket, data, len_data, 0);
	if (retval == SOCKET_ERROR) { 
		fprintf(stderr,"send() failed: error %d\n", WSAGetLastError());
		return -1;
	}
	retval = recv(conn_socket, Buffer, 256, 0 );
	if (retval == SOCKET_ERROR) {
		fprintf(stderr, "recv() failed: error %d\n", WSAGetLastError());
		return -1;
	}
	if(V){
		if (retval == 0)
			printf("Server closed connection\n");
		printf("Received %d bytes: \n", retval);
		str_hex(Buffer, retval);
	}
	return retval;
}



