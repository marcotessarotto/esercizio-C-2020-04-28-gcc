/*
 * esercizio-C-2020-04-28-gcc.c
 *
 *  Created on: Apr 28, 2020
 *      Author: marco
 */
/***************TESTO ESERCIZIO***************
scrivere il seguente programma:

verifica se esiste la cartella src; se non esiste, crea la cartella src
(creare directory: man 2 mkdir, controllo esistenza file, ad esempio: https://github.com/marcotessarotto/exOpSys/blob/6bd8188175c5d4cc380808cbce2befebac65557b/011filesrw/01filesrw.c#L17 )

dentro src, se non esiste già, crea il file hello_world.c contenente il codice sorgente del programma che scrive "hello world" su stdout.

dentro src, crea un file output.txt; se il file esiste già, lo "tronca" a dimensione 0

il programma riceve in un suo signal handler il segnale SIGCHLD
esempio: https://github.com/marcotessarotto/exOpSys/blob/6bd8188175c5d4cc380808cbce2befebac65557b/006.3signals4ipc/signals4ipc.c#L136

entra in un ciclo senza fine:

monitora il file hello_world.c e ogni volta che viene notificato un evento di modifica del file (quando il file viene modificato da un editor quale pico, gedit o altro) il programma esegue il compilatore gcc per compilare il codice sorgente contenuto nel file hello_world.c e produrre un file eseguibile di nome hello (gcc hello_world.c -o hello).
il programma aspetta la terminazione di gcc (e riceve il valore di uscita di gcc perchè servirà nel punto successivo).

*seconda parte dell'esercizio:*
se gcc ha successo, il programma invoca fork() ed il processo figlio esegue il programma hello appena compilato dove però l'output di hello va "redirezionato" sul file output.txt (quello creato all'avvio del programma, più sopra).
il programma non aspetta la terminazione del processo figlio ma "prende nota" del PID del processo figlio aggiungendolo in coda ad un array dinamico chiamato pid_list.

gestire i processi zombie:
ogni volta che un processo figlio termina, il programma riceve un segnale SIGCHLD ed invoca wait.
vedere ad esempio: https://github.com/marcotessarotto/exOpSys/blob/6bd8188175c5d4cc380808cbce2befebac65557b/006.3signals4ipc/signals4ipc.c#L105
[il signal handler potrebbe anche rimuovere il PID del processo figlio "aspettato" da pid_list ma ci può essere un problema di concorrenza nell'accesso a pid_list, questo aspetto va discusso]

*terza parte dell'esercizio*
con un meccanismo da definire più in dettaglio (cioè: parliamone in classe), il programma termina i processi figli eseguiti nel punto precedente, se questi rimangono in esecuzione per più di 10 secondi.
[il controllo sui processi figli ancora in esecuzione potrebbe essere fatto ogni volta che il programma passa attraverso la seconda parte dell'esercizio oppure periodicamente (ad es. ogni secondo) attraverso un timer (che non abbiamo ancora visto a lezione) con l'uso di semafori (che vedremo le prossime lezioni)]
*/

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <sys/inotify.h>

#include <errno.h>

#define BUF_LEN 4096

char buf[BUF_LEN] __attribute__ ((aligned(__alignof__(struct inotify_event))));

pid_t * pid_list;
int pid_list_len;

int create_directory(char * dname);
void process_signal_handler(int signum);

int main(int argc, char * argv[]) {
	char * text_to_write;
	int res, text_to_write_len;
	if(create_directory("src") == 1)
		printf("directory src creata \n");

	text_to_write = "#include <stdio.h>\n int main (){\nprintf(\"Hello World!\\n\");\n return 0;\n}";
	text_to_write_len = strlen(text_to_write);

	int fd = open("./src/hello_world.c",
				  O_CREAT | O_TRUNC | O_WRONLY,
				  S_IRUSR | S_IWUSR // l'utente proprietario del file avrà i permessi di lettura e scrittura sul nuovo file
				 );

	if (fd == -1) { // errore!
		perror("open()");
		exit(EXIT_FAILURE);
	}

	if((res = write(fd, text_to_write, text_to_write_len)) == -1) {
		perror("write()");
		exit(EXIT_FAILURE);
	}

	// close file
	if (close(fd) == -1) {
		perror("close");
		exit(EXIT_FAILURE);
	}

	fd = open("./src/output.txt",
				  O_CREAT | O_TRUNC | O_WRONLY,
				  S_IRUSR | S_IWUSR // l'utente proprietario del file avrà i permessi di lettura e scrittura sul nuovo file
				 );

	if (fd == -1) { // errore!
		perror("open()");
		exit(EXIT_FAILURE);
	}

	// close file
	if (close(fd) == -1) {
		perror("close");
		exit(EXIT_FAILURE);
	}

	if (signal(SIGCHLD, process_signal_handler) == SIG_ERR) {
		perror("signal");
		exit(EXIT_FAILURE);
	}

	//listato per l'inizializzazione del controllo del file hello_world.c
	int inotifyFd, wd, num_bytes_read;
	inotifyFd = inotify_init();
	if (inotifyFd == -1) {
		perror("inotify_init");
		exit(EXIT_FAILURE);
	}
	wd = inotify_add_watch(inotifyFd, "./src/hello_world.c", IN_MODIFY);
	if (wd == -1) {
		perror("inotify_init");
		exit(EXIT_FAILURE);
	}


	//ciclo infinito
	for (;;) {
		num_bytes_read = read(inotifyFd, buf, BUF_LEN);
		if (num_bytes_read == 0) {
			printf("read() from inotify fd returned 0!");
			exit(EXIT_FAILURE);
		}

		if (num_bytes_read == -1) {

			if (errno == EINTR) {
				printf("read(): EINTR\n");
				continue;
			} else {
				perror("read()");
				exit(EXIT_FAILURE);
			}
		}

		pid_t child_pid, ws;
		int wstatus, modal_result;
		switch ((child_pid = fork())) {
				case -1:
					perror("problema con fork");

					exit(EXIT_FAILURE);

				case 0: // processo FIGLIO
					execlp("gcc", "gcc", "./src/hello_world.c", "-o", "hello", (char*)NULL);

					// On success, execve() does not return, on error -1 is returned, and errno is set appropriately.

					perror("execve");

					exit(EXIT_FAILURE);
					break;

				default:
					// processo PADRE
					ws = waitpid(child_pid, &wstatus, WUNTRACED | WCONTINUED);
					if (ws == -1) {
		                perror("[parent] waitpid");
		                exit(EXIT_FAILURE);
		            }

		            if (WIFEXITED(wstatus)) {	//controlla se il figlio è terminato

		            	modal_result = WEXITSTATUS(wstatus);	//valore restituito dal figlio

		                //printf("[parent] child process è terminato, ha restituito: %d\n", modal_result);
		            }
		            if(modal_result == 0){
		            	switch ((child_pid = fork())) {
							case -1:
								perror("problema con fork");

								exit(EXIT_FAILURE);

							case 0: // processo FIGLIO

								fd = open("./src/output.txt",
											  O_WRONLY| O_APPEND,
											  S_IRUSR | S_IWUSR // l'utente proprietario del file avrà i permessi di lettura e scrittura sul nuovo file
											 );

								if (fd == -1) { // errore!
									perror("open()");
									exit(EXIT_FAILURE);
								}

								if (dup2(fd, STDOUT_FILENO) == -1) {
									perror("problema con dup2");

									exit(EXIT_FAILURE);
								}

								if (close(fd) == -1) {
									perror("close");
									exit(EXIT_FAILURE);
								}

								execve("./hello", (char *)NULL , (char *)NULL);

								// On success, execve() does not return, on error -1 is returned, and errno is set appropriately.

								perror("execve");

								exit(EXIT_FAILURE);
								break;

							default:
								// processo PADRE
								pid_list_len++;
								pid_list = realloc(pid_list, sizeof(pid_t) * pid_list_len);
								pid_list[pid_list_len - 1] = child_pid;

						}
		            }
			}
	}

	return 0;
}

int create_directory(char * dname){	//ritorna 0 se esiste già la directory, 1 se l'ha creata
	struct stat st = {0};

	if (stat(dname, &st) == -1) {	//controllo se esiste la directory, se non esiste la crea
	    mkdir(dname, 0700);
		return 1;
	}
	else
		return 0;
}

void process_signal_handler(int signum) {
	// riceviamo SIGCHLD: Child stopped or terminated
	wait(NULL);
	//TODO: manca la gestione per la cancellazione dei pid dalla pid_list
}

