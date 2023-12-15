#define _POSIX_C_SOURCE 199309L
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include "Utils.h"
#include "Picture.h"
#include "BlurExprmt.h"
#include <math.h>
#include <stdlib.h>
#include <time.h>
#include <stdint.h>
#include <string.h>
#include <libgen.h>

#define NO_RGB_COMPONENTS 3
#define BLUR_REGION_SIZE 9
#define PTHREAD_CREATE_FAIL_CODE 1
#define PTHREAD_CREATE_SUCCESS_CODE 0
#define BOUNDARY_WIDTH 1
#define MEMORY_ERROR -2
#define CORE_NUM_FOR_TESTING 7
#define EXP_FUNC_NUM 1
#define BILLION 1000000000 
#define MILLION 1000000


// ---------- MAIN PROGRAM ---------- \\

  static int no_of_cmds;
  static char *cmd_strings[];
  static void (* const cmds[])(struct picture *, const char *);

  static void save_pic_to_res_folder(struct picture* pic, 
                                     char *filename) {
    // Chosen arbitrarily as a decent path limit
    char path[75];
    sprintf(path, "BlurExprmt_output_images/%s.jpg", filename);
    save_picture_to_file(pic, path);
  }


  void blur_experiment_wrapper(struct picture *pic, int number_of_tests,
                              const char* filename) {

    FILE *fp = fopen("BlurExprmt.txt", "w+");

    fprintf(fp, "____New Blur Experiment Started____ \n\n\n");

    clock_t begin = clock();

    int blur_func_total_num = no_of_cmds - EXP_FUNC_NUM;

    // Make time array and init to 0 for all functions
    int time_per_func[blur_func_total_num][number_of_tests];

    for (int fnum = 0; fnum < blur_func_total_num; fnum++) {
      double average_time = 0;
      double slowest_time = 0;
      double fastest_time = 0;

      fprintf(fp, "--Starting Testing on Blur Function: %s-- \n\n", cmd_strings[fnum]);

      fprintf(fp, "Time Spent per Attempt (milliseconds): ");

      for (int iter = 0; iter < number_of_tests; iter++) {
        // Begins clock at current tick number
        struct timespec start;
        struct timespec end;
        clock_gettime(CLOCK_REALTIME, &start);

        // Blur Function Being Tested
        cmds[fnum](pic, NULL);

        // Calculates time spent
        clock_gettime(CLOCK_REALTIME, &end);
        double diff = (BILLION * (end.tv_sec - start.tv_sec) + 
                      end.tv_nsec - start.tv_nsec)
                      / MILLION;

        // FILE SAVING PROCESS -------------
        char new_filename[150];
        char base[150];
        // Since basename can sometimes modify source
        strncpy(base, filename, sizeof(base));
        char* base_name = basename(base);
        // Removes extension (so .jpg)
        char* last_dot = strrchr(base_name, '.');
        if (last_dot) {
          *last_dot = '\0';
        }
        snprintf(new_filename, sizeof(new_filename), "%s_%s_%d", 
                base_name, cmd_strings[fnum], iter);
        save_pic_to_res_folder(pic, new_filename);
        // FILE SAVING PROCESS FINISHED ---------

        // Adds time spent to array entry
        time_per_func[fnum][iter] = diff;
        fprintf(fp, "   %f    ", diff);

        // Updating Metrics
        average_time += diff;
        if (slowest_time == 0 || diff > slowest_time) {
          slowest_time = diff;
        }
        if (fastest_time == 0 || diff < fastest_time) {
          fastest_time = diff;
        }
      }
      
      // Padding for next function tests
      fprintf(fp, "\n\n\n");

      fprintf(fp, "   Average Time: %f ms \n", average_time/number_of_tests);
      fprintf(fp, "   Slowest Time: %f ms \n", slowest_time);
      fprintf(fp, "   Fastest Time: %f ms \n", fastest_time);

      // Padding for next function tests
      fprintf(fp, "\n\n\n");
    }

    // Padding for if file is appended by another set of tests
    fprintf(fp, "\n\n\n\n\n");

    fclose(fp);

  }

  void sequential_blur_testwrapper(struct picture *pic, const char *unused){
      printf("calling sequential blur\n");
      sequential_blur(pic);
    }

  void pixel_by_pixel_blur_testwrapper(struct picture *pic, const char *unused){
      printf("calling pixel by pixel blur\n");
      pixel_by_pixel_blur(pic);
    }

  void sector_core_blur_testwrapper(struct picture *pic, const char *unused){
      printf("calling sector core blur\n");
      sector_core_blur(pic, CORE_NUM_FOR_TESTING);
    }

  void row_blur_testwrapper(struct picture *pic, const char *unused){
      printf("calling row blur\n");
      row_blur(pic);
    }

  void column_blur_testwrapper(struct picture *pic, const char *unused){
      printf("calling column blur\n");
      column_blur(pic);
    }

  static void (* const cmds[])(struct picture *, const char *) = { 
    sequential_blur_testwrapper,
    pixel_by_pixel_blur_testwrapper,
    sector_core_blur_testwrapper,
    row_blur_testwrapper,
    column_blur_testwrapper,
  };

  // list of all possible picture transformations
  static char *cmd_strings[] = { 
    "seq-blur",
    "pixel-by-pixel-blur",
    "sector-core-blur",
    "row-blur",
    "column-blur",
  };

  static int no_of_cmds = sizeof(cmds) / sizeof(cmds[0]);

  int main(int argc, char **argv){

    // capture and check command line arguments
    const char * number_of_tests_str = argv[1];
    const char * input_file = argv[2];
    
    if(number_of_tests_str == NULL || input_file == NULL) {
      printf("[!] insufficient command line arguments provided\n");
      exit(IO_ERROR);
    }        
  
    printf("  number of tests per pic = %s\n", number_of_tests_str);
    printf("  image file to use       = %s\n", input_file);
  
    printf("\n");
  
    // create original image object
    struct picture pic;
    if(!init_picture_from_file(&pic, input_file)){
      exit(IO_ERROR);   
    }   

    int number_of_tests = atoi(number_of_tests_str);
    if (number_of_tests == 0) {
      printf("You specified 0 tests to be run, recheck your input.");
      return 0;
    }

    blur_experiment_wrapper(&pic, number_of_tests, input_file);

    return 0;
  }



  //UTILITY ---------------------------------------------------------



  static void get_avg_pixel_and_set(struct picture* orig_pic,
                                struct picture* new_pic,
                                int x_coord, int y_coord) {
    // set-up a local pixel on the stack
    struct pixel rgb = get_pixel(orig_pic, x_coord, y_coord);
    
    // don't need to modify boundary pixels
    if(x_coord != 0 && y_coord != 0 && 
        x_coord != orig_pic->width - BOUNDARY_WIDTH && 
        y_coord != orig_pic->height - BOUNDARY_WIDTH) {
    
      // set up running RGB component totals for pixel region
      int sum_red = rgb.red;
      int sum_green = rgb.green;
      int sum_blue = rgb.blue;
  
      // check the surrounding pixel region (provided code)
      for(int n = -1; n <= 1; n++){
        for(int m = -1; m <= 1; m++){
          if(n != 0 || m != 0){
            rgb = get_pixel(orig_pic, x_coord+n, y_coord+m);
            sum_red += rgb.red;
            sum_green += rgb.green;
            sum_blue += rgb.blue;
          }
        }
      }
  
      // compute average pixel RGB value
      rgb.red = sum_red / BLUR_REGION_SIZE;
      rgb.green = sum_green / BLUR_REGION_SIZE;
      rgb.blue = sum_blue / BLUR_REGION_SIZE;
    }
    set_pixel(new_pic, x_coord, y_coord, &rgb);
  }

  static void thread_cleanup_handler(void* args)
  {
      free(args);
  }

  static void thread_join_then_return(struct thread_queue* queue) {

    struct thread_node *node_to_rm = dequeue(queue);
    if (node_to_rm == NULL) {
      return;
    }
    pthread_t *thread_to_join = node_to_rm->thread;
    free(node_to_rm);
    pthread_join(*thread_to_join, NULL);
    free(thread_to_join);
  }

  static void clear_threads(struct thread_queue* queue) {
    while (!isNull(queue)) {
      thread_join_then_return(queue);
    }
  }

  static void *malloc_clear_if_need(size_t size_to_malloc, 
                                    struct thread_queue* queue) {
    void *new_space = malloc(size_to_malloc);
    while (new_space == NULL) {
      thread_join_then_return(queue);
      new_space = malloc(size_to_malloc);
    }
    return new_space;
  }

  static void make_thread_and_enqueue(void*(*worker_func)(void*),
                                      void *thread_params,
                                      struct thread_queue* queue) {
    pthread_t *thread = malloc_clear_if_need(sizeof(pthread_t), queue);

    int pthread_create_code = PTHREAD_CREATE_FAIL_CODE;
    while (pthread_create(thread, NULL, worker_func, thread_params) != 
           PTHREAD_CREATE_SUCCESS_CODE) {
      thread_join_then_return(queue);
    }

    struct thread_node *new_node = 
          malloc_clear_if_need(sizeof(struct thread_node), queue);
    enqueue(queue, thread, new_node);
  }



  // Sequential Blurring Implementation (provided) ------------------



  void sequential_blur(struct picture *pic){
    // make new temporary picture to work in
    struct picture tmp;
    init_picture_from_size(&tmp, pic->width, pic->height);
  
    // iterate over each pixel in the picture
    for(int i = 0 ; i < tmp.width; i++){
      for(int j = 0 ; j < tmp.height; j++){
      
        // set-up a local pixel on the stack
        struct pixel rgb = get_pixel(pic, i, j);
        
        // don't need to modify boundary pixels
        if(i != 0 && j != 0 && i != tmp.width - 1 && j != tmp.height - 1){
        
          // set up running RGB component totals for pixel region
          int sum_red = rgb.red;
          int sum_green = rgb.green;
          int sum_blue = rgb.blue;
      
          // check the surrounding pixel region
          for(int n = -1; n <= 1; n++){
            for(int m = -1; m <= 1; m++){
              if(n != 0 || m != 0){
                rgb = get_pixel(pic, i+n, j+m);
                sum_red += rgb.red;
                sum_green += rgb.green;
                sum_blue += rgb.blue;
              }
            }
          }
      
          // compute average pixel RGB value
          rgb.red = sum_red / BLUR_REGION_SIZE;
          rgb.green = sum_green / BLUR_REGION_SIZE;
          rgb.blue = sum_blue / BLUR_REGION_SIZE;
        }
      
        // set pixel to computed region RBG value (unmodified if boundary)
        set_pixel(&tmp, i, j, &rgb);
      }
    }
    
    // clean-up the old picture and replace with new picture
    clear_picture(pic);
    overwrite_picture(pic, &tmp);
  }



  // Pixel by Pixel Blurring Implementation -------------------------



  static void *single_pixel_worker(void *args) {
    pthread_cleanup_push(thread_cleanup_handler, args);
    struct pixel_work_args *pargs = (struct pixel_work_args*) args;

    struct pixel rgb;

    int sum_red = 0;
    int sum_green = 0;
    int sum_blue = 0;

    for(int n = -1; n <= 1; n++){
      for(int m = -1; m <= 1; m++){
        rgb = get_pixel(pargs->orig_pic, pargs->x_coord+n, pargs->y_coord+m);
        sum_red += rgb.red;
        sum_green += rgb.green;
        sum_blue += rgb.blue;
        }
      }
    rgb.red = sum_red / BLUR_REGION_SIZE;
    rgb.green = sum_green / BLUR_REGION_SIZE;
    rgb.blue = sum_blue / BLUR_REGION_SIZE;

    set_pixel(pargs->new_pic, pargs->x_coord, pargs->y_coord, &rgb);

    pthread_cleanup_pop(1);
  }
  
  static void *bound_pixel_worker(void *args) {
    pthread_cleanup_push(thread_cleanup_handler, args);
    struct pixel_work_args *pargs = (struct pixel_work_args*) args;

    struct pixel rgb = 
      get_pixel(pargs->orig_pic, pargs->x_coord, pargs->y_coord);
    set_pixel(pargs->new_pic, pargs->x_coord, pargs->y_coord, &rgb);

    pthread_cleanup_pop(1);
  }

  static void make_pixel_thread(void*(*worker_func)(void*), 
                    struct picture *orig_pic, struct picture *new_pic, 
                    int x_coord, int y_coord, struct thread_queue* queue) {
    struct pixel_work_args *pixel_params  = 
          malloc_clear_if_need(sizeof(struct pixel_work_args), queue);
    pixel_params->orig_pic = orig_pic;
    pixel_params->new_pic = new_pic;
    pixel_params->x_coord = x_coord;
    pixel_params->y_coord = y_coord;

    make_thread_and_enqueue(worker_func, pixel_params, queue);
  }

  void pixel_by_pixel_blur(struct picture *pic){
    // make new temporary picture to work in
    struct picture tmp;
    init_picture_from_size(&tmp, pic->width, pic->height);

    struct thread_queue thread_store;
    init_queue(&thread_store);

    //TOP AND BOTTOM BOUNDARY PIXELS
    for(int x_coord = 0; x_coord<tmp.width; x_coord++) {
      make_pixel_thread(&bound_pixel_worker, 
                        pic, &tmp, x_coord, 0, 
                        &thread_store);
      make_pixel_thread(&bound_pixel_worker, 
                        pic, &tmp, x_coord, tmp.height - BOUNDARY_WIDTH,
                        &thread_store);
    }

    //LEFT AND RIGHT BOUNDARY PIXELS
    for (int y_coord = BOUNDARY_WIDTH; 
         y_coord<(tmp.height-BOUNDARY_WIDTH); 
         y_coord++) {

      make_pixel_thread(&bound_pixel_worker, 
                        pic, &tmp, 0, y_coord, 
                        &thread_store);
      make_pixel_thread(&bound_pixel_worker, 
                        pic, &tmp, tmp.width-BOUNDARY_WIDTH, y_coord, 
                        &thread_store);
    }

    //INSIDE PIXELS
    for(int x_coord = BOUNDARY_WIDTH; 
        x_coord < tmp.width - BOUNDARY_WIDTH; 
        x_coord++){
      for(int y_coord = BOUNDARY_WIDTH; 
          y_coord < tmp.height - BOUNDARY_WIDTH; 
          y_coord++){

        make_pixel_thread(&single_pixel_worker, pic, &tmp, 
                          x_coord, y_coord, &thread_store);
      }
    }
    clear_threads(&thread_store);    

    clear_picture(pic);
    overwrite_picture(pic, &tmp);
  }



  // Blurring by Sectors (number of sectors = core number) ----------
  
  static void *sector_pixel_worker(void *args) {
    struct sector_work_args *pargs = args;

    for(int x_coord = pargs->start_x; x_coord < pargs->end_x; x_coord++){
      for(int y_coord = pargs->start_y; y_coord < pargs->end_y; y_coord++){
        get_avg_pixel_and_set(pargs->orig_pic, pargs->new_pic, 
                            x_coord, y_coord);
      }
    }
  }

  // All sectors are split vertically
  void sector_core_blur(struct picture *pic, int num_cores){

    // can't have less than 1 thread, and cores assumed to be 32 or less
    if (num_cores < 1 || num_cores > 32) {
      exit(IO_ERROR);
    }

    // make new temporary picture to work in
    struct picture tmp;
    init_picture_from_size(&tmp, pic->width, pic->height);

    int sector_width = floor(pic->width / num_cores);

    pthread_t sector_threads[num_cores];

    struct sector_work_args *sector_worker_args = 
      malloc(sizeof(struct sector_work_args) * num_cores);

    if (sector_worker_args == NULL) {
      exit(MEMORY_ERROR);
    }

    // Main loop to create all threads
    // creates thread arguments
    for (int sector_num = 0; sector_num<num_cores; sector_num++) {
      struct sector_work_args *pargs = &sector_worker_args[sector_num];
      pargs->orig_pic = pic;
      pargs->new_pic = &tmp;
      pargs->start_x = sector_num * sector_width;
      if (sector_num + 1 == num_cores) {
        pargs->end_x = tmp.width;
      } else {
        pargs->end_x = (sector_num + 1) * sector_width;
      }
      
      pargs->start_y = 0; //top of the image
      pargs->end_y = tmp.height;

      pthread_create(&sector_threads[sector_num], NULL,
                    sector_pixel_worker,
                    pargs);
    }

    for (int i = 0; i<num_cores; i++) {
      pthread_join(sector_threads[i], NULL);
    }

    free(sector_worker_args);
    
    // clean-up the old picture and replace with new picture
    clear_picture(pic);
    overwrite_picture(pic, &tmp);
  }



  // Row Blurring Implementation ------------------------------------



  static void make_row_thread(void*(*worker_func)(void*), 
                    struct picture *orig_pic, struct picture *new_pic, 
                    int row_num, struct thread_queue* queue) {
    struct row_work_args *row_params  = 
          malloc_clear_if_need(sizeof(struct pixel_work_args), queue);
    row_params->orig_pic = orig_pic;
    row_params->new_pic = new_pic;
    row_params->row_num = row_num;

    make_thread_and_enqueue(worker_func, row_params, queue);
  }

  static void *row_pixel_worker(void *args) {
    pthread_cleanup_push(thread_cleanup_handler, args);

    struct row_work_args *pargs = (struct row_work_args*) args;

    // iterate over each pixel in the row
    for(int x_coord = 0; x_coord < pargs->new_pic->width; x_coord++) {
      get_avg_pixel_and_set(pargs->orig_pic, pargs->new_pic, 
                            x_coord, pargs->row_num);
    }

    pthread_cleanup_pop(1);
  }

  void row_blur(struct picture *pic){
    // make new temporary picture to work in
    struct picture tmp;
    init_picture_from_size(&tmp, pic->width, pic->height);

    struct thread_queue thread_store;
    init_queue(&thread_store);
    
    // iterate over each pixel in the picture
    // TODO: Make boundary size adjustable since its still based on
    // boundary width 1

    // iterate over each pixel in the picture
    for(int row_num = 0 ; row_num < tmp.height; row_num++){
      make_row_thread(&row_pixel_worker, pic, &tmp, row_num, &thread_store);
    }
    clear_threads(&thread_store);
    
    clear_picture(pic);
    overwrite_picture(pic, &tmp);
  }



  // Column Blurring Implementation ---------------------------------



  static void make_column_thread(void*(*worker_func)(void*), 
                    struct picture *orig_pic, struct picture *new_pic, 
                    int column_num, struct thread_queue* queue) {
    struct column_work_args *column_params  = 
          malloc_clear_if_need(sizeof(struct pixel_work_args), queue);
    column_params->orig_pic = orig_pic;
    column_params->new_pic = new_pic;
    column_params->column_num = column_num;

    make_thread_and_enqueue(worker_func, column_params, queue);
  }


  static void *column_pixel_worker(void *args) {
    pthread_cleanup_push(thread_cleanup_handler, args);

    struct column_work_args *pargs = (struct column_work_args*) args;

    // iterate over each pixel in the column
    for(int y_coord = 0 ; y_coord < pargs->new_pic->height; y_coord++){
      get_avg_pixel_and_set(pargs->orig_pic, pargs->new_pic, 
                            pargs->column_num, y_coord);
    }

    pthread_cleanup_pop(1);
  }

  void column_blur(struct picture *pic){
    // make new temporary picture to work in
    struct picture tmp;
    init_picture_from_size(&tmp, pic->width, pic->height);

    struct thread_queue thread_store;
    init_queue(&thread_store);

    for(int column_num = 0 ; column_num < tmp.width; column_num++){
      make_column_thread(&column_pixel_worker, 
                               pic, &tmp, column_num, 
                               &thread_store);
    }
    clear_threads(&thread_store);
    
    clear_picture(pic);
    overwrite_picture(pic, &tmp);
  }



// Picture Comparison Function (provided in another form) -----------



  bool picture_compare(struct picture* pic1, struct picture* pic2) {
    int width = pic1->width;
    int height = pic1->height;
  
    if(width != pic2->width || height != pic2->height){
      printf("[!] fail - pictures do not have equal dimensions\n");
      return 1;
    }
  
    // iterate over the picture pixel-by-pixel and compare RGB values
    for(int i = 0; i < width; i++){
      for(int j = 0; j < height; j++){
        struct pixel pixel1 = get_pixel(pic1, i, j);
        struct pixel pixel2 = get_pixel(pic2, i, j);
        
        int red_diff = pixel1.red - pixel2.red;
        int green_diff = pixel1.green - pixel2.green;
        int blue_diff = pixel1.blue - pixel2.blue;
        
        if( red_diff > 1 || red_diff < -1 || green_diff > 1 || green_diff < -1 || blue_diff > 1 || blue_diff < -1 ) {
          /*
          printf("[!] fail - pictures not equal at cell (%i,%i)\n", i ,j);
          printf("    pixel1 RGB = \t(%i,\t %i,\t %i)\n", pixel1.red, pixel1.green, pixel1.blue);
          printf("    pixel2 RGB = \t(%i,\t %i,\t %i)\n", pixel2.red, pixel2.green, pixel2.blue);
          */
          return 1;
        }
      }
    }
  
    printf("success - pictures identical!\n");
    return 0;
  }