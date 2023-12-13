#include "PicProcess.h"
#include <pthread.h>

  #define NO_RGB_COMPONENTS 3
  #define BLUR_REGION_SIZE 9
  #define PTHREAD_CREATE_FAIL_CODE 1
  #define PTHREAD_CREATE_SUCCESS_CODE 0
  #define BOUNDARY_WIDTH 1


  void invert_picture(struct picture *pic){
    // iterate over each pixel in the picture
    for(int i = 0 ; i < pic->width; i++){
      for(int j = 0 ; j < pic->height; j++){
        struct pixel rgb = get_pixel(pic, i, j);
        
        // invert RGB values of pixel
        rgb.red = MAX_PIXEL_INTENSITY - rgb.red;
        rgb.green = MAX_PIXEL_INTENSITY - rgb.green;
        rgb.blue = MAX_PIXEL_INTENSITY - rgb.blue;
        
        // set pixel to inverted RBG values
        set_pixel(pic, i, j, &rgb);
      }
    }   
  }

  void grayscale_picture(struct picture *pic){
    // iterate over each pixel in the picture
    for(int i = 0 ; i < pic->width; i++){
      for(int j = 0 ; j < pic->height; j++){
        struct pixel rgb = get_pixel(pic, i, j);
        
        // compute gray average of pixel's RGB values
        int avg = (rgb.red + rgb.green + rgb.blue) / NO_RGB_COMPONENTS;
        rgb.red = avg;
        rgb.green = avg;
        rgb.blue = avg;
        
        // set pixel to gray-scale RBG value
        set_pixel(pic, i, j, &rgb);
      }
    }    
  }

  void rotate_picture(struct picture *pic, int angle){
    // capture current picture size
    int new_width = pic->width;
    int new_height = pic->height;
  
    // adjust output picture size as necessary
    if(angle == 90 || angle == 270){
      new_width = pic->height;
      new_height = pic->width;
    }
    
    // make new temporary picture to work in
    struct picture tmp;
    init_picture_from_size(&tmp, new_width, new_height);     
  
    // iterate over each pixel in the picture
    for(int i = 0 ; i < new_width; i++){
      for(int j = 0 ; j < new_height; j++){
        struct pixel rgb;
        // determine rotation angle and execute corresponding pixel update
        switch(angle){
          case(90):
            rgb = get_pixel(pic, j, new_width -1 - i); 
            break;
          case(180):
            rgb = get_pixel(pic, new_width - 1 - i, new_height - 1 - j);
            break;
          case(270):
            rgb = get_pixel(pic, new_height - 1 - j, i);
            break;
          default:
            printf("[!] rotate is undefined for angle %i (must be 90, 180 or 270)\n", angle);
            clear_picture(&tmp);
            clear_picture(pic);
            exit(IO_ERROR);
        }
        set_pixel(&tmp, i,j, &rgb);
      }
    }
    
    // clean-up the old picture and replace with new picture
    clear_picture(pic);
    overwrite_picture(pic, &tmp);
  }

  void flip_picture(struct picture *pic, char plane){
    // make new temporary picture to work in
    struct picture tmp;
    init_picture_from_size(&tmp, pic->width, pic->height);
    
    // iterate over each pixel in the picture
    for(int i = 0 ; i < tmp.width; i++){
      for(int j = 0 ; j < tmp.height; j++){    
        struct pixel rgb;
        // determine flip plane and execute corresponding pixel update
        switch(plane){
          case('V'):
            rgb = get_pixel(pic, i, tmp.height - 1 - j);
            break;
          case('H'):
            rgb = get_pixel(pic, tmp.width - 1 - i, j);
            break;
          default:
            printf("[!] flip is undefined for plane %c\n", plane);
            clear_picture(&tmp);
            clear_picture(pic);
            exit(IO_ERROR);
        } 
        set_pixel(&tmp, i, j, &rgb);
      }
    }

    // clean-up the old picture and replace with new picture
    clear_picture(pic);
    overwrite_picture(pic, &tmp);
  }

  void blur_picture(struct picture *pic){
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

  static void thread_cleanup_handler(void* args)
  {
      free(args);
  }
  
  static void *single_pixel_worker(void *args) {
    pthread_cleanup_push(thread_cleanup_handler, args);
    struct p_work_args *pargs = (struct p_work_args*) args;

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
    struct p_work_args *pargs = (struct p_work_args*) args;

    struct pixel rgb = 
      get_pixel(pargs->orig_pic, pargs->x_coord, pargs->y_coord);
    set_pixel(pargs->new_pic, pargs->x_coord, pargs->y_coord, &rgb);

    pthread_cleanup_pop(1);
  }

  static void thread_join_then_return(struct thread_queue* queue) {
    struct thread_node *node_to_rm = dequeue(queue);
    if (node_to_rm == NULL) {
      return;
    }
    pthread_t *thread_to_join = node_to_rm->thread;
    free(node_to_rm);
    pthread_join(*thread_to_join, NULL);
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

  static void make_pixel_thread_loop(void*(*worker_func)(void*), 
                    struct picture *orig_pic, struct picture *new_pic, 
                    int x_coord, int y_coord, struct thread_queue* queue) {
    pthread_t pixel_worker;
    struct p_work_args *pixel_params  = 
          malloc_clear_if_need(sizeof(struct p_work_args), queue);
    pixel_params->orig_pic = orig_pic;
    pixel_params->new_pic = new_pic;
    pixel_params->x_coord = x_coord;
    pixel_params->y_coord = y_coord;

    int pthread_create_code = PTHREAD_CREATE_FAIL_CODE;
    while (pthread_create_code != PTHREAD_CREATE_SUCCESS_CODE) {
      pthread_create_code = pthread_create(&pixel_worker, NULL, 
                    worker_func, 
                    pixel_params);
      thread_join_then_return(queue);
    }

    struct thread_node *new_node = 
          malloc_clear_if_need(sizeof(struct thread_node), queue);
    enqueue(queue, &pixel_worker, new_node);
  }

  void parallel_blur_picture(struct picture *pic){
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
      fprintf(stderr, "\nNew Pixel Worker at: x:%d y:%d\n", i, 0);
      fprintf(stderr, "\nNew Pixel Worker at: x:%d y:%d\n", i, tmp.height);
      make_pixel_thread_loop(&bound_pixel_worker, 
                               pic, &tmp, i, 0, 
                               &thread_store);
      make_pixel_thread_loop(&bound_pixel_worker, 
                               pic, &tmp, i, tmp.height - BOUNDARY_WIDTH, 
                               &thread_store);
    }

    //LEFT AND RIGHT BOUNDARY PIXELS
    for(int j = BOUNDARY_WIDTH; j<(tmp.height-BOUNDARY_WIDTH); j++) {
      fprintf(stderr, "\nNew Pixel Worker at: x:%d y:%d\n", 0, j);
      fprintf(stderr, "\nNew Pixel Worker at: x:%d y:%d\n", tmp.width, j);
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
        pthread_t pixel_worker;

        make_pixel_thread_loop(&single_pixel_worker, 
                               pic, &tmp, i, j, 
                               &thread_store);

        fprintf(stderr, "\nNew Pixel Worker at: x:%d y:%d\n", i, j);
      }
    }

    clear_threads(&thread_store);
    
    // clean-up the old picture and replace with new picture
    clear_picture(pic);
    overwrite_picture(pic, &tmp);
  }
