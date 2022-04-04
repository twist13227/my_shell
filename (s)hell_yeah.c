#include <stdio.h>
#include <string.h>
#include <sys/file.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#define COMMAND_MAXLENGTH 50
#define MAX_AMOUNT 30

int pid;

void convert_to_argv (char * command, int *count, char *argv1[]) {
    int cur = 0;
    int end = 0;
    int beginning, ending;
    if (cur == strlen(command)) {
        ++end;
    }
    while (!end) { //not end of input
        while (!end && (command[cur] == ' ')) { //skip all spaces
            ++cur;
            if (cur == strlen(command)) {
                ++end;
            }
        }
        if (end) {
            --*count;
            break;
        }
        beginning = cur;
        while (!end && (command[cur] != ' ')) {
            ++cur;
            if (cur == strlen(command)) {
                ++end;
            }
        }
        ending = cur;
        argv1[*count] = (char *) malloc (sizeof(char) * (ending - beginning + 1));
        for (int i = beginning; i < ending; i++) {
            argv1[*count][i - beginning] = command[i];
        }
        argv1[*count][ending - beginning] = '\0';
        ++*count;
    }
}
int redirection (int index, char *argv[]) {
    if (strcmp(argv[index], "<") == 0) {
        ++index;
        int fd = open(argv[index], O_RDONLY);
        dup2(fd, 0);
        close(fd);
        ++index;
        return 1; //success
    } else if (strcmp(argv[index], ">") == 0) {
        ++index;
        int fd = open(argv[index], O_WRONLY | O_CREAT | O_TRUNC, 0666);
        dup2(fd, 1);
        close(fd);
        ++index;
        return 1; //success
    } else if (strcmp(argv[index], ">>") == 0) {
        ++index;
        int fd = open(argv[index], O_WRONLY | O_CREAT | O_APPEND, 0666);
        dup2(fd, 1);
        close(fd);
        ++index;
        return 1; //success
    } else return 0; //fault
}

void handler (int s) {
    waitpid(pid, NULL, 0);
}
void close_pipe(int pipes[2]) {
    close(pipes[0]);
    close(pipes[1]);
}
int conveyor(int cur_ind, char *argv[], int count, int end_of_end, int *flag) {
    pid_t last_pid;
    int status, ret_val = 0;
    int t = cur_ind;
    int start;
    int argc = 1;
    int end = 0;
    int pipes[2][2], part = 0;
    pipe(pipes[1]);
    while (!end) {
        int cur1 = t;
        start = 0;
        while (cur1 < count) {
            if ((strcmp(argv[cur1], "|") == 0) || (strcmp(argv[cur1], ">") == 0) || (strcmp(argv[cur1], "<") == 0) || (strcmp(argv[cur1], ">>") == 0) ||
            (strcmp(argv[cur1], ")") == 0) || (strcmp(argv[cur1], "(") == 0)){
                break;
            }
            ++cur1;
        }
        char *buf[cur1 - t + 1];
        buf[cur1 - t] = NULL;
        int i = 0;
        while (t != cur1) {
            buf[i] = argv[t];
            ++i;
            ++t;
        }
        if ((t == count) || (strcmp(argv[t], ">") == 0) || ((strcmp(argv[t], ">>") == 0))) {
            end = 1;
        } else if (argc == 1) {
            start = 1;
            ++t;
        } else {
            ++t;
        }
        pipe(pipes[part]);
        if ((last_pid = fork()) == 0) {
            if (!end) {
                dup2(pipes[part][1], 1);
            } else {
                if (*flag == 1) {
                    redirection(t, argv);
                    t = t + 2;
                    *flag = 2;
                    if ((t < end_of_end) && (redirection(t, argv))) {
                        count = t + 2;
                        *flag = 3;
                    }
                }
            }
            if (!start) {
                dup2(pipes[(part + 1) % 2][0], 0);
            }
            close_pipe(pipes[part]);
            close_pipe(pipes[(part + 1) % 2]);
            execvp(buf[0], buf);
            return 1;
        }
        part = (part + 1) % 2;
        close_pipe(pipes[part]);
        ++argc;
    }
    close_pipe(pipes[(part + 1) % 2]);
    for (int i = 0; i < argc; i++) {
        if (last_pid == wait(&status) && last_pid > 0) {
            if (WIFEXITED(status)) {
                ret_val = WEXITSTATUS(status);
            }
        }
    }
    return ret_val;
}

void clear(int start, int finish, char *argv[]) {
    for (int i = start; i < finish; i++) {
        free(argv[i]);
    }
}
int command1(int command_status[], int cur_ind, char *argv[], int count) {
    int status1;
    pid_t pid1;
    int con_flag = 0;
    if ((pid1 = fork()) == 0) {
        int status;
        if (strcmp(argv[cur_ind], "(") == 0) {
            clear(cur_ind, count, argv);
            int stat = command_status[cur_ind];
            exit(stat);
        }
        int cur_ind1 = cur_ind;
        if (redirection(cur_ind1, argv)) {
            cur_ind1 = cur_ind1 + 2;
            if (redirection(cur_ind1, argv)) {
                cur_ind1 = cur_ind1 + 2;
            }
            status = conveyor(cur_ind1, argv, count, count, &con_flag);
            clear(cur_ind, count, argv);
            if (status == 0) {
                exit(1);
            } else {
                exit(0);
            }
        } else {
            int cur_ind2 = cur_ind;
            int buf1 = count;
            while (cur_ind2 < count) {
                if ((strcmp(argv[cur_ind2], ">") == 0) || (strcmp(argv[cur_ind2], ">>") == 0)) {
                    buf1 = cur_ind2;
                    con_flag = 1;
                    break;
                }
                ++cur_ind2;
            }
            status = conveyor(cur_ind, argv, buf1, count, &con_flag);
            if (con_flag == 2) {
                cur_ind2 = cur_ind2 + 2;
            }
            if (con_flag == 3) {
                cur_ind2 = cur_ind2 + 4;
            }
            clear(cur_ind, count, argv);
            if (status == 0) {
                exit(1);
            } else {
                exit(0);
            }
        }
    }
    waitpid(pid1, &status1, 0);
    if (WIFEXITED(status1) == 1){
        return WEXITSTATUS(status1);
    } else {
        return 1;
    }
}
int exec_process (int command_status[], int cur_ind, char *argv[], int count) {
    int finished = 0, unfinished = 0;
    int end = 0;
    int cur = cur_ind;
    while (cur < count) {
        if (strcmp(argv[cur], "||") == 0) {
            ++unfinished;
            break;
        } else if (strcmp(argv[cur], "&&") == 0) {
            ++finished;
            break;
        }
        ++cur;
    }
    if (cur == count) {
        end = 1;
    }
    int status = command1(command_status, cur_ind, argv, cur);
    if (!end){
        if ((!status && unfinished) || (finished && status)){
            status = exec_process(command_status, cur + 1, argv, count);
        }
    }
    return status;
}


void background_process (int command_status[], int cur_ind, char *argv[], int count) {
    int status;
    if (fork() == 0) {
        if ((pid = fork()) == 0) {
            int fd[2];
            fd[0] = open("/dev/null", O_RDONLY);
            dup2(fd[0], 0);
            close(fd[0]);
            fd[1] = open("/dev/null", O_WRONLY);
            dup2(fd[1], 1);
            close(fd[1]);
            signal(SIGINT, SIG_IGN);
            status = exec_process(command_status, cur_ind, argv, count);
            exit(status);
        } else {
            exit(0);
        }
    } else {
        signal(SIGCHLD, handler);
    }
}



int command_handler(int cur_ind, char *argv[], int count) {
    int command_status[MAX_AMOUNT];
    int buf1, buf2;
    int bad_flag = 0;
    buf1 = cur_ind;
    int end = 0;
    if (cur_ind == count) {
        return 1;
    }
    while (!end && !bad_flag) {
        bad_flag = 0;
        if (buf1 == count) {
            end = 1;
        }
        while (!end && !bad_flag) {
            if (strcmp(argv[buf1], "(") == 0) {
                ++buf1;
                break;
            }
            if ((strcmp(argv[buf1], ";") == 0) || (strcmp(argv[buf1], "&") == 0)){
                ++buf1;
                bad_flag = 1;
                break;
            }
            ++buf1;
            if (buf1 == count) {
                end = 1;
                break;
            }
        }
        if (end || bad_flag) {
            break;
        }
        buf2 = buf1;
        int shells = 1;
        while ((strcmp(argv[buf2], ")") != 0) || (shells != 1)) {
            if (strcmp(argv[buf2], "(") == 0) {
                ++shells;
            }
            if (strcmp(argv[buf2], ")") == 0) {
                --shells;
            }
            ++buf2;
        }
        int st;
        if (fork() == 0) {
            int status = command_handler(buf1, argv, buf2);
            exit(status);
        }
        wait(&st);
        if (WIFEXITED(st)) {
            command_status[buf1 - 1] = WEXITSTATUS(st);
        } else {
            command_status[buf1 - 1] = 1;
        }
        buf1 = buf2;
    }
    end = 0;
    int continuous = 0;
    int background = 0;
    buf2 = cur_ind;
    while (buf2 < count) {
        if (strcmp(argv[buf2], "&") == 0) {
            background = 1;
            break;
        } else if (strcmp(argv[buf2], ";") == 0) {
            continuous = 1;
            break;
        }
        ++buf2;
    }
    if (buf2 + background + continuous == count) {
        end = 1;
    }
    int status = 0;
    if (background) {
        background_process(command_status, cur_ind, argv, buf2);
    } else {
        status = exec_process(command_status, cur_ind, argv, buf2);
    }
    if (!end) {
        ++buf2;
        command_handler(buf2, argv, count);
    }
    return status;
}

int main() {
    while (1) {
        char command [COMMAND_MAXLENGTH + 1];
        char *argv[MAX_AMOUNT];
        int count = 0;
        int cur_ind = 0;
        fgets(command, COMMAND_MAXLENGTH + 1, stdin);
        if (strlen(command) != 0) {
            if (command[strlen(command) - 1] == '\n') {
                command[strlen(command) - 1] = '\0';
            }
        }
        convert_to_argv(command, &count, argv);
        command_handler(cur_ind, argv, count);
    }
    return 0;
}
