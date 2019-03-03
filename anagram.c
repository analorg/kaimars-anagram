#define _GNU_SOURCE
#include <sys/sysinfo.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/time.h>
#include <assert.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>

#pragma region Basic Data structures and memory management routines

#define handle_error(msg) \
    do { perror(msg); exit(EXIT_FAILURE); } while (0)

static void* safe_malloc(size_t n, unsigned long line)
{
    void* p = malloc(n);
    if (!p)
    {
        fprintf(stderr, "[%s:%lu]Out of memory(%lu bytes)\n",
                __FILE__, line, (unsigned long)n);
        exit(EXIT_FAILURE);
    }
    return p;
}
#define SAFEMALLOC(n) safe_malloc(n, __LINE__);


struct Node // Linked list element
{
    // Any data type can be stored in this node
    void  *data;
    struct Node *next;
};

// Adds new node to the top of linked list referenced by head_ref.
void push(struct Node** head_ref, void *data)
{
    struct Node* new_node = (struct Node*) SAFEMALLOC(sizeof(struct Node));
    new_node->data = data;
    new_node->next = (*head_ref);
    // Change head pointer as new node is added at the beginning
    (*head_ref) = new_node;
}

// Removes topmost element of linked list referenced by head_ref.
// Returns data value of removed element.
void *pop(struct Node** head_ref)
{
    struct Node *current_node = (*head_ref);
    assert(current_node != NULL);
    void *result = current_node->data;
    (*head_ref) = current_node->next;
    free(current_node);
    return result;
}

// Finds last element of linked list referenced by node
struct Node* tail_of(struct Node *node)
{
    while (node->next != NULL)
    {
        node = node->next;
    }
    return node;
}
# pragma endregion Basic Data structures and memory management routines

struct SearchTask {
    int id;  // job id

    // data about search area
    unsigned char *data; // text to be searched from
    int start; // start position of search scope
    int end; // start position of search scope

    // data about the anagram searched
    int anagram_length;
    // Here we store how many times each character is used.
    // For searched word "foo" char_counts is filled with zeros except
    // char_counts['f'] == 1 and char_counts['o'] == 2
    unsigned char* char_counts;
    int diff_char_count; // how many different characters anagram contains
    // Here we enumerate which characters are used in anagram, so we don't have to
    // scan whole character map but only the ones used.
    // For searched word "foo", char_counts_guide[0] refers to 'f', char_counts_guide[1] refers to 'o'.
    unsigned char* char_counts_guide;

    // buffer to store search results
    struct Node *result;
};

void *search_anagrams(void *arguments) {
    // Searching for anagrams effectively is done so that the more expensive comparisions are
    // skipped if no hope for match. Fail as fast as possible.
    // So, first we compare the phrase length against target, next we check whether the
    // characters used match and finally count the characters.
    struct SearchTask *task = arguments;
    // printf("THREAD %d: Started working on bytes [%d..%d]\n", task->id, task->start, task->end);
    // printf("THREAD %d: diff_char_count:%d, char_count: ", task->id, task->diff_char_count);
    // for (int i=0; i<256; i++)  printf(" %d", task->char_counts[i]);
    unsigned char match_char_counts[256];
    int line_start_idx = task->start;
    for (int i = task->start; i <= task->end; i++) {
        // printf(" %c[%d]", task->data[i], task->data[i]);
        switch(task->data[i])
        {
        case '\r':
        case '\n':
            // look only lines of same length
            if (task->anagram_length == i - line_start_idx)
            {
                // printf("Found candidate of same length at position %d.", line_start_idx);
                int found = 1;
                for(int j = line_start_idx; j < i; j++)
                {
                    // on first non-matching character go to next line
                    if(!task->char_counts[task->data[j]]) {
                        found = 0;
                        break;
                    }
                }
                if (found) {
                    // here we check that match candidate contains the same amount of all characters
                    for(int j = 0; j < task->diff_char_count; j++) {
                        match_char_counts[task->char_counts_guide[j]] = 0;
                    }
                    for(int j = line_start_idx; j < i; j++)
                    {
                        match_char_counts[task->data[j]]++;
                    }
                    for(int j = 0; j < task->diff_char_count; j++) {
                        if (match_char_counts[task->char_counts_guide[j]] !=
                                task->char_counts[task->char_counts_guide[j]]) {
                            found = 0;
                            break;
                        }
                    }
                }
                if (found) {
                    // extract the match to result list
                    char *match = (char *)SAFEMALLOC(task->anagram_length+1);
                    strncpy(match, &task->data[line_start_idx], task->anagram_length);
                    match[task->anagram_length] = 0;
                    // printf(" Thread %d found ANAGRAM MATCH at position %d: %s\n",
                    //     task->id, line_start_idx, match);
                    push(&task->result, match);
                }
            }
            line_start_idx = i + 1;
            break;
        }
    }
}

// Searches anagrams from file_content and returns all matches as linked list.
struct Node *search_anagrams_parallel(unsigned char *anagram, int thread_count, unsigned char *file_content, int file_size)
{
    struct Node *result = NULL;

    // build up the search heuristics all the tasks are using
    unsigned char char_counts[256] = {0};
    unsigned char char_counts_guide[256];
    int diff_char_count;
    for (int i = 0; anagram[i] != 0; i++) {
        char_counts[anagram[i]]++;
    }
    // build index for char_counts
    diff_char_count = 0;
    for (int i = 0; i<256; i++) {
        if (char_counts[i]) {
            diff_char_count++;
            char_counts_guide[diff_char_count-1] = i;
        }
    }

    int segment_size = file_size/thread_count;
    int next_segment_start = 0;
    int segment_end = 0;
    struct SearchTask tasks[thread_count];
    pthread_t threads[thread_count];

    for (int i = 0; i < thread_count; i++) {
        segment_end = next_segment_start + segment_size;
        // we split the search area into equally sized segments and adjust the segments so
        // that they do not split the lines
        while(file_content[segment_end] != '\n' && segment_end < file_size) {
            segment_end++;
        }
        if (segment_end >= file_size) {
            segment_end = file_size-1;
        }
        tasks[i].id=i;
        tasks[i].start = next_segment_start;
        tasks[i].end = segment_end;
        tasks[i].data = file_content;
        tasks[i].anagram_length = strlen(anagram);
        tasks[i].result = NULL;
        tasks[i].char_counts = &char_counts[0];
        tasks[i].char_counts_guide = &char_counts_guide[0];
        tasks[i].diff_char_count = diff_char_count;
        assert(!pthread_create(&threads[i], NULL, search_anagrams, &tasks[i]));
        next_segment_start = segment_end + 1;
        if (next_segment_start >= file_size) {
            next_segment_start = file_size-1;
        }
    }

    // wait for each thread to complete and merge all results
    for (int i = 0; i < thread_count; i++) {
        assert(!pthread_join(threads[i], NULL));
        if (tasks[i].result != NULL) {
            tail_of(tasks[i].result)->next = result;
            result = tasks[i].result;
        }
    }
    return result;
}


int main(int argc, char **argv)
{
    // start the stopwatch
    struct timeval tval_before;
    gettimeofday(&tval_before, NULL);

    if (argc != 3) {
        printf("Usage: %s <path to dictionary file> <search string>\n", argv[0]);
        printf("It is assumed that both inputs use same single-byte character encoding.\n");
        exit(EXIT_FAILURE);
    }
    char* file_name = argv[1];

    // read in the file as memory map
    struct stat s;
    int fd = open(file_name, O_RDONLY);
    if (fd == -1)
        handle_error("open");
    if (fstat(fd, &s) == -1)  // read file size
        handle_error("fstat");
    char *file_content = mmap(0, s.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (file_content == MAP_FAILED)
        handle_error("mmap");

    // search the file in parallel using all available CPU cores
    int cpu_count = get_nprocs();
    struct Node* result = search_anagrams_parallel(argv[2], cpu_count, file_content, s.st_size);
    if (munmap(file_content, s.st_size) ==-1)
        handle_error("munmap");

    // stop the stopwatch
    struct timeval tval_after, tval_result;
    gettimeofday(&tval_after, NULL);
    timersub(&tval_after, &tval_before, &tval_result);

    // print out the result
    printf("%ld", 1000000 * (long int)tval_result.tv_sec + (long int)tval_result.tv_usec);
    while (result != NULL)
    {
        char *match = pop(&result);
        printf(",%s", match);
        free(match);
    }
    exit(EXIT_SUCCESS);
}