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
#include <map>
using namespace std;


vector <string> internal_commands {"cd","pwd","time"};


// встроенные функции
void cd ();
void pwd ();
void time ();


// мои будущие функции
void cd (vector <string> arg = {"cd", "nothing or name of the directory"}) {
    if (arg.size() == 1) {
        arg.push_back("home");
    }
    cout << "*тут нужно перейти в директорию с именем : " << arg[1] << "*"<< endl;
}


void pwd () {
    cout << "*тут нужно вывести полное имя текущей директории*" << endl;
}


void time (vector <string> parsed_line = {"time", "cmd", "other args for cmd"}) {
    if (parsed_line.size() < 3) {
        perror("слишком мало аргументов для time");
    }
    cout << "*тут нужно вывести три времени для команды : " << parsed_line[1] << " c аргументами : ";
    for (size_t i = 2; i < parsed_line.size(); i++) {
        cout << parsed_line[i] << ",";
    }
    cout << endl;
}


void execute_internal_commands (vector <string> parsed_line) {
    if (parsed_line[0] == "cd") {
        cd(parsed_line);
    } else if (parsed_line[0] == "pwd") {
        pwd();
    } else if (parsed_line[0] == "time") {
        time(parsed_line);
    } else {
        // не внутреняя команда
    }
}


struct Type_for_Execvp {
public :
    Type_for_Execvp() {cmd = NULL; *args = NULL;};
    Type_for_Execvp(vector <string> parsed_line);
    ~Type_for_Execvp() {delete [] cmd; ...};
private :
    char* cmd;
    char* args[];
};


Type_for_Execvp :: Type_for_Execvp (vector <string> parsed_line) {

    assert(parsed_line.size() > 0);

    cmd = new char [parsed_line[0].size()];
    args = new char* [parsed_line.size()];
    for (size_t i = 0; i < parsed_line.size(); i++) {
        args[i] = new char [parsed_line[i].size()];
        // для NULL
        if (i == parsed_line.size() - 1) {
            args[i] = new char (1);
            args[i][parsed_line[i].size()] = NULL;
        }
    }

    for (size_t i = 0; i < parsed_line[0].size(); i++) {
        cmd[i] = parsed_line[0][i];
    }
    for (size_t i = 0; i < parsed_line.size(); i++) {
        for (size_t j = 0; j < parsed_line[i].size(); j++) {
            args[i][j] = parsed_line[i][j];
        }
    }
}


void execute_external_commands (vector <string> parsed_line) {
    pid_t ret_pid, wpid;
    int status;
    char** parsed_line_double_pointer = convert_vs_to_cdp (parsed_line);

    ret_pid = fork();

    if (ret_pid == 0) {
        // дочерний процесс
        if (execvp(parsed_line_double_pointer[0], parsed_line_double_pointer) == -1) {
            // запускаем другой процесс с переданными аргументами и делаем проверку одновременно
            perror("not correct execvp");
        }
        // завершаем процесс, но делаем это так, что-бы AS продолжало работу
        exit(1);
    } else if (ret_pid > 0) {
        // родительский процесс
        // потомок собирается исполнить процесс, поэтому родитель должен дождаться завершения команды
        // процесс может либо завершиться обычным путём (успешно либо с кодом ошибки), либо быть остановлен сигналом.
        while ((wpid = wait(&status)) > 0);
    } else {
        perror("ret_pid < 0");
    }
}


void execute_commands (vector <string> parsed_line) {

    // если была введена пустая команда
    if (parsed_line.size() == 0) {
        perror ("была введена пустая команда");
    }

    // если у нас внутренняя команда
    if (find(internal_commands.begin(), internal_commands.end(), parsed_line[0]) != internal_commands.end()) {
        execute_internal_commands(parsed_line);
    }

    // если у нас внешняя команда
    else {
        execute_external_commands(parsed_line);
    }
}


vector <string> define_commands (string a) {
    // разделяется пробелами и знаками табуляции
    istringstream input_stream(a);
    vector <string> tokens {istream_iterator<string>{input_stream}, istream_iterator<string>{},{    }};
    return tokens;
}


string read_commands (void) {
    string line;
    getline(cin, line);
    return line;
}


void loop_for_commands (void) {
    while (true) {
        // чтение
        string line = read_commands();
        // парсинг
        vector <string> parsed_line = define_commands(line);
        // исполнение
        execute_commands(parsed_line);
    }
}


// считается, что AlexShell без инициализации, поэтому нет смысла использовать argc, argv
int main()
{

    // интерпретация (считывание команд, их определение и исполнение)
    loop_for_commands();

    return 0;
}
