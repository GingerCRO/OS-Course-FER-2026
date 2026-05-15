#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

struct Tim {

    int boksZauzet;                 // je li boks zauzet
    int vozacJeUBoksu;              // je li neki vozač iz tog tima već u boksu
    int zastavicaJeSpustena;        // je li zastavica spuštena
    int mehanicariSuZavrsili;       // brojac koliko je mehaničara promijenilo gumu
    int zastavicaJePodignuta;       // je li zastavica podignuta
    int preostaloZamjenaGuma;       // koliko još puta mehaničari moraju zamijeniti gume kako bi tim završio utrku

    pthread_mutex_t mutex;          

    pthread_cond_t uvjetBoksJeZauzet;               // dretve koje čekaju dok boks ne bude slobodan
    pthread_cond_t uvjetVozacJeUBoksu;              // dretve koje čekaju dok vozač ne stane u boks
    pthread_cond_t uvjetZastavicaJeSpustena;        // dretve koje čekaju dok je zastavica spuštena
    pthread_cond_t uvjetMehanicariSuZavrsili;       // dretve koje čekaju dok mehaničari ne promijene sve gume
    pthread_cond_t uvjetZastavicaJePodignuta;       // dretve koje čekaju dok lollipop ne podigne zastavicu

};

struct Tim tim[2];      // strukture za dva tima

int utrkaJeZapocela = 0;
pthread_mutex_t mutexUtrka;
pthread_cond_t uvjetUtrka;

void odiUBoks(int brojTima) {

    // zaključaj mutex
    pthread_mutex_lock(&tim[brojTima].mutex);

    printf("Vozac tima %d ceka ispred boksa\n", brojTima);
    // čekaj sve dok boks nije slobodan
    while (tim[brojTima].boksZauzet == 1) {
        pthread_cond_wait(&tim[brojTima].uvjetBoksJeZauzet, &tim[brojTima].mutex);
    }

    // nakon što se boks oslobodio, odi u boks
    tim[brojTima].boksZauzet = 1;
    printf("Vozac tima %d je stao u boks, ceka zamjenu guma\n", brojTima);

    // podigni zastavicu vozacJeUBoksu kako bi osoba sa zastavicom znala da si došao
    tim[brojTima].vozacJeUBoksu = 1;
    pthread_cond_signal(&tim[brojTima].uvjetVozacJeUBoksu);

    // čekaj da osoba sa zastavicom ne podigne zastavicu (odnosno čekaj sve dok zamjena guma ne bude gotova)
    while (tim[brojTima].zastavicaJePodignuta == 0) {
        pthread_cond_wait(&tim[brojTima].uvjetZastavicaJePodignuta, &tim[brojTima].mutex);
    }
    printf("Vozac tima %d se vraca na stazu\n", brojTima);

    // nakon što je zamjena guma gotova, vrati se na stazu, odnosno oslobodi boks
    tim[brojTima].boksZauzet = 0;
    tim[brojTima].vozacJeUBoksu = 0;
    tim[brojTima].zastavicaJePodignuta = 0;
    pthread_cond_signal(&tim[brojTima].uvjetBoksJeZauzet);

    // otključaj mutex
    pthread_mutex_unlock(&tim[brojTima].mutex);

}

void* vozacFunkcija(void* brojTima) {

    int tim = (int)(long)brojTima;                      // broj tima (tim 0 ili tim 1)

    pthread_mutex_lock(&mutexUtrka);                    // zaključaj mutex mutexUtrka
    while (utrkaJeZapocela == 0) {                      // sve dok utrka nije počela
        pthread_cond_wait(&uvjetUtrka, &mutexUtrka);    // čekaj početak utrke
    }
    pthread_mutex_unlock(&mutexUtrka);                  // otključaj mutex mutexUtrka

    int x = 2000 + rand() % (5000 - 2000 + 1);
    printf("Vozac tima %d vozi %d ms\n", tim, x);
    usleep(x * 1000);                                   // vozi x milisekundi

    odiUBoks(tim);                                      // odi u boks zamijeniti gume

    usleep((7000 - x) * 1000);                          // vozi (7000 - x) milisekundi

    odiUBoks(tim);                                      // odi u boks zamijeniti gume

    int y = 2000 + rand() % (3000 - 2000 + 1);
    usleep(y * 1000);                                   // vozi y milisekundi

    printf("Vozac tima %d je zavrsio utrku\n", tim);

    return NULL;

}

void* mehanicarFunkcija(void* brojTima) {

    int brojtima = (int)(long)brojTima; 

    while (tim[brojtima].preostaloZamjenaGuma > 0) {        // dok traje utrka

        pthread_mutex_lock(&tim[brojtima].mutex);           // zaključaj mutex

        while (tim[brojtima].zastavicaJeSpustena == 0) {
            pthread_cond_wait(&tim[brojtima].uvjetZastavicaJeSpustena, &tim[brojtima].mutex);       // čekaj dok je zastavica spuštena
        }

        pthread_mutex_unlock(&tim[brojtima].mutex);         // otključaj mutex prije zamjene kotača

        int z = 2000 + rand() % (4000 - 2000 + 1);
        usleep(z * 1000);                                   // simuliraj zamjenu guma u trajanju od z milisekunda
        printf("Tim %d: zavrsena zamjena guma (%d/4)\n", brojtima, tim[brojtima].mehanicariSuZavrsili + 1);

        pthread_mutex_lock(&tim[brojtima].mutex);           // zaključaj mutex nakon zamjene guma

        tim[brojtima].mehanicariSuZavrsili++;             // javi osobi sa zastavicom da su gume zamijenjene
        pthread_cond_signal(&tim[brojtima].uvjetMehanicariSuZavrsili);      // javi osobi sa zastavicom da je zamjena guma obavljena

        pthread_mutex_unlock(&tim[brojtima].mutex);

    }

    return NULL;

}

void* lollipopFunkcija(void* brojTima) {
    
    int brojtima = (int)(long)brojTima;

    while (tim[brojtima].preostaloZamjenaGuma > 0) {            // dok traje utrka

        pthread_mutex_lock(&tim[brojtima].mutex);               // zaključaj mutex

        while (tim[brojtima].vozacJeUBoksu == 0) {
            pthread_cond_wait(&tim[brojtima].uvjetVozacJeUBoksu, &tim[brojtima].mutex);     // čekaj dok neki vozač ne dođe u boks
        }

        tim[brojtima].zastavicaJeSpustena = 1;                  // spusti zastavicu
        printf("Tim %d: zastavica je spustena\n", brojtima);
        pthread_cond_broadcast(&tim[brojtima].uvjetZastavicaJeSpustena);        // probudi sve mehaničare

        while (tim[brojtima].mehanicariSuZavrsili < 4) {
            pthread_cond_wait(&tim[brojtima].uvjetMehanicariSuZavrsili, &tim[brojtima].mutex);      // čekaj dok nisu svi mehaničari promijenili gume
        }

        tim[brojtima].zastavicaJePodignuta = 1;
        printf("Tim %d: Zastavica je podignuta\n", brojtima);
        pthread_cond_signal(&tim[brojtima].uvjetZastavicaJePodignuta);       // pusti vozača natrag u utrku

        // resetiraj za novu zamjenu guma
        tim[brojtima].zastavicaJeSpustena = 0;
        tim[brojtima].mehanicariSuZavrsili = 0;
        tim[brojtima].preostaloZamjenaGuma--;

        pthread_mutex_unlock(&tim[brojtima].mutex);

    }

    return NULL;

}

int main() {

    srand(time(NULL));

    pthread_t vozac[6], mehanicar[8], lollipop[2];          // deklariranje potrebnih dretvi

    // inicijalizacija timova
    for (int i = 0; i < 2; ++i) {
        tim[i].boksZauzet = 0;
        tim[i].vozacJeUBoksu = 0;
        tim[i].zastavicaJeSpustena = 0;
        tim[i].mehanicariSuZavrsili = 0;
        tim[i].zastavicaJePodignuta = 0;
        tim[i].preostaloZamjenaGuma = 3 * 2;

        pthread_mutex_init(&tim[i].mutex, NULL);

        pthread_cond_init(&tim[i].uvjetBoksJeZauzet, NULL);
        pthread_cond_init(&tim[i].uvjetVozacJeUBoksu, NULL);
        pthread_cond_init(&tim[i].uvjetZastavicaJeSpustena, NULL);
        pthread_cond_init(&tim[i].uvjetMehanicariSuZavrsili, NULL);
        pthread_cond_init(&tim[i].uvjetZastavicaJePodignuta, NULL);
    }

    // inicijalizacija prije početka utrke
    pthread_mutex_init(&mutexUtrka, NULL);
    pthread_cond_init(&uvjetUtrka, NULL);

    // stvaramo potrebnih šest dretvi za vozače
    for (int i = 0; i < 6; ++i) {
        pthread_create(&vozac[i], NULL, vozacFunkcija, (void*)(long)(i / 3));
    }

    // stvaramo potrebnih osam dretvi za mehaničare
    for (int i = 0; i < 8; ++i) {
        pthread_create(&mehanicar[i], NULL, mehanicarFunkcija, (void*)(long)(i / 4));
    }

    // stvaramo potrebne dvije dretve za osobe sa zastavicom
    for (int i = 0; i < 2; ++i) {
        pthread_create(&lollipop[i], NULL, lollipopFunkcija, (void*)(long)i);
    }

    // POČETAK UTRKE
    pthread_mutex_lock(&mutexUtrka);
    utrkaJeZapocela = 1;
    printf("Utrka je zapocela\n");
    pthread_cond_broadcast(&uvjetUtrka);
    pthread_mutex_unlock(&mutexUtrka);

    // pričekaj da sve dretve završe s poslom
    for (int i = 0; i < 6; ++i) {
        pthread_join(vozac[i], NULL);
    }
    for (int i = 0; i < 8; ++i) {
        pthread_join(mehanicar[i], NULL);
    }
    for (int i = 0; i < 2; ++i) {
        pthread_join(lollipop[i], NULL);
    }

    return 0;

}