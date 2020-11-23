#include <iostream>
#include <fstream>
#include <vector>
#include <cstring>
#include <string>
#include <algorithm>
#include <sstream>
#include <iterator>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <assert.h>
#include <stdlib.h>
using namespace std;


char** convert_vs_to_cdp (vector <string> strings)  {
    vector<char*> cstrings{};
    char** result;

    for(auto& string : strings) {
        cstrings.push_back(&string.front());
    }

    result = cstrings.data();
    return result;
}


string as_read_commands (void) {
    string line;
    getline(cin, line);
    return line;
}


vector <string> as_define_commands (string a) {
    // разделяется пробелами и знаками табуляции
    istringstream input_stream(a);
    vector <string> tokens {istream_iterator<string>{input_stream}, istream_iterator<string>{},{    }};
    // для execvp
    tokens.push_back(NULL);
    return tokens;
}


void as_execute_commands (vector <string> parsed_line) {
    pid_t ret_pid, wait_pid;
    int status;
    char** parsed_line_double_pointer = convert_vs_to_cdp (parsed_line);

    ret_pid = fork();

    if (ret_pid == 0) {
        // дочерний процесс
        if (execvp(parsed_line[0],parsed_line_double_pointer) == -1) {
            // запускаем другой процесс с переданными аргументами
            perror("not correct execvp");
        }
        // завершаем процесс, но делаем это так, что-бы AS продолжало работу
        exit(1);
    } else if (ret_pid > 0) {
        // родительский процесс
        // потомок собирается исполнить процесс, поэтому родитель должен дождаться завершения команды
        // процесс может либо завершиться обычным путём (успешно либо с кодом ошибки), либо быть остановлен сигналом.
        // мы используем макросы, предоставляемые waitpid(), чтобы убедиться, что процесс завершен.
        do {
            wait_pid = waitpid(ret_pid, &status, WUNTRACED);
        } while (!WIFEXITED(status) && !WIFSIGNALED(status));
    } else {
        perror("ret_pid < 0");
    }
}


void as_loop_for_commands (void) {
    while (true) {
    // чтение
    string line = as_read_commands();
    // парсинг
    vector <string> parsed_line = as_define_commands(line);
    // исполнение
    //as_execute_commands();
    }
}


// считается, что AlexShell без инициализации, поэтому нет смысла использовать argc, argv
int main()
{
    // интерпретация (считывание команд, их определение и исполнение)
    as_loop_for_commands();

    return 0;
}
