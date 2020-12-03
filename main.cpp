#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <iterator>
#include <sys/wait.h>
#include <unistd.h>
#include <pwd.h>
using namespace std;


char* home_dir = getpwuid(getuid())->pw_dir;
bool catch_ctrl_C = false;


// внутренние команды
void cd ();
string pwd ();
void time ();


//arg = {"cd", "nothing or name of the directory"}
void cd (vector <string> arg) {
    size_t max_length = 1024, k = 0;
    char path [max_length];
    if (arg.size() == 1) {
        arg.push_back(string(home_dir));
    }
    for (size_t i = 1; i < arg.size(); i++) {
        for (size_t j = 0; j < arg[i].size(); j++) {
            path[k] = arg[i][j];
            k++;
        }
    }
    path[k] = NULL;

    int ret_result = chdir (path);

    if (ret_result == -1) {
        cout << " невозможно перейти по каталогу : " << string(path) << endl;
    }
}


// no leacks, I've cheked
string pwd () {
    size_t max_length = 1024;
    char buff [max_length];
    getcwd (buff, max_length);
    string cwd (buff);
    return cwd;
}


// parsed_line = {"time", "cmd", "other args for cmd"}
void time (vector <string> parsed_line) {
    if (parsed_line.size() < 3) {
        perror("слишком мало аргументов для time");
    }
    cout << "*тут нужно вывести три времени для команды : " << parsed_line[1] << " c аргументами : ";
    for (size_t i = 2; i < parsed_line.size(); i++) {
        cout << parsed_line[i] << ",";
    }
    cout << endl;
}


char* convert_string_to_char_pointer (string input) {
    char* output = (char*) malloc(input.size() + 1);
    for (size_t i = 0; i < input.size(); i++) {
        output[i] = input[i];
    }
    output[input.size()] = NULL;
    return output;
}


void execute_commands (vector <string> parsed_line) {

    // если была введена пустая команда или сигнал
    if (parsed_line.size() == 0) {
        cout << "была введена пустая команда или сигнал" << endl;
        return;
    }

    // я не создаю новый процесс для внутренних команд
    // внутренние команды
    if (parsed_line[0] == "cd") {
        cd(parsed_line);
        return;
    } else if (parsed_line[0] == "pwd") {
        cout << pwd() << endl;
        return;
    } else if (parsed_line[0] == "time") {
        time(parsed_line);
        return;
    }

    // я создаю новый процесс для внешних команд и заменяю его с помощью execvp
    // внешние команды
    pid_t ret_pid;
    int status;
    char* cmd = convert_string_to_char_pointer (parsed_line[0]);
    char** argv = (char**) calloc (parsed_line.size() + 1, sizeof(char*));
    for (size_t i = 0; i < parsed_line.size(); i++) {
        argv[i] = convert_string_to_char_pointer(parsed_line[i]);
    }

    // создали новый процесс
    ret_pid = fork();

    // для ловли ctrl C
    catch_ctrl_C = false;

    if (ret_pid == 0) {
        // дочерний процесс
        if (execvp(cmd, argv) == -1) {
            // запускаем другой процесс с переданными аргументами и делаем проверку одновременно
            perror("not correct execvp");
        }
        // завершаем процесс, но делаем это так, что-бы AS продолжало работу
        exit(0);
    } else if (ret_pid > 0) {
        // родительский процесс
        // потомок собирается исполнить процесс, поэтому родитель должен дождаться завершения команды либо сигнала
        // процесс может либо завершиться обычным путём (успешно либо с кодом ошибки), либо быть остановлен сигналом.
        while (catch_ctrl_C == false && (waitpid(-1, &status, 0) > 0));

        if (catch_ctrl_C == true) {
            cout << "we have killed the process with Ctrl C" << endl;
            kill(ret_pid, SIGQUIT);
        }
    } else {
        perror("ret_pid < 0");
    }

    // чистка
    free (cmd);
    for (size_t i = 0; i < parsed_line.size() + 1; i++) {
        free(argv[i]);
    }
    free (argv);
}


vector <string> read_and_define_commands () {
    cout << pwd() << " > : ";
    string line;
    getline(cin, line);
    // разделяется пробелами и знаками табуляции
    istringstream input_stream (line);
    vector <string> tokens {istream_iterator<string>{input_stream}, istream_iterator<string>{},{    }};
    return tokens;
}


void signalHandler(int signum) {
   catch_ctrl_C = true;
}


// считается, что AlexShell без инициализации, поэтому нет смысла использовать argc, argv
int main() {

    // ловим ctrl C
    signal(SIGINT, signalHandler);

    // while (cin) ловит ctrl D
    // интерпретация (считывание команд, их определение и исполнение)
    while (cin) {
        // чтение и парсинг
        vector <string> parsed_line = read_and_define_commands();
        // исполнение
        execute_commands(parsed_line);
    }

    return 0;
}
