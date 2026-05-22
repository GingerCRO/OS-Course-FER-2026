#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <stdint.h>

int M, N;                           // M okvira, N procesa
uint8_t disk[10][1024];             // simulirani disk koji služi za pohranu sadržaja stranica
uint8_t okvir[100][64];             // simulirani radni spremnik od M okvira veličine 64 okteta
uint16_t tablica[10][16];           // tablica prevođenja za svaki od N procesa
int t = 0;                          // sat

// za podatak iz fizičkog RAM-a, gdje se on nalazi u okviru
int okvir_proces[100];          // u kojem procesu je spremljen taj okvir iz RAM-a
int okvir_stranica[100];        // u kojoj stranici se nalazi taj proces iz RAM-a

uint16_t dohvati_fizicku_adresu(int proces, uint16_t x) {

    int index = (x >> 6) & 0x0F;        // gornja četiri bita predstavljaju indeks stranice
    int offset = x & 0x3F;              // donjih šest bitova predstavlja pomak u okviru stranice

    // pogledaj bit prisutnosti (šesti bit)
    if ((tablica[proces][index] & 0x20) == 0) {
        printf("\tPromasaj!\n\t\t");

        // pronađi prazan okvir
        int trenutniOkvir = -1;
        for (int i = 0; i < M; ++i) {
            if (okvir_proces[i] == -1) {
                trenutniOkvir = i;
                break;
            }
        }

        // nema praznih okvira -> LRU izbacivanje
        if (trenutniOkvir == -1) {
            int izbaciIndex = -1, izbaciProces = -1, najmanjiLRU = 999;

            // nađi okvir s najmanjim LRU
            int vlasnikProcesa, vlasnikStranice, trenutniLRU;
            for (int i = 0; i < M; ++i) {
                vlasnikProcesa = okvir_proces[i];
                vlasnikStranice = okvir_stranica[i];
                trenutniLRU = tablica[vlasnikProcesa][vlasnikStranice] & 0x1F;    // lru = donjih pet bitova  

                if (trenutniLRU < najmanjiLRU) {
                    najmanjiLRU = trenutniLRU;
                    izbaciProces = vlasnikProcesa;
                    izbaciIndex = vlasnikStranice;
                    trenutniOkvir = i;
                }
            }

            // izbačena adresa
            uint16_t izbacenaAdresa = izbaciIndex << 6;
            printf("Izbacujem stranicu 0x%04x iz procesa %d\n\t\tlru izbacene stranice: 0x%04x\n\t\t", izbacenaAdresa, izbaciProces, najmanjiLRU);

            // izbačeni sadržaj spremi na disk
            for (int i = 0; i < 64; ++i) {
                disk[izbaciProces][izbaciIndex * 64 + i] = okvir[trenutniOkvir][i];
            }

            // postavi bit prisutnosti na 0
            tablica[izbaciProces][izbaciIndex] = 0;

        }

        printf("dodijeljen okvir 0x%04x\n", trenutniOkvir << 6);

        // stranicu iz diska spremi u okvir
        for (int i = 0; i < 64; ++i) {
            okvir[trenutniOkvir][i] = disk[proces][index * 64 + i];
        }

        // ažuriramo tablice kako bi sustav znao tko je novi vlasnik okvira
        okvir_proces[trenutniOkvir] = proces;
        okvir_stranica[trenutniOkvir] = index;

        // zapiši okvir u tablicu i stavi bit prisutnosti na 1
        tablica[proces][index] = (trenutniOkvir << 6) | 0x20;
    }

    // pseudokod: ažuriraj tablicu prevođenja procesa
    tablica[proces][index] = (tablica[proces][index] & 0xFFE0) | (t & 0x1F);        // očisti starih pet bitova pa dodaj t

    // je li donjih 5 bitova došlo do 31
    if ((tablica[proces][index] & 0x1F) == 31) {
        t = 0;          // resetiraj sat

        // postavi lru svim zapisima na nulu
        for (int procesi = 0; procesi < N; ++procesi) {
            for (int stranice = 0; stranice < 16; ++stranice) {
                tablica[procesi][stranice] &= 0xFFE0;
            }
        }

        tablica[proces][index] |= 1;     // trenutna stranica
    }

    int trenutniOkvir = tablica[proces][index] >> 6;                // konačni okvir
    uint16_t fizickaAdresa = (trenutniOkvir << 6) | offset;         // spoji adresu okvira i pomak

    printf("\tfiz. adresa: 0x%04x\n", fizickaAdresa);
    printf("\tzapis tablice: 0x%04x\n", tablica[proces][index]);

    return fizickaAdresa;

}

uint16_t dohvati_sadrzaj(int proces, uint16_t x) {

    // pseudokod: dohvati fizičku adresu
    uint16_t fizickaAdresa = dohvati_fizicku_adresu(proces, x);

    // pronađi indeks okvira i offset
    // okvir je velicine 2^6 = 64 bajta
    int index = fizickaAdresa >> 6;             // indeks okvira je spremljen u viših 6 bitova fizičke adrese
    int offset = fizickaAdresa & 0x3F;          // pomak se nalazi u najnižih 6 bitova

    // little-endian -> prvo zapisujemo niži bajt pa onda viši
    uint8_t niziBajt = okvir[index][offset];
    uint8_t visiBajt = okvir[index][offset + 1];

    // spoji niži i viši bajt u jedan broj
    uint16_t spojeniBajt = niziBajt | (visiBajt << 8);

    return spojeniBajt;

}

void zapisi_sadrzaj(int proces, uint16_t x, uint16_t i) {

    // pseudokod: dohvati fizičku adresu
    uint16_t fizickaAdresa = dohvati_fizicku_adresu(proces, x);

    // odredi index u okviru i offset
    int index = fizickaAdresa >> 6;             // indeks okvira je spremljen u viših 6 bitova fizičke adrese
    int offset = fizickaAdresa & 0x3F;          // pomak se nalazi u najnižih 6 bitova

    // broj u varijabli i rastavi na dva 8-bitna little-endian broja
    // little-endian -> prvo spremamo niži bajt pa viši
    uint8_t niziBajt = i & 0xFF;             // niži bajt = donjih 8 bitova
    uint8_t visiBajt = (i >> 8) & 0xFF;      // viši bajt = gornjih 8 bitova

    // zapiši brojeve u okvir
    okvir[index][offset] = niziBajt;
    okvir[index][offset + 1] = visiBajt;

}

int main(int argc, char* argv[]) {

    // dohvaćanje parametara
    N = atoi(argv[1]);
    M = atoi(argv[2]);

    srand(time(NULL));

    // inicijalizacija tablice i diska
    for (int i = 0; i < N; ++i) {
        
        // postavi sve bitove prisutnosti na nulu
        for (int j = 0; j < 16; ++j) {
            tablica[i][j] = 0;
        }

        // inicijalno popuni disk nulama
        for (int j = 0; j < 1024; ++j) {
            disk[i][j] = 0;
        }

    }

    // inicijalizacija okvira procesa i stranice
    for (int i = 0; i < M; ++i) {
        okvir_proces[i] = -1;
        okvir_stranica[i] = -1;
    }

    // glavni dio programa
    while (1) {       
        for (int proces = 0; proces < N; ++proces) {

            printf("Proces: %d\n", proces);
            printf("\tt: %d\n", t);

            // pseudokod: generiraj nasumičnu logičku adresu koja će biti parna
            uint16_t x = (rand() & 0x3FF) & 0x3FE;             // 0x3FF = 1023 (maksimalna adresa), 0x3FE osigurava da je parna
            printf("\tlog. adresa: 0x%04x\n", x);

            // pseudokod: dohvati sadržaj
            uint16_t sadrzaj = dohvati_sadrzaj(proces, x);
            printf("\tSadrzak adrese: %d\n", sadrzaj);

            // pseudokod: povećaj varijablu sadrzaj
            ++sadrzaj;

            // pseudokod: zapiši sadržaj
            zapisi_sadrzaj(proces, x, sadrzaj);

            // pseudokod: povećaj sat
            ++t;

            // pseudokod: spavaj
            printf("--------------------------------------------------\n");
            sleep(1);

        }
    }

}