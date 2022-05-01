/*** includes ***/
#define _DEFAULT_SOURCE
#define _BSD_SOURCE
// #define _GNU_SOURCE
#include <cctype>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>
#include <iostream>
#include <vector>
#include <fcntl.h>
#include <string>


using namespace std;

/*** defines ***/
#define CTRL_KEY(k) ((k)&0x1f)
#define KILO_VERSION "1.0.0"

enum editorKey {
    ARROW_LEFT = 1000,
    ARROW_RIGHT,
    ARROW_UP,
    ARROW_DOWN
};

/*** data ***/

vector<vector<char>> original_data;

vector<string> faculty;
vector<string> student;
vector<vector<char>> data;
vector<string> sum;
int num_rows;
int num_cols;

typedef struct erow {
    int size;
    char *chars;
} erow;

struct editorConfig {
    int cx, cy;
    int screenrows;
    int screencols;
    int numrows;
    erow *row;
    struct termios orig_termios;
};

struct editorConfig E;
string username = "admin";

/*** terminal ***/
void die(const char *s) {
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);
    perror(s);
    exit(1);
}

void disableRawMode() {
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) == -1) {
        die("tcsetattr");
    }
}

void find_sum(vector<string>& sum, vector<vector<char>>& data) {
    for (int i = 0; i < data.size(); i++) {
        int total = 0;
        for (int j = 0; j < data[0].size(); j++) {
            if (data[i][j] != ' '){
                total += (data[i][j] - '0');
            }
        }
        if(total < 10)  
            sum[i] = "0" + to_string(total);
        else
            sum[i] = to_string(total);
    }
}


void enableRawMode() {
    if (tcgetattr(STDIN_FILENO, &E.orig_termios) == -1) {
        die("tcgetattr");
    }
    atexit(disableRawMode);

    struct termios raw = E.orig_termios;
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag &= (CS8);
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) {
        die("tcsetattr");
    }
}

int editorReadKey() {
    // int nread;
    // char c;
    // while ((nread = read(STDIN_FILENO, &c, 1)) != 1)
    // {
    //         if (nread == -1 && errno != EAGAIN)
    //         {
    //                 die("read");
    //         }
    // }
    // return c;

    int nread;
    char c;
    while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
        if (nread == -1 && errno != EAGAIN)
            die("read");
    }
    if (c == '\x1b') {
        char seq[3];
        if (read(STDIN_FILENO, &seq[0], 1) != 1)
            return '\x1b';
        if (read(STDIN_FILENO, &seq[1], 1) != 1)
            return '\x1b';
        if (seq[0] == '[') {
            switch (seq[1]) {
                case 'A':
                    return ARROW_UP;
                case 'B':
                    return ARROW_DOWN;
                case 'C':
                    return ARROW_RIGHT;
                case 'D':
                    return ARROW_LEFT;
            }
        }
        return '\x1b';
    } else {
        return c;
    }
}

int getWindowSize(int *rows, int *cols) {
    struct winsize ws;

    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
        return -1;
    } else {
        *cols = ws.ws_col;
        *rows = ws.ws_row;
        return 0;
    }
}

/*** row operations ***/

void editorAppendRow(char *s, size_t len) {
    E.row = (erow *)realloc(E.row, sizeof(erow) * (E.numrows + 1));
    int at = E.numrows;
    E.row[at].size = len;
    E.row[at].chars = (char *)malloc(len + 1);
    memcpy(E.row[at].chars, s, len);
    E.row[at].chars[len] = '\0';
    ++E.numrows;
}

// void editorRowInsertChar(erow *row, int at, int c) {
//     if (at < 0 || at > row->size) {
//         at = row->size;
//     }
//     row->chars = realloc(row->chars, row->size + 2);
//     memmove(&row->chars[at + 1], &row->chars[at], row->size - at + 1);
//     row->size++;
//     row->chars[at] = c;
//     //        editorUpdateRow(row);
// }

void editorInsertChar(int c) {
    data[E.cy / 4 - 1][E.cx / 6 - 1] = c;
}

/*** file i/o ***/
void editorOpen(char *filename) {
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        die("fopen");
    }

    char *line = NULL;
    size_t linecap = 0;
    ssize_t linelen;
    while ((linelen = getline(&line, &linecap, fp)) != -1) {
        while (linelen > 0 && (line[linelen - 1] == '\n' || line[linelen - 1] == '\r')) {
            --linelen;
        }
        editorAppendRow(line, linelen);
    }
    free(line);
    fclose(fp);
}

/*** append buffer ***/
struct abuf {
    char *b;
    int len;
};

#define ABUF_INIT \
    {             \
        NULL, 0   \
    }

void abAppend(struct abuf *ab, const char *s, int len) {
    char *new_ = (char *)realloc(ab->b, ab->len + len);
    if (new_ == NULL) {
        return;
    }
    memcpy(&new_[ab->len], s, len);
    ab->b = new_;
    ab->len += len;
}

void abFree(struct abuf *ab) {
    free(ab->b);
}

/*** output ***/
void editorDrawRows(struct abuf *ab) {
    find_sum(sum, data);
    for (int row = 0; row <= num_rows * 4; ++row) {
        if (row % 4 == 0) {
            int len = (num_cols - 1) * 6 + 7;
            for (int col = 0; col <= len; ++col) {
                if (col % 6 == 0) {
                    if (col != len - 1)
                        abAppend(ab, "+", 1);
                    else
                        abAppend(ab, "-", 1);
                } else {
                    if (col == len)
                        abAppend(ab, "+", 1);
                    else
                        abAppend(ab, "-", 1);
                }
            }
        } else {
            int len = (num_cols - 1) * 6 + 7;
            for (int col = 0; col <= len; ++col) {
                if (col % 6 == 0) {
                    if (col == len - 1)
                        abAppend(ab, " ", 1);
                    else
                        abAppend(ab, "|", 1);
                } 
                else if (row > 2 && col == len - 4 && (row + 2) % 4 == 0) {
                    abAppend(ab, &sum[row / 4 - 1][0], 2);
                    col++;
                } 
                else if (col == 3 && (row + 2) % 4 == 0) {
                    abAppend(ab, &student[row / 4][0], 2);
                    col++;
                } 
                else if (row == 2 && (col + 3) % 6 == 0) {
                    abAppend(ab, &faculty[col / 6][0], 2);
                    col++;
                } 
                else if (row > 2 && col > 3 && (col + 3) % 6 == 0 && (row + 2) % 4 == 0) {
                    abAppend(ab, &data[row / 4 - 1][col / 6 - 1], 1);
                } 
                else {
                    if (col == len)
                        abAppend(ab, "|", 1);
                    else
                        abAppend(ab, " ", 1);
                }
            }
        }
        abAppend(ab, "\r\n", 2);
    }

    char buf[32];
    snprintf(buf, sizeof(buf), "\nS%d F%d\n", E.cy / 4, E.cx / 6);
    abAppend(ab, buf, strlen(buf));

    /*int y;
    for (y = 0; y < E.screenrows; y++) {
            if (y >= E.numrows) {
                    if (E.numrows == 0 && y == E.screenrows / 3) {
                            char welcome[80];
                            int welcomelen = snprintf(welcome, sizeof(welcome),
                            "Kilo editor -- version %s", KILO_VERSION);
                            if (welcomelen > E.screencols) welcomelen = E.screencols;
                            int padding = (E.screencols - welcomelen) / 2;
                            if (padding) {
                                    abAppend(ab, "~", 1);
                                    padding--;
                            }
                            while (padding--) abAppend(ab, " ", 1);
                            abAppend(ab, welcome, welcomelen);
                    } else {
                            abAppend(ab, "~", 1);
                    }
            } else {
                    int len = E.row[y].size;
                    if (len > E.screencols) len = E.screencols;
                    abAppend(ab, E.row[y].chars, len);
            }
            abAppend(ab, "\x1b[K", 3);
            if (y < E.screenrows - 1) {
                    abAppend(ab, "\r\n", 2);
            }
    }*/
}

void editorDrawStatusBar(struct abuf *ab) {
    abAppend(ab, "\x1b[7m", 4);
    int len = 0;
    while (len < E.screencols) {
        abAppend(ab, " ", 1);
        ++len;
    }
    abAppend(ab, "\x1b[m", 3);
}

void editorRefreshScreen() {
    struct abuf ab = ABUF_INIT;

    abAppend(&ab, "\x1b[?25l", 6);
    abAppend(&ab, "\x1b[2J", 4);
    abAppend(&ab, "\x1b[H", 3);

    editorDrawRows(&ab);
    // editorDrawStatusBar(&ab);

    char buf[32];
    snprintf(buf, sizeof(buf), "\x1b[%d;%dH", E.cy, E.cx);
    abAppend(&ab, buf, strlen(buf));

    abAppend(&ab, "\x1b[?25h", 6);

    write(STDOUT_FILENO, ab.b, ab.len);
    abFree(&ab);
}

/*** input ***/
void editorMoveCursor(int key) {
    switch (key) {
        case ARROW_LEFT:
            if ((E.cx - 12) > 0) {
                E.cx -= 6;
            }
            break;
        case ARROW_RIGHT:
            if ((E.cx + 6) < ((num_cols - 1) * 6)) {
                E.cx += 6;
            }
            break;
        case ARROW_UP:
            if ((E.cy - 8) > 0) {
                E.cy -= 4;
            }
            break;
        case ARROW_DOWN:
            if ((E.cy + 4) < (num_rows * 4)) {
                E.cy += 4;
            }
            break;
    }
}

void editMode() {
    char prevNum = data[E.cy / 4 - 1][E.cx / 6 - 1];
    editorInsertChar(' ');
    while (1) {
        editorRefreshScreen();
        char c = editorReadKey();
        if (c == 27) {
            editorInsertChar(prevNum);
            break;
        }
        if (c == '\r' && data[E.cy / 4 - 1][E.cx / 6 - 1] != ' ') {
            break;
        }
        switch (c) {
            case '0':
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
            case '9':
                editorInsertChar(c);
                break;
        }
    }
    // writeData();
}

void editorProcessKeypress() {
    int c = editorReadKey();

    switch (c) {
        case 27:
            write(STDOUT_FILENO, "\x1b[24;1H", 7);
            exit(0);
            break;
        case ARROW_DOWN:
        case ARROW_LEFT:
        case ARROW_RIGHT:
        case ARROW_UP:
            editorMoveCursor(c);
            break;
        case '\r':
            if(username[0] != 'S'){
                editMode();
            }
            break;
    }

    // move data to original data
    for (int i = 0; i < data.size(); ++i) {
        for (int j = 0; j < data[0].size(); ++j) {
            original_data[i][j] = data[i][j];
        }
    }

    // writeData();
}

/*** init ***/
void initEditor() {
    E.cx = 10;
    E.cy = 7;
    if (getWindowSize(&E.screenrows, &E.screencols) == -1) {
        die("getWindowSize");
    }
    E.numrows = 0;
    E.row = NULL;
}

void initData(){
    int fd = open("marks.csv", O_RDWR);

    char c;
    vector<char> temp;
    
    while(read(fd, &c, 1) == 1){
        if(c == ',')
            continue;
        else if(c == '\r'){
            continue;
        }
        else if(c == '\n'){
            original_data.push_back(temp);
            temp.clear();
        }
        else
            temp.push_back(c);
    }
    if(temp.size() != 0){
        original_data.push_back(temp);
    }

}

void initOtherData(){
    num_rows = data.size() + 1;
    num_cols = data[0].size() + 2;

    faculty.push_back("  ");
    for(int i=1; i<=data[0].size(); i++){
        faculty.push_back("F" + to_string(i));
    }
    faculty.push_back("  ");

    student.push_back("  ");
    for(int i=1; i<=data.size(); i++){
        student.push_back("S" + to_string(i));
        sum.push_back("00");
    }
}

void writeData(){
    int fd = open("marks.csv", O_WRONLY);
    lseek(fd, 0, SEEK_SET);
    system("echo \"\" > marks.csv");
    string s;

    for(int i=0; i<original_data.size(); i++){
        s += original_data[i][0];
        for(int j=1; j<original_data[i].size(); j++){
            s += ",";
            s += original_data[i][j];
        }
        s += "\n";
    }

    write(fd, &s[0], s.length());
}

void printData(){
    for(auto x : original_data){
        for(auto y : x){
            cout << y << " ";
        }
        cout << endl;
    }

    for(auto x : faculty)
        cout << x << " ";
    cout << endl;

    for(auto x : student)
        cout << x << " ";
    cout << endl;

    for(auto x : sum)
        cout << x << " ";
    cout << endl;
}

void printAverageMarks(){    //Average marks of class in a subject taught by a particular faculty
    int sum = 0;
    int count = data.size();
    for(int i = 0; i < data.size(); i++){
        sum += (data[i][0] - '0');
    }
    cout << "Average marsk of the class : " << sum/count << endl;
}

void printHighestAndLowestMarks(){    //Highest and lowest marks of class in a subject taught by a particular faculty
    int max = INT32_MIN;
    int min = INT32_MAX;
    for(int i = 0; i < data.size(); i++){
         if((data[i][0] - '0') > max)
            max = data[i][0] - '0';
        if((data[i][0] - '0') < min)
            min = data[i][0] - '0'; 
    }
    cout << "Highest marks : "<< max << endl;
    cout << "Lowest marks : " << min << endl;
}

int main(int argc, char *argv[]) {
    // struct passwd* userinfo = getpwuid(getuid());
    // string username = userinfo->pw_name;
    

    initData();
    // printData();

    // writeData();
    
    if(username == "admin"){
        data = original_data;
        initOtherData();

        cout << "Press 1 : View all marks\nPress 2 : Add user\nPress 3 : Add faculty" << endl;
        int choice;
        cin >> choice;

        switch(choice) {
            case 1: {
                enableRawMode();
                initEditor();
                if (argc >= 2) {
                    editorOpen(argv[1]);
                }
                while (1) {
                    editorRefreshScreen();
                    editorProcessKeypress();
                }
                break;
            }
            case 2: {
                string s;
                s = "pw useradd s"+to_string(original_data.size()+1)+" -G student";
                system(&s[0]);

                original_data.push_back(vector<char>());
                // Initialize new row with 0 marks
                for(int i=0; i<original_data[0].size(); i++){
                    original_data[original_data.size()-1].push_back('0');
                }

                writeData();
            }
            case 3: {
                string s;
                s = "pw useradd f"+to_string(original_data[0].size()+1)+" -G faculty";
                system(&s[0]);

                // Add new column in original_data
                for(int i=0; i<original_data.size(); i++){
                    original_data[i].push_back('0');
                }

                writeData();
            }
        }
        
    }

    if(username[0] == 'S'){
        int index = username[1] - '0' - 1;
        data.push_back(original_data[index]);
    }

    if (username[0] == 'F') {
        // Initialize data array
        int num_students = original_data.size();
        int num_faculty = original_data[0].size();

        int index = username[1] - '0' - 1;
        for (int i = 0; i < num_students; i++) {
            vector<char> temp;
            temp.push_back(original_data[i][index]);
            data.push_back(temp);
        }
        
        initOtherData();
        faculty[1][1] = index + '0' + 1;    

        cout << "Press 1 : Marks of the class\nPress 2 : Average marks of the class\nPress 3 : Highest and Lowest marks of the class" << endl;
        int c;
        cin >> c;
        switch (c) {
            case 1:
                enableRawMode();
                initEditor();
                if (argc >= 2) {
                    editorOpen(argv[1]);
                }
                while (1) {
                    editorRefreshScreen();
                    editorProcessKeypress();
                }
                break;
            case 2:
                printAverageMarks();
                break;
            case 3:
                printHighestAndLowestMarks();
                break;
            default:
                cout << "Invalid Input !" << endl;
        }

    }
    
    return 0;
}


// user
// read, write

