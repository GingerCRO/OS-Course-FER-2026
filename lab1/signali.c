#include <stdio.h> 
#include <stdlib.h> 
#include <unistd.h> 
#include <signal.h> 
#include <time.h>
#include <errno.h>
#include <string.h>

#define STACK_SIZE 20
#define STACK_STRING_LENGTH 7

struct timespec t0;         // vrijeme pocetka programa 
char KON[5][9] = {"—"};     // kontekst 
int K_Z[5] = {0};           // kontrolne zastavice
int T_P = 0;                // trenutni prioritet

char stack[STACK_SIZE][STACK_STRING_LENGTH] = {"—"};            // sustavski stog
int top = -1;                                                   // stack pointer

void push(const char *string) {                                 // funkcija za dodavanje elementa na stog
    if (top < STACK_SIZE - 1) {                                 // provjera ima li slobodnog mjesta na stogu
        strcpy(stack[++top], string);                           // dodaj element na stog
        return;
    }
}

char *pop() {                                                   // funkcija za uzimanje elementa sa stoga
    if (top > -1) {                                             // provjera imamo li ista na stogu
        return stack[top--];                                    // vrati trazeni element sa stoga
    }
    return NULL;                                                // ako je stog prazan, vrati NULL
}

char *peek() {                                                  // funkcija koja vraca koji element je prvi na stogu
    if (top > -1) {                                             // provjera imamo li ista na stogu
        return stack[top];                                      // vrati trazeni element sa stoga
    }
    return NULL;                                                // ako je stog prazan, vrati NULL
}


// funkcija koja trenutno vrijeme sprema u varijablu t0
void postavi_pocetno_vrijeme() {
	clock_gettime(CLOCK_REALTIME, &t0);
}

// funkcija koja vraca koliko je vremena proslo od pokretanja programa
void vrijeme(void) {
	struct timespec t;

	clock_gettime(CLOCK_REALTIME, &t);

	t.tv_sec -= t0.tv_sec;
	t.tv_nsec -= t0.tv_nsec;
	if (t.tv_nsec < 0) {
		t.tv_nsec += 1000000000;
		t.tv_sec--;
	}

	printf("%03ld.%03ld:", t.tv_sec, t.tv_nsec/1000000);        // ispisi vrijeme proteklo od pocetka rada programa
}

// funkcija za delay odredeni broj sekundi (ako je prekinuta signalom, kasnije nastavlja gdje je stala)
void spavaj(time_t sekundi) {
	struct timespec koliko;
	koliko.tv_sec = sekundi;
	koliko.tv_nsec = 0;

	while (nanosleep(&koliko, &koliko) == -1 && errno == EINTR) {}      // cekaj odredeni broj sekundi
}

// funkcija za obradu prekida
void obradi_prekid() {
    for (int i = 0; i < 5; ++i) {
        spavaj(1);                          // obrada prekida traje 5 sekundi (simulacija)
    }
    strcpy(KON[T_P], "—");                  // nakon obradenog prekida, ponisti sadrzaj KON[T_P]
}

// funkcija za obradu signala SIGINT
void obradi_sigint(int sig) { 
    
    int newT_P;                             // varijabla u koju spremamo prioritet prekida
    scanf("%d", &newT_P);                   // unesi prioritet

    K_Z[newT_P] = 1;                        // podigni odgovarajucu kontrolnu zastavicu

    // pronadi najvecu zastavicu - prekid najviseg prioriteta prvi se obraduje
    newT_P = 0;
    for (int i = 4; i >= 1; --i) {
        if (K_Z[i] == 1) {
            newT_P = i;
            break;
        }
    }

    while (newT_P > T_P) {                  // dok je novi prioritet veci od trenutnog prioriteta

        // T_P pretvori u string
        char T_P_str[4];
        snprintf(T_P_str, sizeof(T_P_str), "%d", T_P);

        // spremi string vrijednost u obliku "reg[T_P]" na stog 
        char newStackValue[STACK_STRING_LENGTH];
        snprintf(newStackValue, sizeof(newStackValue), "reg[%s]", T_P_str);
        push(newStackValue);

        // spremi kontekst trenutne dretve u obliku "T_P,reg[T_P]" u KON[newT_P]
        char newKonValue[9];
        snprintf(newKonValue, sizeof(newKonValue), "%s,%s", T_P_str, peek());
        strcpy(KON[newT_P], newKonValue);

        // output
        vrijeme();
        printf("\t%d\t\t%s\t\t%s\t\t%s\t\t%s\t\t%s\t\t%d%d%d%d\n", T_P, peek(), KON[1], KON[2], KON[3], KON[4], K_Z[1], K_Z[2], K_Z[3], K_Z[4]);
        
        K_Z[newT_P] = 0;
        T_P = newT_P;
        
        printf("\t\t%d\t\t%s\t\t%s\t\t%s\t\t%s\t\t%s\t\t%d%d%d%d\n", newT_P, "—", KON[1], KON[2], KON[3], KON[4], K_Z[1], K_Z[2], K_Z[3], K_Z[4]);
        fflush(stdout);

        // skoci na funkciju za obradu prekida
        obradi_prekid();

        // povratak na prethodni prioritet
        char *prethodni = pop();
        int stari_T_P = T_P;
        if (prethodni != NULL) {
            sscanf(prethodni, "reg[%d]", &T_P);         // iz "reg[T_P]" izvuci broj T_P
        }

        // output
        vrijeme();
        printf("\t%d\t\t%s\t\t%s\t\t%s\t\t%s\t\t%s\t\t%d%d%d%d\n", stari_T_P, peek() ? peek() : "—", KON[1], KON[2], KON[3], KON[4], K_Z[1], K_Z[2], K_Z[3], K_Z[4]);
        // K_Z[newT_P] = 0;            // ponisti kontrolnu zastavicu u K_Z
        printf("\t\t%d\t\t%s\t\t%s\t\t%s\t\t%s\t\t%s\t\t%d%d%d%d\n", T_P, "—", KON[1], KON[2], KON[3], KON[4], K_Z[1], K_Z[2], K_Z[3], K_Z[4]);
        fflush(stdout);

        // ažuriraj newT_P za sljedeću iteraciju (nadi novi najveci T_P)
        newT_P = 0;
        for (int i = 4; i >= 1; --i) {
            if (K_Z[i] == 1) {
                newT_P = i;
                break;
            }
        }

    }
    
} 

int main() { 

    postavi_pocetno_vrijeme();              // spremi vrijeme s pocetka rada programa

    struct sigaction act;                   // stvori strukturu tipa sigaction za rad s potrebnim funkcijama
 
    act.sa_handler = obradi_sigint;         // funkcija koja se pokrece u slucaju prekida
    sigemptyset(&act.sa_mask);              // nemoj blokirati obradu signala za vrijeme oporavka od prekida
    act.sa_flags = SA_NODEFER;              // prekid može prekinuti sam sebe
    sigaction(SIGINT, &act, NULL);          // u slucaju signala SIGINT koristi strukturu act za obradu

    // konteksti su na pocetku prazni, odnosno inicijaliziraj ih na "—"
    for (int i = 1; i < 5; ++i) {
        strcpy(KON[i], "—");
    } 

    // output
    printf("%s\t\t%s\t\t%s\t\t%s\t\t%s\t\t%s\t\t%s\t\t%s\n", "vrijeme", "T_P", "stog", "KON[1]", "KON[2]", "KON[3]", "KON[4]", "K_Z");
    vrijeme();
    printf("\t%d\t\t%s\t\t%s\t\t%s\t\t%s\t\t%s\t\t%d%d%d%d\n", T_P, "—", KON[1], KON[2], KON[3], KON[4], K_Z[1], K_Z[2], K_Z[3], K_Z[4]);
    fflush(stdout);
    
    while(1) { 
        spavaj(1); 
    } 
 
    return 0; 

}