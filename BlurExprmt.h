#include "myUtils.h"

struct p_work_args {
    struct picture *orig_pic;
    struct picture *new_pic;
    int x_coord;
    int y_coord;
};

struct sector_work_args {
    struct picture *orig_pic;
    struct picture *new_pic;
    int start_x;
    int start_y;
    int end_x;
    int end_y;
};

void blur_picture(struct picture *);
void parallel_blur_picture(struct picture *);
void sector_core_blur(struct picture *, int);
void column_blur(struct picture *);
void row_blur(struct picture *);