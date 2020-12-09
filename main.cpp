#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <iterator>
#include <sys/wait.h>
#include <sys/times.h>
#include <unistd.h>
#include <pwd.h>
#include <ctime>
#include <chrono>
#include <unistd.h>
#include <fstream>
using namespace std;
using namespace chrono;


char* home_dir = getpwuid(getuid())->pw_dir;
bool catch_ctrl_C = false;


// внутренние команды
void cd ();
string pwd ();


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
        while (catch_ctrl_C == false && (wait(&status) > 0));

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

        // реализация метасимвола > для предпоследнего элемента
        bool reassining_input = false;
        stringstream buffer;
        streambuf * old;
        string name_of_file_for_input;
        if (parsed_line.size() > 2 && parsed_line[parsed_line.size()-2] == ">") {
            reassining_input = true;
            name_of_file_for_input = parsed_line[parsed_line.size()-1];
            parsed_line.erase(parsed_line.end() - 2, parsed_line.end() - 1);
            old = cout.rdbuf(buffer.rdbuf());
        }

        // реализация time
        auto clock_start = high_resolution_clock::now();
        bool command_is_time = false;
        if (parsed_line.size() > 0 && parsed_line[0] == "time") {
            command_is_time = true;
            parsed_line.erase(parsed_line.begin());
        }

        // до execute_commands. tms_cutime и tms_cstime содержит кумулятивное user time и system time дестких процессов
        struct tms before_the_command;
        times(&before_the_command);

        // исполнение команд
        execute_commands(parsed_line);

        // после execute_commands. Вычислим разницу и получим user time своей команды и system time своей команды
        struct tms after_the_command;
        times(&after_the_command);

        // если команда была time
        if (command_is_time) {
            auto clock_end = high_resolution_clock::now();
            milliseconds d = duration_cast <milliseconds> (duration <double> (clock_end - clock_start));
            auto user_time = after_the_command.tms_cutime - before_the_command.tms_cutime;
            auto sys_time = after_the_command.tms_cstime - before_the_command.tms_cstime;
            string result = "real time : " + to_string (d.count()) + "ms\n" + "user time : " + to_string((long long int) user_time) + "ms\n" + "sys time : " + to_string((long long int) sys_time) + "ms\n";
            const char* real_time = result.c_str();
            perror(real_time);
        }

        // если был метасимвол >
        if (reassining_input == true) {

            ofstream f_out;
            f_out.open(name_of_file_for_input);
            if (f_out.is_open() == false) {
                perror("неправильный open");
            }
            f_out << buffer.str();
            f_out.close();
        }
    }

    return 0;
}
