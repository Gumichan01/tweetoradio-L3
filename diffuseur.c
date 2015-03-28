

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <pthread.h>

#include "tweet_posix_lib.h"

#include "diffuseur.h"
#include "queue.h"
#include "stack.h"
#include "ip_convert.h"
#include "parser.h"



void Diffuseur_init(Diffuseur *d)
{
    memset(d->id,'#',ID_LENGTH);

    memset(d->ip_multicast,'0',IP_LENGTH);
    memset(d->port_multicast,'0',PORT_LENGTH);

    memset(d->ip_local,'0',IP_LENGTH);
    memset(d->port_local,'0',PORT_LENGTH);

    d->file_attente = NULL;
    d->historique = NULL;
}


void * tcp_server(void *param)
{
    Diffuseur *diff = (Diffuseur *) param;
    /** TODO création serveur TCP pour reception du client */
    int err;
    int sockserv;
    int sockclt;
    Client_info * clt_info = NULL;

    char err_msg[MSG_LENGTH];

    socklen_t sz;
    struct sockaddr_in in;
    struct sockaddr_in clt;

    pthread_t th;

    memset(&in, 0, sizeof(struct sockaddr));    /* Nettoyage */


    sockserv = socket(PF_INET,SOCK_STREAM,0);

    if(sockserv == -1)
    {
        perror("tcp_server - socket() ");
        pthread_exit(NULL);
    }

    in.sin_family = AF_INET;
    in.sin_port = htons(atoi(diff->port_local));
    in.sin_addr.s_addr = htonl(INADDR_ANY);

    ip_set(diff->ip_local,MAX_BYTES);
    err = ip_to15(inet_ntoa(in.sin_addr),diff->ip_local);

    if(err == -1)
    {
        perror("tcp_server - ip_from15() ");
        close(sockserv);
        pthread_exit(NULL);
    }

    sz = sizeof(struct sockaddr_in);

    err = bind(sockserv,(struct sockaddr *) &in, sz);

    if(err == -1)
    {
        perror("tcp_server - bind() ");
        close(sockserv);
        pthread_exit(NULL);
    }

    err = listen(sockserv,NB_CLIENTS);

    if(err == -1)
    {
        perror("tcp_server - listen() ");
        close(sockserv);
        pthread_exit(NULL);
    }

    printf("Diffuseur %.8s en attente sur le port : %d \n",diff->id,ntohs(in.sin_port));

    sz = sizeof(in);

    while(1)
    {
            sockclt = accept(sockserv,(struct sockaddr *) &clt,&sz);

            if(sockclt == -1)
            {
                perror("tcp_server - accept() ");
                break;
            }

            printf("Client connecté - IP : %s | Port : %d \n",inet_ntoa(clt.sin_addr),ntohs(clt.sin_port));

            /* On crée une structure relative au client */
            clt_info = malloc(sizeof(Client_info));


            if(clt_info == NULL)
            {   /* On ne peut pas sous-traiter ça au thread, on ferme la connexion */
                perror("tcp_server - malloc() ");

                /* On envoie un message d'erreur */
                sprintf(err_msg,"SRVE Communication evec le serveur %.8s impossible\r\n",diff->id);
                send(sockclt,err_msg,strlen(err_msg),0);

                close(sockclt);
                continue;
            }

            /* On récupère les information sur le client */
            strcpy(clt_info->ip,inet_ntoa(clt.sin_addr));
            clt_info->port = ntohs(clt.sin_port);
            clt_info->sockclt = sockclt;

            pthread_create(&th,NULL,tcp_request,clt_info);

    }

    close(sockserv);

    pthread_exit(NULL);
}



/*
    Cette fonction traite les messages clients
*/
void * tcp_request(void * param)
{
    Client_info *c = (Client_info *) param;

    int sockclt;
    char ip_clt[ID_LENGTH+1];
    int port;

    char msg[TWEET_LENGTH];
    int lus;
    int err;

    /* On va utiliser la structure de parsing */
    ParsedMSG p;

    ParserMSG_init(&p);

    /* On récupère les champs */
    strcpy(ip_clt,c->ip);
    port = c->port;
    sockclt = c->sockclt;

    free(param);
    c = NULL;

    /* On rend le thread indépendant */
    pthread_detach(pthread_self());

    lus = recv(sockclt,msg,TWEET_LENGTH,0);

    if(lus < 0 )
    {
       perror("tcp_request - recv() ");
       close(sockclt);
       pthread_exit(NULL);
    }

    /* message trop court ou n'ayant pas le couple '\r''\n' -> INVALIDE */
    if(lus < 2 || msg[lus-1] != '\n' || msg[lus-2] != '\r')
    {
        printf("Message invalide issue du client %s %d \n",ip_clt,port);

        close(sockclt);
        pthread_exit(NULL);
    }


    /* Il faut aussi vider le cache, sinon -> problème d'affichage */
    printf("Message reçu depuis le client %s - %d : ",ip_clt,port);
    fflush(stdout);

    write(1,msg,lus);
    printf("\n");

    /* On analyse le message */
    err = parse(msg,&p);

    if(err == -1)
    {
        perror("tcp_request - parse() ");

        printf("Message non reconnu du client %s - %d | Fermeture connexion. \n",ip_clt,port);

        close(sockclt);
        pthread_exit(NULL);
    }

    printf("Message reconnu par le diffuseur et prète à être traité\n");

    close(sockclt);
    pthread_exit(NULL);
}




























