#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <math.h>
#include <sys/time.h>

#define MAX_THREADS 16 // максимальное количество потоков
#define MAX_PIXELS 1000000 // максимальное количество пикселей

// структура для передачи аргументов потоку
typedef struct {
    int start_row; // номер строки, с которой начинается обработка
    int end_row; // номер строки, на которой заканчивается обработка
    int width; // ширина изображения
    int height; // высота изображения
    unsigned char* input_data; // входные данные (RGB)
    unsigned char* output_data; // выходные данные (grayscale)
} thread_args_t;

// функция для загрузки изображения из файла
unsigned char* load_image(char* filename, int* width, int* height) {
    FILE* f = fopen(filename, "rb");
    if (!f) {
        fprintf(stderr, "Error: could not open file '%s'\n", filename);
        exit(1);
    }
    char header[16];
    fgets(header, sizeof(header), f);
    if (header[0] != 'P' || header[1] != '6') {
        fprintf(stderr, "Error: file '%s' is not a valid PPM image\n", filename);
        exit(1);
    }
    int maxval;
    fscanf(f, "%d %d %d\n", width, height, &maxval);
    if (maxval > 255) {
        fprintf(stderr, "Error: file '%s' has more than 8 bits per channel\n", filename);
        exit(1);
    }
    unsigned char* data = (unsigned char*) malloc(3 * (*width) * (*height) * sizeof(unsigned char));
    fread(data, 1, 3 * (*width) * (*height), f);
    fclose(f);
    return data;
}

// функция для сохранения изображения в файл
void save_image(char* filename, int width, int height, unsigned char* data) {
    FILE* f = fopen(filename, "wb");
    if (!f) {
        fprintf(stderr, "Error: could not create file '%s'\n", filename);
        exit(1);
    }
    fprintf(f, "P5\n%d %d\n255\n", width, height);
    fwrite(data, 1, width * height, f);
    fclose(f);
}

// функция для обработки изображения в отдельном потоке
void* process_image(void* args) {
    thread_args_t* ta = (thread_args_t*) args;
    int width = ta->width;
    int height = ta->height;
    int start_row = ta->start_row;
    int end_row = ta->end_row;
    unsigned char* input_data = ta->input_data;
    unsigned char* output_data = ta->output_data;
    // применяем фильтр Собела к каждому пикселю изображения
    for (int y = start_row; y < end_row; y++) {
        for (int x = 0; x < width; x++) {
            int gx = 0, gy = 0;
            // вычисляем градиент по x и y
            if (x > 0 && y > 0) {
                gx += -1 * input_data[3*((y-1)*width+x-1)] + 0 * input_data[3*((y-1)*width+x)] + 1 * input_data[3*((y-1)*width+x+1)];
                gy += -1 * input_data[3*((y-1)*width+x-1)] + -2 * input_data[3*((y)*width+x-1)] + -1 * input_data[3*((y+1)*width+x-1)];
            }
            if (x > 0 && y < height-1) {
                gx += -2 * input_data[3*((y)*width+x-1)] + 0 * input_data[3*((y)*width+x)] + 2 * input_data[3*((y)*width+x+1)];
                gy += 1 * input_data[3*((y)*width+x+1)] + 2 * input_data[3*((y+1)*width+x)] + 1 * input_data[3*((y+1)*width+x+1)];
            }
            // вычисляем модуль градиента и записываем его в выходной массив
            output_data[y*width+x] = (unsigned char) (sqrt(gx*gx + gy*gy) / 3.0);
        }
    }
    return NULL;
}

int main(int argc, char** argv) 
{
    if (argc != 3) 
    {
        fprintf(stderr, "Usage: %s <input_file> <output_file>\n", argv[0]);
        return 1;
    }
    // загружаем входное изображение
    int width, height;
    unsigned char* input_data = load_image(argv[1], &width, &height);
    // создаем выходной массив
    unsigned char* output_data = (unsigned char*) malloc(width * height * sizeof(unsigned char));
    // создаем массив для аргументов потоков
    thread_args_t thread_args[MAX_THREADS];
    // создаем потоки
    pthread_t threads[MAX_THREADS];
    int num_threads = MAX_THREADS;
    int rows_per_thread = height / num_threads;
    // передаем аргументы каждому потоку
    for (int i = 0; i < num_threads; i++) 
    {
        thread_args[i].start_row = i * rows_per_thread;
        thread_args[i].end_row = (i == num_threads-1) ? height : (i+1) * rows_per_thread;
        thread_args[i].width = width;
        thread_args[i].height = height;
        thread_args[i].input_data = input_data;
        thread_args[i].output_data = output_data;
        // запускаем потоки
        pthread_create(&threads[i], NULL, process_image, &thread_args[i]);
    }
  // засекаем время начала выполнения
    struct timeval start_time, end_time;
    gettimeofday(&start_time, NULL);
    // ожидаем завершения всех потоков
    for (int i = 0; i < num_threads; i++) 
    {
        pthread_join(threads[i], NULL);
    }
    // засекаем время окончания выполнения
    gettimeofday(&end_time, NULL);
    // вычисляем время выполнения в миллисекундах
    struct timeval elapsed_time;
    timersub(&end_time, &start_time, &elapsed_time);
    long long elapsed_ms = elapsed_time.tv_sec * 1000LL + elapsed_time.tv_usec / 1000;
    printf("Elapsed time: %lld ms\n", elapsed_ms);
    // сохраняем выходное изображение
    save_image(argv[2], width, height, output_data);
    // освобождаем память
    free(input_data);
    free(output_data);
    return 0;
}