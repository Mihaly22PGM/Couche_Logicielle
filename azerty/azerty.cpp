#include <iostream>
#include <winsock2.h>
#pragma comment(lib, "ws2_32.lib")
//#include <pqxx/pqxx>          //Telecharger sur https://github.com/jtv/libpqxx
//Linux socket librairies => erreurs sous windows
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <unistd.h>

using namespace std;

int main()
{
    //std::cout << "yolo" << std::endl;
    int client, server;
    int port = 23;
    char buffer[1024];

    // Initialisation de WinSock
    struct sockaddr_in server_addr;
    socklen_t size;

    //Creation du socket (SOCK_STREAM (TCP) ou SOCK_DGRAM (UDP))
    client = socket(AF_INET, SOCK_STREAM, 0);
    if (client < 0)
    {
        cout << "\nError establishing socket..." << endl;
        exit(1);
    }
    cout << "\n=> Socket server has been created..." << endl;

    //Paramétrage du socket
    server_addr.sin_addr.s_addr = INADDR_ANY;           //Adresse de serveur
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);                   //Port
    
    if ((bind(client, (struct sockaddr*)&server_addr, sizeof(server_addr))) < 0)
    {
        cout << "=> Error binding connection, the socket has already been established..." << endl;
        return -1;
    }

    size = sizeof(server_addr);
    cout << "=> Looking for clients..." << endl;
    listen(client, 1);

    //Database => include pqxx/pqxx
    /*try
    {
        connection C ("dbname = testdb user = postgres password = cohondob hostaddr = 127.0.0.1 port = 5432");
        if (C.is_open())
        {
            std::cout << "Connexion a la base de donnees etablie: " << C.dbname() << std::endl;
        }
        else
        {
            std::cout << "Can't open database" << std::endl;
            return 1;
        }
    }
    catch (const std::exception& e)
    {
        cerr << e.what() << std::endl;
        return 1;
    }*/

    while (1) /* Boucle infinie. Exercice : améliorez ce code. */
    {
        //Réception de la requête du client
        server = recv(client, buffer, sizeof(buffer), 0);

        if (server > 0)
        {
            //Traitement
            std::cout << buffer << std::endl;

            /*Envoi de la requête remaniée à la DB
            sql = "";
            if(SELECT...)
            {
                nontransaction N(C);

                result R( N.exec( sql ));

                for (result::const_iterator c = R.begin(); c != R.end(); ++c)
                {
                    cout << "ID = " << c[0].as<int>() << endl;
                    cout << "Name = " << c[1].as<string>() << endl;
                    cout << "Age = " << c[2].as<int>() << endl;
                    cout << "Address = " << c[3].as<string>() << endl;
                    cout << "Salary = " << c[4].as<float>() << endl;
                }
            }
            else
            {
                work W(C);

                W.exec( sql );
                W.commit();
            }
            */

            close(server);
        }
    }

    //C.disconnect();
    close(client);     //Pas réellement nécessaire puisque boucle infinie juste avant

    return 0;
}