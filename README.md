## Dumitru Bianca-Andreea 322CA

# Tema 2 - Aplicatie TCP/UDP

## Descriere generala

Aplicatia implementeaza un server si un client care comunica prin protocolul TCP/UDP. Severul este capabil sa primeasca de la clienti comenzi pentru manipularea subscriptiilor (subscribe, unsubscribe, exit, store-and-forward). In functie de mesajele primite de la UDP, serverul notifica clientii abonati la un anumit topic.

## Implementare

Pentru realizarea codului m-am folosi de urmatoarele structuri:
* __packet_from_TCP__ - structura care retine si delimiteaza mesajele primite de la clientii TCP in topic, SF si action_type (subscribe/unsubscribe/exit)
* __packet_from_UDP__ - structura care retine si delimiteaza mesajele primite de la clientii UDP in topic, tipul de date si continutul mesajului
* __packet_to_TCP__ - structura care formeaza pachetul trimis catre clientii TCP
* __subscription__ - structura care reprezinta o subscriptie a unui client la un anumit topic si cu SF-ul ales (0/1)
* __client__ - structura care retine informatii despre un client:
    * socket, id, port
    * **waiting_topics** = vector de pachete care trebuiesc trimise clientului imedat dupa revonectarea acestuia la server. **De mentionat faptul ca acest vector contine doar pachetele care au SF-ul setat pe 1**
    * subscribed_topics = vectorul care contine toate subscriptiile clientului, indiferent de SF

### Serverul

Drept 'baze de date' ale serverului m-am folosit de **registered_clients** care este un map de forma **<id_client, client>** si de **topics** care este un vector cu toate topicurile primite de la UDP. Serverul trateaza urmatoarele cazuri:
1. Primesc de pe socketul de listen un pachet, care reprezinta id-ul unui client TCP. In functia __verify_client()__ verific statusul clientului. Daca nu a mai fost gasit in registered_clients, il inregistrez, daca exista deja un client inregistrat cu acelasi id, afisez un mesaj de eroare, iar daca clientul a mai fost conectat la server in trecut si acum se reconnecteaza, ii trimit toate pachetele din waiting_topics si il adaug inapoi in registered_clients.
2. Primesc un pachet de pe socket-ul UDP. Cu ajutorul functiei __construct_TCP()__ construiesc pachetul care trebuie trimis catre clientul TCP. Ma folosesc de functia __manage_packet_from_UDP()__ pentru a vedea daca trimit pachetul clientului sau il adaug in coada de asteptare pentru a fi retrimis la reconectare.
3. Primesc un pachet de la un client TCP cu o comanda. In functia __manage_packet_from_TCP()__ realizez comanda primita (subscribe/unsubscribe/exit). **De mentionaat** ca in aceasta functie tratez cazurile de comenzi invalide cum ar fi:
    * clientul incearca sa se aboneze la un topic care nu a fost primit de la UDP
    * clientul incearca sa se aboneze de 2 ori la acelasi topic
    * clientul incearca sa se dezaboneze de la un topic la care nu este abonat
Pentru fiecare dintre aceste cazuri, afisez un mesaj corespunzator de eroare la stderr.
4. Primesc comanda exit de la tastatura, iar in acest caz inchid toti socketii si ies din program.

### Clientul

Pentru implementarea clientului m-am folosit de 2 functii:
1. __send_to_server()__ - construieste un pachet de tip packet_from_TCP in functie de comenzile primite de la stdin si il trimite serverului
2. __receive_from_server()__ - primeste pachete de la server de tipul: confirmari/erori ale realizarii unei comenzi cerute de client, mesaje de la UDP, comanda "exit" (inchiderea clientului)

## Referinte
https://stackoverflow.com/questions/9164848/how-to-use-select-and-fd-set-in-socket-programming
https://www.ibm.com/docs/en/zos/2.4.0?topic=calls-select
https://stackoverflow.com/questions/16720672/using-stdin-stdout-as-a-socket
https://www.ibm.com/docs/en/zos/2.3.0?topic=functions-accept-accept-new-connection-socket
https://man7.org/linux/man-pages/man2/poll.2.html
