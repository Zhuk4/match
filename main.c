#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <limits.h>
#include <string.h>

#define MAX_SIZE UCHAR_MAX // Максимальным размером для строк выбран максимальный размер беззнакового char

enum metaSign{
    STAR,
    QUESTION,
    NONE
};

struct sign{
    size_t position;
    enum metaSign metasign;
};

struct templateTextParts{
    char* keyPart;
    size_t keyPartSize;
};

bool match(char* text, char* template){

    FILE *log = fopen("log.txt", "w+");

    // Узнаём размеры строк
    size_t templSize = strlen(template);
    size_t textSize = strlen(text);

    // Узнаём формат записи строк (Windows, Linux, Mac)
    char c = 0;
    int has_cr = 0;  // Флаг наличия \r (carriege return)
    int has_lf = 0;  // Флаг наличия \n (line feed)

    for(size_t i = 0; i != textSize; ++i){
        c = text[i];
        if (c == '\r'){
            has_cr = 1;
        }
        else if (c == '\n'){
            has_lf = 1;
        }
    }

    if (has_cr && has_lf) {
        fprintf(log,"Text format: Windows (\\r\\n)\n");
    } else if (has_lf) {
        fprintf(log,"Text format: Unix/Linux (\\n)\n");
    } else if (has_cr) {
        fprintf(log,"Text format: Mac OS(\\r)\n");
    } else {
        fprintf(log,"Text format undefined\n");
    }

    /*
     * Реверсим строку для упрощения работы из-за гарантированного отсутствия
     * конечного метасимвола (далее МС) из-за вездесущего escape sequence.
     * Сделано это для облегчения сравнения.
     */

    char* textStart = text;
    char* textEnd = text + textSize - 1;
    char* templateStart = template;
    char* templateEnd = template + templSize - 1;

    while(textStart < textEnd){
        char temp = *textStart;
        *textStart = *textEnd;
        *textEnd = temp;
        ++textStart;
        --textEnd;
    }

    while(templateStart < templateEnd){
        char temp = *templateStart;
        *templateStart = *templateEnd;
        *templateEnd = temp;
        ++templateStart;
        --templateEnd;
    }

    /*
     * Возможно рассмотреть вариант, где количество МС превышает половину длины шаблона,
     * но это не имеет смысла, т.к два МС не могут стоять подряд.
     * Следовательно максимальное количество МС - половина шаблона + 1
     * из-за возможности поставить МС в начале и в конце
     */

    int signsCurrentAmount = 0;                         // текущее количество МС
    size_t signsAvailableAmount = templSize / 2 + 1;    // возможное количество МС
    int keyPartsCurrentAmount = 0;                      // текущее количество ключевых частей (кусков текста) -- далее КЧ
    struct sign signs[signsAvailableAmount];            // массив структур МС

    // Заполнение массива позициями МС
    for(size_t i = 0; i != templSize; ++i){
        if(template[i] == '?') {
            signs[signsCurrentAmount].metasign = QUESTION;
            signs[signsCurrentAmount].position = i;
            ++signsCurrentAmount;
        }
        else if(template[i] == '*'){
            signs[signsCurrentAmount].metasign = STAR;
            signs[signsCurrentAmount].position = i;
            ++signsCurrentAmount;
        }

    }

    // Заполнение оставшегося массива структур обозначениями о том, что МС нет
    for(size_t i = signsCurrentAmount; i != signsAvailableAmount; ++i){
        signs[i].metasign = NONE;
        signs[i].position = 0;
        fprintf(log, "Changed number %lu unused metasign structure place\n" , i);
    }

    /*
     * Подсчёт КЧ частей зависящий от наличия или отсутствия
     * МС в начале и в конце.
     * Т.к. шаблон и текст перевернуты, порядок "Стартового" и
     * "Финального" МС поменялись местами.
     */

    keyPartsCurrentAmount = signsCurrentAmount + 1;
    bool firstMetaSign = 0;
    bool lastMetaSign = 0;
    size_t txtFormatGap = has_cr + has_lf;

    if (signs[signsCurrentAmount - 1].position == templSize - 1){
        --keyPartsCurrentAmount;
        firstMetaSign = 1;
        fprintf(log, "Start metasign found\n");
    }

    if (signs[0].position == txtFormatGap){
        lastMetaSign = 1;
        fprintf(log, "Final metasign found\n");
    }

    struct templateTextParts templateTextParts[keyPartsCurrentAmount];    // массив структур КЧ
    char part[MAX_SIZE] = {0};
    size_t partSize = 0;
    size_t keyPartsIndex = 0;

    /*
     * Запись КЧ шаблона в струтуру посимвольным копированием во временную переменную
     * и цельным переносом их в четко выделенный кусок памяти в соответствии с их размером
     */

    for(size_t i = 0; i != templSize;++i){
        // Запись в каждую ячейку по символу до встречи с МС
        if(template[i] != '*' && template[i] != '?'){
            part[partSize] = template[i];
            ++partSize;
        }
        // В ином случае -- встретился первый МС, значит что КЧ закончилась и её можно записывать в структуру
        else{
            templateTextParts[keyPartsIndex].keyPartSize = partSize;
            templateTextParts[keyPartsIndex].keyPart = (char*)malloc(sizeof(char) * templateTextParts[keyPartsIndex].keyPartSize);
            strncpy(templateTextParts[keyPartsIndex].keyPart, part, templateTextParts[keyPartsIndex].keyPartSize);
            ++keyPartsIndex;
            partSize = 0;
            fprintf(log, "Keypart number %lu written\n", keyPartsIndex);
        }
        // Обязательная постоянная проверка на конец шаблона при отсутствии завершающего (перед разворотом начального) МС
        if(i == templSize - 1 && !firstMetaSign){
            templateTextParts[keyPartsIndex].keyPartSize = partSize;
            templateTextParts[keyPartsIndex].keyPart = (char*)malloc(sizeof(char) * templateTextParts[keyPartsIndex].keyPartSize);
            strncpy(templateTextParts[keyPartsIndex].keyPart, part, templateTextParts[keyPartsIndex].keyPartSize);
            fprintf(log, "Last keypart from text without first metasign written\n");
        }
    }

    /*
     * Здесь происходят основные вычисления после препараций шаблона по вычленению МС и КЧ.
     * Ответ вычисляется посредством нахождения совпадений в тексте и отметки об этом в переменной
     * answer, которая, при нахождении всех совпадений становится равна нынешнему количеству КЧ.
     * Не очень элегантный способ, но это работает достаточно логично для того метода,
     * который был выбран в данной программе.
     */

    int answer = 0;
    keyPartsIndex = 0;
    size_t metasignsIndex = 0;
    size_t gap = 0;
    int i = metasignsIndex;

    /*
     * В зависимости от того, что больше -- количество МС или же ключевых частей
     * определяем итератор для корректного количества проходов по тексту
     */

    size_t iterator;
    if (keyPartsCurrentAmount > signsCurrentAmount){
        iterator = keyPartsCurrentAmount;
    }
    else {
        iterator = signsCurrentAmount;
    }

    while((i < iterator)){
        // Копирование "куска" текста (далее КТ) для сравнения с КЧ
        char* textPart = (char*)malloc(sizeof(char) * (templateTextParts[keyPartsIndex].keyPartSize));
        strncpy(textPart, text + gap, templateTextParts[keyPartsIndex].keyPartSize);
        /*
         * Если при сравнении КЧ и КТ результатом не будет ноль, т.е. они не совпадают,
         * начинается проверка на то, какой сейчас МС по порядку
         */
        if(strcmp(textPart, templateTextParts[keyPartsIndex].keyPart) != 0){
            /*
             * При текущем МС равном "звезда" мы принудительно увеличиваем количество
             * итераций, а затем конце цикла переносим "зазор" поиска на следующий символ
             */
            if(signs[metasignsIndex].metasign == STAR){
                --i;
                // Если предыдущий МС равен "вопрос" -- значит дальше проверка не имеет смысла
                if(metasignsIndex > 0 && signs[metasignsIndex - 1].metasign == QUESTION){
                    return 0;
                }
            }
            else if(signs[metasignsIndex].metasign == QUESTION){
                /*
             * При текущем МС равном "вопрос" мы проверяем не является ли данный МС первым (при соблюдении данного условия
             * текст сразу считается неправильным), а затем проверяем какой МС стоит перед ним. При предыдущем МС равным "звезда"
             * мы принудительно увеличиваем количество итераций и в конце цикла переносим "зазор" поиска на следующий символ.
             * В ином случае -- мы ищем МС "звезда", при работе с которым мы могли найти задублированное значение,
             * которое "запутало" программу, параллельно с этим "распутывая" цепочку ошибочных совпадений текста и шаблона путём
             * уменьшения значения найденных КЧ в тексте, индексов МС и КЧ и "зазора" на длины всех ошибочно совпавших КЧ.
             * При отсутствии МС "звезда" до данного МС "вопрос" при проходе по циклу
             * "зазор" станет меньше нуля, что сигнализирует о несоответствии текста шаблону
             */
                if(keyPartsIndex!= 0){
                    if (signs[metasignsIndex - 1].metasign == STAR){
                        --i;
                    }
                    else{
                        if (gap > 0){
                            gap -= templateTextParts[keyPartsIndex - 1].keyPartSize;
                            --i;
                            --answer;
                            --metasignsIndex;
                            --keyPartsIndex;
                        }
                    }
                }
                else {
                    return 0;
                }
            }
        }
        /*
         * В случае, когда КЧ и КТ совпали мы вне зависимости от МС увеличиваем
         * индекс МС, КЧ и переносим "зазор" на длину найденной КЧ, а затем
         * в конце цикла снова увеличиваем его на единицу
         * всвязи с пропуском символа, который "стоит под метасимволом"
         */
        else{
            ++answer;
            if(firstMetaSign){
                if(signs[metasignsIndex].position != templSize - 1){
                    gap += templateTextParts[keyPartsIndex].keyPartSize;
                    ++keyPartsIndex;
                    ++metasignsIndex;
                }
                /*
                * Проверка на последний символ при которой под МС "вопрос" должен стоять ровно один символ текста
                * в то время, как при МС "звезда" нам всё равно на лишние символы после
                */
                else{
                    if(signs[metasignsIndex].metasign == QUESTION && signs[metasignsIndex].position == templSize - 1){
                        gap += templateTextParts[keyPartsIndex].keyPartSize;
                        if(gap != textSize - 1){
                            return 0;
                        }
                        else{
                            break;
                        }
                    }
                }
            }
            /*
             * Здесь при отсутствии заканчивающего МС идёт проверка на то, является ли текущий МС последним.
             * Если он не является таковым -- проделываем такие же действия, что и в случае с наличием "первого" МС.
             * В ином случае -- просто увеличиваем "зазор" и индекс КЧ для последней проверки.
             */
            else{
                if(metasignsIndex != signsCurrentAmount - 1 ){
                    gap += templateTextParts[keyPartsIndex].keyPartSize;
                    ++keyPartsIndex;
                    ++metasignsIndex;
                }
                else{
                    gap += templateTextParts[keyPartsIndex].keyPartSize;
                    ++keyPartsIndex;
                }
            }
        }
        ++gap;
        ++i;
    }

    /*
     * При наличии "последнего" МС для нормального отображения количества
     * соответствий уменьшаем значения совпадений и текущего количества КЧ
     * для изоляции пользователя от особенностей записи строк.
     */

    if(lastMetaSign){
        --answer;
        --keyPartsCurrentAmount;
    }

    printf("%d coincidences from %d\n", answer, keyPartsCurrentAmount);

    if(answer == keyPartsCurrentAmount){
        return 1;
    }
    else{
        return 0;
    }
}

int main()
{
    FILE *txtFile = fopen("texts.txt", "r");

    if (txtFile == NULL){
        perror("File read error\n");
        return 1;
    }

    char text[MAX_SIZE];
    char template[MAX_SIZE];

    fgets(text, sizeof(text), txtFile);
    fgets(template, sizeof(template), txtFile);

    printf("%s",text);
    printf("%s", template);


    if (match(text, template)){
        printf("True\n");
    }
    else{
        printf("False\n");
    }

    fclose(txtFile);
    return 0;
}
