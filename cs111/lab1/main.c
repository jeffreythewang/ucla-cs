// UCLA CS 111 Lab 1 main program

#include <errno.h>
#include <error.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#include "alloc.h"
#include "command.h"
#include "command-internals.h"

static char const *program_name;
static char const *script_name;

// Linked list implementation
typedef struct Node Node;
struct Node {
  char *word;
  Node *next;
};

typedef struct linkedList {
  Node *head;
  Node *tail;
  int size;
} linkedList;

static void insert(linkedList *self, char *item) {
  Node *newNode = checked_malloc(sizeof(Node));
  newNode->word = item;
  newNode->next = NULL;
  if (self->head == NULL)
  {
    self->head = newNode;
    self->tail = newNode;
  }
  else
  {
    self->tail->next = newNode;
    self->tail = newNode;
  }
  (self->size)++;
}

static void removeNode(linkedList *self) {
  if (self->head == NULL)
    return;
  else if (self->head->next != NULL)
  {
    Node *temp = self->head;
    self->head = self->head->next;
    free(temp);
  }
  else
  {
    Node *temp = self->head;
    self->head = NULL;
    self->tail = NULL;
    free(temp);
  }
  (self->size)--;
}

static void initList(linkedList *s) {
  s->head = NULL;
  s->tail = NULL;
  s->size = 0;
}

static void destroyList(linkedList *s){
  while (s->head != NULL)
    removeNode(s);
}

#define ND_LEN 15
#define D_LEN 15

typedef struct graphNode graphNode;
struct graphNode {
  command_t command;
  graphNode **before;
  pid_t pid;
};

typedef struct listNode {
  graphNode *gNode;
  linkedList *readList;
  linkedList *writeList;
} listNode;

typedef struct dependencyGraph {
  listNode *noDepend[ND_LEN];
  int ndSize;
  listNode *depend[D_LEN];
  int dSize;
} dependencyGraph;

static void processCommand(command_t cmd, listNode *node)
{
  if (cmd->type == SIMPLE_COMMAND)
  {
    int i = 1;
    while (cmd->u.word[i] != NULL)
    {
      if (strncmp(cmd->u.word[i], "-", 1) != 0)
        insert(node->readList, cmd->u.word[i]);
      i++;
    }
    if (cmd->input != NULL)
      insert(node->readList, cmd->input);
    if (cmd->output != NULL)
      insert(node->writeList, cmd->output);
  }
  else if (cmd->type == SUBSHELL_COMMAND)
  {
    if (cmd->input != NULL)
      insert(node->readList, cmd->input);
    if (cmd->output != NULL)
      insert(node->writeList, cmd->output);
    processCommand(cmd->u.subshell_command, node);
  }
  else
  {
    processCommand(cmd->u.command[0], node);
    processCommand(cmd->u.command[1], node);
  }
}

static void buildDepend(listNode *node, listNode **list, int *size, int *len, int*maxLen)
{
  int i = 0;
  bool isBefore;
  while (i < *size)
  {
    isBefore = false;
    Node *n = list[i]->readList->head;
    while (!isBefore && n != NULL)
    {
      if (node->writeList->head != NULL && strcmp(node->writeList->head->word, n->word) == 0)
      { //WAR case
        if (*len >= *maxLen - 1)
        {
          *maxLen = *maxLen * 2;
          node->gNode->before = checked_realloc(node->gNode->before, sizeof(graphNode*)*(*maxLen));
        }
        node->gNode->before[(*len)] = list[i]->gNode;
        *len = *len + 1;
        isBefore = true;
      }
      n = n->next;
    }
    if (list[i]->writeList->head != NULL)
    {
      Node *cur = node->readList->head;
      while (!isBefore && cur != NULL)
      {
        if (strcmp(cur->word, list[i]->writeList->head->word) == 0)
        { //RAW case
          if (*len >= *maxLen - 1)
          {
            *maxLen = *maxLen * 2;
            node->gNode->before = checked_realloc(node->gNode->before, sizeof(graphNode*)*(*maxLen));
          }
          node->gNode->before[(*len)] = list[i]->gNode;
          *len = *len + 1;
          isBefore = true;
        }
        cur = cur->next;
      }
      if (!isBefore && node->writeList->head != NULL && strcmp(node->writeList->head->word, list[i]->writeList->head->word) == 0)
      { //WAW case
        if (*len >= *maxLen - 1)
        {
          *maxLen = *maxLen * 2;
          node->gNode->before = checked_realloc(node->gNode->before, sizeof(graphNode*)*(*maxLen));
        }
        node->gNode->before[(*len)] = list[i]->gNode;
        *len = *len + 1;
        isBefore = true;
      }
    }
    i++;
  }
}

static dependencyGraph createGraph(command_stream_t stream)
{
  dependencyGraph graph;
  int i, len;
  for (i = 0; i < ND_LEN; i++)
    graph.noDepend[i] = NULL;
  for (i = 0; i < D_LEN; i++)
    graph.depend[i] = NULL;
  graph.ndSize = 0;
  graph.dSize = 0;
  command_t command;
  while ((command = read_command_stream(stream)))
  {
    listNode *newNode = checked_malloc(sizeof(listNode));
    newNode->gNode = (graphNode*)checked_malloc(sizeof(graphNode));
    newNode->gNode->command = command;
    newNode->gNode->before = NULL;
    newNode->gNode->pid = -1;
    newNode->readList = checked_malloc(sizeof(linkedList));
    newNode->writeList = checked_malloc(sizeof(linkedList));
    initList(newNode->readList);
    initList(newNode->writeList);
    processCommand(command, newNode);
    len = 0;
    int maxLen = 4;
    newNode->gNode->before = checked_malloc(sizeof(graphNode*)*maxLen);
    buildDepend(newNode, graph.noDepend, &graph.ndSize, &len, &maxLen);
    buildDepend(newNode, graph.depend, &graph.dSize, &len, &maxLen);
    newNode->gNode->before[len] = NULL; // Set last word of before to NULL to mark end
    if (len == 0)
    {
      graph.noDepend[graph.ndSize] = newNode;
      graph.ndSize++;
    }
    else
    {
      graph.depend[graph.dSize] = newNode;
      graph.dSize++;
    }
  }
  return graph;
}

static void executeNoDepend(listNode **noDepend, int ndSize)
{
  //Yet to be tested.
  int i;
  for (i = 0; i < ndSize; i++)
  {
    pid_t pid = fork();
    if (pid < 0)
    {
      error(1, errno, "Fork was unsuccessful!");
    }
    if (pid == 0)
    {
      execute_command(noDepend[i]->gNode->command, false);
      _exit(noDepend[i]->gNode->command->status);
    }
    if (pid > 0)
    {
      noDepend[i]->gNode->pid = pid;
    }
  }
}

static void executeDepend(listNode **depend, int dSize)
{
  //Yet to be tested.
  int i;
  for (i = 0; i < dSize; i++)
  {
    loopLabel: ;
    int j;
    for (j = 0; depend[i]->gNode->before[j] != NULL; j++)
    {
      if (depend[i]->gNode->before[j]->pid == -1)
        goto loopLabel;
    }

    int status;
    for (j = 0; depend[i]->gNode->before[j] != NULL; j++)
    {
      waitpid(depend[i]->gNode->before[j]->pid, &status, 0);
    }
    pid_t pid = fork();
    if (pid < 0)
    {
      error(1, errno, "Fork was unsuccessful!");
    }
    if (pid == 0)
    {
      execute_command(depend[i]->gNode->command, false);
      _exit(depend[i]->gNode->command->status);
    }
    if (pid > 0)
    {
      depend[i]->gNode->pid = pid;
    }
  }
}

static void executeGraph(dependencyGraph g)
{
  executeNoDepend(g.noDepend, g.ndSize);
  executeDepend(g.depend, g.dSize);
  int i, status;
  for (i = 0; i < g.ndSize; i++)
    waitpid(g.noDepend[i]->gNode->pid, &status, 0);
  for (i = 0; i < g.dSize; i++)
    waitpid(g.depend[i]->gNode->pid, &status, 0);
}

static void
usage (void)
{
  error (1, 0, "usage: %s [-pt] SCRIPT-FILE", program_name);
}

static int
get_next_byte (void *stream)
{
  return getc (stream);
}

int
main (int argc, char **argv)
{
  int command_number = 1;
  bool print_tree = false;
  bool time_travel = false;
  program_name = argv[0];

  for (;;)
    switch (getopt (argc, argv, "pt"))
      {
      case 'p': print_tree = true; break;
      case 't': time_travel = true; break;
      default: usage (); break;
      case -1: goto options_exhausted;
      }
 options_exhausted:;

  // There must be exactly one file argument.
  if (optind != argc - 1)
    usage ();

  script_name = argv[optind];
  FILE *script_stream = fopen (script_name, "r");
  if (! script_stream)
    error (1, errno, "%s: cannot open", script_name);
  command_stream_t command_stream =
    make_command_stream (get_next_byte, script_stream);

  command_t last_command = NULL;
  command_t command;
  if (time_travel)
  {
    dependencyGraph g = createGraph(command_stream);
    executeGraph(g);
  }
  else
  {
    while ((command = read_command_stream (command_stream)))
    {
      if (print_tree)
    	{
        printf ("# %d\n", command_number++);
	      print_command (command);
    	}
      else
  	  {
    	  last_command = command;
    	  execute_command (command, time_travel);
    	}
    }
  }

  return print_tree || !last_command ? 0 : command_status (last_command);
}
