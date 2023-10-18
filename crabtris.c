#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#define ESC "\x1B["

#define HIGHT 10       // y i
#define WIDTH 25       // x j
int map[HIGHT][WIDTH]; // map[y][x] 表示坐标点 (x, y)

typedef int16_t Coordinate;
typedef struct COORD {
    Coordinate Y;
    Coordinate X;
} COORD;

COORD faller[4];
COORD next_faller[4];

short o_shape[8] = {4, 3, 5, 3, 4, 4, 5, 4};
short j_shape[8] = {4, 4, 5, 4, 6, 4, 4, 3};
short l_shape[8] = {5, 4, 4, 4, 3, 4, 5, 3};
short t_shape[8] = {5, 4, 4, 4, 6, 4, 5, 3};
short i_shape[8] = {4, 4, 5, 4, 3, 4, 6, 4};
short s_shape[8] = {4, 3, 5, 3, 4, 2, 5, 4};
short z_shape[8] = {4, 3, 5, 3, 5, 2, 4, 4};

short shape[7][8] = {
    {4, 3, 5, 3, 4, 4, 5, 4}, {4, 4, 5, 4, 6, 4, 4, 3},
    {5, 4, 4, 4, 3, 4, 5, 3}, {5, 4, 4, 4, 6, 4, 5, 3},
    {4, 4, 5, 4, 3, 4, 6, 4}, {4, 3, 5, 3, 4, 2, 5, 4},
    {4, 3, 5, 3, 5, 2, 4, 4},
};

typedef enum SHAPE_TYPE {
    O_SHAPE,
    J_SHAPE,
    L_SHAPE,
    T_SHAPE,
    I_SHAPE,
    S_SHAPE,
    Z_SHAPE
} SHAPE_TYPE;

SHAPE_TYPE now_shape;
typedef enum ELEMENT { AIR, BLOCK, MOVING } ELEMENT;

#define TICK 1000 * 1000    // 1 seconds
#define COOLDOWN 200 * 1000 // 0.2 seconds
int keyboard_flag;
unsigned long start_time;

static int old_fcntl;
static struct termios term;
// static struct winsize console_size;

int is_legal(COORD test[4]) // 1为合法 0为不合法
{
    for (int i = 0; i < 4; i++) {
        if (test[i].Y < 0 || test[i].Y >= HIGHT)
            return 0;
        if (test[i].X >= WIDTH)
            return 0;
    }
    for (int i = 0; i < 4; i++) {
        if (map[test[i].Y][test[i].X] == BLOCK)
            return 0;
    }
    return 1;
}

void generate() {
    now_shape = rand() % 7;
    memcpy(&faller, shape[now_shape], 4 * sizeof(COORD));
    for (int i = 0; i < 4; i++)
        map[faller[i].Y][faller[i].X] = MOVING;
}

void set_cursor_absolute_position(Coordinate x, Coordinate y) {
    // POSIX控制台坐标从1开始
    printf(ESC "%d;%dH", y + 1, x + 1);
}

void print_map() {
    printf("\t\t\t  俄罗斯方蟹\n"
           "\t  ______________________________________________\n");
    for (int i = 0; i < HIGHT; i++) {
        printf("\t<<|");
        for (int j = 3; j < WIDTH; j++) {
            if (map[i][j] == AIR) {
                printf(". ");
            } else {
                printf("██");
            }
        }
        printf("|>>\n");
    }
    printf("\t <----------------①----------①----------------->\n"
           "\t    /                                        \\\n"
           "\t   /                                          \\\n"
           "\t   \\                                          /\n"
           "\t    \\                                        /\n"
           "\t    /\\                                      /\\\n"
           "\n\th/r/R/H: 转(↻↺)\t\t\t开始没得选择\n"
           "\tSPACE: 最右(→→)\t\t\t游戏不能暂停\n"
           "\tl/L: 右(→)\t\t\t结束只能Ctrl+c\n"
           "\tk/K: 上(↑)\t\t\t不计分数，看能玩多久\n"
           "\tj/J: 下(↓)\t\t\t蟹如人生矣，戏T!\n");
}

void restore_console(void) {
    fcntl(STDIN_FILENO, F_SETFL, old_fcntl);
    tcsetattr(STDIN_FILENO, TCSANOW, &term);
    printf(ESC "?25h");
    // 到窗口左下角去，但要必须要输出一个换行才能恢复输出文本属性
    printf(ESC "0m\n");
}

// 被杀死前，先恢复控制台
void signal_kill(int sig) {
    unsigned long now = clock();
    printf("\t\t\t你能玩(花费了) %f 秒!!!\n",
           (now - start_time) / 1000.0 / 1000.0);
    restore_console();
    memset(map, 0, WIDTH * HIGHT * sizeof(int));
    exit(0);
}

int try_move_right() {
    memcpy(next_faller, faller, sizeof(faller));
    for (int i = 0; i < 4; i++) {
        next_faller[i].X++;
        if (next_faller[i].X >= WIDTH)
            return -1;
        if (map[next_faller[i].Y][next_faller[i].X] == BLOCK)
            return -1;
    }
    for (int i = 0; i < 4; i++)
        map[faller[i].Y][faller[i].X] = AIR;
    memcpy(faller, next_faller, sizeof(faller));
    for (int i = 0; i < 4; i++)
        map[faller[i].Y][faller[i].X] = MOVING;
    return 0;
}

void clear_row() {
    int flag;
    for (int i = WIDTH - 1; i >= 3; i--) {
        flag = 1;
        for (int j = 0; j < HIGHT; j++) {
            if (map[j][i] != BLOCK) {
                flag = 0;
                break;
            }
        }
        if (flag) {
            for (int y = 0; y < HIGHT; y++) {
                for (int x = i; x > 0; x--) {
                    // 当前列等于前一列，即清行
                    map[y][x] = map[y][x - 1];
                }
                continue;
            }
            i++;
        }
    }
}

// 向下输入1，向上输入-1
int try_move_vertical(int direction) {
    memcpy(next_faller, faller, sizeof(faller));
    for (int i = 0; i < 4; i++) {
        next_faller[i].Y += direction;
        if (next_faller[i].Y >= HIGHT || next_faller[i].Y < 0)
            return -1;
        if (map[next_faller[i].Y][next_faller[i].X] == BLOCK)
            return -1;
    }
    keyboard_flag = 1;
    for (int i = 0; i < 4; i++)
        map[faller[i].Y][faller[i].X] = AIR;
    memcpy(faller, next_faller, sizeof(faller));
    for (int i = 0; i < 4; i++)
        map[faller[i].Y][faller[i].X] = MOVING;
    return 0;
}

int try_goto_last_right() {
    int result = -1;
    while (try_move_right() == 0)
        result = 0;
    return result;
}

// 以中心点旋转
// 以所有点旋转后向下
// 以其他点旋转
int t_spin(int direction) {
    for (int j = 0; j < 4; j++) {
        next_faller[j].X =
            faller[0].X + direction * (faller[0].Y - faller[j].Y);
        next_faller[j].Y =
            faller[0].Y + direction * (faller[j].X - faller[0].X);
    }
    if (is_legal(next_faller)) {
        for (int i = 0; i < 4; i++)
            map[faller[i].Y][faller[i].X] = AIR;
        memcpy(faller, next_faller, sizeof(faller));
        for (int i = 0; i < 4; i++)
            map[faller[i].Y][faller[i].X] = MOVING;
        return 0;
    }

    for (int round = 1; round >= 0; round--) {
        for (int i = 0; i < 4; i++) {
            for (int j = 0; j < 4; j++) {
                next_faller[j].X =
                    faller[i].X + direction * (faller[i].Y - faller[j].Y);
                next_faller[j].Y = faller[i].Y +
                                   direction * (faller[j].X - faller[i].X) +
                                   round;
            }
            if (is_legal(next_faller)) {
                for (int i = 0; i < 4; i++)
                    map[faller[i].Y][faller[i].X] = AIR;
                memcpy(faller, next_faller, sizeof(faller));
                for (int i = 0; i < 4; i++)
                    map[faller[i].Y][faller[i].X] = MOVING;
                return 0;
            }
        }
    }
    return -1;
}

// 以中心点旋转
// 以其他点旋转
// 检查以中心点旋转后能否向下移动一格
// 检查以其他点旋转后能否向下移动一格
int try_rotate(int direction) {
    if (now_shape == O_SHAPE)
        return -1;
    if (now_shape == T_SHAPE)
        return t_spin(direction);
    for (int round = 0; round <= 1; round++) {
        for (int i = 0; i < 4; i++) {
            for (int j = 0; j < 4; j++) {
                next_faller[j].X =
                    faller[i].X + direction * (faller[i].Y - faller[j].Y);
                next_faller[j].Y = faller[i].Y +
                                   direction * (faller[j].X - faller[i].X) +
                                   round;
            }
            if (is_legal(next_faller)) {
                for (int i = 0; i < 4; i++)
                    map[faller[i].Y][faller[i].X] = AIR;
                memcpy(faller, next_faller, sizeof(faller));
                for (int i = 0; i < 4; i++)
                    map[faller[i].Y][faller[i].X] = MOVING;
                return 0;
            }
        }
    }
    return -1;
}

int main(int argc, char *argv[]) {
    // 信号处理
    signal(SIGABRT, signal_kill);
    signal(SIGINT, signal_kill);
    signal(SIGTERM, signal_kill);

    tcgetattr(STDIN_FILENO, &term);
    struct termios t = term;
    t.c_lflag &= ~(ECHO | ICANON);
    tcsetattr(STDIN_FILENO, TCSANOW, &t);

    // 无阻塞
    old_fcntl = fcntl(STDIN_FILENO, F_GETFL);
    fcntl(STDIN_FILENO, F_SETFL, old_fcntl | O_NONBLOCK);

    // 无光标
    printf(ESC "?25l");
    // POSIX清屏
    printf(ESC "2J");
    // 获取控制台大小
    // ioctl(STDOUT_FILENO, TIOCGWINSZ, &console_size);
    set_cursor_absolute_position(0, 0);

    memset(map, 0, WIDTH * HIGHT * sizeof(int));
    srand(time(NULL));
    generate();
    print_map();

    unsigned long last = clock();
    start_time = last;
    char c;
    int fd = fileno(stdin);
    keyboard_flag = 0;
    while (1) {
        unsigned long now = clock();
        // 说明该向下移动了
        if (keyboard_flag) {
            keyboard_flag = 0;
            last = clock() + COOLDOWN;
        }
        if (now > last) {
            last += TICK;
            if (try_move_right() == -1) {
                for (int i = 0; i < 4; i++)
                    map[faller[i].Y][faller[i].X] = BLOCK;
                clear_row();
                generate();
            }
            set_cursor_absolute_position(0, 0);
            print_map();
        }
        // 非阻塞读取输入
        ssize_t bytesRead = read(fd, &c, sizeof(c));
        if (bytesRead > 0) {
            // 在这里添加其他按键的处理逻辑
            switch (c) {
            case 'R':
            case 'r':
                if (try_rotate(1) == 0)
                    keyboard_flag = 1;
                break;
            case 'H':
            case 'h':
                if (try_rotate(-1) == 0)
                    keyboard_flag = 1;
                break;
            case 'L':
            case 'l':
                if (try_move_right() == 0)
                    keyboard_flag = 1;
                break;
            case ' ':
                if (try_goto_last_right() == 0)
                    keyboard_flag = 1;
                break;
            case 'J':
            case 'j':
                if (try_move_vertical(1) == 0)
                    keyboard_flag = 1;
                break;
            case 'K':
            case 'k':
                if (try_move_vertical(-1) == 0)
                    keyboard_flag = 1;
                break;
            }
            set_cursor_absolute_position(0, 0);
            print_map();
        }
    }

    return EXIT_SUCCESS;
}
