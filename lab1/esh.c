#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>

#define ERROR_INFO "An error has occurred\n"
#define MAX_CMD_LEN 1024
#define MAX_PATHS 100
#define MAX_BG_JOBS 100

// 全局变量
char *path_list[MAX_PATHS];
int path_count = 0;

// 用于存储后台作业信息的结构体
typedef struct {
    pid_t pid;
    char cmd[MAX_CMD_LEN];
} bg_job_t;

bg_job_t bg_jobs[MAX_BG_JOBS];
int bg_job_count = 0;

//
// You should use the following functions to print information
// Do not modify these functions
//
void print_error_info() {
    printf(ERROR_INFO);
    fflush(stdout);
}

void print_path_info(int index, char *path) {
    printf("%d\t %s\n", index, path);
    fflush(stdout);
}

void print_bg_info(int index, int pid, char *cmd) {
    printf("%d\t %d\t %s\n", index, pid, cmd);
    fflush(stdout);
}

void print_current_bg(int pid, char *cmd) {
    printf("Process %d %s: running in background\n", pid, cmd);
    fflush(stdout);
}

void parse_command(char *cmd, char **args);//解析command字符串
void execute_command(char *cmd);//处理输入command
void execute_external_command(char **args);//执行外部命令
void execute_pipeline(char *cmd);//处理管道
void execute_redirection(char *cmd);// 处理重定向命令
void execute_sequence(char *cmd);//执行多个命令的顺序执行
void handle_cd(char **args);//处理 cd 命令
void handle_exit();//处理exit命令
void handle_paths(char **args);//处理 paths 命令
void add_path(const char *new_path);//添加新路径
void handle_bg();//显示当前的后台任务(fail)
void handle_bg_processes();//检查所有后台进程是否已结束(fail)


int main() {

     char input[MAX_CMD_LEN];

    add_path("/bin");

    while(1) {
        printf("esh > ");
        fflush(stdout);

    if (fgets(input, MAX_CMD_LEN, stdin) == NULL) {
                break;  //如果读取失败，则退出循环
            }
            input[strcspn(input, "\n")] = 0;  // 去除输入末尾换行符
            //printf("Received command successfully: %s\n", input);
            execute_command(input);
        }

        return 0;
}

void parse_command(char *cmd, char **args) {
    char *token = strtok(cmd, " ");// 以空格分割command
    int i = 0;

    while (token != NULL) {
        args[i++] = token;
        token = strtok(NULL, " ");
    }
    args[i] = NULL;// 以 NULL 结束参数数组
    
    // printf("Parsed command args: ");
    // for (int j = 0; j < i; j++) {
    //     printf("%s ", args[j]);
    // }
    // printf("\n"); 
}


void execute_command(char *cmd) {
    // 去除命令开头和结尾的空白字符
    while (isspace(*cmd)) cmd++;

    // 是否包含管道符号|
    if (strchr(cmd, '|') != NULL) {
        execute_pipeline(cmd);
        return;
    }

    // 是否包含重定向符号>
    if (strchr(cmd, '>') != NULL) {
        execute_redirection(cmd);
        return;
    }

    // 是否包含分号;
    if (strchr(cmd, ';') != NULL) {
        execute_sequence(cmd);
        return;
    }

    // 检查是否为内置命令
    if (strcmp(cmd, "exit") == 0) {
        handle_exit();
        return;
    } else if (strncmp(cmd, "cd", 2) == 0) {
        char *args[MAX_CMD_LEN];
        parse_command(cmd, args);  // 使用解析函数
        handle_cd(args);
        return;
    } else if (strncmp(cmd, "paths", 5) == 0) {
        char *args[MAX_CMD_LEN];
        parse_command(cmd, args);  // 使用解析函数
        handle_paths(args);
        return;
    } else if (strncmp(cmd, "bg", 2) == 0) {
        handle_bg();
        return;
    }

    // 执行外部命令
    char *args[MAX_CMD_LEN];
    parse_command(cmd, args);  // 使用解析函数
    execute_external_command(args);
}

void handle_cd(char **args) {
    if (args[1] == NULL || args[2] != NULL) {
        print_error_info();//检查参数个数
        return;
    }
    if (strcmp(args[1], "~") == 0) {
        chdir(getenv("HOME"));//切换到当前用户的主目录
    } else {
          // 切换到指定目录
        if (chdir(args[1]) != 0) {
            print_error_info();
        }
    }
}

void handle_exit() {
    exit(0);
}

void handle_paths(char **args) {
    if (args[1] == NULL) {
        for (int i = 0; i < path_count; i++) {
            // 打印路径信息，索引从 1 开始
            print_path_info(i + 1, path_list[i]);
        }
    } else {
        // 更新 PATH
        path_count = 0;  // 清空现有的路径列表
        for (int i = 1; args[i] != NULL; i++) {
            add_path(args[i]);
        }
    }
}

void add_path(const char *new_path) {
    if (path_count < MAX_PATHS) {
        path_list[path_count++] = strdup(new_path);
    } else {
        print_error_info();
    }
}

void execute_external_command(char **args) {
    pid_t pid = fork();
    if (pid == 0) {
        //printf("Executee external command: %s\n", args[0]); 
       // 将标准错误重定向到标准输出
        dup2(STDOUT_FILENO, STDERR_FILENO); 
        execvp(args[0], args);
        // execvp 失败
        print_error_info();
        exit(1); 
    } else if (pid > 0) {
        waitpid(pid, NULL, 0); // 等待子进程结束
    } else {
        print_error_info(); // 创建子进程失败，打印错误信息
    }
}

void handle_bg() {
    // 检查当前是否有后台任务
    if (bg_job_count == 0) {
        printf("No background jobs.\n");
        return;
    }
    //printf(" currrent background jobs:\n");
    // 遍历并打印所有后台任务的信息
    for (int i = 0; i < bg_job_count; i++) {
        print_bg_info(i + 1, bg_jobs[i].pid, bg_jobs[i].cmd);
    }
}

void execute_pipeline(char *cmd) {
    char *commands[MAX_CMD_LEN];
    int num_commands = 0;
    char *token = strtok(cmd, "|");

    //printf("Executing pipeline: %s\n", cmd);
    // 将命令分割并存储到 commands 数组中
    while (token != NULL) {
        commands[num_commands++] = token;
        token = strtok(NULL, "|");
    }

    int pipefd[2 * (num_commands - 1)]; // 管道数组
    for (int i = 0; i < num_commands - 1; i++) {
        if (pipe(pipefd + i * 2) < 0) {
            print_error_info(); // 创建管道失败
            return;
        }
    }

    int status;
    int error_occurred = 0; // 用于标记是否出现错误
    for (int i = 0; i < num_commands; i++) {
        pid_t pid = fork();
        if (pid == 0) { // 子进程
            // 如果不是最后一个命令，重定向输出到下一个管道
            if (i < num_commands - 1) {
                dup2(pipefd[i * 2 + 1], STDOUT_FILENO);
            }
            // 如果不是第一个命令，重定向输入来自前一个管道
            if (i > 0) {
                dup2(pipefd[(i - 1) * 2], STDIN_FILENO);
            }
            // 关闭所有管道
            for (int j = 0; j < 2 * (num_commands - 1); j++) {
                close(pipefd[j]);
            }

           
            char *args[MAX_CMD_LEN];
            parse_command(commands[i], args);
            execvp(args[0], args);
            print_error_info(); // execvp 失败
            exit(1); // 退出子进程
        } else if (pid < 0) {
            print_error_info(); // fork 失败
        }
    }

    // 关闭所有管道
    for (int i = 0; i < 2 * (num_commands - 1); i++) {
        close(pipefd[i]);
    }

    // 等待所有子进程结束并获取状态
    for (int i = 0; i < num_commands; i++) {
        pid_t child_pid = wait(&status);
        if (child_pid > 0) {
            if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
                error_occurred = 1; // 记录是否有错误发生
            }
        }
    }

    // 处理最后一个命令的输出
    if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
        // 如果最后一个命令成功，检查是否有错误发生
        if (error_occurred) {
            print_error_info(); 
        }
    } else {
        // 如果最后一个命令失败，直接输出错误信息
        print_error_info(); 
    }
}




void execute_redirection(char *cmd) {
    char *args[MAX_CMD_LEN];
    char *token = strtok(cmd, ">");
    char *output_file = strtok(NULL, ">");
    
    // 去除多余空格
    while (isspace(*token)) token++;  
    if (output_file != NULL) {
        while (isspace(*output_file)) output_file++; 
    }

    // 检查输出文件名是否有效
    if (output_file == NULL || strlen(output_file) == 0) {
        print_error_info();
        return;
    }

    // 去除文件名末尾空格
    char *end = output_file + strlen(output_file) - 1;
    while (end > output_file && isspace(*end)) end--;
    end[1] = '\0';

    // 解析命令参数
    int arg_count = 0;
    char *arg_token = strtok(token, " ");
    while (arg_token != NULL) {
        args[arg_count++] = arg_token;
        arg_token = strtok(NULL, " ");
    }
    args[arg_count] = NULL;

    // 创建输出文件并重定向输出
    FILE *file = fopen(output_file, "w");
    if (file == NULL) {
        print_error_info();
        return;
    }
    
    // 重定向输出
    int saved_stdout = dup(STDOUT_FILENO);
    dup2(fileno(file), STDOUT_FILENO);
    fclose(file); // 关闭文件指针

    // 执行命令
    pid_t pid = fork();
    if (pid == 0) {
        execvp(args[0], args);
        print_error_info(); // 如果 execvp 失败
        exit(1);
    } else if (pid > 0) {
        int status;
        waitpid(pid, &status, 0); // 等待子进程结束
    } else {
        print_error_info(); //  fork 失败
    }

    // 恢复标准输出
    dup2(saved_stdout, STDOUT_FILENO);
    close(saved_stdout);
}


void execute_sequence(char *cmd) {
    char *commands[MAX_CMD_LEN]; 
    int num_commands = 0;
    char *token = strtok(cmd, ";");

    // 将命令分割并存储到 commands 数组中
    while (token != NULL) {
        commands[num_commands++] = token;
        token = strtok(NULL, ";");
    }

    for (int i = 0; i < num_commands; i++) {
        char *command = commands[i];
        
        // 去除首尾空格
        while (isspace(*command)) command++;
        char *end = command + strlen(command) - 1;
        while (end > command && isspace(*end)) end--;
        end[1] = '\0'; 

        if (strlen(command) == 0) {
            continue; 
        }

      //printf("Executing command: %s\n", command);

        // 检查是否是内置命令或执行管道
         char *args[MAX_CMD_LEN];
        parse_command(command, args);

        if (strcmp(args[0], "cd") == 0) {
            handle_cd(args);
        } else if (strcmp(args[0], "paths") == 0) {
            handle_paths(args);
        } else if (strcmp(args[0], "bg") == 0) {
            handle_bg();
        } else {
            // 执行外部命令
            pid_t pid = fork();
            if (pid == 0) {
                execvp(args[0], args);
                print_error_info(); // execvp 失败
                exit(1);
            } else if (pid > 0) {
                int status;
                waitpid(pid, &status, 0); // 等待子进程
                //printf("Child process: %d  status: %d\n", pid, status);
            } else {
                print_error_info(); // fork 失败
            }
        }
    }
}


void handle_bg_processes() {
    int status;//存储子进程退出状态
    pid_t pid;

    for (int i = 0; i < bg_job_count; ) {
        pid = waitpid(bg_jobs[i].pid, &status, WNOHANG);
        if (pid > 0) {
            //如果进程已结束，打印该后台作业的信息
            print_bg_info(i + 1, bg_jobs[i].pid, bg_jobs[i].cmd);
           //printf("Background job %d completed: %s\n", bg_jobs[i].pid, bg_jobs[i].cmd)
           // 作业完成，从列表中移除它
            for (int j = i; j < bg_job_count - 1; j++) {
                bg_jobs[j] = bg_jobs[j + 1];
            }
            bg_job_count--;
            // 不增加 i；此时 i 指向下一个作业
        } else {
            i++;
        }
    }
}