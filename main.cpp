#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <iterator>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <pwd.h>
#include <signal.h>
#include <stdlib.h>
#include <thread>
using namespace std;


char* home_dir = getpwuid(getuid())->pw_dir;


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


void execute_external_commands (vector <string> parsed_line) {
    pid_t ret_pid, wpid;
    int status;
    char* cmd = convert_string_to_char_pointer (parsed_line[0]);
    char** argv = (char**) calloc (parsed_line.size() + 1, sizeof(char*));
    for (size_t i = 0; i < parsed_line.size(); i++) {
        argv[i] = convert_string_to_char_pointer(parsed_line[i]);
    }

    ret_pid = fork();

    if (ret_pid == 0) {
        // дочерний процесс
        if (execvp(cmd, argv) == -1) {
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

    // чистка
    free (cmd);
    for (size_t i = 0; i < parsed_line.size() + 1; i++) {
        free(argv[i]);
    }
    free (argv);
}


void execute_commands (vector <string> parsed_line) {

    // если была введена пустая команда
    if (parsed_line.size() == 0) {
        cout << "была введена пустая команда" << endl;
        return ;
    }

    // выполняем
    if (parsed_line[0] == "cd") {
        cd(parsed_line);
    } else if (parsed_line[0] == "pwd") {
        cout << pwd() << endl;
    } else if (parsed_line[0] == "time") {
        time(parsed_line);
    } else {
        execute_external_commands(parsed_line);
    }
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


void my_handler(int s){
    printf("Caught signal ctrl C %d\n",s);
    exit(1);
}


// вроде все норм
void catch_ctrl_C () {
   struct sigaction sigIntHandler;

   sigIntHandler.sa_handler = my_handler;
   sigemptyset(&sigIntHandler.sa_mask);
   sigIntHandler.sa_flags = 0;

   sigaction(SIGINT, &sigIntHandler, NULL);

   pause();
}


// не правильно работает, тк не выходит сразу при exit(1) а еще выводит другие строки
void catch_ctrl_D () {
    while (cin);
    printf("Caught signal ctrl D \n");
    exit(1);
}


// считается, что AlexShell без инициализации, поэтому нет смысла использовать argc, argv
int main()
{

    // будем ловить ctrl D и ctrl D в отдельных потоках
    thread thread_for_ctrl_C (catch_ctrl_C);
    thread thread_for_ctrl_D (catch_ctrl_D);

    // интерпретация (считывание команд, их определение и исполнение)
    while (true) {
        // чтение и парсинг
        vector <string> parsed_line = read_and_define_commands();
        // исполнение
        execute_commands(parsed_line);
    }

    return 0;
}
