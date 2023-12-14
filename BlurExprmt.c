#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include "Utils.h"
#include "Picture.h"
#include "PicProcess.h"
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

  void sector_core_blur_testwrapper(struct picture *pic, const char *unused){
      printf("calling sector core blur\n");
      sector_core_blur(pic, CORE_NUM_FOR_TESTING);
    }

  static void (* const cmds[])(struct picture *, const char *) = { 
    sector_core_blur_testwrapper
    //blur_picture_testwrapper,
    //parallel_blur_testwrapper
    
  };

  // list of all possible picture transformations
  static char *cmd_strings[] = { 
    "sector_core_blur"
  };

  static int no_of_cmds = sizeof(cmds) / sizeof(cmds[0]);

  int main(int argc, char **argv){

    printf("Support Code for Running the Blur Optimisation Experiments... \n");

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


  //UTILITY
  static void thread_cleanup_handler(void* args)
  {
      free(args);
  }


  // Sequential Blurring Implementation (provided)


  // Pixel by Pixel Blurring Implementation


  // Blurring by Sectors (with number of sectors = core number)
  
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
      pargs->end_x= (sector_num + 1) * sector_width;
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
