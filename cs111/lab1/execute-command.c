// UCLA CS 111 Lab 1 command execution

#include "command.h"
#include "command-internals.h"

#include <error.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <string.h>

int
command_status (command_t c)
{
  return c->status;
}

void executingSimple(command_t c);
void executingSubshell(command_t c);
void executingAnd(command_t c);
void executingOr(command_t c);
void executingSequence(command_t c);
void executingPipe(command_t c);

void execute_switch(command_t c)
{
  switch(c->type)
  {
    case SIMPLE_COMMAND:
      executingSimple(c);
      break;
    case SUBSHELL_COMMAND:
      executingSubshell(c);
      break;
     case AND_COMMAND:
      executingAnd(c);
      break;
    case OR_COMMAND:
      executingOr(c);
      break;
    case SEQUENCE_COMMAND:
      executingSequence(c);
      break;
    case PIPE_COMMAND:
      executingPipe(c);
      break;
    default:
      error(1, 0, "Not a valid command!");
  }
}

void executingSimple(command_t c)
{
  pid_t cPid;
  int eStatus, inputRedir, outputRedir;
  cPid = fork();
  switch (cPid)
  {
    case -1:
      error (1, errno, "Fork was unsuccessful!");
      break;
    case 0:
      if (c->input != NULL)
      {
        inputRedir = open(c->input, O_RDONLY);
        if (inputRedir < 0)
          _exit(1);
        if (dup2(inputRedir, 0) < 0)
          error(1, errno, "Error with dup2!");
      }
      if (c->output != NULL)
      {
        outputRedir = open(c->output, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (outputRedir < 0)
          error(1, errno, "Opening output file failed!");
        if (dup2(outputRedir, 1) < 0)
          error(1, errno, "Error with dup2!");
      }
      if (c->input != NULL)
        close(inputRedir);
      if (c->output != NULL)
        close(outputRedir);
      if (strncmp (c->u.word[0],"exec",4) == 0)
        execvp(c->u.word[1], c->u.word);
      else
        execvp(c->u.word[0], c->u.word);
      _exit(1); // If we reach here without returning after execvp, something went wrong.
      break;
    default:
      waitpid(cPid, &eStatus, 0); 
      c->status = WEXITSTATUS(eStatus);
      break;
  }
}

void executingSubshell(command_t c)
{
  pid_t firstPid;
  int eStatus;

  int inputRedir, outputRedir;

  firstPid = fork();
  if (firstPid < 0)
  {
    error(1, errno, "Fork was unsuccessful!");
  }
  else if (firstPid == 0)
  {
    if (c->input != NULL)
    {
      inputRedir = open(c->input, O_RDONLY);
      if (inputRedir < 0)
        _exit(1);
      if (dup2(inputRedir, 0) < 0)
        error(1, errno, "Error with dup2!");
    }
    if (c->output != NULL)
    {
      outputRedir = open(c->output, O_WRONLY | O_CREAT | O_TRUNC, 0644);
      if (outputRedir < 0)
        error(1, errno, "Opening output file failed!");
      if (dup2(outputRedir, 1) < 0)
        error(1, errno, "Error with dup2!");
    }
    if (c->input != NULL)
      close(inputRedir);
    if (c->output != NULL)
      close(outputRedir);
    execute_switch(c->u.subshell_command);
    _exit(c->u.subshell_command->status);
  }
  else
  {
    waitpid(firstPid, &eStatus, 0);
    c->status = WEXITSTATUS(eStatus);
    return;
  }
}

void executingAnd(command_t c)
{
  pid_t firstPid;
  pid_t secondPid;
  int eStatus;

  firstPid = fork();
  if (firstPid < 0)
  {
    error(1, errno, "Fork was unsuccessful!");
  }
  else if (firstPid == 0)
  {
    execute_switch(c->u.command[0]);
    _exit(c->u.command[0]->status);
  }
  else
  {
    waitpid(firstPid, &eStatus, 0);

    if (eStatus == 0)
    {
      secondPid = fork();
      
      if (secondPid < 0)
      {
        error(1, errno, "Fork was unsuccessful!");
      }
      else if (secondPid == 0)
      {
        execute_switch(c->u.command[1]);
        _exit(c->u.command[1]->status);
      }
      else
      {
        waitpid(secondPid, &eStatus, 0);
        c->status = WEXITSTATUS(eStatus);
        return;
      }
    }
    else
    {
      c->status = WEXITSTATUS(eStatus);
      return;
    }
  }
}

void executingOr(command_t c)
{
  pid_t firstPid;
  pid_t secondPid;
  int eStatus;

  firstPid = fork();
  if (firstPid < 0)
  {
    error(1, errno, "Fork was unsuccessful!");
  }
  else if (firstPid == 0)
  {
    execute_switch(c->u.command[0]);
    _exit(c->u.command[0]->status);
  }
  else
  {
    waitpid(firstPid, &eStatus, 0);

    if (eStatus != 0)
    {
      secondPid = fork();
      
      if (secondPid < 0)
      {
        error(1, errno, "Fork was unsuccessful!");
      }
      else if (secondPid == 0)
      {
        execute_switch(c->u.command[1]);
        _exit(c->u.command[1]->status);
      }
      else
      {
        waitpid(secondPid, &eStatus, 0);
        c->status = WEXITSTATUS(eStatus);
        return;
      }
    }
    else
    {
      c->status = WEXITSTATUS(eStatus);
      return;
    }
  }
}

void executingSequence(command_t c)
{
  pid_t firstPid;
  pid_t secondPid;
  int eStatus;

  firstPid = fork();
  if (firstPid < 0)
  {
    error(1, errno, "Fork was unsuccessful!");
  }
  else if (firstPid == 0)
  {
    execute_switch(c->u.command[0]);
    _exit(c->u.command[0]->status);
  }
  else
  {
    waitpid(firstPid, &eStatus, 0);

    secondPid = fork();

    if (secondPid < 0)
    {
      error(1, errno, "Fork was unsuccessful!");
    }
    else if (secondPid == 0)
    {
      execute_switch(c->u.command[1]);
      _exit(c->u.command[1]->status);
    }
    else
    {
      waitpid(secondPid, &eStatus, 0);
      c->status = WEXITSTATUS(eStatus);
      return;
    }
  }
}

void executingPipe(command_t c)
{
  pid_t returnedPid;
  pid_t firstPid;
  pid_t secondPid;
  int buffer[2];
  int eStatus;

  if ( pipe(buffer) < 0 )
  {
    error (1, errno, "Pipe was not created!");
  }

  firstPid = fork();
  if (firstPid < 0)
  {
    error(1, errno, "Fork was unsuccessful!");
  }
  else if (firstPid == 0) //child executes command on the right of the pipe
  {
    close(buffer[1]); //close unused write end
           //redirect standard input to the read end of the pipe
           //so that input of the command (on the right of the pipe)
           //comes from the pipe
    if ( dup2(buffer[0], 0) < 0 )
    {
      error(1, errno, "Error with dup2!");
    }
    execute_switch(c->u.command[1]);
    _exit(c->u.command[1]->status);
  }
  else 
  {
  // Parent process
    secondPid = fork(); //fork another child process
                       //have that child process executes command on the left of the pipe
    if (secondPid < 0)
    {
      error(1, 0, "Fork was unsuccessful!");
    }
    else if (secondPid == 0)
    {
      close(buffer[0]); //close unused read end
      if(dup2(buffer[1], 1) < 0) //redirect standard output to write end of the pipe
      {
        error (1, errno, "Error with dup2!");
      }
      execute_switch(c->u.command[0]);
      _exit(c->u.command[0]->status);
    }
    else
    {
      // Finishing processes
      returnedPid = waitpid(-1, &eStatus, 0); //this is equivalent to wait(&eStatus);
                      //we now have 2 children. This waitpid will suspend the execution of
                      //the calling process until one of its children terminates
                      //(the other may not terminate yet)
      //Close pipe
      close(buffer[0]);
      close(buffer[1]);
      if (secondPid == returnedPid )
      {
        //wait for the remaining child process to terminate
        waitpid(firstPid, &eStatus, 0); 
        c->status = WEXITSTATUS(eStatus);
        return;
      }
      if (firstPid == returnedPid)
      {
        //wait for the remaining child process to terminate
        waitpid(secondPid, &eStatus, 0);
        c->status = WEXITSTATUS(eStatus);
        return;
      }
    }
  }
}

void
execute_command (command_t c, bool time_travel)
{
  if (time_travel)
    error (1, 0, "Time travel not yet implemented!");
  else
    execute_switch(c);
}
