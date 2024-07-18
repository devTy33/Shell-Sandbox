                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   
//Ty Anderson


#include "vector.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <sys/resource.h>

//this gets called by other functions to remove strings from the vector that gets turned into the exec arg list
void remove_unwanted_strings(Vector *list, char* flag){
    int64_t val;
    for(int i = 0; i < vector_size(list); i++){
        vector_get(list,i,&val);
        if(strcmp(flag, (char*)val) == 0){
            vector_remove(list,i);
            break;
        }
    }
}

void parse_env(char *s){
	//expands env variables that have accompanying paths: $PWD/tmp
    if(strchr(s, '/') != NULL){

        char *substring = strstr(s, "/");
        char env[256];
        strncpy(env, s, (strlen(s) - strlen(substring)));
        env[(strlen(s) - strlen(substring))] = '\0';
        char *unpack = getenv(env);
		if(unpack == NULL) strcpy(s, "");					//If the env variables does't exist output a \n
		else{
			char ans[256];
			strcpy(ans, unpack);
			strcat(ans,substring);

			strcpy(s, ans);
		}

    }
    else{
        char *p = getenv(s);
        //return path;
        if(p != NULL) strcpy(s,p);
		if(p == NULL) strcpy(s, "");
    }

}




void update_jobs(Vector *jobs, Vector *commands){
	
	int64_t job_val;
    int64_t command_val;
	// this is the vector that contains the printed jobs so it has to be up to date before printing 
    for(int i = vector_size(jobs)-1; i >= 0; i--){
        int status;
        vector_get(jobs, i, &job_val);

        pid_t check = waitpid(job_val, &status, WNOHANG);	
        vector_get(commands, i, &command_val);
	
        if(check == job_val){								//if the process is finished running remove its data so it doesn't get printed in jobs
            vector_remove(jobs, i);
            vector_remove(commands, i);
            
        }
    }
}

//contains the 3 internal commands that don't require a child to execute
int parse_internal(char *cmd, Vector *commands, Vector *jobs){
    size_t newline_index = strcspn(cmd, "\n");
    cmd[newline_index] = '\0';
    char n_cmd[4096];
    char envvar[4096];

    if(strncmp(cmd, "cd", strlen("cd")) == 0){

        if(strlen(cmd) > 2){						//meaing that there is more to parse

            strcpy(n_cmd, cmd + strlen("cd "));
            if(strncmp(n_cmd, "$", strlen("$")) == 0){

                strcpy(envvar, n_cmd + strlen("$")); // remove the $ so that it can be expanded
                parse_env(envvar);
                
                if(chdir(envvar) == -1){
                    perror(cmd);
                }
            }
			else if(strncmp(n_cmd, "~", strlen("~")) == 0){
				char tilde_cmd[] = "HOME";    //if the command is just cd, add home path so it functions
				parse_env(tilde_cmd);

				if(chdir(tilde_cmd) == -1){
					perror(tilde_cmd);
				}	
			}
			else{
				if(chdir(n_cmd) == -1){
					perror(n_cmd);
				}
			}
		}

        else{		
			char new_cmd[] = "HOME";	//if the command is just cd, add home path so it functions
			parse_env(new_cmd);
                
			if(chdir(new_cmd) == -1){
				perror(cmd);
			}
				
		}
	

        return 1;
    }

    else if(strncmp(cmd, "jobs", strlen("jobs")) == 0){
        //print out info in the vector
		update_jobs(jobs, commands);						//make sure only active jobs are printed
		printf("%d jobs.\n", vector_size(jobs));
        
		int64_t job_val;
		int64_t command_val; 
		for(int i = 0; i < vector_size(jobs); i++){
			vector_get(jobs, i, &job_val);
			vector_get(commands, i, &command_val);
			printf("%d - %s\n", job_val, (char*)command_val);		
	
		}
        return 1;
    }

	else if(strncmp(cmd, "exit", strlen("exit")) == 0){
		int status;
		int64_t val;
		if(vector_size(jobs) > 0){
			for(int i = 0; i < vector_size(jobs); i++){		//if there are any jobs remaining we have to wait for them to finish before exit
				
				vector_get(jobs, i, &val);
				waitpid(val, &status, 0);
			}
		}
		vector_free(commands);								// avoid mem leaks
		vector_free(jobs);
		return 0;
	}

    return -1;
}


bool check_for_backround(char **stringArray, int v_size, Vector* list){

    for(int i = 0; i < v_size; i++){			//if anywhere in our vector command contains & the process must be run backround
        if(strcmp(stringArray[i], "&") == 0){
			remove_unwanted_strings(list,"&");
            return true;
        }
    }
    return false;
}


int check_for_redirection(char **stringArray, int v_size, Vector* list, Vector* redirect_fds){
	char *filename;
	int fd = -1;
	for(int i = 0; i < v_size; i++){
		if(strstr(stringArray[i], ">>")){
            strcpy(filename, stringArray[i] + 2);
			if(strstr(filename, ">")) return -2;							//check if there is more than exected >> and return -2 so no exec
            fd = open(filename, O_WRONLY | O_CREAT | O_APPEND, 0666);		// must add permisions for when I file is created with OCREAT
            if (fd == -1) {
                perror("open");
            }
			remove_unwanted_strings(list, stringArray[i]);
			dup2(fd, STDOUT_FILENO);
			vector_push(redirect_fds, (int64_t)fd);					//use a vector cause 1 process can have mulitple redirects
            //return fd;

        }	
		else if(strstr(stringArray[i], ">") != NULL){
			strcpy(filename, stringArray[i] + 1);
			fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0666);
			if (fd == -1) {
				perror("open");
			}
			
			remove_unwanted_strings(list,stringArray[i]); //remove the redirect strings from the exec arg list to avoid interference with exec
			dup2(fd, STDOUT_FILENO);
			vector_push(redirect_fds, (int64_t)fd);
			//return fd;
		}
		
		else if(strstr(stringArray[i], "<")){
			strcpy(filename, stringArray[i] + 1);
			if(strstr(filename, "<")) return -2;
            fd = open(filename, O_RDONLY);
            if (fd == -1) {
                perror("open");
            }
			remove_unwanted_strings(list,stringArray[i]);
			dup2(fd, STDIN_FILENO);
			vector_push(redirect_fds, (int64_t)fd);
			//return fd;
		}

	}

	return fd;
}


char** execute(char *cmd, Vector *initial_inp){
     if (strlen(cmd) > 0 && cmd[strlen(cmd) - 1] == '\n') { //fgets annoyingly adds new line
        cmd[strlen(cmd) - 1] = '\0';
    }
    // iterate through what was grabbed with fgets and allocate memory for it 
    char *token = strtok(cmd, " ");
    while(token != NULL){
        vector_push(initial_inp, (int64_t)token);
        token = strtok(NULL, " ");
    }


    char **stringArray = (char **)malloc((vector_size(initial_inp)+1) * sizeof(char *)); //this will act as the array passed to exec

	
    for(int j = 0; j < vector_size(initial_inp); j++){
        int64_t val;
        vector_get(initial_inp, j, &val);
        stringArray[j] = (char*)val;
    }

    stringArray[vector_size(initial_inp)] = NULL;
	return stringArray;
}



void set_r_limit(int v_size, char** command, Vector *list){
	
	//set all of the default resource limits first and if a new limit is specified the while loop will change it 
	setrlimit(RLIMIT_NPROC, &(struct rlimit){256,256});
	setrlimit(RLIMIT_DATA, &(struct rlimit){1<<30,1<<30});
	setrlimit(RLIMIT_STACK, &(struct rlimit){1<<30,1<<30});
	setrlimit(RLIMIT_NOFILE, &(struct rlimit){256,256});
	setrlimit(RLIMIT_FSIZE, &(struct rlimit){1<<30,1<<30});
	setrlimit(RLIMIT_CPU, &(struct rlimit){1<<30,1<<30});
	int op;
	opterr = 0;	//set to zero so it doesn't freak out about uknown flags
	while((op = getopt(v_size, command, "p:d:s:n:f:t:")) != -1){
	
		if(op == 'p'){
			int opt1;
			sscanf(optarg, "%d", &opt1);
			setrlimit(RLIMIT_NPROC, &(struct rlimit){.rlim_cur = opt1,.rlim_max = opt1});
			
			remove_unwanted_strings(list,optarg);	//remove flag strings from the vector that gets turned into the exec array cause they are
			remove_unwanted_strings(list,"-p");		// not needed in the exec execution
		}
		else if(op == 'd'){
			int opt2;
            sscanf(optarg, "%d", &opt2);
            setrlimit(RLIMIT_DATA, &(struct rlimit){.rlim_cur = opt2,.rlim_max = opt2});
			
			remove_unwanted_strings(list,optarg);
            remove_unwanted_strings(list,"-d");
		}
		else if(op == 's'){ 
			int opt3;
            sscanf(optarg, "%d", &opt3);
            setrlimit(RLIMIT_STACK, &(struct rlimit){.rlim_cur = opt3,.rlim_max = opt3});
			remove_unwanted_strings(list,optarg);
            remove_unwanted_strings(list,"-s");
		}
		else if(op == 'n'){
			int opt4;
            sscanf(optarg, "%d", &opt4);
            setrlimit(RLIMIT_NOFILE, &(struct rlimit){.rlim_cur = opt4,.rlim_max = opt4});
			remove_unwanted_strings(list,optarg);
            remove_unwanted_strings(list,"-n");
		}
		else if(op == 'f'){
			int opt5;
            sscanf(optarg, "%d", &opt5);
            setrlimit(RLIMIT_FSIZE, &(struct rlimit){.rlim_cur = opt5,.rlim_max = opt5});
			remove_unwanted_strings(list,optarg);
            remove_unwanted_strings(list,"-f");
		}
		else if(op == 't'){
			int opt6;
            sscanf(optarg, "%d", &opt6);
            setrlimit(RLIMIT_CPU, &(struct rlimit){.rlim_cur = opt6,.rlim_max = opt6});
			remove_unwanted_strings(list,optarg);
            remove_unwanted_strings(list,"-t");
		}
	
	}

	

}
//throughout the parsing process certain strings are removed from the vector so that the final exec array can be built
char **build_exec_array(Vector *list){
	char **execArray = (char **)malloc((vector_size(list)+1) * sizeof(char *));
    
    for(int j = 0; j < vector_size(list); j++){
        int64_t val;
        vector_get(list, j, &val);
        execArray[j] = (char*)val;
    }
    execArray[vector_size(list)] = NULL;
	return execArray;
}

//checks if an environtment var needs to be expanded inside of the exec array
void check_for_env(Vector *list, char **arr){
	char envvar[4096];
	for(int i = 0; i < vector_size(list); i++){
		if(strncmp(arr[i], "$", strlen("$")) == 0){
			strcpy(envvar, arr[i] + strlen("$"));
            parse_env(envvar);
			strcpy(arr[i], envvar);

		}
	}
}


int main(){

    Vector *jobs = vector_new();							//keeps track of pids for jobs and waitpid
    Vector *commands = vector_new();						//keeps track of command inputs for jobs
	Vector *initial_inp = vector_new();						//the command input
	Vector *redirect_fds = vector_new();					//keeps track of the redirect file descriptors 
    while(1){
        
        const char *user = getenv("USER");
        char path[1024];
        getcwd(path, sizeof(path));
        char exec_command[4096];

        const char *homeDir = getenv("HOME");

        if(strncmp(path, homeDir, strlen(homeDir)) == 0) {
            char s[1024];
            char t[1024] = "~";
            strcpy(s, path + strlen(homeDir));
            strcat(t, s);
            strcpy(path, t);
        }
        printf("%s@sandbox:%s> ", user, path);
       //----------------------------------------- Get inp 
        char inp[1024];
        fgets(inp, sizeof(inp), stdin);



		int internal = parse_internal(inp, commands, jobs);
        if(internal == 0 || internal == 1){
			if(internal == 0){								// 0 means the exit command was given and things have to be freed
				vector_free(initial_inp);
				vector_free(redirect_fds);  //clean up mem
				return 0;
			}
        }

        else{
            char **stringArray = execute(inp, initial_inp);

			int v_size = vector_size(initial_inp);
			bool should_i_backround = check_for_backround(stringArray, v_size, initial_inp);
			
			pid_t pid = fork();
			
			if(pid == -1){
				perror("fork failed");
			}

			if(pid == 0){  
				int fd = check_for_redirection(stringArray, v_size, initial_inp, redirect_fds);
				set_r_limit(v_size,stringArray,initial_inp);
				char **exec_list = build_exec_array(initial_inp);
                check_for_env(initial_inp, exec_list);

				if(fd != -2){										//fd is -2 on invalid redirects
					if(execvp(exec_list[0], exec_list) == -1){
						perror(exec_list[0]);
					}
					
				}
				
				//Go through and close the file descriptors of the redirects
				int64_t vals;
				for(int i = 0; i < vector_size(redirect_fds); i++){
					vector_get(redirect_fds, i, &vals);
					if(fd != -1){
						close(vals);
					 }
				}

				free(exec_list);	//free so it can be used for next input
				
				exit(0);

			}else {
				int status;
				
				char *copy_inp = strdup(inp);
				vector_push(jobs, (int64_t)pid);
				vector_push(commands, (int64_t)copy_inp);
			
				if(!should_i_backround){	//if we don't backround then wait for the process to finish
					waitpid(pid, &status, 0);
					//remove command and pid when process does finish so that it doesn't show up in jobs
					int index = vector_bsearch(jobs, (int64_t)pid);
					vector_remove(jobs, index);

					index = vector_bsearch(commands, (int64_t)copy_inp);
					vector_remove(commands, index);
				}
				
			}
			free(stringArray);
        }

		// continuously check on backround processes
		vector_clear(initial_inp);
		update_jobs(jobs, commands);

    }

}
   
