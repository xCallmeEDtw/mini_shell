#include <cstdio>
#include <cstdlib>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <fcntl.h>
#include <iostream>
#include <string>
#include <vector>
#include <sstream>

#define num_builtin (sizeof(builtin_str) / sizeof(string))

using namespace std;
char pwdir[100];
int execute(char **, bool);
int builtin_cd(char **args) {
    if (args[1] == NULL) chdir(getenv("HOME"));
    else if (chdir(args[1]) == -1 ) perror("cd");
    getcwd(pwdir, 100);
    return 1;
}

int builtin_exit(char **args) {
    return 0;
}

string builtin_str[] = {
    "cd",
    "exit"
};

int (*builtin_func[])(char **) = {
    &builtin_cd,
    &builtin_exit
};

char **split(const string &str, const char &delimiter) {
    vector<string> result;
    stringstream ss(str);
    string tok;

    while (getline(ss, tok, delimiter)) {
        result.push_back(tok);
    }

    if (result.empty()) {
        char **array = new char *[1];
        array[0] = nullptr;
        return array;
    }

    char **array = new char *[result.size() + 1];
    for (size_t i = 0; i < result.size(); ++i) {
        array[i] = new char[result[i].length() + 1];
        strcpy(array[i], result[i].c_str());
    }
    array[result.size()] = nullptr;

    return array;
}

void loop() {
    string str;
    char **tokens;
    char delimiter = ' ';
    bool background = false; // Flag to track if background execution has been done
    char* login_name = NULL;
    char* pwd_dirname= NULL;   

    login_name = getenv("LOGNAME");
    pwd_dirname= getcwd(pwdir, 100);
    do {
        cout << login_name << "@"<< pwd_dirname<<"-shell>";
        getline(cin, str);

        // Check if the command should be executed in the background
        if (!str.empty() && str.back() == '&') {
            if (!background) {
                // If background execution hasn't been done yet, treat '&' as an indicator for background execution
                str.pop_back(); // Remove the '&' character
                background = true;
            }
        }

        tokens = split(str, delimiter);

        if (str.empty()) {
            // Skip empty commands
            continue;
        }

        // Execute the command
        int status = execute(tokens, background);

        // Cleanup allocated memory
        for (size_t i = 0; tokens[i] != nullptr; ++i) {
            delete[] tokens[i];
        }
        delete[] tokens;

        // Display "Done" message if background execution completed
        if (background) {
            background = false; // Reset background flag
        }

        // Wait for foreground command to complete
        if (status == 0) {
            break; // Exit loop if status indicates exit command
        }
    } while (true);
}

int launch(vector<vector<string> > &commands, bool background, bool input_redirect, bool output_redirect, string input_file, string output_file) {
    size_t num_commands = commands.size();
    pid_t pid;
    int pipefd[num_commands - 1][2]; // Create pipes

    // Create pipes
    for (size_t i = 0; i < num_commands - 1; ++i) {
        if (pipe(pipefd[i]) == -1) {
            perror("pipe");
            exit(EXIT_FAILURE);
        }
    }

    // Execute commands
    for (size_t i = 0; i < num_commands; ++i) {
        // Check for built-in commands
        bool is_builtin = false;

        for (int j = 0; j < sizeof(builtin_str) / sizeof(string); ++j) {
            if (builtin_str[j] == commands[i][0]) {
                is_builtin = true;
                break;
            }
        }

        if (!is_builtin) {
            pid = fork();
            if (pid == -1) {
                perror("fork");
                exit(EXIT_FAILURE);
            }

            if (pid == 0) { // Child process
                // Redirect input if necessary
                if (i == 0 && input_redirect) {
                    int fd_in = open(input_file.c_str(), O_RDONLY);
                    if (fd_in == -1) {
                        perror("open");
                        exit(EXIT_FAILURE);
                    }
                    dup2(fd_in, STDIN_FILENO);
                    close(fd_in);
                } else if (i != 0) {
                    dup2(pipefd[i - 1][0], STDIN_FILENO);
                    close(pipefd[i - 1][0]);
                }

                // Redirect output if necessary
                if (i == num_commands - 1 && output_redirect) {
                    int fd_out = open(output_file.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
                    if (fd_out == -1) {
                        perror("open");
                        exit(EXIT_FAILURE);
                    }
                    dup2(fd_out, STDOUT_FILENO);
                    close(fd_out);
                } else if (i != num_commands - 1) {
                    dup2(pipefd[i][1], STDOUT_FILENO);
                    close(pipefd[i][1]);
                }

                // Close all pipes
                for (size_t j = 0; j < num_commands - 1; ++j) {
                    close(pipefd[j][0]);
                    close(pipefd[j][1]);
                }

                // Execute command
                char **args = new char *[commands[i].size() + 1];
                for (size_t j = 0; j < commands[i].size(); ++j) {
                    args[j] = new char[commands[i][j].size() + 1];
                    strcpy(args[j], commands[i][j].c_str());
                }
                args[commands[i].size()] = nullptr;

                if (execvp(args[0], args) == -1) {
                    perror(args[0]);
                    exit(EXIT_FAILURE);
                }
            }
        }
    }

    // Close all pipes
    for (size_t i = 0; i < num_commands - 1; ++i) {
        close(pipefd[i][0]);
        close(pipefd[i][1]);
    }

    // Wait for all child processes to finish
    for (size_t i = 0; i < num_commands; ++i) {
        if (!background) {
            int status;
            waitpid(pid, &status, 0);
        }
    }
    // Check for built-in commands and execute them
    for (size_t i = 0; i < num_commands; ++i) {
        for (int j = 0; j < sizeof(builtin_str)/sizeof(string); ++j) {
            if (builtin_str[j] == commands[i][0]) {
                // Convert vector<string> to char**
                char** args = new char*[commands[i].size() + 1];
                for (size_t k = 0; k < commands[i].size(); ++k) {
                    args[k] = new char[commands[i][k].size() + 1];
                    strcpy(args[k], commands[i][k].c_str());
                }
                args[commands[i].size()] = nullptr;

                // Execute built-in function
                int result = (*builtin_func[j])(args);

                // Cleanup allocated memory
                for (size_t k = 0; args[k] != nullptr; ++k) {
                    delete[] args[k];
                }
                delete[] args;

                return result;
            }
        }
    }
    return 1;
}

int execute(char **args, bool background = false) {
    if (args == nullptr) return 1;
    if (args[0] == NULL) return 1;

    vector<vector<string> > commands;
    vector<string> command;
    bool input_redirect = false;
    bool output_redirect = false;
    string input_file;
    string output_file;

    // Parse command and handle redirection
    for (int i = 0; args[i] != nullptr; ++i) {
        if (strcmp(args[i], "<") == 0) {
            input_redirect = true;
            input_file = args[i + 1]; // Next argument is input file
            ++i; // Skip next argument
        } else if (strcmp(args[i], ">") == 0) {
            output_redirect = true;
            output_file = args[i + 1]; // Next argument is output file
            ++i; // Skip next argument
        } else if (strcmp(args[i], "|") == 0) {
            if (!command.empty()) {
                commands.push_back(command);
                command.clear();
            }
        } else {
            command.push_back(args[i]);
        }
    }
    if (!command.empty()) {
        commands.push_back(command);
    }

    return launch(commands, background, input_redirect, output_redirect, input_file, output_file);
}

int main() {
    char* login_name = NULL;
    char* pwd_dirname= NULL;
    login_name = getenv("LOGNAME");
    pwd_dirname= getenv("PWD"); 
    printf("==============================\n");
    printf("LOGNAME : %s\n", login_name);
    printf("PWD : %s\n", pwd_dirname);
    printf("ProgramDate: 2022/4/4 Ver:1.0\n");
    printf("==============================\n");
    loop();
    return 0;
}
