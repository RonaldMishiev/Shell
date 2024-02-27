#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdbool.h>

//primary function declarations
void batch_mode(char *filename);
void interactive_mode();
int command_helper(char **args, int *argsSize, char **argsP, int *argsSizeP);
char input(char *str);
char **parse_input(char *input, char *delimiter, int *argsSize);
void execute_command(char **args);
void execute_command_piped(char **args, char **argspipe);
void add_to_history(char *cmd);
void print_history();
void variableReplacement(char *str, char *varName, char *varValue);
char* get_history_item(int index);

//////////////////////////////////local data////////////////////////////
typedef struct Node { //node struct used for local data traversal and collection
    char key[128];
    char value[128];
    struct Node *next;
    struct Node *prev;
} Node;
Node *head = NULL; //head node
//function declarations
char* getLocal(char key[], Node* head);
Node* findNode(char key[], Node* head);
int insertLocal(char key[], char value[], Node** head, bool varMode);
void removeVar(char key[], char value[], Node** head);
void printLocalVars();

char* getLocal(char key[], Node* head) {
    Node* node = findNode(key, head);
    if (node) {
        return node->value; //gives value of node if there
    }
    return ""; //blank if not
}

Node* findNode(char key[], Node* head) { 
    Node* current = head;
    while (current != NULL) {//iterates through to see if node exists
        if (strcmp(current->key, key) == 0) {
            return current;
        }
        current = current->next;
    }
    return NULL;
}

int insertLocal(char key[], char value[], Node** head, bool varMode) { //insert function
    Node* existingNode = findNode(key, *head);
    if (!varMode){ //incase needed to be expanded upon for history use, adjustable var mode
        Node* newNode = malloc(sizeof(Node));
        strcpy(newNode->key, key);
        strcpy(newNode->value, value);
        newNode->prev = *head;
        *head = newNode;
        return 1;
    }
    if (existingNode) { //if its there, just change the value
        strcpy(existingNode->value, value);
    } else { //if not, make a new node and put it at the end
        Node* newNode = malloc(sizeof(Node));
        strcpy(newNode->key, key);
        strcpy(newNode->value, value);
        newNode->next = NULL;
        //if no head yet, just set it
        if (*head == NULL) {
            *head = newNode;
        } else { //drop it off at end of list
            Node* current = *head;
            while (current->next != NULL){
                current = current->next;
            }
            current->next = newNode;
        }
    }
    return 1;
}

void removeVar(char key[], char value[], Node** head) { //check if value is needed later

    //store head node 
    Node *temp = *head, *prev; 
  
    //if head node is the one
    if (temp != NULL && strcmp((temp)->key, key) == 0) { 
        *head = temp->next; //changed head 
        free(temp); //free old head 
        return; 
    } 
  
    //search for key we want to delete
     if (temp != NULL && strcmp((temp)->key, key) != 0) { 
        prev = temp; 
        temp = temp->next; 
    } 
  
    //if its not there, just end it
    if (temp == NULL) 
        return; 
  
    //make it so prev and next go over it so it isnt there anymore
    prev->next = temp->next; 
  
    free(temp); //free it
}

void printLocalVars() { //iterates thru and prints it out
    Node* current = head;
    while (current != NULL) {
        printf("%s=%s\n", current->key, current->value);
        current = current->next;
    }
}
/////////////////////////////////////////////////////////////////////////////////
/////////////////most recent from history/////////////////
//function declarations:
void add_to_history(char *cmd);
void print_history();
void changeHistorySize(int newSize);
void clearHistory();

typedef struct HistoryNode { //node for cmd history
    char command[256];
    struct HistoryNode *prev; //backwards cuz of history format
} HistoryNode;

HistoryNode* historyTail = NULL; //tail instead of head concept
int currentHistoryCount = 0;   //amoutn in there
int historySize = 5; //adjustable size limit

void add_to_history(char *cmd)
{
    // make sure its not empty
    if (cmd == NULL || strcmp(cmd, "") == 0){
        return;
    } else { //adds to end and becomes new tail
        HistoryNode* newNode = (HistoryNode*)malloc(sizeof(HistoryNode));
        strncpy(newNode->command, cmd, sizeof(newNode->command) - 1);
        newNode->command[sizeof(newNode->command) - 1] = '\0';
        newNode->prev = historyTail;
        historyTail = newNode;
        currentHistoryCount++;
    }
}

void print_history()
{ //iterates thru list for more recent x cmds
    HistoryNode* current = historyTail;
    int count = 0;
    while (current != NULL && count < historySize) {
        printf("%d) %s\n", count + 1, current->command);
        current = current->prev;
        count++;
    }
}

char* get_history_item(int index)
{ //gets item from the linked list
    HistoryNode* current = historyTail;
    int count = 0; //iterates thru
    while (current != NULL && count < historySize) {
      if(count + 1 == index) return current->command;
    }
    return NULL;
}

void changeHistorySize(int newSize){ //changing size
    if (newSize == 0){ //if its 0, we cna just clear it since nothing's left
        clearHistory();
    } else {
        while (currentHistoryCount > newSize) {
            HistoryNode* current = historyTail;
            for (int i = 1; i < currentHistoryCount - 1; ++i) {
                current = current->prev;
            } //goes backwards to find where the new most recent amount would start^
            //starts going backwards from beyond start to delete
            if (current && current->prev) {
                HistoryNode* toDelete = current->prev;
                free(toDelete);
                current->prev = NULL;
                currentHistoryCount--;
            } else if (currentHistoryCount == 1) { //only one
                free(historyTail);
                historyTail = NULL;
                currentHistoryCount = 0;
            }
        }
    }
    historySize = newSize; //sets new size
}

void clearHistory() { //freeing memory
    HistoryNode* current = historyTail;
    while (current != NULL) {
        HistoryNode* toFree = current;
        current = current->prev;
        free(toFree);
    }
    currentHistoryCount = 0;
    historyTail = NULL;
}

/////////////////////////////////////////////////////////

void batch_mode(char *filename)
{
    FILE *fp = fopen(filename, "r");
    if (!fp)
    {
        perror("couldnt open file");
        return; // TODO right return/error #
    }
    char *line = NULL;
    size_t len = 0;
    ssize_t read;
    char **args;
    int argsSize=0;
    char **argsP;
    int argsSizeP=0;
    while ((read = getline(&line, &len, fp)) > -1)
    {
       // piping logic
            char **strpiped;
            
            strpiped = parse_input(line, "|", &argsSize);

            //check if piped
            if (argsSize==2)
            {
                args = parse_input(strpiped[0], " \t\r\n\a", &argsSize);
                argsP = parse_input(strpiped[1], " \t\r\n\a", &argsSizeP);
            }
            else
            {
                args = parse_input(line, " \t\r\n\a", &argsSize);
            }
        if (args[0] != NULL)
        {
            if (strcmp(args[0], "exit") == 0)
            { // exit
                free(line);
                free(args);
                if(argsSizeP>0) free(argsP);
                exit(0);
            }
            command_helper(args, &argsSize, argsP, &argsSizeP);
        }
        free(args);
        if(argsSizeP>0) free(argsP);
    }

    fclose(fp); //free memory
    if (line)
    {
        free(line);
    }
}

void interactive_mode()
{
    char *line = NULL;
    size_t len = 0;
    ssize_t read;
    char **args;
    int argsSize = 0;
    char **argsP;
    int argsSizeP = 0;

    while (1)
    {
        printf("wsh> ");
        read = getline(&line, &len, stdin);

        if ((read) != -1)
        {
            // piping logic
            char **strpiped;
            
            strpiped = parse_input(line, "|", &argsSize);

            //check if piped
            if (argsSize==2)
            {
                args = parse_input(strpiped[0], " \t\r\n\a", &argsSize);
                 //history command substitution
                if (argsSize == 2 && strcmp(args[0], "history") == 0 && atoi(args[1]) >= 0) {
                    char *histItem = get_history_item(atoi(args[1]));
                    if(histItem != NULL) {
                        args = parse_input(histItem, " \t\r\n\a", &argsSize);
                    }
                }
                argsP = parse_input(strpiped[1], " \t\r\n\a", &argsSizeP);
                //history command substitution
                if (argsSize == 2 && strcmp(args[0], "history") == 0 && atoi(args[1]) >= 0) {
                    char *histItem = get_history_item(atoi(args[1]));
                    if(histItem != NULL) {
                        argsP = parse_input(histItem, " \t\r\n\a", &argsSizeP);
                    }
                }
            }
            else
            {
                
                args = parse_input(line, " \t\r\n\a", &argsSize);
                //history command substitution
                if (argsSize == 2 && strcmp(args[0], "history") == 0 && atoi(args[1]) >= 0) {
                    char *histItem = get_history_item(atoi(args[1]));
                    if(histItem != NULL) {
                        args = parse_input(histItem, " \t\r\n\a", &argsSize);
                    }
                }
            }

            if (args[0] != NULL)
            {
                if (strcmp(args[0], "exit") == 0)
                { // exit
                    free(line);
                    free(args);
                    if(argsSizeP>0) free(argsP);
                    exit(0);
                }

                command_helper(args, &argsSize, argsP, &argsSizeP);
            }
            free(args);
             if(argsSizeP>0) free(argsP);
             argsSize = 0;
             argsSizeP = 0;
        }
        else
        { // https://www.tutorialspoint.com/c_standard_library/c_function_feof.htm
            if (feof(stdin))
            { // eof
                free(line);
                // exit(0);
                break;
            }
            else
            {
                perror("couldnt read line (interactive)");
                exit(-1);
            }
        }
    }
}

// https://cplusplus.com/reference/cstring/strtok/
char **parse_input(char *input, char *delimiter, int *argsSize)
{
    int position = 0;
    char **parts = malloc(64 * sizeof(char *)); // alloc mem
    char *part;

    if (!parts)
    {
        exit(-1); //exits if it mem alloc messed up
    }

    // splitting into parts based on the spaces
    part = strtok(input, delimiter);
    while (part != NULL)
    {
        parts[position] = part;
        position++;

        // make sure not running outta mem
        if (position % 64 == 0)
        {
            parts = realloc(parts, (position + 64) * sizeof(char *)); // expanding
            if (!parts)
            {
                fprintf(stderr, "messed up realloc\n");
                exit(-1);
            }
        }
        part = strtok(NULL, delimiter);
    }
    parts[position] = NULL; // make it null so no memory-related error
    *argsSize = position;

    return parts;
}

int command_helper(char **args, int *argsSize, char **argsP, int *argsSizeP) {
    // execute var replacement logic fro simple input
    for (size_t i = 0; i < *argsSize; i++)
    {
        if (memcmp("$", args[i], 1) == 0)
        {
            char *varName = memmove(args[i], args[i] + 1, strlen(args[i]));
            char *varValue = getenv(varName);
            if (varValue == NULL)
            {
                varValue = getLocal(varName, head);
            }
            variableReplacement(args[i], varName, varValue);
        }
        if(strcmp(args[i],"") == 0){
            //loop to remove empty spot
            for(size_t j = i; j < *argsSize-1; j++){
                args[j] = args[j+1];
            }
            args[*argsSize-1] = '\0';
            *argsSize = *argsSize - 1;
            i = i - 1;
        }
    }
    // execute var replacement logic for piped input
     if (*argsSizeP>1) {     
    for (size_t i = 0; i < *argsSizeP; i++)
    {
        if (memcmp("$", argsP[i], 1) == 0)
        {
            char *varName = memmove(argsP[i], argsP[i] + 1, strlen(argsP[i]));
            char *varValue = getenv(varName);
            if (varValue == NULL)
            {
                varValue = getLocal(varName, head);
            }
            variableReplacement(argsP[i], varName, varValue);
        }
        if(strcmp(argsP[i],"") == 0){
            //loop to remove empty spot
            for(size_t j = i; j < *argsSizeP-1; j++){
                argsP[j] = argsP[j+1];
            }
            argsP[*argsSizeP-1] = '\0';
            *argsSizeP = *argsSizeP - 1;
            i = i - 1;
        }
    }
}

    int argsSizeForVar;
    if (args[0] == NULL)
    {
        return 1; // no command
    }
    if (strcmp(args[0], "exit") == 0)
    {
        exit(0); //they asked to exit
    }
    else if (strcmp(args[0], "cd") == 0)
    {
        if (args[1] == NULL)
        {
            fprintf(stderr, "you need to add a file after \"cd\"\n");
            return 1;
        }
        else
        {
            if (chdir(args[1]) != 0)
            {
                perror("cd");
                exit(-1);
            }
        }
        return 1;
    }
    else if (strcmp(args[0], "export") == 0) //env var
    {
        if (args[1] == NULL)
        {
            fprintf(stderr, "you need to give the export instructions after\n");
            return 1;
        }
        else
        {
            char **export_args = parse_input(args[1], "=", &argsSizeForVar);

            if (export_args[1] == NULL)
            { //https://linux.die.net/man/3/unsetenv
                unsetenv(export_args[0]);
            } else {
                // https://stackoverflow.com/questions/58565517/using-setenv-in-c
                setenv(export_args[0], export_args[1], 1);
            }
        }
    }
    else if (strcmp(args[0], "local") == 0) //local var
    {
        if (args[1] == NULL)
        {
            fprintf(stderr, "you need to give the local var instructions after\n");
        }
        else
        {
            char **export_args = parse_input(args[1], "=", &argsSizeForVar);
            if (export_args[1] == NULL)
            { //if its null, it means they set it to empty which just removes val
                removeVar(export_args[0], export_args[1], &head);
            }
            else
            {   //adding it in
                insertLocal(export_args[0], export_args[1], &head, true);
            }
        }
    }
    else if (strcmp(args[0], "vars") == 0) //vars
    { //prints out current vars
        printLocalVars();
    }
    else if (strcmp(args[0], "history") == 0) //history
    { 
        if ((*argsSize > 1 && strcmp(args[1], "set") == 0) && (*argsSize > 2 && atoi(args[2]) >= 0)) {
            changeHistorySize(atoi(args[2])); //changing size limit to new val
        } else { //printing most recent chosen amt of history
            print_history();
        }
    } 
    else
    {
        // doing another command
        if(*argsSizeP > 0)
            execute_command_piped(args, argsP);
        else
            execute_command(args);

        // recording it in history
        char *addition = malloc(128);
        strcpy(addition, args[0]);
        for (int i = 1; args[i] != NULL; i++)
        {
            strcat(addition, " ");
            strcat(addition, args[i]);
        }
        add_to_history(addition); //adding it to history since not builtin

        return 1;
    }
    return 1;
}

void execute_command(char **args) //executes non builtins
{
    pid_t pid;
    int status = 0;
    pid = fork();
    if (pid == 0)
    { // child
        if (execvp(args[0], args) == -1)
        {
            perror("execvp");
            exit(-1);
        }
        exit(0);
    }
    else if (pid < 0)
    {
        perror("");
        exit(-1);
    }
    else
    {
        waitpid(pid, &status, 0);
        return;
    }
}

void execute_command_piped(char **args, char **argspipe) //for pipe handling
{
    // 0 is read end, 1 is write end
    int pipefd[2];
    pid_t pid1, pid2;
    int status = 0;

    if (pipe(pipefd) < 0)
    {
        perror("\nPipe could not be initialized");
        exit(-1);
    }

    pid1 = fork();
    if (pid1 < 0)
    {
        perror("");
        exit(-1);
    }

    if (pid1 == 0)
    {
        // Child 1 executing..
        // It only needs to write at the write end
        
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[0]);
        close(pipefd[1]);
        waitpid(pid1, &status, 0);
        if (execvp(args[0], args) < 0)
        {
            perror("\nCould not execute command 1..");
            exit(0);
        }
    }
    else
    {
        // Parent executing
        pid2 = fork();

        if (pid2 < 0)
        {
            perror("");
            exit(-1);
        }

        // Child 2 executing..
        // It only needs to read at the read end
        if (pid2 == 0)
        {
            
            dup2(pipefd[0], STDIN_FILENO);
            close(pipefd[1]);
            close(pipefd[0]);
            waitpid(pid1, &status, 0);
            if (execvp(argspipe[0], argspipe) < 0)
            {
                printf("\nCould not execute command 2..");
                exit(0);
            }
        }
        else
        {
            // parent executing, waiting for two children
            close(pipefd[0]);
            close(pipefd[1]);
            waitpid(pid1, &status, 0); //for waiting in situation of sleep() call
            waitpid(pid2, &status, 0);
            return;
        }
    }
}

/////////////////////////////////////var substitution helper funcitons////////////////////////
void variableReplacement(char *str, char *varName, char *varValue)
{
    char *pos;
    int index = 0;
    if ((pos = strstr(str, varName)) != NULL)
    {
        //index of current found word
        index = pos - str;
        //terminate str after word found index
        str[index] = '\0';
        //concatenate str with new word
        strcpy(str, varValue);
    }
}

//////////////////////////////////////main////////////////////////
int main(int argc, char **argv)
{
    if (argc == 1)
    { // interactive mode
        interactive_mode();
    }
    else if (argc == 2)
    { // batch mode
        batch_mode(argv[1]);
    }
    else
    {
        fprintf(stderr, "Usage: %s [scriptfile]\n", argv[0]);
        exit(-1);
    }
    clearHistory();
}
