#include <stdio.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <semaphore.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/wait.h>

struct zajednickiProstor {
    char M[5];                  // međuspremnik u kojeg proizvođači zapisuju, a iz kojeg potrošači čitaju
    int ULAZ;                   // od kojeg mjesta proizvođači zapisuju znakove
    int IZLAZ;                  // od kojeg mjesta potrošači čitaju znakove
    sem_t PISI;                 // semafor za pisanje u međuspremnik
    sem_t PORUKE;               // semafor za čitanje iz međuspremnika
    sem_t slobodnaMjesta;       // broj slobodnih mjesta u međuspremniku 
};

// funkcija koju obavljaju proizvođači
void posaoProizvodaca(int id, char* string, struct zajednickiProstor* zp) {

    int i = 0;
    do {
        sleep(1);                                           // spavaj 1 sekundu

        sem_wait(&zp->slobodnaMjesta);                      // čekaj semafor slobodnaMjesta
        sem_wait(&zp->PISI);                                // čekaj semafor PISI

        zp->M[zp->ULAZ] = string[i];                        // upiši slovo u međuspremnik
        printf("PROIZVOĐAČ%d -> %c\n", id, string[i]);      // traženi output
        zp->ULAZ = (zp->ULAZ + 1) % 5;                      // povećaj ULAZ (čitaj sa sljedećeg mjesta)

        sem_post(&zp->PISI);                                // postavi semafor PISI
        sem_post(&zp->PORUKE);                              // postavi semafor PORUKE

        ++i;                                                // sljedeći znak
    } while (string[i - 1] != '\0');                        // vrti petlju sve dok nismo došli do kraja argumenta

}

void posaoPotrosaca(struct zajednickiProstor* zp, int brojProizvodaca) {

    char s[1000];
    int i = 0, countBackslash0 = 0;
    do {

        sem_wait(&zp->PORUKE);
        s[i] = zp->M[zp->IZLAZ];
        printf("POTROŠAČ <- %c\n", s[i]);

        if (s[i] == '\0') {
            ++countBackslash0;
        }

        zp->IZLAZ = (zp->IZLAZ + 1) % 5;
        sem_post(&zp->slobodnaMjesta);

        ++i;

    } while (countBackslash0 < brojProizvodaca);

    printf("Primljeno je: ");
    for (int j = 0; j < i; ++j) {
        if (s[j] != '\0') {
            printf("%c", s[j]);
        }
    }

    printf("\n");

}

int main(int argc, char** argv) {

    struct zajednickiProstor* zp;       // pokazivač na strukturu zajednickiProstor
    int ID;                             // ID dijeljenog segmenta

    int brojProizvodaca = argc - 1;     // broj proizvođača = broj argumenata (-1 jer prvi argument je ime programa)

    // Stvaramo segment zajedničke memorije
    ID = shmget(IPC_PRIVATE, sizeof(struct zajednickiProstor), 0600);

    // Povezi se na segment zajedničke memorije
    zp = shmat(ID, NULL, 0);

    // Na kraju obrisi segment zajedničke memorije
    shmctl(ID, IPC_RMID, NULL);

    // inicijalizacija varijabli za čitanje i pisanje u međuspremnik (indeksi)
    zp->ULAZ = 0;
    zp->IZLAZ = 0;

    sem_init(&zp->PISI, 1, 1);                  // inicijalizacija semafora za pisanje (inicijalno: smijemo upisivati podatke)
    sem_init(&zp->PORUKE, 1, 0);                // inicijalizacija semafora za čitanje (inicijalno: nema podataka za čitanje)
    sem_init(&zp->slobodnaMjesta, 1, 5);        // inicijalizacija semafora koji broji slobodna mjesta (inicijalno: 5 slobodnih mjesta)

    // stvaranje procesa proizvođača
    for (int i = 0; i < brojProizvodaca; ++i) {
        if (fork() == 0) {
            posaoProizvodaca(i + 1, argv[i + 1], zp);
            exit(0);
        }
    }

    // stvaranje procesa potrošača
    if (fork() == 0) {
        posaoPotrosaca(zp, brojProizvodaca);
        exit(0);
    }

    // čekaj da proizvođači i potrošači završe s poslom
    for (int i = 0; i < brojProizvodaca + 1; ++i) {
        wait(NULL);
    }

    sem_destroy(&zp->PISI);                     // na kraju programa obriši semafor za pisanje u međuspremnik
    sem_destroy(&zp->PORUKE);                   // na kraju programa obriši semafor za čitanje iz međuspremnika
    sem_destroy(&zp->slobodnaMjesta);           // na kraju programa obriši semafor koji pamti broj slobodnih mjesta međuspremnika

    shmdt(zp);          // oslobodi dijeljenu memoriju

    return 0;

}