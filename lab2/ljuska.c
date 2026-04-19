#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <termios.h>

struct Proces
{
    pid_t pid;
    char ime[128];
};

pid_t foregroundProcesPid = -1;         // inicijalno nema pokrenutog procesa

char** split(char *str, char delimiter, int *count) {
    int parts = 1;

    // koliko uneseni string ima dijelova
    for (int i = 0; str[i] != '\0'; i++) {
        if (str[i] == delimiter) {
            parts++;
        }
    }

    *count = parts;

    // dinamicko polje u koje spremamo dijelove unesene naredbe
    char **result = malloc(parts * sizeof(char*));

    int start = 0;
    int index = 0;

    // iteriramo kroz naredbu i razdvajamo ju po razmaku
    for (int i = 0; ; i++) {
        if (str[i] == delimiter || str[i] == '\0') {
            int len = i - start;

            result[index] = malloc((len + 1) * sizeof(char));
            strncpy(result[index], str + start, len);
            result[index][len] = '\0';

            index++;
            start = i + 1;
        }

        if (str[i] == '\0'){
            break;
        }
    }

    return result;      // vrati dinamicko polje s dijelovima naredbe
}

// funkcija za obradu signala SIGINT (2)
void ObradiSigint(int sig) {
    if (foregroundProcesPid != -1) {
        kill(foregroundProcesPid, SIGINT);
    }
}

int main(void) {

    // stvaranje dinamickog polja za history unesenih naredbi
    char** povijestUnesenihNaredbi = NULL;
    int brojUnesenihNaredbi = 0;

    // stvaranje dinamickog polja struktura za stvorene procese (programe koji se vrte u pozadini)
    struct Proces* pokrenutiProcesi = NULL;
    int brojPokrenutihProcesa = 0;

    struct sigaction act;                   // stvori strukturu tipa sigaction za rad s potrebnim funkcijama

    act.sa_handler = ObradiSigint;          // funkcija koja se pokrece u slucaju prekida
    sigemptyset(&act.sa_mask);              // nemoj blokirati obradu signala za vrijeme oporavka od prekida
    act.sa_flags = SA_NODEFER;              // prekid moze prekinuti sam sebe
    sigaction(SIGINT, &act, NULL);          // u slucaju signala SIGINT koristi strukturu act za obradu
    signal(SIGTTOU, SIG_IGN);               // ignoriraj signal SIGTTOU

    while(1) {      

        // stvaranje polja znakova u koji cemo spremiti trenutni direktorij
        char trenutniDirektorij[128];
        getcwd(trenutniDirektorij, 128);

        // korisnik unosi naredbu
        char unosNaredbe[128];
        printf("[%s] > ", trenutniDirektorij);
        fgets(unosNaredbe, 128, stdin);

        // dodavanje znaka \0 na kraj unesene naredbe
        int i = 0;
        while (unosNaredbe[i] != '\n' && unosNaredbe[i] != '\0') {
            ++i;
        }
        unosNaredbe[i] = '\0';

        // stvaranje polja povijesti unesenih naredbi
        // dodavanje unesene naredbe u polje povijesti
        // dodaj naredbu u povijest ako unesena naredba nije prazan string
        if (strcmp(unosNaredbe, "") > 0) {
            povijestUnesenihNaredbi = realloc(povijestUnesenihNaredbi, (brojUnesenihNaredbi + 1) * sizeof(char*));
            povijestUnesenihNaredbi[brojUnesenihNaredbi] = malloc(strlen(unosNaredbe) + 1);
            strcpy(povijestUnesenihNaredbi[brojUnesenihNaredbi], unosNaredbe);
            ++brojUnesenihNaredbi;
        }

        int brojDijelova = 0;                                               // koliko unesena naredba ima parametara
        char** dijeloviNaredbe = split(unosNaredbe, ' ', &brojDijelova);    // splitaj naredbu po razmacima

        if (unosNaredbe[0] == '!') {                                    // korisnik trazi naredbu iz historya preko !
            int rbr = atoi(unosNaredbe + 1);                            // pretvori string nakon znaka ! u broj
            if (rbr < 1 || rbr > brojUnesenihNaredbi) {                 // ako je korisnik unio pogresan broj
                printf("Naredba s brojem %d ne postoji.\n", rbr);
                continue;
            }
            printf("%s\n", povijestUnesenihNaredbi[rbr - 1]);           // ispisi naredbu pod trazenim brojem
            strcpy(unosNaredbe, povijestUnesenihNaredbi[rbr - 1]);      // kopiraj naredbu iz povijesti u unos
            brojDijelova = 0;
            dijeloviNaredbe = split(unosNaredbe, ' ', &brojDijelova);
        }

        if (strcmp(dijeloviNaredbe[0], "cd") == 0) {                    // korisnik je upisao naredbu cd
            chdir(dijeloviNaredbe[1]);
        }
        else if (strcmp(dijeloviNaredbe[0], "history") == 0) {          // korisnik je upisao naredbu history
            for (int i = 0; i < brojUnesenihNaredbi; ++i) {
                printf("\t%4d %s\n", i + 1, povijestUnesenihNaredbi[i]);
            }
        }
        else if (strcmp(dijeloviNaredbe[0], "ps") == 0) {               // korisnik je upisao naredbu ps
            printf("%3s\t%3s\n", "PID", "ime");
            for (int i = 0; i < brojPokrenutihProcesa; ++i) {
                int status;
                int procesJosTraje = waitpid(pokrenutiProcesi[i].pid, &status, WNOHANG);
                if (procesJosTraje == 0) {                              // proces je aktivan
                    printf("%3d\t%s\n", pokrenutiProcesi[i].pid, pokrenutiProcesi[i].ime);
                }
            }
        }
        else if (strcmp(dijeloviNaredbe[0], "kill") == 0) {             // korisnik je upisao naredbu kill
            pid_t trazeniPid = atoi(dijeloviNaredbe[1]);                // pid procesa kojeg zelimo iskljuciti
            int brojSignala = atoi(dijeloviNaredbe[2]);                 // signal kojeg korisnik zeli poslati

            int pronadenSignal = 0;
            for (int i = 0; i < brojPokrenutihProcesa; ++i) {           
                if (pokrenutiProcesi[i].pid == trazeniPid) {            // je li proces pokrenut od strane ove ljuske
                    pronadenSignal = 1;         
                    break;
                }
            }

            if (pronadenSignal) {                                       // ako smo pronasli trazeni proces                                
                kill(trazeniPid, brojSignala);                          // posalji signal procesu
            }
            else {                                                      // proces nije pokrenut od strane ove ljuske
                printf("Proces s PID-om %d nije pokrenut od strane ove ljuske.\n", trazeniPid);     
            }
        }
        else if (strcmp(dijeloviNaredbe[0], "exit") == 0) {             // korisnik je upisao naredbu exit
            for (int i = 0; i < brojPokrenutihProcesa; ++i) {
                kill(pokrenutiProcesi[i].pid, SIGKILL);                 // zaustavi sve procese koji su pokrenuti u ovoj ljusci
            }
            exit(0);
        } 
        else {                                                          // korisnik je upisao neku drugu naredbu (fork + execvp)                                         
            int uPozadini = (strcmp(dijeloviNaredbe[brojDijelova - 1], "&")) == 0;
            if (uPozadini) {  // korisnik pokrece program u backgroundu
                // izbrisi & iz unesene naredbe
                free(dijeloviNaredbe[brojDijelova - 1]);
                dijeloviNaredbe[brojDijelova - 1] = NULL;                
            } 
            pid_t pid = fork();             // stvori novi proces
            if (pid == 0) {                 // posao djeteta
                setpgid(0, 0);
                execvp(dijeloviNaredbe[0], dijeloviNaredbe);
                perror(dijeloviNaredbe[0]);          // greska u naredbi
                exit(1);
            }
            else {                          // posao roditelja
                if (uPozadini) {
                    pokrenutiProcesi = realloc(pokrenutiProcesi, (brojPokrenutihProcesa + 1) * sizeof(struct Proces));  // napravi mjesto u dinamickom polju za novi proces
                    pokrenutiProcesi[brojPokrenutihProcesa].pid = pid;                                                  // dodaj pid novog procesa u polje pokrenutih procesa
                    strcpy(pokrenutiProcesi[brojPokrenutihProcesa].ime, dijeloviNaredbe[0]);
                    ++brojPokrenutihProcesa;
                }
                else {
                    foregroundProcesPid = pid;
                    tcsetpgrp(STDIN_FILENO, getpgid(pid));          // terminal daj procesu dijete
                    waitpid(pid, NULL, 0);                          // pricekaj da dijete zavrsi s poslom
                    tcsetpgrp(STDIN_FILENO, getpgid(0));            // vrati terminal glavnom programu
                    foregroundProcesPid = -1;
                }
            }
        }

    }

    return 0;

}