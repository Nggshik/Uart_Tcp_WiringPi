#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <pthread.h>
#include <dlfcn.h>

#define _portDigits 6
#define _addressLength 16
#define _dirLength 32
#define SUCCESS 0

void *libHandle; /* через этот указатель будем "общаться" с библиотекой */
int mutex = 0;
int stop_thread = 1;

typedef struct thread_control_structure
{
    int client_socket;
    int rx_buf_num;
    int tx_buf_num;
    int wait_recv;
    char *rx_buf;
    char *tx_buf;
} thread_control_structure;

struct uart_func_struct
{
    int (*Uart_set_configuration)(int, int);
    int (*Uart_define_init)(int);
    int (*Uart_tasks)(int, int, char *);
    int (*Uart_data_send)(int , char *);
} uart_func_struct;


void *thread_handler(void *arg)
{
    thread_control_structure *cntrl = (thread_control_structure *)arg;

    if (cntrl->tx_buf_num)
    {
        send(cntrl->client_socket, cntrl->tx_buf, cntrl->tx_buf_num, 0);
        cntrl->tx_buf_num = 0;
        free(cntrl->tx_buf_num);
    }
    if (cntrl->wait_recv)
        if (0 >= (cntrl->rx_buf_num = recv(cntrl->client_socket, cntrl->rx_buf, 100, NULL)))
            pthread_exit(1);
            else
            {
                uart_func_struct.Uart_data_send(0,cntrl->rx_buf);
                stop_thread = 1;
                memset(cntrl->rx_buf,0,sizeof(char)*100);
            }
            
    pthread_exit(0);
}

struct parameters
{
    char listenPort[_portDigits]; // сетевой порт, связанный с нашей программой
    char listenIpAddress[16];     // сетевой адрес для приёма соединений от клиентов
    char workDir[32];             // рабочий каталог программы с нужными ей файлами
};



int openSocket(struct parameters *params, struct uart_func_struct *uart_func_ptr);
void openUartLib(struct uart_func_struct *func_str_ptr);

int main()
{
    pid_t pid;
    int count = 0, fd;
    struct parameters *params = (struct parameters *)calloc(1, sizeof(struct parameters));
    *(char **)(&params->listenPort) = (char *)calloc(1, _portDigits);
    *(char **)(&params->listenIpAddress) = (char *)calloc(1, _addressLength);
    *(char **)(&params->workDir) = (char *)calloc(1, _dirLength);

    fd = open("./config", O_RDONLY);
    if (0 < fd)
    {
        count = read(fd, params->listenPort, _portDigits - 1);
        count = read(fd, params->listenIpAddress, _addressLength - 1);
        count = read(fd, params->workDir, _dirLength - 1);
        close(fd);
        fprintf(stdout, " Port = [%s]\n Address = [%s]\n WorkDir = [%s]\n",
                params->listenPort,
                params->listenIpAddress,
                params->workDir);
    }

    openUartLib(&uart_func_struct);
    printf("Lib opened\n");
    struct uart_func_struct *uart_func_ptr = &uart_func_struct;
    pid = fork();
    if (pid < 0)
    {
        fprintf(stderr, "РОДИТЕЛЬ: породить потомка не получилось: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    if (pid > 0)
    {
        /* Порождение прошло успешно, а значение pid, большее 0, означает, 
         * что здесь мы в родительском процессе, который можно уже завершить. 
         */
        fprintf(stdout, "РОДИТЕЛЬ: потомок порождён, и родитель завершается.\n");
        exit(EXIT_SUCCESS);
    }

    openSocket(params, uart_func_ptr);

    // while (1)
    // {;}
    return 0;
}

int openSocket(struct parameters *params, struct uart_func_struct *uart_func_ptr)
{
    int sockId, client_socket, status, i = 0, clientStructSize = 0;
    struct addrinfo *hints, *server;
    struct sockaddr_in *client;
    char clientIpAddress[16];
    memset(clientIpAddress, '\0', 16);
    client = (struct sockaddr_in *)calloc(1, sizeof(struct sockaddr_in));
    clientStructSize = sizeof(struct sockaddr_in);

    sockId = socket(AF_INET, SOCK_STREAM, 6);
    if (-1 == sockId)
    {
        fprintf(stderr, "Ошибка при создании сокеты: %s\n", strerror(errno));
        free(client);
        close(sockId);
        exit(EXIT_FAILURE);
    }
    // hints – набор указаний для getaddrinfo(3)
    hints = (struct addrinfo *)calloc(1, sizeof(struct addrinfo));
    hints->ai_family = AF_INET;       // использовать семейство TCP/IP
    hints->ai_socktype = SOCK_STREAM; // использовать поточную передачу данных
    hints->ai_protocol = 6;           // использовать TCP
    hints->ai_flags = AI_PASSIVE;     // создать принимающую часть соединения

    status = getaddrinfo(params->listenIpAddress, params->listenPort, hints, &server);
    if (0 != status)
    {
        fprintf(stderr, "Ошибка при преобразовании адреса: %s\n", gai_strerror(status));
        free(client);
        free(hints);
        close(sockId);
        exit(EXIT_FAILURE);
    }
    if (-1 == bind(sockId, server->ai_addr, server->ai_addrlen))
    {
        fprintf(stderr, "Ошибка привязки сокеты к адресу: %s\n", strerror(errno));
        free(client);
        free(hints);
        close(sockId);
        exit(EXIT_FAILURE);
    }
    if (-1 == listen(sockId, 10))
    {
        fprintf(stderr, "Ошибка включения прослушивания на сокете: %s\n", strerror(errno));
        free(client);
        free(hints);
        close(sockId);
        exit(EXIT_FAILURE);
    }
    for (;;)
    {
        if (-1 == (client_socket = accept(sockId, (struct sockaddr *)client, &clientStructSize)))
        {
            fprintf(stderr, "Ошибка ожидания на сокете: %s\n", strerror(errno));
            free(client);
            free(hints);
            close(sockId);
            exit(EXIT_FAILURE);
        }
        inet_ntop(AF_INET, &(client->sin_addr), clientIpAddress, sizeof(clientIpAddress));
        fprintf(stdout, "Client connected [%s:%d]\n", clientIpAddress,
                ntohs(client->sin_port));
        // for (;;)
        //     ;
        // здесь можно выполнять всякие полезные действия по поводу полученного запроса.
        pthread_t *threadId = malloc(sizeof(pthread_t));
        int result = 0;

        thread_control_structure thread_cntr;
        char *tx_str = "Hello from server!\n";
        char rx_buf[100] = {0};
        char buf[100] = {0};
        thread_cntr.rx_buf = rx_buf;
        thread_cntr.tx_buf = tx_str;
        thread_cntr.tx_buf_num = strlen(tx_str);
        thread_cntr.client_socket = client_socket;
        thread_cntr.wait_recv = 1;


        int num = 0;
        for (;;)
        {
            if (stop_thread)
            {
                stop_thread = 0;
                thread_cntr.tx_buf_num = num;
                thread_cntr.tx_buf = malloc(sizeof(char)*thread_cntr.tx_buf_num);
                memcpy(thread_cntr.tx_buf,buf,sizeof(char)*thread_cntr.tx_buf_num);
                memset(buf,0,sizeof(char)*100);

                result = pthread_create(threadId, NULL, thread_handler, (thread_control_structure *)&thread_cntr);
                if (result != 0)
                {
                    printf("main error: can't create thread, status = %d\n", result);
                    exit(1);
                }

            }
            num = uart_func_ptr->Uart_tasks(0, 115200, buf);

            if (num)
            {
               stop_thread = 1;
            } 
            if(stop_thread){
                if (pthread_cancel(*threadId))
                {
                    printf("pthread_cancel err\n");
                }
                int retval = 0;
                if (0 != (result = pthread_join(*threadId, &retval)))
                    printf("do smth\n");
                if (retval == 1)
                {
                    printf("Client disconected\n");
                    close(client_socket);
                    break;
                }
                // printf("Thread done\n");
                fflush(stdout);
            }
        }
        free(threadId);
    }

    free(client);
    free(hints);

    close(client_socket);
    close(sockId);
    dlclose(libHandle);
    return 0;
}

void openUartLib(struct uart_func_struct *func_str_ptr)
{

    char *error;
    /* С этого места начинается основная работа нашей программы. */

    libHandle = dlopen("./uart.dll", RTLD_NOW);
    if (NULL == libHandle)
    {
        printf("Can't open lib %s.\n", dlerror());
        exit(EXIT_FAILURE);
    }
    *(void **)(&func_str_ptr->Uart_set_configuration) = dlsym(libHandle, "Uart_set_configuration");
    if ((error = dlerror()) != NULL)
    {
        printf("Can't open Uart_set_configuration\n");
        exit(EXIT_FAILURE);
    }
    *(void **)(&func_str_ptr->Uart_define_init) = dlsym(libHandle, "Uart_define_init");
    if ((error = dlerror()) != NULL)
    {
        printf("Can't open Uart_define_init\n");
        exit(EXIT_FAILURE);
    }
    *(void **)(&func_str_ptr->Uart_tasks) = dlsym(libHandle, "Uart_tasks");
    if ((error = dlerror()) != NULL)
    {
        printf("Uart_tasks\n");
        exit(EXIT_FAILURE);
    }
        *(void **)(&func_str_ptr->Uart_data_send) = dlsym(libHandle, "Uart_data_send");
    if ((error = dlerror()) != NULL)
    {
        printf("Uart_data_send\n");
        exit(EXIT_FAILURE);
    }

}