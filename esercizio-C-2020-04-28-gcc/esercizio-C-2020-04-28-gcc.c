#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/inotify.h>
#include <errno.h>

#define BUF_LEN 4096

char buf[BUF_LEN] __attribute__ ((aligned(__alignof__(struct inotify_event))));

int check_file_exist(char * fname) {

	if( access( fname, F_OK ) != -1 ) {
	    // file exists
		return 1;
	} else {
	   	return 0;
	}
}

int inotify_modify(struct inotify_event *i){


	int modify =-1;

    if (i->cookie > 0)
        printf("cookie=%4d \n", i->cookie);

    // The  name  field is present only when an event is returned for a file
    // inside a watched directory; it identifies the filename within the watched directory.
    // This filename is null-terminated .....

    if (i->len > 0)
        printf("file name = %s \n", i->name);
    else
    	printf("*no file name* \n"); // event refers to watched directory

    // IN_CLOSE_NOWRITE  File or directory not opened for writing was closed.
    if (i->mask & IN_CLOSE_WRITE){
    	system("gcc src/hello_world.c -o hello");
    	modify = 0;
    }
    return modify;

}


void signal_handler(int signum) {
	// riceviamo SIGCHLD: Child stopped or terminated

	pid_t child_pid;

	child_pid = wait(NULL);

	printf("[parent] signal handler: processo %u è terminato, ora termino anche io\n", child_pid);

}

int main(){

	if(check_file_exist("src") == 0)
		mkdir("src", ACCESSPERMS);

//dentro src, se non esiste già, crea il file hello_world.c contenente il codice sorgente del programma
//che scrive "hello world" su stdout.
//uso funzione PIPE


	int fd_hello = open("src/hello_world.c", O_WRONLY | O_CREAT | O_TRUNC , S_IRUSR | S_IWUSR);

	if(fd_hello == -1) { // errore!
		perror("open()");
		exit(EXIT_FAILURE);
	}

	FILE* hello = fdopen(fd_hello, "w");
	if(hello == NULL) { // errore!
		perror("fdopen()");
		exit(EXIT_FAILURE);
	}

	int fd_hello_orig = open("/home/utente/git/exOpSys/000hello-world/hello_world.c", O_WRONLY | O_CREAT | O_TRUNC , S_IRUSR | S_IWUSR);

	if(fd_hello_orig == -1) { // errore!
		perror("open()");
		exit(EXIT_FAILURE);
	}

	FILE* hello_orig = fdopen(fd_hello, "w");
	if(hello_orig == NULL) { // errore!
		perror("fdopen()");
		exit(EXIT_FAILURE);
	}
	int ch;

	while((ch = fgetc(hello_orig)) != EOF){
	      fputc(ch, hello);
	}

	close(fd_hello_orig);
	fclose(hello);

//dentro src, crea un file output.txt; se il file esiste già, lo "tronca" a dimensione 0

	int fd_out = open("src/output.txt", O_CREAT | O_WRONLY | O_TRUNC, S_IRUSR | S_IWUSR);

	if(fd_out == -1) { // errore!
		perror("open()");
		exit(EXIT_FAILURE);
	}

//il programma riceve in un suo signal handler il segnale SIGCHLD



//entra in un ciclo senza fine:

	pid_t * pid_list;
	int contapid=1;

	pid_list = malloc(sizeof(pid_t));

	while(1){

//monitora il file hello_world.c e ogni volta che viene notificato un evento di modifica del file

	int fd_notify = inotify_init();
	fd_notify = inotify_add_watch(fd_notify, "src/hello_world.c", IN_CLOSE_WRITE);

	if(fd_notify==-1){
		perror("inotify_add_watch");
		exit(EXIT_FAILURE);
	}

//(quando il file viene modificato da un editor quale pico, gedit o altro) il programma esegue
//il compilatore gcc per compilare il codice sorgente contenuto nel file hello_world.c e produrre
//un file eseguibile di nome hello (gcc hello_world.c -o hello).
	int num_bytes_read;

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

	struct inotify_event *event;
	int res;

	for (char * p = buf; p < buf + num_bytes_read; ) {
		event = (struct inotify_event *) p;

		res = inotify_modify(event);

		p += sizeof(struct inotify_event) + event->len;
		// event->len is length of (optional) file name
	}

	if(res == 0){

		switch(fork()){

		case 0:
			pid_list[contapid-1] = getpid();
			int pfd[2];

			if (pipe(pfd) == -1) {
				perror("problema con pipe");

				exit(EXIT_FAILURE);
			}
			if (dup2(pfd[0], STDIN_FILENO) == -1) {
				perror("problema con dup2");
				exit(EXIT_FAILURE);
			}
			close(pfd[0]);

			if (dup2(pfd[1], STDOUT_FILENO) == -1) {
				perror("problema con dup2");
				exit(EXIT_FAILURE);
			}
			close(pfd[1]);

			execlp("hello", "hello", (char * ) NULL);

			perror("problema con execlp(1)");

			exit(EXIT_SUCCESS);
			break;
		case -1:
			break;
			exit(-1);
		default:
			pid_list = realloc(pid_list, contapid*sizeof(pid_t));
			if(pid_list == NULL){
				perror("malloc()");
				exit(-1);
			}
			contapid++;

			break;
			exit(EXIT_SUCCESS);

	}
		if (signal(SIGCHLD, signal_handler) == SIG_ERR) {
			perror("signal");
			exit(EXIT_FAILURE);
		}

	}
//*seconda parte dell'esercizio:*
//se gcc ha successo, il programma invoca fork() ed il processo figlio esegue il programma hello
//appena compilato dove però l'output di hello va "redirezionato" sul file output.txt
//(quello creato all'avvio del programma, più sopra).



//
//il programma non aspetta la terminazione del processo figlio ma "prende nota" del PID
//del processo figlio aggiungendolo in coda ad un array dinamico chiamato pid_list.


	}

}



