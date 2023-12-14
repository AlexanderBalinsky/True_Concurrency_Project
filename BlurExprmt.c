#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include "Utils.h"
#include "Picture.h"
#include "BlurExprmt.h"
#include <math.h>
#include <stdlib.h>

#define NO_RGB_COMPONENTS 3
#define BLUR_REGION_SIZE 9
#define PTHREAD_CREATE_FAIL_CODE 1
#define PTHREAD_CREATE_SUCCESS_CODE 0
#define BOUNDARY_WIDTH 1
#define MEMORY_ERROR -2
#define CORE_NUM_FOR_TESTING 8


// ---------- MAIN PROGRAM ---------- \\

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
    column_blur_testwrapper
    //optimised_row_column_blur_testwrapper
  };

  // list of all possible picture transformations
  static char *cmd_strings[] = { 
    "seq-blur",
    "pixel-by-pixel-blur",
    "sector-core-blur",
    "row-blur",
    "column-blur"
  };

  static int no_of_cmds = sizeof(cmds) / sizeof(cmds[0]);

  int main(int argc, char **argv){

    // capture and check command line arguments
    const char * filename = argv[1];
    const char * target_file = argv[2];
    const char * process = argv[3];
    const char * extra_arg = argv[4];
    
    if(filename == NULL || target_file == NULL || process == NULL){
      printf("[!] insufficient command line arguments provided\n");
      exit(IO_ERROR);
    }        
  
    printf("  filename  = %s\n", filename);
    printf("  target    = %s\n", target_file);
    printf("  process   = %s\n", process);
    printf("  extra arg = %s\n", extra_arg);
  
    printf("\n");
  
    // create original image object
    struct picture pic;
    if(!init_picture_from_file(&pic, filename)){
      exit(IO_ERROR);   
    }    
  
    // identify the picture transformation to run
    int cmd_no = 0;
    while(cmd_no < no_of_cmds && strcmp(process, cmd_strings[cmd_no])){
      cmd_no++;
    }
  
    // IO error check
    if(cmd_no == no_of_cmds){
      printf("[!] invalid process requested: %s is not defined\n    aborting...\n", process);  
      clear_picture(&pic);
      exit(IO_ERROR);   
    }
  
    // dispatch to appropriate picture transformation function
    cmds[cmd_no](&pic, extra_arg);

    // save resulting picture and report success
    save_picture_to_file(&pic, target_file);
    printf("-- picture processing complete --\n");
    
    clear_picture(&pic);
    return 0;
  }



  //UTILITY ---------------------------------------------------------



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

  static void make_pixel_thread_loop(void*(*worker_func)(void*), 
                    struct picture *orig_pic, struct picture *new_pic, 
                    int x_coord, int y_coord, struct thread_queue* queue) {
    pthread_t *pixel_worker = malloc_clear_if_need(sizeof(pthread_t), queue);
    struct pixel_work_args *pixel_params  = 
          malloc_clear_if_need(sizeof(struct pixel_work_args), queue);
    pixel_params->orig_pic = orig_pic;
    pixel_params->new_pic = new_pic;
    pixel_params->x_coord = x_coord;
    pixel_params->y_coord = y_coord;

    int pthread_create_code = PTHREAD_CREATE_FAIL_CODE;
    while (pthread_create_code != PTHREAD_CREATE_SUCCESS_CODE) {
      pthread_create_code = pthread_create(pixel_worker, NULL, 
                    worker_func, 
                    pixel_params);
      thread_join_then_return(queue);
    }

    struct thread_node *new_node = 
          malloc_clear_if_need(sizeof(struct thread_node), queue);
    enqueue(queue, pixel_worker, new_node);
  }

  void pixel_by_pixel_blur(struct picture *pic){
    // make new temporary picture to work in
    struct picture tmp;
    init_picture_from_size(&tmp, pic->width, pic->height);

    struct thread_queue thread_store;
    init_queue(&thread_store);
    
    // iterate over each pixel in the picture
    // TODO: Make boundary size adjustable since its still based on
    // boundary width 1

    //TOP AND BOTTOM BOUNDARY PIXELS
    for(int i = 0; i<tmp.width; i++) {
      make_pixel_thread_loop(&bound_pixel_worker, 
                               pic, &tmp, i, 0, 
                               &thread_store);
      make_pixel_thread_loop(&bound_pixel_worker, 
                               pic, &tmp, i, tmp.height - BOUNDARY_WIDTH, 
                               &thread_store);
    }

    //LEFT AND RIGHT BOUNDARY PIXELS
    for(int j = BOUNDARY_WIDTH; j<(tmp.height-BOUNDARY_WIDTH); j++) {
      make_pixel_thread_loop(&bound_pixel_worker, 
                               pic, &tmp, 0, j, 
                               &thread_store);
      make_pixel_thread_loop(&bound_pixel_worker, 
                               pic, &tmp, tmp.width-BOUNDARY_WIDTH, j, 
                               &thread_store);
    }

    //INSIDE PIXELS
    for(int i = BOUNDARY_WIDTH ; i < tmp.width - BOUNDARY_WIDTH; i++){
      for(int j = BOUNDARY_WIDTH ; j < tmp.height - BOUNDARY_WIDTH; j++){
        make_pixel_thread_loop(&single_pixel_worker, 
                               pic, &tmp, i, j, 
                               &thread_store);
      }
    }
    clear_threads(&thread_store);    

    clear_picture(pic);
    overwrite_picture(pic, &tmp);
  }



  // Blurring by Sectors (number of sectors = core number) ----------




  //TODO ADD WAY TO HANDLE NON EVEN PIXELS BUT MAKING LAST
  // SECTOR GO TO THE END RATHER THAN FLOOR() SPLIT VALUE
  
  static void *sector_pixel_worker(void *args) {
    struct sector_work_args *pargs = args;

    for(int i = pargs->start_x; i < pargs->end_x; i++){
      for(int j = pargs->start_y; j < pargs->end_y; j++){
      
        // set-up a local pixel on the stack
        struct pixel rgb = get_pixel(pargs->orig_pic, i, j);
        
        // don't need to modify boundary pixels
        if(i != 0 && j != 0 && i != pargs->orig_pic->width - BOUNDARY_WIDTH
            && j != pargs->orig_pic->height - BOUNDARY_WIDTH){
        
          // set up running RGB component totals for pixel region
          int sum_red = rgb.red;
          int sum_green = rgb.green;
          int sum_blue = rgb.blue;
      
          // check the surrounding pixel region
          for(int n = -1; n <= 1; n++){
            for(int m = -1; m <= 1; m++){
              if(n != 0 || m != 0){
                rgb = get_pixel(pargs->orig_pic, i+n, j+m);
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
        set_pixel(pargs->new_pic, i, j, &rgb);
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



  static void make_row_thread_loop(void*(*worker_func)(void*), 
                    struct picture *orig_pic, struct picture *new_pic, 
                    int row_num, struct thread_queue* queue) {
    pthread_t *pixel_worker = malloc_clear_if_need(sizeof(pthread_t), queue);
    struct row_work_args *pixel_params  = 
          malloc_clear_if_need(sizeof(struct pixel_work_args), queue);
    pixel_params->orig_pic = orig_pic;
    pixel_params->new_pic = new_pic;
    pixel_params->row_num = row_num;


    int pthread_create_code = PTHREAD_CREATE_FAIL_CODE;
    while (pthread_create_code != PTHREAD_CREATE_SUCCESS_CODE) {
      pthread_create_code = pthread_create(pixel_worker, NULL, 
                    worker_func, 
                    pixel_params);
      thread_join_then_return(queue);
    }

    struct thread_node *new_node = 
          malloc_clear_if_need(sizeof(struct thread_node), queue);
    enqueue(queue, pixel_worker, new_node);
  }

  static void *row_pixel_worker(void *args) {
    pthread_cleanup_push(thread_cleanup_handler, args);

    struct row_work_args *pargs = (struct row_work_args*) args;

    // iterate over each pixel in the row
    for(int x_coord = 0 ; x_coord < pargs->new_pic->width; x_coord++){
    
      // set-up a local pixel on the stack
      struct pixel rgb = get_pixel(pargs->orig_pic, x_coord, pargs->row_num);
      
      // don't need to modify boundary pixels
      if(x_coord != 0 && pargs->row_num != 0 && 
        x_coord != pargs->orig_pic->width - 1 && 
        pargs->row_num != pargs->orig_pic->height - 1){
      
        // set up running RGB component totals for pixel region
        int sum_red = rgb.red;
        int sum_green = rgb.green;
        int sum_blue = rgb.blue;
    
        // check the surrounding pixel region
        for(int n = -1; n <= 1; n++){
          for(int m = -1; m <= 1; m++){
            if(n != 0 || m != 0){
              rgb = get_pixel(pargs->orig_pic, x_coord+n, pargs->row_num+m);
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
      set_pixel(pargs->new_pic, x_coord, pargs->row_num, &rgb);
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
      make_row_thread_loop(&row_pixel_worker, 
                               pic, &tmp, row_num, 
                               &thread_store);
    }
    clear_threads(&thread_store);
    
    clear_picture(pic);
    overwrite_picture(pic, &tmp);
  }



  // Column Blurring Implementation ---------------------------------



  static void make_column_thread_loop(void*(*worker_func)(void*), 
                    struct picture *orig_pic, struct picture *new_pic, 
                    int column_num, struct thread_queue* queue) {
    pthread_t *pixel_worker = malloc_clear_if_need(sizeof(pthread_t), queue);
    struct column_work_args *pixel_params  = 
          malloc_clear_if_need(sizeof(struct pixel_work_args), queue);
    pixel_params->orig_pic = orig_pic;
    pixel_params->new_pic = new_pic;
    pixel_params->column_num = column_num;


    int pthread_create_code = PTHREAD_CREATE_FAIL_CODE;
    while (pthread_create_code != PTHREAD_CREATE_SUCCESS_CODE) {
      pthread_create_code = pthread_create(pixel_worker, NULL, 
                    worker_func, 
                    pixel_params);
      thread_join_then_return(queue);
    }

    struct thread_node *new_node = 
          malloc_clear_if_need(sizeof(struct thread_node), queue);
    enqueue(queue, pixel_worker, new_node);
  }


  static void *column_pixel_worker(void *args) {
    pthread_cleanup_push(thread_cleanup_handler, args);

    struct column_work_args *pargs = (struct column_work_args*) args;

    // iterate over each pixel in the row
    for(int y_coord = 0 ; y_coord < pargs->new_pic->height; y_coord++){
    
      // set-up a local pixel on the stack
      struct pixel rgb = get_pixel(pargs->orig_pic, pargs->column_num, y_coord);
      
      // don't need to modify boundary pixels
      if(pargs->column_num != 0 && y_coord != 0 && 
        pargs->column_num != pargs->orig_pic->width - 1 && 
        y_coord != pargs->orig_pic->height - 1){
      
        // set up running RGB component totals for pixel region
        int sum_red = rgb.red;
        int sum_green = rgb.green;
        int sum_blue = rgb.blue;
    
        // check the surrounding pixel region
        for(int n = -1; n <= 1; n++){
          for(int m = -1; m <= 1; m++){
            if(n != 0 || m != 0){
              rgb = get_pixel(pargs->orig_pic, pargs->column_num+n, y_coord+m);
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
      set_pixel(pargs->new_pic, pargs->column_num, y_coord, &rgb);
    }

    pthread_cleanup_pop(1);
  }

  void column_blur(struct picture *pic){
    // make new temporary picture to work in
    struct picture tmp;
    init_picture_from_size(&tmp, pic->width, pic->height);

    struct thread_queue thread_store;
    init_queue(&thread_store);
    
    // iterate over each pixel in the picture
    // TODO: Make boundary size adjustable since its still based on
    // boundary width 1

    // iterate over each pixel in the picture
    for(int column_num = 0 ; column_num < tmp.width; column_num++){
      make_column_thread_loop(&column_pixel_worker, 
                               pic, &tmp, column_num, 
                               &thread_store);
    }
    clear_threads(&thread_store);
    
    clear_picture(pic);
    overwrite_picture(pic, &tmp);
  }