#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/time.h>
#include <assert.h>
#include <unistd.h>  //Header file for sleep(). man 3 sleep for details.
#include <pthread.h>

#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
// #include <sys/io.h>
#include <sys/mman.h>
// Let us create a global variable to change it in threads
int g = 0;

char buffer[2*1024*1024];

// A normal C function that is executed as a thread
// when its name is specified in pthread_create()
void *myThreadFun(void *vargp)
{
    printf("Thread started. Global: %d\n", ++g);
}

struct Segment{
    unsigned char *data;
    int start;
    int end;
    int id;
    int anagram_length;
};

//void processFile()
//{
//    FILE *fp;
//    char *line = NULL;
//    size_t len = 0;
//    ssize_t read;
//
//   fp = fopen("lemmad.txt", "r");
//    if (fp == NULL)
//        exit(EXIT_FAILURE);
//
//   while ((read = getline(&line, &len, fp)) != -1) {
//        // printf("Retrieved line of length %zu :\n", read);
//        //printf("%s", line);
//   }
//   free(line);
//   fclose(fp);
//}

#define TARGET_LENGTH 4
unsigned char *file_content;
int size;
unsigned char mask[256];
unsigned char mask_index[256];
int used_chars;

void processFile_singlebuffer()
{

    FILE *f = fopen("lemmad.txt", "rb");
    // 0.0022, koos skänniga 0.007
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);  /* same as rewind(f); */

    char *string = malloc(fsize + 1);
    fread(string, fsize, 1, f);
    fclose(f);
    //printf("Retrieved  %zu :\n", fsize);
     string[fsize] = 0;
    int i, word_start, word_count = 0;

    for(i = 0; i < fsize; i++)
    {
        if (string[i] == '\n')
        {
            word_count++;
        }
    }
    //printf("Retrieved lines %zu :\n", word_count);
}


void processFile_fgets()
{
    FILE *f = fopen("lemmad.txt", "rb");

    // 0.024
    #define MAX_SIZE 1000
    char line[MAX_SIZE];
    int counter = 0; /*Number of lines*/

    while(fgets(line, sizeof(line), f) != NULL){
    //    counter++;
    }

    fclose(f);
}


void processFile_getline()
{
//    int targetChars[256];
//    targetChars['i'] = 1;
//
    FILE *fp;
    char *line = NULL;
    size_t len = 0;
    ssize_t read;

   fp = fopen("lemmad.txt", "r");
//    if (fp == NULL)
//        exit(EXIT_FAILURE);

   while ((read = getline(&line, &len, fp)) != -1) {
//        if (read == TARGET_LENGTH)
//        {
//            printf("Retrieved line of length %zu :\n", read);
//            printf("%s", line);
//        }
   }
   free(line);
   fclose(fp);
}

void processFile_memmap(char * file_name)
{
    // 0.0057
    struct stat s;
    //const char * file_name = "c:\\temp\\lemmad.txt";
    //const char * file_name = "9x.txt";
    //const char * file_name = "lemmad2.txt";
    int fd = open (file_name, O_RDONLY);

    /* Get the size of the file. */
    int status = fstat (fd, & s);
    size = s.st_size;

    file_content = (char *) mmap (0, size, PROT_READ, MAP_PRIVATE, fd, 0);
//    int word_start, word_count = 0;
//    for (int i = 0; i < size; i++) {
//        if (f[i] == '\n')
//        {
//            word_count++;
//        }
//    }
//    int word_start, word_count = 0;
//    for (int i = 0; i < size; i++) {
//        printf("%c", f[i]);
//    }

}


#define NUM_THREADS 2

void *perform_work(void *arguments){
  struct Segment segment = *((struct Segment *)arguments);
  printf("THREAD %d: Started working on bytes [%d..%d]\n", segment.id, segment.start, segment.end);
    unsigned char local_mask[256];
    int word_start = segment.start;
    for (int i = segment.start; i <= segment.end; i++) {
        switch(segment.data[i])
        {
            case '\r':
            case '\n':
                //printf("\n%d:", i+1);
                if (segment.anagram_length == i - word_start)
                {
                    // printf("Found candidate of same length ");
                    int found = 1;
                    for(int j = word_start; j < i; j++)
                    {
                        //printf("%c", segment.data[j]);
                        if(!mask[segment.data[j]]){
                            found = 0;
                            break;
                        }
                    }
                    if (found){
                        for(int j = word_start; j < i; j++){
                            printf("%c", segment.data[j]);
                        }
                        printf(" Character set MATCH\n");
                        // count characters
                        for(int j = 0; j< used_chars; j++){
                            local_mask[mask_index[j]] = 0;
                        }
                        for(int j = word_start; j < i; j++)
                        {
                            local_mask[segment.data[j]]++;
                        }
                        for(int j = 0; j< used_chars; j++){
                            if (local_mask[mask_index[j]] != mask[mask_index[j]]){
                                found = 0;
                                break;
                            }
                        }
                    }
                    if (found){
                        printf(" ANAGRAM MATCH\n");
                    }
                    else{
                        //printf("... No match\n");
                    }
                    //printf("\n");
                }
                word_start = i + 1;
                break;
            default:
                break;
                //printf("%c", segment.data[i]);
                //printf("%d ", i);
        }
    }
  //printf("THREAD %d: Ended.\n", segment.id);
}

void doit(char * anagram)
{
  printf("Searching for %s\n", anagram);
  for (int i = 0; anagram[i] != 0; i++){
    mask[anagram[i]]++;
  }
  // build index for mask
  used_chars = 0;
  for (int i = 0; i<256; i++){
//    printf("%d", mask[i]);
    if (mask[i]){
        used_chars++;
        mask_index[used_chars-1] = i;
    }
  }

  pthread_t threads[NUM_THREADS];
  int segment_size = size/NUM_THREADS;
  int next_segment_start = 0;
  int segment_end = 0;
  struct Segment thread_segments[NUM_THREADS];

  //create all threads one by one
  for (int i = 0; i < NUM_THREADS; i++) {
    if (i == NUM_THREADS - 1)
    {
        segment_end = size-1;
    } else
    {
        segment_end = next_segment_start + segment_size;
        // align the segments with line breaks
        while(segment_end < size && file_content[segment_end] != '\n')
        {
            segment_end++;
        }
    }
    thread_segments[i].id=i;
    thread_segments[i].start = next_segment_start;
    thread_segments[i].end = segment_end;
    thread_segments[i].data = file_content;
    thread_segments[i].anagram_length = strlen(anagram);
//    printf("IN MAIN: Creating thread %d (bytes %d .. %d)\n", i, next_segment_start, segment_end);
    assert(!pthread_create(&threads[i], NULL, perform_work, &thread_segments[i]));
    next_segment_start = segment_end + 1;
  }

  //printf("IN MAIN: All threads are created.\n");

  //wait for each thread to complete
  for (int i = 0; i < NUM_THREADS; i++) {
    assert(!pthread_join(threads[i], NULL));
//    printf("IN MAIN: Thread %d has ended.\n", i);
  }

//  printf("MAIN program has ended.\n");
}
#define MAXCHAR 1000


int main(int argc, char **argv)
{
    struct timeval tval_before, tval_after, tval_result;
    gettimeofday(&tval_before, NULL);

//    FILE *fp;
//    char str[MAXCHAR];
//    char* filename = "lemmad.txt";
//
//    fp = fopen(filename, "r");
////    if (fp == NULL){
////        printf("Could not open file %s",filename);
////        return 1;
////    }
//    while (fgets(str, MAXCHAR, fp) != NULL) {}
//        //printf("%s", str);
//    fclose(fp);

//    FILE *fp;
//    char *line = NULL;
//    size_t len = 0;
//    ssize_t read;
//
//   fp = fopen("lemmad.txt", "r");
//    if (fp == NULL)
//        exit(EXIT_FAILURE);
//
//   while ((read = getline(&line, &len, fp)) != -1) {
//        // printf("Retrieved line of length %zu :\n", read);
//        //printf("%s", line);
//   }
//   free(line);
//   fclose(fp);

//    pthread_t thread_id;
//    printf("Before Thread\n");
//    pthread_create(&thread_id, NULL, myThreadFun, NULL);
//    pthread_join(thread_id, NULL);
//    printf("After Thread\n");
//    doit();
    // processFile_getline();      // 0.022
    // processFile_fgets();        // 0.024
     processFile_memmap(argv[1]);       // 0.0057
    // processFile_singlebuffer(); // 0.0067
    doit(argv[2]);

   gettimeofday(&tval_after, NULL);
   timersub(&tval_after, &tval_before, &tval_result);
   printf("Time elapsed: %ld.%06ld\n", (long int)tval_result.tv_sec, (long int)tval_result.tv_usec);

   exit(EXIT_SUCCESS);
}