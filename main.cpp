#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <string>
#include <vector>
#include <iostream>
#include <chrono>
#include <pwd.h>
#include <sys/times.h>
#include <fcntl.h>
#include <string>
#include <ctime>
#include <dirent.h>
#include <sstream>
#include <iterator>
using namespace std;
using namespace chrono;


char* home_dir = getpwuid(getuid()) -> pw_dir;
bool catch_ctrl_C = false;


// алгоритм, который говорит подходит ли слово заданному паттерну
bool is_match(string text, string pattern) {
    // вся таблица заполнена false
    vector<vector<bool>> table(text.size() + 1, vector<bool>(pattern.size() + 1));
    table[0][0] = true;
    // инициализация пока вначале стоят метасимволы они подходят под пустую строку
    for (size_t j = 0; j < pattern.size(); j++) {
        if (pattern[j] != '*') {
            break;
        }
        table[0][j + 1] = true;
    }
    for (size_t i = 0; i < text.size(); i++) {
        for (size_t j = 0; j < pattern.size(); j++) {
            char p = pattern[j];
            if (p == '*') {
                table[i + 1][j + 1] = table[i + 1][j] || table[i][j + 1];
            } else if (p == '?' || p == text[j]) {
                table[i + 1][j + 1] = table[i][j];
            }
        }
    }
    return table[text.size()][pattern.size()];
}


// пример с семинарa
// рекурсивно обходит директории которые подходят под шаблон и заполняет files найденными файлами
void list_match_files (string name, vector <string>& files, int part, const vector<string>& pattern_parts) {
    DIR *dir;
    struct dirent *entry;

    if (!(dir = opendir(name.c_str()))) {
        cout << "не получилось открыть каталог " << name << endl;
        return;
    }
    while ((entry = readdir(dir)) != NULL) {
        string curname = string(entry -> d_name);
        if (curname == "." || curname == "..") {
            // пропускаем ссылки на текущий и родительский каталог.
            continue;
        }
        if (!is_match(curname, pattern_parts[part])) {
            // пропускаем так как не подходит под шаблон
            continue;
        }
        // полное имя файла
        string fullname = name + string("/") + curname;
        if (size_t(part) == pattern_parts.size() - 1) {
            // если в конечной части шаблона добавляем и файлы и каталоги
            files.push_back(fullname);
        } else if (entry->d_type == DT_DIR) {
            // рекурсивно вызываем на дочерний каталог.
            list_match_files(fullname, files, part + 1, pattern_parts);
        }
    }
    closedir(dir);
}


// находит файлы которые соответствуют метасимволам.
vector <string> get_matching_names (string pattern) {
    if (pattern.size() == 0) {
        cerr << "выражение с мета-символами должно быть не пустое" << endl;
        exit(1);
    }

    // разбиваем pattern по "/"
    string line;
    vector <string> pattern_parts;
    stringstream ss(pattern);
    while (getline(ss, line, '/')) {
        pattern_parts.push_back(line);
    }

    string name_prefix;
    // начинаем с корня
    if (pattern_parts[0] == "") {
        name_prefix = "/";
        pattern_parts.erase(pattern_parts.begin());
    } else if (pattern_parts[0] == "." || pattern_parts[0] == "..") {
        name_prefix = pattern_parts[0];
        pattern_parts.erase(pattern_parts.begin());
    } else {
        // если не указан абсолютный или относительный путь то начинаем с текущей директории
        name_prefix = ".";
    }

    vector<string> files;
    list_match_files(name_prefix, files, 0, pattern_parts);
    if (files.size() == 0) {
        cerr << "ошибка - нет подходящих файлов под шаблон " << pattern << endl;
        exit(1);
    }

    return files;
}


// раскрывает метасимволы в аргументах
vector<string> get_cmd_no_metasymbols(vector<string> cmd_with_args) {
    vector<string> cmd_final;

    // команда (первая) не может содержать метасимволом
    cmd_final.push_back(cmd_with_args[0]);

    for (size_t i = 1; i < cmd_with_args.size(); i++) {
        string arg = cmd_with_args[i];
        // проверяем если аргумент содержит метасимволы
        if (arg.find('*') != string::npos || arg.find('?') != string::npos) {
            // содержит - ищем все подходящии файлы
            vector <string> files = get_matching_names (arg);
            for (auto file : files) {
                cmd_final.push_back(file);
            }
        } else {
            // аргумент не содержит метасимволы - просто добавляем
            cmd_final.push_back(arg);
        }
    }

    return cmd_final;
}


string get_current_working_dir() {
    size_t max_length = 1024;
    char buff[max_length];
    getcwd(buff, max_length);
    string cwd(buff);
    return cwd;
}


char* convert_string_to_char_pointer(string input) {
    char* output = (char*) malloc(input.size() + 1);
    for (size_t i = 0; i < input.size(); i++) {
        output[i] = input[i];
    }
    output[input.size()] = 0;
    return output;
}


//arg = {"cd", "nothing or name of the directory"}
void cd_command (vector <string> cmd_and_args) {
    if (cmd_and_args.size() > 2) {
        cout << "комманда cd получила слишком много аргументов" << endl;
        return;
    }
    if (cmd_and_args.size() == 2) {
        char* path = convert_string_to_char_pointer(cmd_and_args[1]);
        int ret_result = chdir(path);
        if (ret_result == -1) {
            cout << "невозможно перейти по каталогу : " << cmd_and_args[1];
        }
        free(path);
    } else {
        int ret_result = chdir(home_dir);
        if (ret_result == -1) {
            cout << "невозможно перейти в домашний каталог" << endl;
        }
    }
}


void dup_and_check(int oldfd, int newfd) {
    if (dup2(oldfd, newfd) < 0) {
        perror("ошибка в dup2");
        exit(1);
    }
}


int open_and_check(string filename, int flags) {
    // 0644 это rw_r__r__
    // flags определяют возможности
    int fd = open(filename.c_str(), flags, 0644);
    if (fd < 0) {
        perror("ошибка в open");
        perror(filename.c_str());
        exit(1);
    }
    return fd;
}


void fifo_close_and_free(int* fifo) {
    close(fifo[0]);
    close(fifo[1]);
    free(fifo);
}


vector<vector<string>> read_conveyor_components() {
     string line;
     getline(cin, line);
    // разделяется пробелами и знаками табуляции
    istringstream input_stream (line);
    vector <string> tokens {istream_iterator<string>{input_stream}, istream_iterator<string>{},{    }};

    vector <vector <string>> all;
    vector <string> current;
    for (size_t i = 0; i < tokens.size(); i++) {
        if (tokens[i] == "|") {
            all.push_back(current);
            current = {};
        } else {
            current.push_back(tokens[i]);
        }
    }
    all.push_back(current);

    return all;
}


// функция помощник которая делает execvp приводя типы string и vector <string> к нужным
void myexecvp(vector<string> cmd_with_args) {
    char* cmd = convert_string_to_char_pointer (cmd_with_args[0]);
    char** argv = (char**) calloc (cmd_with_args.size() + 1, sizeof(char*));
    for (size_t i = 0; i < cmd_with_args.size(); i++) {
        argv[i] = convert_string_to_char_pointer(cmd_with_args[i]);
    }
    execvp(cmd, argv);
    perror("ошибка исполнения execvp");
    exit(1);
}


// возвращает pid дочернего процесса
pid_t execute_component(int* input_fifo, int* output_fifo, vector<string> component) {
    pid_t pid = fork();
    if (pid < 0) {
        perror("ошибка в fork");
        exit(1);
    }
    if (pid > 0) {
        // родительский процесс - возвращает pid дочернего процесса
        return pid;
    }
    if (input_fifo != NULL) {
        dup_and_check(input_fifo[0], STDIN_FILENO);
        fifo_close_and_free(input_fifo);
    }
    if (output_fifo != NULL) {
        dup_and_check(output_fifo[1], STDOUT_FILENO);
        fifo_close_and_free(output_fifo);
    }

    string in_filename;
    string out_filename;
    vector<string> cmd_with_args;
    size_t i = 0;
    while (i < component.size()) {
        // после > или < обязательно должно быть имя файла
        if ((component[i] == ">" || component[i] == "<") && i == component.size() - 1) {
            perror("ошибка парсинга");
            exit(1);
        }
        if (component[i] == ">") {
            out_filename = component[i + 1];
            i += 2;
        } else if (component[i] == "<") {
            in_filename = component[i + 1];
            i += 2;
        } else {
            cmd_with_args.push_back(component[i]);
            i++;
        }
    }

    // если есть файл на вход или на выход, то у него приоритет над каналами из конвейера
    if (in_filename != "") {
        int fd = open_and_check(in_filename, O_RDONLY);
        dup_and_check(fd, STDIN_FILENO);
        close(fd);
    }
    if (out_filename != "") {
        int fd = open_and_check(out_filename, O_WRONLY | O_CREAT | O_TRUNC);
        dup_and_check(fd, STDOUT_FILENO);
        close(fd);
    }

    // раскрываем метасимволы в команде
    cmd_with_args = get_cmd_no_metasymbols(cmd_with_args);

    if (cmd_with_args[0] == "pwd") {
        cout << get_current_working_dir() << endl;
        // завершаем дочерний процесс.
        exit(0);
    } else if (cmd_with_args[0] == "cd") {
        // только поменяет директорию в родительском процессе
        // но возможно что-то напечатает.
        cd_command(cmd_with_args);
        // завершаем дочерний процесс.
        exit(0);
    } else {
        myexecvp(cmd_with_args);
    }

    return 0;
}


void signalHandler(int signum) {
    catch_ctrl_C = true;
}


int main() {
    // ловим ctrl C
    signal(SIGINT, signalHandler);

    // while (cin) ловит ctrl D
    // интерпретация (считывание команд, их определение и исполнение)
    while (cin) {
        // обновляем
        catch_ctrl_C = false;

        // предлагаем ввод
        // если user id 0 - то есть привилегии
        if (getuid() == 0) {
            cout << get_current_working_dir() << " ! : ";
        } else {
            cout << get_current_working_dir() << " > : ";
        }

        // чтение и парсинг
        vector<vector<string>> components = read_conveyor_components();

        // настоящая консоль меняет директории только если исполняется без pipe
        // в случае с pipe директория поменяется только для дочернего процесса.
        // Для того чтобы сделать так же, если нету pipe мы делаем cd напрямую в родительском процессе
        if (components.size() == 1 && components[0].size() > 0 && components[0][0] == "cd") {
            cd_command(components[0]);
            continue;
        }

        // реализация time
        bool print_time = false;
        // для real time
        auto clock_start = high_resolution_clock::now();
        // до исполнения команды
        // tms_cutime и tms_cstimels содержит кумулятивное user time и system time дестких процессов
        struct tms time_before;
        times(&time_before);

        // на каждый элемент конвейера будет создан один дочерний процесс
        vector <pid_t> child_pids;

        // каналы чтобы связать элементы конвейера
        int *fifo_prev = NULL;
        int *fifo_next = NULL;
        for (size_t i = 0; i < components.size(); i++) {
            if (fifo_prev != NULL) {
                fifo_close_and_free(fifo_prev);
            }
            fifo_prev = fifo_next;
            if (size_t(i) == components.size() - 1) {
                fifo_next = NULL;
            } else {
                fifo_next = (int *) calloc(2, sizeof(int));
                pipe(fifo_next);
            }

            // если для какого-то элемента конвеера был вызван time
            if (components[i].size() > 0 && components[i][0] == "time") {
                // имеет смысл печатать время только один раз.
                print_time = true;
                components[i].erase(components[i].begin());
            }

            // внутри делает форк и запускает дочерний процесс в отдельном контейнере.
            pid_t child = execute_component(fifo_prev, fifo_next, components[i]);
            child_pids.push_back(child);
        }
        if (fifo_prev != NULL) {
            fifo_close_and_free(fifo_prev);
        }

        // ждем пока все дочерние процессы завершаться или пока не будет введен сигнал ctrl C
        int status;
        // wait возвращает > 0 если один из детей вернулся.
        // когда все дети вернулись, wait вернет -1 и цикл закончится
        while (catch_ctrl_C == false && wait(&status) > 0);
        if (catch_ctrl_C) {
            cout << "процесс был завершен с помощью Ctrl C" << endl;
            for (int child : child_pids) {
                kill(child, SIGKILL);
            }
        }

        // после завершения исполнения конвеера
        auto clock_end = high_resolution_clock::now();
        // Вычислим разницу и получим user и system time если был введен time
        struct tms time_after;
        times(&time_after);

        if (print_time) {
            milliseconds d = duration_cast <milliseconds> (duration <double> (clock_end - clock_start));
            auto user_time = time_after.tms_cutime - time_before.tms_cutime;
            auto sys_time = time_after.tms_cstime - time_before.tms_cstime;
            cerr << "real time: " + to_string(d.count()) + "ms" << endl;
            cerr << "user time: " + to_string(user_time) + "ms" << endl;
            cerr << "sys time: " + to_string(sys_time) + "ms" << endl;
        }
    }

    return 0;
}
