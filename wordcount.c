/* wordcount.c

   make wordcount LDFLAGS=-pthread
   make wordcount LDFLAGS=-pthread CFLAGS="-fsanitize=thread -g"

   Usage:

   1. Open terminals and create named pipes (maximum 10) representing the
   CharacterReader inteface
   mkfifo /tmp/mypipe; cat > /tmp/mypipe

   2. Pass the pipes as command line arguments to the progam
   ./wordcount /tmp/mypipe /tmp/mypipe2 [...]

   3. Send text to the pipes from the terminals (NOTE: the io is linebuffered in
   your terminal, so press enter)

   4. The program will print the current (combined) counts of the words to
   stdout.

*/

#include <stdio.h>
#include <string.h>	/* strcmp, strcpy */
#include <stdlib.h>	/* qsort */
#include <pthread.h>
#include <fcntl.h>	/* O_RDONLY */
#include <unistd.h>	/* sleep */
#include <ctype.h>	/* isalpha */

#define WC_IN_WORD 1
#define WC_OUT_OF_WORD 0

typedef struct {
	char word[128];
	int freq;
} wc_Word;

typedef struct {
	wc_Word words[128];
	int len;
} wc_Context;

typedef struct {
	FILE *fd;
	wc_Context *ctxt;
} wc_Thread_Context;

static pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;

static int wc_compare_words(const void *w1, const void *w2) {
	wc_Word *a = (wc_Word *)w1;
	wc_Word *b = (wc_Word *)w2;
	if(a->freq == b->freq)
		return (strcmp(a->word,b->word));
	else
		return (b->freq - a->freq);
}

static void wc_print_words(wc_Context *ctxt) {
	int s;
	s = pthread_mutex_lock(&mtx);
	if (s != 0)
		exit(EXIT_FAILURE);

	printf ("\nCurrent word frequency count: \n");
	for (int i = 0; i < ctxt->len; i++) {
		printf("%s - %d \n",ctxt->words[i].word,ctxt->words[i].freq);
	}
	printf ("----------------------------- \n");

	s = pthread_mutex_unlock(&mtx);
	if (s != 0)
		exit(EXIT_FAILURE);
}

static void wc_store_word(wc_Context *ctxt, char *buf) {
	int found = -1;
	/* lookup word */
	for (int i = 0; i < ctxt->len; i++){
		if (strcmp(ctxt->words[i].word, buf) == 0){
			found = i;
		}
	}
	/* if not found, add as a new word */
	if (found == -1){
		strcpy(ctxt->words[ctxt->len].word, buf);
		ctxt->words[ctxt->len].freq = 1;
		ctxt->len++;
	}
	else {
		ctxt->words[found].freq++;
	}
}

static void wc_new_word_callback(char *buf, wc_Context *ctxt){
	int s;
	s = pthread_mutex_lock(&mtx);
	if (s != 0)
		exit(EXIT_FAILURE);

	wc_store_word(ctxt, buf);
	qsort(ctxt->words, ctxt->len, sizeof(wc_Word), wc_compare_words);

	s = pthread_mutex_unlock(&mtx);
	if (s != 0)
		exit(EXIT_FAILURE);

}

static void wc_read_fd(FILE *fd, void (*callback)(char *buf,wc_Context *ctxt),
		       wc_Context *ctxt) {
	int state,i;
	char c,buf[128];

	state = WC_OUT_OF_WORD;
	i = 0;

	/* read single characters as instructed in the exercise */
	while ((c = getc(fd)) != EOF) {
		if (!isalpha((unsigned char) c)) {
			if (state == WC_IN_WORD){
				buf[i] = '\0';
				callback(buf,ctxt);
			}
			i = 0;
			state = WC_OUT_OF_WORD;
		}
		else
		{
			if (state == WC_OUT_OF_WORD)
				state = WC_IN_WORD;
			buf[i++] = tolower((unsigned char) c);
		}
	}
	/* call close() here */
}

static void* wc_thread_read(void *arg)
{
	wc_Thread_Context *tctxt = (wc_Thread_Context *) arg;

	wc_read_fd(tctxt->fd, wc_new_word_callback, tctxt->ctxt);
	pthread_exit( NULL );
}

static void* wc_thread_print(void *arg)
{
	wc_Thread_Context *tctxt = (wc_Thread_Context *) arg;

	for(;;){
		sleep(10);
		wc_print_words(tctxt->ctxt);
	}
	pthread_exit( NULL );
}


int main(int argc, char *argv[])
{
	/* context stores all the word counts */
	wc_Context ctxt;

	/* we need max 10 threads to handle inputs, and one to print results  */
	pthread_t t[10], print;
	wc_Thread_Context tctxt[10];
	void *res;
	int s,i;

	/* init word frequency counter context */
	ctxt.len = 0;

	/* start thread: read input from stdio */
	tctxt[0].fd = stdin;
	tctxt[0].ctxt = &ctxt;
	s = pthread_create(&t[0], NULL, wc_thread_read, &tctxt[0]);
	if (s != 0)
		exit(EXIT_FAILURE);

	/* start threads: read input from fds */
	for (i=1; i<argc; i++){
		tctxt[i].ctxt = &ctxt;
		tctxt[i].fd = fopen(argv[i], "r");
		if (tctxt[i].fd == NULL)
			exit(EXIT_FAILURE);
		s = pthread_create(&t[i], NULL, wc_thread_read, &tctxt[i]);
		if (s != 0)
			exit(EXIT_FAILURE);
	}

	/* start printer thread */
	s = pthread_create(&print, NULL, wc_thread_print, &tctxt[0]);
	if (s != 0)
		exit(EXIT_FAILURE);

	for (i=0; i<argc; i++){
		s = pthread_join(t[i], &res);
		if (s != 0)
			exit(EXIT_FAILURE);
	}

	/* if all reader threads exited, stop printer thread */
	s = pthread_cancel(print);
	if (s != 0)
		exit(EXIT_FAILURE);

	s = pthread_join(print, &res);
	if (s != 0)
		exit(EXIT_FAILURE);

	/* print the totals once more at the end */
	wc_print_words(tctxt->ctxt);

	exit(EXIT_SUCCESS);
}

