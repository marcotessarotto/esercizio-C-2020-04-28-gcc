#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/inotify.h>

#include <fcntl.h>
#include <errno.h>
#include <semaphore.h>
#include <dirent.h>

#define CHECK_ERR(a, msg){if((a) == -1){perror(msg); exit(-1); } }

#define BUF_LEN 4096

sem_t * semaphore_pid_list;

int pid_list_len=1;
pid_t * pid_list;

char buf[BUF_LEN] __attribute__ ((aligned(__alignof__(struct inotify_event))));

int check_file_exist(char * fname) {

	if( access( fname, F_OK ) != -1 ) {
	    // file exists
		return 1;
	} else {
	   	return 0;
	}
}


//il programma riceve in un suo signal handler il segnale SIGCHLD
void signal_handler(int signum) {
	// riceviamo SIGCHLD: Child stopped or terminated

	pid_t child_pid;

	child_pid = wait(NULL);

	printf("[parent] signal handler: processo %u è terminato, ora termino anche io\n", child_pid);

	if (sem_wait(semaphore_pid_list) == -1) {
			perror("sem_wait");
			exit(EXIT_FAILURE);
		}

	for(int i=0; i< pid_list_len;i++){

		if(pid_list[i] == child_pid){
			pid_list[i] = 0;
			break;
		}
	}

	if (sem_post(semaphore_pid_list) == -1) {
			perror("sem_post");
			exit(EXIT_FAILURE);
		}
}

int main(int argc, char * argv[]){

	if(check_file_exist("./src") == 0)
		mkdir("./src", ACCESSPERMS);

//dentro src, se non esiste già, crea il file hello_world.c contenente il codice sorgente del programma
//che scrive "hello world" su stdout.
//uso funzione PIPE


	int fd_hello = open("./src/hello_world.c", O_WRONLY | O_CREAT | O_TRUNC , S_IRUSR | S_IWUSR);

	CHECK_ERR(fd_hello,"open()")

	int codice_len = -1;

	char * codice;
	codice = "#include <stdio.h>\n int main(){\nprintf(\"Hello world!\\n\");\n return 0;\n}";

	codice_len = strlen(codice);
	int writin;

	writin= write(fd_hello, codice, codice_len);

	CHECK_ERR(writin,"write()")

	close(fd_hello);

//dentro src, crea un file output.txt; se il file esiste già, lo "tronca" a dimensione 0

	int fd_out = open("./src/output.txt", O_CREAT | O_WRONLY | O_TRUNC, S_IRUSR | S_IWUSR);

	CHECK_ERR(fd_out,"open()")

//entra in un ciclo senza fine:

	pid_list = malloc(sizeof(pid_t));

	int fd_notify = inotify_init();

	CHECK_ERR(fd_notify,"inotify_init()")

	int watch_descriptor;

	watch_descriptor = inotify_add_watch(fd_notify, "./src/hello_world.c", IN_MODIFY);

	CHECK_ERR(watch_descriptor,"inotify_add_watch()")


	if ((semaphore_pid_list = sem_open("/semaphore", O_CREAT, 00600, 1)) == SEM_FAILED) {
			perror("sem_open");
			exit(EXIT_FAILURE);
		}
	int num_bytes_read;

	while(1){
//monitora il file hello_world.c e ogni volta che viene notificato un evento di modifica del file
//(quando il file viene modificato da un editor quale pico, gedit o altro) il programma esegue
//il compilatore gcc per compilare il codice sorgente contenuto nel file hello_world.c e produrre
//un file eseguibile di nome hello (gcc hello_world.c -o hello).

	num_bytes_read = read(fd_notify, buf, BUF_LEN);

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

	int status=-1;

	status = system("gcc ./src/hello_world.c -o hello");

		if(WIFEXITED(status)){
			//result =  WEXITSTATUS(status);
			pid_t child_pid;

//*seconda parte dell'esercizio:*
//se gcc ha successo, il programma invoca fork() ed il processo figlio esegue il programma hello
//appena compilato dove però l'output di hello va "redirezionato" sul file output.txt
//(quello creato all'avvio del programma, più sopra).

			switch(child_pid = fork()){

			case 0:
				if (dup2(fd_out, STDOUT_FILENO) == -1) {
					perror("problema con dup2");
					exit(EXIT_FAILURE);
				}

				close(fd_out);

				execve("./hello", (char *)NULL, (char *)NULL);

				perror("execve");

				exit(EXIT_FAILURE);
				break;
			case -1:
				break;
				exit(-1);
			default:

				if (sem_wait(semaphore_pid_list) == -1) {
							perror("sem_wait");
							exit(EXIT_FAILURE);
						}
//il programma non aspetta la terminazione del processo figlio ma "prende nota" del PID
//del processo figlio aggiungendolo in coda ad un array dinamico chiamato pid_list.
				pid_list = realloc(pid_list, pid_list_len*sizeof(pid_t));

				if(pid_list == NULL){
					perror("realloc()");
					exit(-1);
				}

				pid_list[pid_list_len-1] = child_pid;
				pid_list_len++;

				if (sem_post(semaphore_pid_list) == -1) {
						perror("sem_post");
						exit(EXIT_FAILURE);
					}
				break;
				exit(EXIT_SUCCESS);

			}
			if (signal(SIGCHLD, signal_handler) == SIG_ERR) {
						perror("signal");
						exit(EXIT_FAILURE);
			}

		}
}









//

return 0;

	}





