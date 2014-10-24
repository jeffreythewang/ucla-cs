// UCLA CS 111 Lab 1 command reading

#include "command.h"
#include "command-internals.h"
#include "alloc.h"

#include <error.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

/* Begin Linked List implementation */
typedef struct commandNode commandNode;

struct commandNode {
  command_t command;
  commandNode *next;
};

struct command_stream {
  commandNode *head;
  commandNode *tail;
  commandNode *iterator;
  int size;
  void (*insert)();
  void (*removeNode)();
};

void insert(command_stream_t self, commandNode *item) {
  if (self->head == NULL)
  {
    self->head = item;
    self->iterator = item;
    self->tail = item;
  }
  else
  {
    self->tail->next = item;
    self->tail = item;
  }
  (self->size)++;
}

void removeNode(command_stream_t self) {
  if (self->head == NULL)
  {
    return;
  }
  else if (self->head->next != NULL)
  {
    commandNode *temp = self->head;
    self->head = self->head->next;
    self->iterator = self->head;
    free(temp);
  }
  else
  {
    commandNode *temp = self->head;
    self->head = NULL;
    self->tail = NULL;
    free(temp);
  }
  (self->size)--;
}

void initList(command_stream_t s) {
  s->head = NULL;
  s->tail = NULL;
  s->iterator = NULL;
  s->size = 0;
  s->insert = insert;
  s->removeNode = removeNode;
}

void destroyList(command_stream_t s){
  while (s->head != NULL)
  {
    s->removeNode(s);
  }
}

/* Begin Stack implementation */
typedef struct Stack{
  command_t *arr;
  int size;
  int maxSize;
  void (* push)();
  command_t (* pop)();
  command_t (* peek)();
  bool (* isEmpty)();
} Stack;

void push(Stack *self, command_t item){
  if (self->size == self->maxSize)
  {
    self->maxSize += 10;
    command_t *newArr = checked_realloc(self->arr, sizeof(command_t)*self->maxSize);
    self->arr = newArr;
  }
  self->arr[self->size] = item;
  self->size += 1;
}

command_t pop(Stack *self){
 command_t result = self->peek(self);
 if (result != NULL)
    self->size -= 1;
  return result;
}

command_t peek(Stack *self){
  if (self->isEmpty(self))
   return NULL;
  else
   return self->arr[self->size-1];
}

bool isEmpty(Stack *self){
  return (self->size == 0);
}

Stack* initStack(int startSize){
  Stack *s = (Stack*)checked_malloc(sizeof(Stack));
  s->arr=(command_t*)checked_malloc(sizeof(command_t)*startSize);
  int i;
  for (i = 0; i < startSize; i++)
    s->arr[i] = NULL;
  s->size = 0;
  s->maxSize = startSize;
  s->push = push;
  s->pop = pop;
  s->peek = peek;
  s->isEmpty = isEmpty;
  return s;
}

void destroyStack(Stack *self){
  int i;
  for (i = 0; i < self->maxSize; i++)
    free(self->arr[i]);
  free(self->arr);
}

static int lineNum = 1;

void giveError()
{
  fprintf(stderr, "%d: Syntax error.\n", lineNum);
  exit(EXIT_FAILURE);
}

/* Precedence function */
int precedence(command_t command) {
    if (command->type == PIPE_COMMAND)
      return 2;
    else if (command->type == AND_COMMAND || command->type == OR_COMMAND)
      return 1;
    else if (command->type == SEQUENCE_COMMAND)
      return 0;
    else
      return -1;
}

command_t finishCommand(command_t cmd, int numWords, int wordPos, Stack* commandStack)
{
  cmd->u.word[numWords][wordPos] = '\0'; //End prev word with zerobyte
  numWords++;
  cmd->u.word[numWords] = NULL;
  commandStack->push(commandStack, cmd);
  cmd = NULL;
  return cmd;
}

// Assigns two simple commands to the children of an operator struct
void combineCommand(command_t simpleLeft, command_t simpleRight, command_t operator)
{
  if (simpleLeft == NULL || simpleRight == NULL || operator == NULL)
    giveError();
  operator->u.command[0] = simpleLeft;
  operator->u.command[1] = simpleRight;
}

// This function assumes the commandStack and operatorStack are squentially complete
// then builds a tree from it
//
// commandStack and operatorStack should be in non-increasing order at the end of
// the reading. That's when you run this command
command_t buildTree (Stack *commandStack, Stack *operatorStack)
{
  // Iterative implementation
  while (operatorStack->size != 0)
  {
    command_t topOperator = operatorStack->pop(operatorStack);
    command_t rightChild = commandStack->pop(commandStack);
    command_t leftChild = commandStack->pop(commandStack);
    combineCommand(leftChild, rightChild, topOperator);
    commandStack->push(commandStack, topOperator);
  }
  return commandStack->pop(commandStack);
}

void handleOperator (char *opp, Stack* operatorStack, Stack* commandStack)
{
  // if hits close parentheses ")", build a subshell command that points to
  // the top of a subtree and put it on commandStack
  if (strncmp(opp, ")", 1) == 0)
  {
    command_t topOp = operatorStack->peek(operatorStack);
    while (!operatorStack->isEmpty(operatorStack) && topOp->type != SUBSHELL_COMMAND)
    {
      topOp = operatorStack->pop(operatorStack);
      command_t rightCh = commandStack->pop(commandStack);
      command_t leftCh = commandStack->pop(commandStack);
      combineCommand(leftCh, rightCh, topOp);
      commandStack->push(commandStack, topOp);

      topOp = operatorStack->peek(operatorStack);
      if (topOp != NULL)
      {
        if (topOp->type == SUBSHELL_COMMAND)
          break;
      }
      else
        giveError();
    }
    command_t subShell = commandStack->pop(commandStack);
    command_t closeParen = pop(operatorStack);
    closeParen->u.subshell_command = subShell;
    commandStack->push(commandStack, closeParen);
    return;
  }

  //From pseudocode given in discussion/OH. 
  command_t newOperator = (command_t)checked_malloc(sizeof(struct command));
  if (strncmp(opp, "&&", 2) == 0)
    newOperator->type = AND_COMMAND;
  if (strncmp(opp, ";", 1) == 0)
    newOperator->type = SEQUENCE_COMMAND;
  if (strncmp(opp, "|", 1) == 0)
    newOperator->type = PIPE_COMMAND;
  if (strncmp(opp, "||", 2) == 0)
    newOperator->type = OR_COMMAND;
  if (strncmp(opp, "(", 1) == 0) // use SUBSHELL_COMMAND to mark open parentheses
    newOperator->type = SUBSHELL_COMMAND;

  newOperator->status = -1;
  newOperator->input = NULL;
  newOperator->output = NULL;

  // push open parentheses "(" onto operatorStack
  if (newOperator->type == SUBSHELL_COMMAND)
  {
    operatorStack->push(operatorStack, newOperator);
    return;
  }

  if (operatorStack->isEmpty(operatorStack))
  {
    operatorStack->push(operatorStack, newOperator);
  }
  else
  {
    command_t topOperator = operatorStack->peek(operatorStack);

    if (precedence(newOperator) > precedence(topOperator))
    {
      operatorStack->push(operatorStack, newOperator); 
    }
    else
    {
      while (topOperator->type != SUBSHELL_COMMAND && precedence(newOperator) <= precedence(topOperator))
      {
        topOperator = operatorStack->pop(operatorStack);
        command_t rightChild = commandStack->pop(commandStack);
        command_t leftChild = commandStack->pop(commandStack);
        combineCommand(leftChild, rightChild, topOperator);
        commandStack->push(commandStack, topOperator);
        if (operatorStack->isEmpty(operatorStack))
          break;
        topOperator = operatorStack->peek(operatorStack);
      }
      operatorStack->push(operatorStack, newOperator);
    }
  }
}

command_stream_t
make_command_stream (int (*get_next_byte) (void *),
		     void *get_next_byte_argument)
{
  command_stream_t stream = (command_stream_t)checked_malloc(sizeof(struct command_stream));
  initList(stream);
  lineNum = 1;
  Stack *commandStack = initStack(10);
  Stack *operatorStack = initStack(10);
  int c, wordLength, wordPos, numWords, maxWords;
  command_t commandHolder = NULL;
  bool afterSubSh = false;
  c = get_next_byte(get_next_byte_argument);
  while (c != EOF)
  {
    if (c == '&')
    { //And operator case
      if (afterSubSh)
        afterSubSh = false;
      c = get_next_byte(get_next_byte_argument);
      c = get_next_byte(get_next_byte_argument);
      if (c == EOF || c == '&' || c == '|')
        giveError();
      if (commandHolder == NULL)
      {
        if(commandStack->isEmpty(commandStack))
          giveError();
      }
      else
        commandHolder = finishCommand(commandHolder, numWords, wordPos, commandStack);
      handleOperator("&&", operatorStack, commandStack);
      while (c == '\n')
      {
        c = get_next_byte(get_next_byte_argument);
        lineNum++;
      }
      continue;  
    }
    else if (c == ';')
    { //Sequence command case
      if (afterSubSh)
        afterSubSh = false;
      if (commandHolder == NULL)
      {
        if(commandStack->isEmpty(commandStack))
          giveError();
      }
      else
        commandHolder = finishCommand(commandHolder, numWords, wordPos, commandStack);
      handleOperator(";", operatorStack, commandStack);
    }
    else if (c == '|')
    {
      if (afterSubSh)
        afterSubSh = false;
      c = get_next_byte(get_next_byte_argument);
      if (c == EOF || c == '&')
        giveError();
      if (commandHolder == NULL && commandStack->isEmpty(commandStack))
        giveError();  
      if (c == '|')
      { //Or command case
        c = get_next_byte(get_next_byte_argument);
        if (c == EOF || c == '&' || c == '|')
          giveError();
        if (commandHolder != NULL) 
          commandHolder = finishCommand(commandHolder, numWords, wordPos, commandStack);
        handleOperator("||", operatorStack, commandStack);    
      }
      else
      { //Pipe command case
        if (commandHolder != NULL) 
          commandHolder = finishCommand(commandHolder, numWords, wordPos, commandStack);
        handleOperator("|", operatorStack, commandStack);
      }
      while (c == '\n')
      {
        c = get_next_byte(get_next_byte_argument);
        lineNum++;
      }
      continue;
    }
    else if (c == '(')
    { //Open paren case
      handleOperator("(", operatorStack, commandStack);
    } //Close paren case
    else if (c == ')')
    {
      commandHolder = finishCommand(commandHolder, numWords, wordPos, commandStack);
      handleOperator(")", operatorStack, commandStack);
      afterSubSh = true;
    }
    else if (c == '<')
    { //Redirect input case
      c = get_next_byte(get_next_byte_argument);
      while (isspace(c))
        c = get_next_byte(get_next_byte_argument);
      if (c == EOF || c == '>' || c == '<')
        giveError();
      if (commandHolder == NULL && !afterSubSh)
        giveError();
      int size = 13;
      int cur = 0;
      char *fileName = (char *)checked_malloc(sizeof(char)*size);
      while(c!= ';' && c!= '|' && c!='&' && c!='(' && c!=')' && c!='<' && c!='>' && c!='\n')
      {
        if (cur >= size-1)
        {
          size += 5;
          fileName = (char *)checked_realloc(fileName, sizeof(char)*size);
        }
        if (!isspace(c))
          fileName[cur] = c;
        cur++;
        c = get_next_byte(get_next_byte_argument);
      }
      fileName[cur] = NULL;
      if (commandHolder != NULL)
        commandHolder->input = fileName;
      else
        commandStack->peek(commandStack)->input = fileName; 
      continue;
    }
    else if (c == '>')
    { //Redirect output case
      c = get_next_byte(get_next_byte_argument);
      while (isspace(c))
        c = get_next_byte(get_next_byte_argument);
      if (c == EOF || c == '<' || c == '>')
        giveError();
      if (commandHolder == NULL && !afterSubSh)
        giveError();
      int size = 13;
      int cur = 0;
      char *fileName = (char *)checked_malloc(sizeof(char)*size);
      while(c!= ';' && c!= '|' && c!='&' && c!='(' && c!=')' && c!='<' && c!='>' && c!='\n')
      {
        if (cur >= size-1)
        {
          size += 5;
          fileName = (char *)checked_realloc(fileName, sizeof(char)*size);
        }
        if (!isspace(c))
          fileName[cur] = c;
        cur++;
        c = get_next_byte(get_next_byte_argument);
      }
      fileName[cur] = NULL;
      if (commandHolder != NULL)
        commandHolder->output = fileName;
      else
        commandStack->peek(commandStack)->output = fileName;
      continue;
    }
    else if (c == '\n')
    { //Newline case
      lineNum++;
      if (commandHolder != NULL || afterSubSh)
      {
        if (commandHolder != NULL)
          commandHolder = finishCommand(commandHolder, numWords, wordPos, commandStack);
        c = get_next_byte(get_next_byte_argument);
        if (c == EOF)
          break;
        if (c != '\n')
        { //Single newline after command is treated like semicolon.
          handleOperator(";", operatorStack, commandStack);
          continue;
        }
        else
        { //Double newline after command separates nodes.
          lineNum++;
          command_t overall = buildTree(commandStack, operatorStack);
          commandNode *node = checked_malloc(sizeof(commandNode));
          node->command = overall;
          node->next = NULL;
          stream->insert(stream, node);
          afterSubSh = false;
        }
      }
    }
    else if (isspace(c))
    { //Whitespace case
      c = get_next_byte(get_next_byte_argument);
      if (c=='&' || c==';' || c=='|' || c=='(' || c==')' || c=='<' || c=='>' || isspace(c))
        continue;
      else if (commandHolder != NULL)
      { //Get noncommand char after space, start new word
        if (numWords >= maxWords-1) //Leave from for last blank word insertion
        {
          maxWords = maxWords * 2;
          commandHolder->u.word = (char **)checked_realloc(commandHolder->u.word, sizeof(char *) * maxWords);
          int i;
          for (i = maxWords/2; i < maxWords; i++)
            commandHolder->u.word[i] = (char*)checked_malloc(sizeof(char)*wordLength);
        }
        commandHolder->u.word[numWords][wordPos] = '\0'; //End prev word with zerobyte
        numWords++;
        wordPos = 0;
      }
      continue;
    }
    else if (c == '#')
    { // Comment case: discard text until newline.
      if (afterSubSh)
        afterSubSh = false;
      c = get_next_byte(get_next_byte_argument);
      while (c != '\n')
        c = get_next_byte(get_next_byte_argument);
      if(c == '\n')
        lineNum++;
    }
    else if (isalnum(c) || c=='!' || c=='%' || c=='+' || c==',' || c=='-' || c=='.' || c=='/' || c==':' || c=='@' || c=='^' || c=='_')
    { //Alphanumeric or other valid non-special token case
      if (commandHolder == NULL)
      {
        commandHolder = (command_t)checked_malloc(sizeof(struct command));
        commandHolder->type = SIMPLE_COMMAND;
        commandHolder->status = -1;
        commandHolder->input = NULL;
        commandHolder->output = NULL;
        numWords = 0;
        wordLength = 6;
        wordPos = 0;
        maxWords = 5;
        commandHolder->u.word = (char**)checked_malloc(sizeof(char*)*maxWords);
        int i;
        for (i = 0; i < maxWords; i++)
          commandHolder->u.word[i] = (char*)checked_malloc(sizeof(char)*wordLength);
      }
      if (wordPos >= wordLength-1) //Leave room for zerobyte at end
      {
        wordLength = wordLength * 2;
        commandHolder->u.word[numWords] = (char *)checked_realloc(commandHolder->u.word[numWords], sizeof(char) * wordLength);
      }
      commandHolder->u.word[numWords][wordPos] = (char)c;
      wordPos++;
    }
    else
    { //Is invalid character
      giveError();
    }
    c = get_next_byte(get_next_byte_argument);
  }
  if (commandHolder != NULL)
    commandHolder = finishCommand(commandHolder, numWords, wordPos, commandStack);
  command_t overall = (command_t)checked_malloc(sizeof(struct command));
  overall = buildTree(commandStack, operatorStack);
  commandNode *node = checked_malloc(sizeof(commandNode)); 
  node->command = overall;
  node->next = NULL;
  stream->insert(stream, node);

  return stream;
}

command_t
read_command_stream (command_stream_t s)
{
  if (s->iterator != NULL)
  {
    command_t result = s->iterator->command;
    s->iterator = s->iterator->next;
    return result;
  }
  else
   return NULL;
}
