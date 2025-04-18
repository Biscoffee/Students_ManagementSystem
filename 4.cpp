#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <termios.h>  
#include <unistd.h>   
#include <errno.h>
#include <limits.h>
#include <stdlib.h>

#define ADMIN_KEY "iamadmin"
#define TEACHER_KEY "iamteacher"
#define CLEAR_SCREEN system("clear")  
#define MAX_ACCOUNTS 100
#define MAX_STR_LEN 20
#define MIN_SCORE 0
#define MAX_SCORE 100
#define _POSIX_C_SOURCE 200809L
#define MAX_APPEALS 50

typedef struct {
    char username[MAX_STR_LEN];
    char password[MAX_STR_LEN];
    char role[MAX_STR_LEN];
    char className[MAX_STR_LEN];
    char email[MAX_STR_LEN]; 
} Account;

typedef struct {
    char username[MAX_STR_LEN];
    char className[MAX_STR_LEN];
    char reason[100];
    int status;
} Appeal;


typedef struct Student {
    char username[MAX_STR_LEN];
    char name[MAX_STR_LEN];
    char className[MAX_STR_LEN];
    int score;
    struct Student* prev;
    struct Student* next;
} Student;

Appeal appeals[MAX_APPEALS];  // 申诉记录数组
int appealCount = 0;          // 申诉计数器

Account accounts[MAX_ACCOUNTS];
int accountCount = 0;

Student* head = NULL;
Student* tail = NULL;

Student* findStudentByUsername(const char* username);
void saveStudentsToFile();
void loadStudentsFromFile();
void safeInput(char* buffer, int maxLen, const char* prompt);
int safeInputInt(const char* prompt, int min, int max);
void safeInputPassword(char* buffer, int maxLen, const char* prompt);
void analyzeClassScore(Student* s);
void submitAppeal(Account* acc);
void handleAppeals(Account* currentAccount);

void safeInput(char* buffer, int maxLen, const char* prompt) {
    memset(buffer, 0, maxLen);
    printf("%s", prompt);
    fflush(stdout);

    if(!fgets(buffer, maxLen, stdin)) {
        buffer[0] = '\0';
        return;
    }
    char* pos = strpbrk(buffer, "\r\n");
    if(pos) *pos = '\0';

    if (!pos) { 
        int ch;
        while((ch = getchar()) != '\n' && ch != EOF);
    }
}

int validateEmailEx(const char* email) {
    const char* at = strchr(email, '@');
    if(!at || at == email) return 0;       // 不能以@开头
    
    // 本地部分校验（@前部分）
    const char* p = email;
    for(; p < at; p++) {
        if(!isalnum(*p) && *p != '.' && *p != '_' && *p != '%' && *p != '+') {
            return 0;
        }
    }
    
    // 域名部分校验（@后部分）
    const char* dot = strchr(at, '.');
    if(!dot || dot == at+1) return 0;      // 必须包含.且不能紧跟@
    
    const char* tld = strrchr(email, '.') + 1;
    if(strlen(tld) < 2 || strlen(tld) > 6) return 0;
    for(; *tld; tld++) {
        if(!isalpha(*tld)) return 0;  // TLD只能是字母
    }
    
    return 1;
}


void safeInputWithCheck(char* buffer, int maxLen, const char* prompt, int (*validator)(const char*)) {
    while(1) {
        safeInput(buffer, maxLen, prompt);
        
        // 空输入检查
        if(strlen(buffer) == 0) {
            printf("输入不能为空！\n");
            continue;
        }

        if(validator && !validator(buffer)) {
            printf("输入格式不正确！\n");
            continue;
        }
        
        break;
    }
}

int validateClassName(const char* input) {
    // 基础格式校验
    if (strlen(input) < 6 || strlen(input) > MAX_STR_LEN) return 0; // 最小长度 2023_1 (6字符)
    
    // 分割年份与班级部分
    const char* underscore = strchr(input, '_');
    if (!underscore || (underscore - input) != 4) return 0; // 必须包含且仅有一个下划线，且在第5位

    // 校验年份部分（YYYY）
    char year[5];
    strncpy(year, input, 4);
    year[4] = '\0';
    for(int i=0; i<4; i++) {
        if(!isdigit(year[i])) return 0; // 必须全数字
    }
    int yearNum = atoi(year);
    if(yearNum < 2000 || yearNum > 2100) return 0; // 年份范围控制

    // 校验班级编号部分（C）
    const char* classNum = underscore + 1;
    if(strlen(classNum) == 0) return 0; // 禁止空班级号
    for(; *classNum; classNum++) {
        if(!isdigit(*classNum)) return 0; // 必须全数字
    }
    int classId = atoi(underscore + 1);
    if(classId < 1 || classId > 99) return 0; // 班级号1-99

    return 1;
}

int validateUsername(const char* input) {
    // 允许字母开头，包含字母、数字、下划线
    if(strlen(input) < 4 || strlen(input) > MAX_STR_LEN) return 0;
    if(!isalpha(input[0])) return 0;
    
    for(; *input; input++) {
        if(!isalnum(*input) && *input != '_') {
            return 0;
        }
    }
    return 1;
}

int validateEmail(const char* email) {
    const char* at = strchr(email, '@');
    if(!at || at == email) return 0;       // 不能以@开头
    const char* dot = strchr(at, '.');
    if(!dot || dot == at+1) return 0;      // .不能在@后第一位
    return (strcspn(email, " \t\n\r") == strlen(email)); // 禁止空格
}

void validateDataConsistency() {
    // 检查所有学生都有对应账号
    Student* cur = head;
    while(cur) {
        int found = 0;
        for(int i=0; i<accountCount; i++) {
            if(strcmp(accounts[i].username, cur->username) == 0) {
                found = 1;
                break;
            }
        }
        if(!found) {
            printf("发现孤立学生记录：%s\n", cur->username);
        }
        cur = cur->next;
    }
    
    // 检查所有学生账号都有对应记录
    for(int i=0; i<accountCount; i++) {
        if(strcmp(accounts[i].role, "student") == 0 && 
           !findStudentByUsername(accounts[i].username)) 
        {
            printf("发现孤立学生账号：%s\n", accounts[i].username);
        }
    }
}

void saveAppeals() {
    FILE* fp = fopen("appeals.txt", "w");
    if (!fp) return;
    
    for (int i = 0; i < appealCount; i++) {
        // 使用 | 分隔字段，确保包含空格的reason能正确解析
        fprintf(fp, "%s|%s|%s|%d\n", 
                appeals[i].username,
                appeals[i].className,
                appeals[i].reason,
                appeals[i].status);
    }
    fclose(fp);
}

void generateRandomPassword(char* password, int length) {
    const char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    
    // 确保至少一个字母和一个数字
    password[0] = charset[rand() % 52];      // 字母
    password[1] = charset[52 + rand() % 10]; // 数字
    
    // 填充剩余字符
    for(int i=2; i<length; i++) {
        password[i] = charset[rand() % (sizeof(charset)-1)];
    }
    
    // 打乱顺序增强随机性
    for(int i=0; i<length; i++) {
        int j = rand() % length;
        char temp = password[i];
        password[i] = password[j];
        password[j] = temp;
    }
    password[length] = '\0';
}

void analyzeClassScore(Student* s) {
    if (!head) {
        printf("暂无班级数据！\n");
        return;
    }
    
    int total = 0, count = 0;
    int currentScore = s->score;
    int higherCount = 0;
    Student* cur = head;
    
    printf("\n=== 班级 %s 成绩分析 ===\n", s->className);
    while(cur) {
        if(strcmp(cur->className, s->className) == 0) {
            total += cur->score;
            count++;
            if(cur->score > currentScore) higherCount++;
        }
        cur = cur->next;
    }
    
    if(count == 0) return;
    
    printf("你的成绩: %d\n", currentScore);
    printf("班级平均分: %.1f\n", (float)total/count);
    printf("班级排名: %d/%d\n", higherCount+1, count);
}

void submitAppeal(Account* acc) {
    if(appealCount >= MAX_APPEALS) {
        printf("申诉数量已达上限！\n");
        return;
    }
    
    Student* s = findStudentByUsername(acc->username);
    if(!s) {
        printf("未找到学生信息！\n");
        return;
    }
    
    printf("\n=== 提交申诉 ===\n");
    printf("当前成绩: %d\n", s->score);
    
    char reason[100];
    safeInput(reason, sizeof(reason), "请输入申诉理由: ");
    
    strcpy(appeals[appealCount].username, acc->username);
    strcpy(appeals[appealCount].className, s->className);
    strcpy(appeals[appealCount].reason, reason);
    appeals[appealCount].status = 0;
    appealCount++;
    
    saveAppeals();
    printf("申诉已提交，等待管理员处理！\n");
}

void handleAppeals(Account* currentAccount) {
    int hasPending = 0;
    printf("\n=== 待处理申诉 ===\n");
    
    for(int i=0; i<appealCount; i++) {
        if(appeals[i].status == 0) {
            hasPending = 1;
            printf("[%d] 班级:%s 学生:%s\n理由: %s\n", 
                   i+1, 
                   appeals[i].className,
                   appeals[i].username,
                   appeals[i].reason);
        }
    }
    
    if(!hasPending) {
        printf("暂无待处理申诉！\n");
        return;
    }
    
    int choice = safeInputInt("请输入要处理的申诉编号(0取消): ", 0, appealCount);
    if(choice == 0) return;
    
    choice--; // 转换为数组索引
    if(choice <0 || choice >= appealCount) {
        printf("无效编号！\n");
        return;
    }
    
    printf("\n1. 通过申诉\n2. 拒绝申诉\n");
    int action = safeInputInt("请选择处理方式: ", 1, 2);
    
    if(action == 1) {
        Student* s = findStudentByUsername(appeals[choice].username);
        // 添加空指针检查
        if (currentAccount && 
            strcmp(currentAccount->role, "teacher") == 0 && 
            strcmp(s->className, currentAccount->className) != 0) 
        {
            printf("❌ 无权修改其他班级学生成绩！\n");
            return;
        }
        if(s) {
            int newScore = safeInputInt("请输入修正后的成绩: ", MIN_SCORE, MAX_SCORE);
            s->score = newScore;
            saveStudentsToFile();
        } else {
            printf("学生不存在，自动拒绝申诉！\n");
            appeals[choice].status = 1; // 标记为已处理
        }
    }
    
    appeals[choice].status = 1;
    saveAppeals();
    printf("申诉已处理！\n");
}

void safeInputPassword(char* buffer, int maxLen, const char* prompt) {
    struct termios oldt, newt;
    static time_t lock_time = 0;  // 冻结时间戳
    const int lock_duration = 30; // 冻结时间（秒）
    int attempts = 0;
    const int max_attempts = 3;

    // 检查冻结状态
    if (time(NULL) - lock_time < lock_duration) {
        int remain = lock_duration - (time(NULL) - lock_time);
        printf("账户已被冻结，请等待%d秒后重试\n", remain);
        while (time(NULL) - lock_time < lock_duration) {
            sleep(1);
        }
    }

    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ECHO | ICANON);

    while(1) {
        printf("%s", prompt);
        fflush(stdout);
        
        int pos = 0;
        int ch;
        memset(buffer, 0, maxLen);
        
        tcsetattr(STDIN_FILENO, TCSANOW, &newt);
        while((ch = getchar()) != '\n' && ch != EOF) {
            if(ch == 127 || ch == 8) { // 退格处理
                if(pos > 0) {
                    pos--;
                    printf("\b \b");
                    fflush(stdout);
                }
            }
            else if(isprint(ch)) {
                if(pos < maxLen-1) {
                    buffer[pos++] = ch;
                    printf("*");
                    fflush(stdout);
                }
                else {
                    printf("\n警告：已达最大输入长度\n");
                    break;
                }
            }
        }
        tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
        printf("\n");

        // 复杂度验证
        int has_alpha = 0, has_digit = 0;
        for(int i=0; buffer[i]; i++){
            if(isalpha(buffer[i])) has_alpha = 1;
            else if(isdigit(buffer[i])) has_digit = 1;
        }

        if(strlen(buffer) < 8) {
            printf("密码需要至少8个字符\n");
        } 
        else if(!has_alpha) {
            printf("必须包含字母\n");
        }
        else if(!has_digit) {
            printf("必须包含数字\n");
        }
        else {
            lock_time = 0; // 重置冻结状态
            break;
        }
        
        if(++attempts >= max_attempts) {
            lock_time = time(NULL); // 记录冻结开始时间
            printf("尝试次数过多，账户将被冻结%d秒\n", lock_duration);
            return;
        }
    }
}

int safeInputInt(const char* prompt, int min, int max) {
    char input[32];
    long value;
    char* endptr;

    while(1) {
        safeInput(input, sizeof(input), prompt);
        
        if(strlen(input) == 0) {
            printf("输入不能为空\n");
            continue;
        }

        errno = 0; 
        value = strtol(input, &endptr, 10);
        
        // 错误检测
        if(*endptr != '\0') {
            printf("包含非数字字符: %s\n", endptr);
        } else if(errno == ERANGE) {  
            printf("数值超出范围\n");
        } else if(value < min || value > max) {
            printf("请输入%d~%d之间的值\n", min, max);
        } else {
            return (int)value;
        }
    }
}

void loadAccounts() {
    FILE* fp = fopen("accounts.txt", "r");
    if (!fp) return;
    
    char line[256];
    while (fgets(line, sizeof(line), fp) && accountCount < MAX_ACCOUNTS) {
        char* username = strtok(line, "|");
        char* password = strtok(NULL, "|");
        char* role = strtok(NULL, "|");
        char* className = strtok(NULL, "|");
        char* email = strtok(NULL, "\n");

        if (!username || !password || !role || !className || !email) {
            printf("发现不完整账户记录，已跳过\n");
            continue;
        }

        if (strcmp(role, "student") != 0 && 
            strcmp(role, "teacher") != 0 &&
            strcmp(role, "admin") != 0) 
        {
            printf("发现无效角色账户：%s，已跳过\n", username);
            continue;
        }

        strncpy(accounts[accountCount].username, username, MAX_STR_LEN);
        strncpy(accounts[accountCount].password, password, MAX_STR_LEN);
        strncpy(accounts[accountCount].role, role, MAX_STR_LEN);
        strncpy(accounts[accountCount].className, 
               (strcmp(role, "admin") == 0) ? "ADMIN_CLASS" : className, 
               MAX_STR_LEN);
        strncpy(accounts[accountCount].email, email, MAX_STR_LEN);
        accountCount++;
    }
    fclose(fp);
}

void saveAccounts() {
    FILE* fp = fopen("accounts.txt", "w");
    if (!fp) return;
    
    for (int i = 0; i < accountCount; i++) {
        fprintf(fp, "%s|%s|%s|%s|%s\n", 
                accounts[i].username, 
                accounts[i].password, 
                accounts[i].role,
                accounts[i].className,
                accounts[i].email); 
    }
    fclose(fp);
}

void registerAccount() {
    CLEAR_SCREEN;
    if (accountCount >= MAX_ACCOUNTS) {
        printf("系统用户已达上限！\n");
        return;
    }

    Account acc;
    safeInputWithCheck(acc.username, MAX_STR_LEN, "请输入用户名(4-20位字母数字): ", validateUsername);
    
    for (int i = 0; i < accountCount; i++) {
        if (strcmp(accounts[i].username, acc.username) == 0) {
            printf("用户名已存在！\n");
            return;
        }
    }

    do {
        safeInputPassword(acc.password, MAX_STR_LEN, "请输入密码(至少8位，含字母和数字): ");
    } while(strlen(acc.password)<8 || !(strpbrk(acc.password, "0123456789") && strpbrk(acc.password, "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ")));

        do {
        safeInputWithCheck(acc.email, MAX_STR_LEN, "请输入有效邮箱: ", validateEmailEx);
    } while(0);



     while (1) {
        CLEAR_SCREEN;
        safeInputWithCheck(acc.role, MAX_STR_LEN, "请输入角色 (student/teacher/admin): ", 
            [](const char* role) -> int {
                return strcmp(role, "student")==0 || strcmp(role, "teacher")==0 || strcmp(role, "admin")==0;
            });

        // 管理员密钥验证
        if (strcmp(acc.role, "admin") == 0) {
            char key[MAX_STR_LEN];
            safeInput(key, MAX_STR_LEN, "请输入管理员注册密钥: ");
            if(strcmp(key, ADMIN_KEY) != 0) {
                printf("管理员密钥错误！\n");
                sleep(1);
                continue;
            }
            strcpy(acc.className, "ADMIN_CLASS");
            break;
        }
        
        // 教师密钥验证
        if (strcmp(acc.role, "teacher") == 0) {
            char key[MAX_STR_LEN];
            safeInput(key, MAX_STR_LEN, "请输入教师注册密钥: ");
            if(strcmp(key, TEACHER_KEY) != 0) {
                printf("教师密钥错误！\n");
                sleep(1);
                continue;
            }
            safeInputWithCheck(acc.className, MAX_STR_LEN, 
                "请输入负责班级(格式：2023_1): ", validateClassName);
            break;
        }

        // 学生注册流程
        if (strcmp(acc.role, "student") == 0) {
            safeInputWithCheck(acc.className, MAX_STR_LEN, 
                "请输入所在班级(格式：2023_1): ", validateClassName);
            break;
        }
    }
    if(strlen(acc.className)==0 && strcmp(acc.role,"admin")!=0){
        printf("班级信息异常！\n");
        return;
    }

    accounts[accountCount++] = acc;
    saveAccounts();
    printf("注册成功！\n");
}

void forgotPassword() {
    char username[MAX_STR_LEN], email[MAX_STR_LEN];
    safeInput(username, MAX_STR_LEN, "请输入用户名: ");

    Account *target = NULL;
    for (int i = 0; i < accountCount; i++) {
        if (strcmp(accounts[i].username, username) == 0) {
            target = &accounts[i];
            break;
        }
    }
    
    if (!target) {
        printf("该用户名未注册！\n");
        return;
    }

    safeInput(email, MAX_STR_LEN, "请输入注册邮箱: ");
    
    if (strlen(target->email) == 0 || strcasecmp(target->email, email) != 0) {
        printf("邮箱验证失败！\n");
        return;
    }
    
    // 生成随机验证码
    srand(time(NULL));
    int verifyCode = rand() % 9000 + 1000;
    printf("验证码已发送到 %s：%d\n", email, verifyCode);
    
    int inputCode = safeInputInt("请输入收到的验证码: ", 1000, 9999);
    if (inputCode != verifyCode) {
        printf("验证码错误！\n");
        return;
    }
    
    char newpass[MAX_STR_LEN];
    safeInputPassword(newpass, MAX_STR_LEN, "请输入新密码: ");
    strcpy(target->password, newpass);
    saveAccounts();
    printf("密码重置成功！\n");
}

void analyzeAllScores() {
    if (!head) {
        printf("没有学生记录！\n");
        return;
    }
    
    int total = 0, count = 0;
    Student* cur = head;
    printf("\n=== 全局成绩分析 ===\n");
    printf("%-10s | %-15s | %-15s | 成绩 | 柱状图\n", "班级", "用户名", "姓名");
    while (cur) {
        total += cur->score;
        count++;
        printf("%-10s | %-15s | %-15s | %3d | ", 
               cur->className, cur->username, cur->name, cur->score);
        for (int i = 0; i < cur->score/5; i++) printf("█");
        printf("\n");
        cur = cur->next;
    }
    if (count > 0) {
        printf("\n平均成绩: %.1f\n", (float)total/count);
    }
}

void sortStudentsByClass(const char* className, int ascending) {

    Student* students[100];
    int count = 0;
    Student* cur = head;
    
    while(cur && count < 100) {
        if(strcmp(cur->className, className) == 0) {
            students[count++] = cur;
        }
        cur = cur->next;
    }

    for(int i = 0; i < count-1; i++) {
        for(int j = 0; j < count-i-1; j++) {
            if(ascending ? (students[j]->score > students[j+1]->score) 
                       : (students[j]->score < students[j+1]->score)) {
                Student* temp = students[j];
                students[j] = students[j+1];
                students[j+1] = temp;
            }
        }
    }

    printf("\n%-15s | %-15s | 成绩\n", "用户名", "姓名");
    for(int i = 0; i < count; i++) {
        printf("%-15s | %-15s | %3d\n", 
              students[i]->username, 
              students[i]->name, 
              students[i]->score);
    }
}

Account* login() {
    char username[MAX_STR_LEN], password[MAX_STR_LEN];
    safeInput(username, MAX_STR_LEN, "用户名: ");
    safeInputPassword(password, MAX_STR_LEN, "密码: ");
    
    for (int i = 0; i < accountCount; i++) {
        if (strcmp(accounts[i].username, username) == 0 && 
            strcmp(accounts[i].password, password) == 0) {
            printf("欢迎 %s！\n", accounts[i].role);
            return &accounts[i];
        }
    }
    printf("登录失败！\n");
    return NULL;
}

Student* findStudentByUsername(const char* username) {
    Student* cur = head;
    while (cur) {
        if (strcmp(cur->username, username) == 0) return cur;
        cur = cur->next;
    }
    return NULL;
}

void saveStudentsToFile() {
    if (access("students.txt", F_OK) == 0) {
        if (rename("students.txt", "students.bak") != 0) {
            perror("无法创建备份文件");
            return;
        }
    }
    FILE* fp = NULL;
    int retry = 0;
    const int max_retry = 3;
    
    while(retry < max_retry) {
        if((fp = fopen("students.txt", "w"))) break;
        printf("文件保存失败(%d/3)，重试中...\n", ++retry);
        sleep(1);
    }
    
    if(!fp) {
        perror("无法保存学生数据");
        return;
    }

    Student* cur = head;
    while(cur) {
        // 数据完整性检查
        if(validateUsername(cur->username) && 
           validateClassName(cur->className) &&
           cur->score >= MIN_SCORE && 
           cur->score <= MAX_SCORE) 
        {
            fprintf(fp, "%s|%s|%s|%d\n",
                   cur->username,
                   cur->name,
                   cur->className,
                   cur->score);
        }
        cur = cur->next;
    }
    
    if(fflush(fp) != 0 || fclose(fp) != 0) {
        perror("数据可能保存失败");
    }
}

void addStudent(Account* currentAccount) {
    Student s;
    if (strcmp(currentAccount->role, "teacher") == 0) {
    strcpy(s.className, currentAccount->className);
    }

    // 修复后的权限校验
    if (!currentAccount || !(strcmp(currentAccount->role, "teacher") == 0 || 
                            strcmp(currentAccount->role, "admin") == 0)) {
        printf("❌ 权限不足！\n");
        return;
    }

    safeInput(s.username, MAX_STR_LEN, "学生用户名: ");

    // 检查学生是否已存在（保持不变）
    if (findStudentByUsername(s.username)) {
        printf("⚠️ 该学生已有成绩记录！\n");
        return;
    }

    // 新增：管理员必须明确指定班级
    if (strcmp(currentAccount->role, "admin") == 0) {
        safeInputWithCheck(s.className, MAX_STR_LEN, "请输入班级(格式：2023_1): ", validateClassName);
    } 
    else {  // 教师自动关联班级
        strcpy(s.className, currentAccount->className);
        printf("🏫 自动设置班级: %s\n", s.className);
    }

    // 新增：班级有效性验证
    if (!validateClassName(s.className)) {
        printf("❌ 无效的班级格式！\n");
        return;
    }

    // 成绩录入部分（保持不变）
    safeInput(s.name, MAX_STR_LEN, "学生姓名: ");
    s.score = safeInputInt("成绩: ", MIN_SCORE, MAX_SCORE);

    // 链表操作（修复指针初始化问题）
    Student* newStu = (Student*)calloc(1, sizeof(Student)); // 使用calloc避免野指针
    *newStu = s;
    newStu->prev = tail;
    newStu->next = NULL;

    if (tail) {
        tail->next = newStu;
    } else {
        head = newStu;
    }
    tail = newStu;

    // 新增：立即同步数据到文件
    saveStudentsToFile();
    printf("✅ 添加成功！%s | %s班 | 成绩:%d\n", s.username, s.className, s.score);
}

void teacherMenu(Account* acc) {
    int choice;
    do {
        printf("\n=== 教师菜单 (%s) ===\n", acc->className);
        printf("1. 添加学生成绩\n");
        printf("2. 查看本班学生\n");
        printf("3. 修改学生信息\n");
        printf("4. 删除学生\n");
        printf("5. 班级成绩分析\n");
        printf("6. 成绩排序\n");  // 新增选项
        printf("0. 返回上级\n");
        choice = safeInputInt("请选择: ", 0, 6);  // 修改选项范围
        
        switch(choice) {
           case 1: 
                addStudent(acc);
                break;
            case 2: {
                Student* cur = head;
                printf("\n%-15s | %-15s | 成绩\n", "用户名", "姓名");
                while (cur) {
                    if (strcmp(cur->className, acc->className) == 0) {
                        printf("%-15s | %-15s | %3d\n", 
                               cur->username, cur->name, cur->score);
                    }
                    cur = cur->next;
                }
                break;
            }
            case 3: {
                char username[MAX_STR_LEN];
                safeInput(username, MAX_STR_LEN, "要修改的学生用户名: ");
                Student* target = findStudentByUsername(username);
                
                if (target && strcmp(target->className, acc->className) == 0) {
                    target->score = safeInputInt("新成绩: ", MIN_SCORE, MAX_SCORE);
                    saveStudentsToFile();
                    printf("修改成功！\n");
                } else {
                    printf("找不到该学生！\n");
                }
                break;
            }
            case 4: {
                char username[MAX_STR_LEN];
                safeInput(username, MAX_STR_LEN, "要删除的学生用户名: ");
                Student* target = findStudentByUsername(username);
                
                if (target && strcmp(target->className, acc->className) == 0) {
                    if (target->prev) target->prev->next = target->next;
                    else head = target->next;
                    
                    if (target->next) target->next->prev = target->prev;
                    else tail = target->prev;
                    
                    free(target);
                    saveStudentsToFile();
                    printf("删除成功！\n");
                } else {
                    printf("找不到该学生！\n");
                }
                break;
            }
            case 5: {
                int total = 0, count = 0;
                Student* cur = head;
                printf("\n=== 班级 %s 成绩分析 ===\n", acc->className);
                while (cur) {
                    if (strcmp(cur->className, acc->className) == 0) {
                        total += cur->score;
                        count++;
                        printf("%-15s | %-15s | %3d | ", 
                               cur->username, cur->name, cur->score);
                        for (int i = 0; i < cur->score/5; i++) printf("█");
                        printf("\n");
                    }
                    cur = cur->next;
                }
                if (count > 0) {
                    printf("\n平均成绩: %.1f\n", (float)total/count);
                }
                break;
            }
            
            case 6: {
                printf("\n排序方式：");
                int sortType = safeInputInt("1.降序 2.升序 : ", 1, 2);
                sortStudentsByClass(acc->className, sortType == 2);
                break;
            }
        }
    } while (choice != 0);
}


void adminAccountManagement() {
    int choice;
    do {
        printf("\n=== 账号管理 ===\n");
        printf("1. 查看所有账号\n");
        printf("2. 删除账号\n");
        printf("3. 修改密码\n");
        printf("4. 导出账号到文件\n");
        printf("5. 从文件导入账号\n");
        printf("0. 返回\n");
        choice = safeInputInt("请选择: ", 0, 5);

        switch(choice) {
            case 1: {
                printf("\n%-15s | %-10s | 班级/角色\n", "用户名", "角色");
                for(int i=0; i<accountCount; i++) {
                    printf("%-15s | %-10s | %s\n", 
                        accounts[i].username,
                        accounts[i].role,
                        (strcmp(accounts[i].role, "teacher") == 0) ? 
                        accounts[i].className : "");
                }
                break;
            }
            case 2: {
                char username[MAX_STR_LEN];
                safeInput(username, MAX_STR_LEN, "要删除的用户名: ");
                for(int i=0; i<accountCount; i++) {
                    if(strcmp(accounts[i].username, username) == 0) {
                        // 删除账号
                        memmove(&accounts[i], &accounts[i+1], 
                               (accountCount-i-1)*sizeof(Account));
                        accountCount--;
                        saveAccounts();
                        printf("删除成功！\n");
                        return;
                    }
                }
                printf("未找到该用户！\n");
                break;
            }
            case 3: {
                char username[MAX_STR_LEN], newpass[MAX_STR_LEN];
                safeInput(username, MAX_STR_LEN, "要修改的用户名: ");
                for(int i=0; i<accountCount; i++) {
                    if(strcmp(accounts[i].username, username) == 0) {
                        safeInput(newpass, MAX_STR_LEN, "新密码: ");
                        strcpy(accounts[i].password, newpass);
                        saveAccounts();
                        printf("密码已更新！\n");
                        return;
                    }
                }
                printf("未找到该用户！\n");
                break;
            }
            case 4: {
                saveAccounts();
                printf("账号数据已导出到accounts.txt\n");
                break;
            }
            case 5: {
                loadAccounts();
                printf("账号数据已从文件导入！\n");
                break;
            }
        }
    } while(choice != 0);
}

void adminStudentManagement() {
    int subChoice;
    do {
        printf("\n=== 学生管理 ===\n");
        printf("1. 查看所有学生\n");
        printf("2. 添加学生成绩\n");
        printf("3. 修改学生信息\n");
        printf("4. 删除学生\n");
        printf("5. 全局成绩分析\n");
        printf("0. 返回\n");
        subChoice = safeInputInt("请选择: ", 0, 5);
        
        switch(subChoice) {
            case 1: {
                Student* cur = head;
                printf("\n%-15s | %-15s | %-10s | 成绩\n", "用户名", "姓名", "班级");
                while(cur) {
                    printf("%-15s | %-15s | %-10s | %3d\n", 
                          cur->username, cur->name, cur->className, cur->score);
                    cur = cur->next;
                }
                break;
            }
            case 2: 
                addStudent(NULL); // 传递NULL表示管理员权限
                break;
            case 3: {
                char username[MAX_STR_LEN];
                safeInput(username, MAX_STR_LEN, "要修改的学生用户名: ");
                Student* target = findStudentByUsername(username);
                
                if (target) {
                    printf("原姓名: %s\n", target->name);
                    safeInput(target->name, MAX_STR_LEN, "新姓名(直接回车保留): ");
                    
                    printf("原班级: %s\n", target->className);
                    safeInput(target->className, MAX_STR_LEN, "新班级(直接回车保留): ");
                    
                    printf("原成绩: %d\n", target->score);
                    target->score = safeInputInt("新成绩(输入-1保留): ", -1, MAX_SCORE);
                    if(target->score == -1) target->score = target->score; // 保留原值
                    
                    saveStudentsToFile();
                    printf("修改成功！\n");
                } else {
                    printf("找不到该学生！\n");
                }
                break;
            }
            case 4: {
                char username[MAX_STR_LEN];
    safeInput(username, MAX_STR_LEN, "要删除的学生用户名: ");
    Student* target = findStudentByUsername(username);
    
    if (target) {
            for(int i=0; i<accountCount; i++) {
            if(strcmp(accounts[i].username, username) == 0 && 
               strcmp(accounts[i].role, "student") == 0) 
            {
                memmove(&accounts[i], &accounts[i+1], 
                       (accountCount-i-1)*sizeof(Account));
                accountCount--;
                saveAccounts();
                break;
            }
        }

        if (target->prev) target->prev->next = target->next;
        else head = target->next;
        
        if (target->next) target->next->prev = target->prev;
        else tail = target->prev;
        
        free(target);
        saveStudentsToFile();
        printf("删除成功！\n");
    } else {
                    printf("找不到该学生！\n");
                }
                break;
            }
            case 5:
                analyzeAllScores();
                break;
        }
    } while(subChoice != 0);
}

void loadAppeals() {
    FILE* fp = fopen("appeals.txt", "r");
    if (!fp) return;
    
    char line[256];
    while (fgets(line, sizeof(line), fp) && appealCount < MAX_APPEALS) {
        // 清理换行符
        line[strcspn(line, "\r\n")] = '\0';
        
        // 使用strtok分割字段
        char* username = strtok(line, "|");
        char* className = strtok(NULL, "|");
        char* reason = strtok(NULL, "|");
        char* statusStr = strtok(NULL, "|");

        if (!username || !className || !reason || !statusStr) {
            printf("无效申诉记录: %s\n", line);
            continue;
        }

        // 复制数据到结构体
        strncpy(appeals[appealCount].username, username, MAX_STR_LEN);
        strncpy(appeals[appealCount].className, className, MAX_STR_LEN);
        strncpy(appeals[appealCount].reason, reason, 99);
        appeals[appealCount].status = atoi(statusStr);
        appealCount++;
    }
    fclose(fp);
}


void adminMenu(Account* acc) {
    int choice;
    do {
        printf("\n=== 管理员菜单 ===\n");
        printf("1. 学生管理\n");
        printf("2. 账号管理\n");
        printf("3. 处理申诉\n");  // 新增
        printf("0. 返回上级\n");
        choice = safeInputInt("请选择: ", 0, 3);  // 修改选项范围
        
        switch(choice) {
            case 1: 
                adminStudentManagement();
                break;
            case 2: 
                adminAccountManagement();
                break;
            case 3:
                handleAppeals(acc);
                break;
        }
    } while(choice != 0);
}

void loadStudentsFromFile() {
    FILE* fp = fopen("students.txt", "r");
    if (!fp) {
        printf("未找到学生数据文件，将创建新文件\n");
        return;
    }
    
    char line[256];
    int lineNum = 0;
    while (fgets(line, sizeof(line), fp)) {
        lineNum++;
        Student* s = NULL;
        char *username, *name, *className, *scoreStr;

        // 清理行结尾
        line[strcspn(line, "\r\n")] = '\0';

        // 字段解析
        if(!(username = strtok(line, "|")) ||
           !(name = strtok(NULL, "|")) ||
           !(className = strtok(NULL, "|")) ||
           !(scoreStr = strtok(NULL, "|"))) 
        {
            printf("第%d行格式错误，已跳过\n", lineNum);
            continue;
        }

        // 创建新节点
        if(!(s = (Student*)malloc(sizeof(Student)))){
            printf("内存分配失败，终止加载\n");
            break;
        }
        memset(s, 0, sizeof(Student));

        // 数据验证
        if(!validateUsername(username) || 
           strlen(name) == 0 ||
           !validateClassName(className)) 
        {
            printf("第%d行数据无效，已跳过\n", lineNum);
            free(s);
            continue;
        }

        // 成绩转换
        char* endptr;
        long score = strtol(scoreStr, &endptr, 10);
        if(*endptr || score < MIN_SCORE || score > MAX_SCORE){
            printf("第%d行成绩无效，已跳过\n", lineNum);
            free(s);
            continue;
        }

        // 数据赋值
        strncpy(s->username, username, MAX_STR_LEN-1);
        strncpy(s->name, name, MAX_STR_LEN-1);
        strncpy(s->className, className, MAX_STR_LEN-1);
        s->score = (int)score;

        // 链接链表
        s->prev = tail;
        s->next = NULL;
        tail ? (tail->next = s) : (head = s);
        tail = s;
    }
    
    if(ferror(fp)) {
        perror("读取错误");
    }
    fclose(fp);
}


void studentMenu(Account* acc) {
    Student* s = findStudentByUsername(acc->username);
    if(!s) {
        printf("\n=== 新生 %s ===\n", acc->username);
        if(safeInputInt("检测到未初始化成绩，是否现在录入？(1/0)",0,1) == 1) {
            addStudent(acc); // 允许教师/管理员初始化
        }
        return;
    }

    int choice;
    do {
        Student* s = findStudentByUsername(acc->username);
    if(!s) {
        printf("\n=== 欢迎新生 %s ===\n", acc->username);
        printf("检测到您尚未录入成绩信息\n");
        printf("1. 查看班级成绩\n");
        printf("0. 返回\n");

        return;
    }
        
        printf("\n=== 学生菜单 ===\n");
        printf("1. 查看我的成绩\n");
        printf("2. 查看班级成绩\n");
        printf("3. 成绩分析\n");       // 新增
        printf("4. 提交成绩申诉\n");   // 新增
        printf("0. 返回\n");
        choice = safeInputInt("请选择: ", 0, 4);  // 修改选项范围
        
        switch(choice) {
            case 1: {
                printf("\n姓名: %s\n", s->name);
                printf("班级: %s\n", s->className);
                printf("成绩: %d\n", s->score);
                break;
            }
            case 2: {
                Student* cur = head;
                printf("\n=== %s 班级成绩 ===\n", s->className);
                printf("%-15s | %-15s | 成绩\n", "用户名", "姓名");
                while(cur) {
                    if(strcmp(cur->className, s->className) == 0) {
                        printf("%-15s | %-15s | %3d\n", 
                              cur->username, cur->name, cur->score);
                    }
                    cur = cur->next;
                }
                break;
            }
            case 3:
                analyzeClassScore(s);
                break;
            case 4:
                submitAppeal(acc);
                break;
        }
    } while(choice != 0);
}

int main() {
    srand(time(NULL));
    loadAccounts();
    loadStudentsFromFile();
    loadAppeals();
    
    int choice;
    do {
        CLEAR_SCREEN;
        printf("\n=== 主菜单 ===\n");
        printf("1. 注册\n");
        printf("2. 登录\n");
        printf("3. 找回密码\n"); 
        printf("0. 退出\n");
        choice = safeInputInt("请选择: ", 0, 3);
        
        switch(choice) {
            case 1: 
                registerAccount(); 
                break;
            case 2: {
                Account* acc = login();
                if (acc) {
                    if (strcmp(acc->role, "student") == 0) studentMenu(acc);
                    else if (strcmp(acc->role, "teacher") == 0) teacherMenu(acc);
                    else if (strcmp(acc->role, "admin") == 0) adminMenu(acc);
                }
                break;
            }
            case 3:
                forgotPassword();
                break;
        }
    } while (choice != 0);
    while (head) {
        Student* temp = head;
        head = head->next;
        free(temp);
    }
    saveAppeals();
    printf("系统已安全退出\n");
    return 0;
}

