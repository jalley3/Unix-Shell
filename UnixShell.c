/*
  Jacob Alley
  COP4610
  PROJ2
  1/24/16
  myshell.c
  This file contains the driver class (main) and a number of functions and
  structs that work together to simulate a working tsch shell. This additional
  code will enable the shell to run background processes*/

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

//global variables

int buffMax = 1024;



typedef struct //declare command structure
{
  char * cmdArg[10]; //array to hold user tokens
  char * infile;
  char * outfile;
  int count; //counter
  bool background; //flag to determine background processing
} cmdStrc;

typedef struct //declare directory stack struct
{
  char* stk[10];//stack
  char top[1024]; //top
  int count; //counter
} stck;

typedef struct //declare struct for background processing
{
  int jid; //store processes job number
  pid_t pid; //store process number
  char* command; //command name
  bool terminated; //check for terminated function
  bool killed; //bool to check and see if kill command has been called
  bool done; //bool to determine whether it has been printed as "done"
} bckGrnd;

bckGrnd processArray[100]; //global array
int processSize = 0; //Size counter


//Prototypes

cmdStrc ParseInput(char[]); //parses the input
void CdEmpty(); //returns to home directory if no other arguements
void CD(const char *); //change directories
stck Pushd (const char*, stck); //push on directory stack
stck Popd (stck); //pop stack
void Extern (const char *, cmdStrc); //execute external commands
int Redir (cmdStrc); //redirect helper function
char ** DupArray (cmdStrc); //copy array into dynamic array
void BackProcess (bckGrnd[],cmdStrc, const char *); //background processing func                                                                                        tion
void Jobs(); // fxn to print the processArray

int main()
{

  stck dirStck; //declare directory stack
  dirStck.count = 0; //initialize counter

  //prompt variables
  char* userName;
  char* dirBuff = (char*)malloc(sizeof(char) * (buffMax+1));
  char buffer[101];
  printf ("\n");

  while (1)
  {
    userName = getenv ("USER"); //get path of username
    if(!getcwd(dirBuff, buffMax-1)) //get directory w/ error test
    {perror("ERROR");}
    printf("%s@myshell%s> " , userName,dirBuff); //display prompt
    fgets(buffer,101,stdin); //get user input

    /* Parse input parses the tokens and builds the contents of the command stru                                                                                                                                                                                                                                                                        ctur*/
    cmdStrc com_Struc = ParseInput(buffer);

    //main processing where first user argument is examined
    if (strcmp(com_Struc.cmdArg[0],"cd") == 0) //test for CD
    {
      if(com_Struc.count == 1) //if only CD is entered
      {
        CdEmpty();
      }
      else if (com_Struc.count > 2) //test for correct # of arguments
        printf("Error: Too many arguments\n");
      else
        CD(com_Struc.cmdArg[1]); //call CD
    }
    //check for ehco
    else if (strcmp(com_Struc.cmdArg[0],"echo")==0)
    {
      for(int i = 1; i < (com_Struc.count); i++)
      {
        printf("%s ", com_Struc.cmdArg[i]); //repeat input back to screen
      }
      printf("\n");
    }
    else if (strcmp(com_Struc.cmdArg[0],"pushd")==0) //check for pushd
    {
      if (com_Struc.count == 1) //error test
        printf("Error: Add arguments\n");
      else
        dirStck =  Pushd(com_Struc.cmdArg[1],dirStck);
    }
    else if (strcmp(com_Struc.cmdArg[0],"popd") == 0) //check for popd
    {
      dirStck = Popd(dirStck);
    }
    else if (strcmp(com_Struc.cmdArg[0],"jobs") == 0) // call jobs method
    {
      Jobs();
    }
    else if (strcmp(com_Struc.cmdArg[0],"exit") == 0) //check for exit
    {
      if (processSize != 0)//test for background processes
        printf ("You must kill background processes before exiting\n");
      else
        exit(0);
    }
    else if (strcmp(com_Struc.cmdArg[0],"kill")==0) //test for kill command
    {
      int kill = atoi (com_Struc.cmdArg[1]); //converts string to int
      for (int i = 0; i < processSize; i++) //tests for a matching PID
      {
        if (kill == processArray[i].pid)
          processArray[i].killed = true;
      }
    }
    else //all other input
    {
      char * path; //for path variables
      int test = 0; //counter
      if (strchr(com_Struc.cmdArg[0], '/') == NULL) //check for slash
      {

        path = getenv("PATH"); //store absolute path in path variable

        //tokenize the path variable, separating by colons
        const char * colon = ":";
        char buffer2[101];
        char * token2;
        strcpy (buffer2, path);
        token2 = strtok(buffer2,colon);
        while(token2 != NULL) //examine tokens
        {
          char * check = (char*)malloc(strlen(token2+1));
          strcpy (check, token2); //store token in a variable
          strcat (check,"/");
          strcat (check, com_Struc.cmdArg[0]); //create command to be executed
          struct stat fileStat;
          if (stat(check, &fileStat) < 0) //check to make sure it exists
          {
            token2 = strtok(NULL,colon); //move on to the next path variable
          }
          else
          {
            if (com_Struc.background) //check for background flag
              BackProcess(processArray, com_Struc, check);
            else
              Extern(check, com_Struc); //execute external command
            test = 1; //update counter
            token2 = NULL; //exit while loop
          }
        }
        if (test == 0) //if command was never found print error
        {
          printf ("%s: Invalid command\n", com_Struc.cmdArg[0]);
        }
      }
      else //path is given as argument
      {
        struct stat filestat;
        //check and see if the path given actually exists
        char check[1020];
        strcpy (check, com_Struc.cmdArg[0]);

        //if not then print an error
        if (stat(check, &filestat) < 0)
          printf ("%s: Command not found.\n", check);
        else
        {
          if (com_Struc.background)
            BackProcess(processArray, com_Struc, check);
          else
            Extern(check,com_Struc); //execute command
        }
      }
    }
  }
  free(dirBuff);
}
cmdStrc ParseInput (char buffer[])
{
  //variables for parsing and tokenizing
  const char* whitespace = " \n\r\f\t\v";
  char* token = (char*)malloc(sizeof(buffer+1));
  token = strtok(buffer, whitespace);
  cmdStrc command_Str;
  command_Str.outfile = NULL;
  command_Str.infile = NULL;
  command_Str.count = 0;
  command_Str.background = 0;

  /*parsing loop follows the following logic:
    check for needed redirection, and store the desired files in their
    respective
    variables. Then test for tokens that begin with $ as they reflect an
    environment variable. Trim off the $ and get the path and store the path in
    the command array. Finally store anything else in the command array, until
    it is full*/
  while (token != NULL)
  {
    if ((strcmp(token,">")) == 0)
    {
      token = strtok(NULL, whitespace);
      command_Str.outfile = token;
      token = strtok(NULL, whitespace);
    }
    else if ((strcmp(token,"<")) == 0)
    {
      token = strtok(NULL, whitespace);
      command_Str.infile = token;
      token = strtok(NULL, whitespace);
    }
    else if ((strncmp(token,"$",1))==0)
    {
      memmove(token, token + 1, strlen(token));
      if (command_Str.count == 10)
        printf("Too many arguments\n");
      else
      {
        if (getenv(token) == NULL)
        {
          printf("$%s: Undefined variable \n", token);
          token = NULL;
        }
        else
        {
          token = getenv(token);
          command_Str.cmdArg[command_Str.count] = token;
          ++command_Str.count;
          token = strtok(NULL, whitespace);
        }
      }
    }
    else if (strcmp(token, "&") == 0)
    {
      command_Str.background = 1;
      token = strtok(NULL, whitespace);
    }
    else
    {
      if (command_Str.count == 10)
      {printf("too many arguments\n");}
      else
      {
        command_Str.cmdArg[command_Str.count] = token;
        token = strtok(NULL,whitespace);
        command_Str.count++;
        if (command_Str.count < 10)
          command_Str.cmdArg[command_Str.count + 1] = NULL;
      }
    }
  }
  return command_Str;
}
void CdEmpty () //return to home directory
{
  chdir(getenv("HOME"));
}
void CD (const char* path) //change directory based on path
{
  if(chdir(path) != 0)
  {
    printf("<%s>: No such file or directory\n",path); //error test
  }

}
stck Pushd (const char *directory, stck stack) //push on directory stack
{
  if (stack.count >= 10) //check if full
  {
    printf("Error: Stack is full\n");
  }
  else
  {
    if (chdir(directory) != 0) //make sure directory exists and change direc
      printf("<%s>: No such file or directory\n", directory);
    else
    {
      char my_cwd[buffMax];
      char *temp; //temp variable
      temp = getcwd(my_cwd,buffMax); //temp stores the absolute path
      char * pathCopy = malloc(strlen(temp)+1); //ensure space is allocated
      strcpy(pathCopy, temp); //path to pathCopy
      stack.stk[stack.count] = pathCopy; //push on stack
      strcpy(stack.top, stack.stk[stack.count]); //push on top
      stack.count++; //increment counter

      for (int i = stack.count; i > 0; i--)
      {
        printf("%s\n", stack.stk[i-1]); //display output
      }
    }

  }
  return stack;

}
stck Popd (stck stack)
{
  if (stack.count > 0) //print stack
  {
    for (int i = stack.count; i > 0; i--)
    {
      printf("%s\n", stack.stk[i-1]);
    }
    stack.count--; //pop
    strcpy(stack.top, stack.stk[stack.count]); //assign new top
    CD(stack.top); //cd to top of stack
  }
  else
    printf("Stack is empty\n");
  return stack;
}
void Extern(const char* path, cmdStrc com_Struc)
{
  char ** tempArray = DupArray(com_Struc);
  pid_t pid= fork(); //create pid variable and execute fork
  if (pid < 0) //error
  {
    fprintf(stderr, "Failed to fork!\n");
    return;
  }
  if (pid == 0)
  {
    if(Redir(com_Struc)!= 2) //check for needed redirection
    {
      if(execv(path,tempArray)== -1) //execv to execute in child process
        printf("error with the fork \n"); //error test
    }
  }
  if (pid > 0)
  {
    waitpid(pid, NULL, 0); //wait for child to finish
  }
}

int Redir (cmdStrc com_Struc)
{
  if (com_Struc.infile != NULL) //chek for null infile
  {
    struct stat buffer;
    if (stat(com_Struc.infile, &buffer) < 0)
    {
      printf("%s: No such file or directory\n", com_Struc.infile);
      return 2;
    }
    else
    {
      int x = open(com_Struc.infile, O_RDONLY, S_IRUSR | S_IWUSR);
      if (x != -1)
      {
        dup2(x,0);
        close(x);
      }
    }
  }
  if (com_Struc.outfile != NULL) //check for null outfile
  {
    int x = open(com_Struc.outfile, O_CREAT | O_WRONLY, S_IRUSR | S_IWUSR);
    if (x != -1)
    {
      dup2(x,1);
      close(x);
    }
  }
  return 1;
}

char** DupArray (cmdStrc com_Struc)
{
  for (int i = 0; i < com_Struc.count; i++)//ensure strings are null terminatd
  {
    strcat(com_Struc.cmdArg[i],"\0");
  }
  char ** p_array; //declare dynamic array
  p_array = (char**)malloc(sizeof(char*)*10); //malloc for sizing
  for (int i = 0; i < com_Struc.count; i++) //copy contents
  {
    p_array[i] = com_Struc.cmdArg[i];
  }
  return p_array;
}
void BackProcess (bckGrnd processArray[] , cmdStrc com_Struc, const char * path)                                                                                        
{
  processArray[processSize].terminated = false;
  processArray[processSize].killed = false;
  processArray[processSize].done = false;
  processArray[processSize].command = malloc(sizeof (char) * 100);

  for (int j = 0; j < com_Struc.count; j++) //copy array into string
  {
    strcat(processArray[processSize].command, com_Struc.cmdArg[j]);
    strcat(processArray[processSize].command, " ");
  }
  pid_t id;
  char ** tempArray = DupArray(com_Struc);
  if (processSize > 0)
  {
    processArray[processSize].jid = (processArray[processSize - 1].jid) + 1;
  }
  else
  {
    processArray[processSize].jid = 1;
  }
  processArray[processSize].pid = fork();
  if (processArray[processSize].pid < 0)
  {
    fprintf(stderr,"\nFailed to fork!\n");
  }
  if (processArray[processSize].pid == 0)
  {
    if (execv(path, tempArray) == -1)
    {
      fprintf(stderr, "\nCommand Failed!\n");
      exit(EXIT_FAILURE);
    }
  }
  printf ("[%d] %d\n" , processArray[processSize].jid,
          processArray[processSize].pid);
  processSize++;
  do
  {
    id = waitpid(-1, NULL, WNOHANG);
    if (id > 0)
    {
      for (int i = 0; i < processSize; i++)
      {
        if (processArray[i].pid == id)
          processArray[i].terminated = true;
      }
    }
  } while (id > 0);
}
void Jobs(){
  pid_t id;
  int test = 0;
  do
  {
    id = waitpid(-1, NULL, WNOHANG);
    if (id > 0)
    {
      for (int i = 0; i < processSize; i++)
      {
        if (processArray[i].pid == id)
          processArray[i].terminated = true;
       }
    }
  } while (id > 0);

  for (int i = 0; i < processSize; i++)
  {
    if (processArray[i].killed == true) //check if process was "killed"
    {
      printf("[%d] Terminated %s\n",processArray[i].jid, processArray[i].command                                                                                        );
      test = 1;
      processArray[i].done = true;
    }
    else if (processArray[i].terminated == true)
    {
      printf("[%d] Done %s\n",processArray[i].jid, processArray[i].command);
      test = 1;
      processArray[i].done = true;
    }
    else
      printf("[%d] %d %s\n",processArray[i].jid, processArray[i].pid
             , processArray[i].command);
  }

  //now reorganize array
  if (test == 1) //test to see if it needs to be updated
  {
    if(processSize > 1)
    {
      for (int j = 0; j < processSize; j++)
      {
        if (processArray[j].done == true)
        {
          for (int i = j; i <processSize; i++)
          {
            processArray[i] = processArray[i+1];
          }
          processSize--;
        }
      }
    }
    else
      processSize = 0;
  }
}
