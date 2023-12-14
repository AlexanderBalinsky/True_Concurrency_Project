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

struct row_work_args {
    struct picture *orig_pic;
    struct picture *new_pic;
    int row_num;
};

struct column_work_args {
    struct picture *orig_pic;
    struct picture *new_pic;
    int col_num;
};

void blur_picture(struct picture *);
void parallel_blur_picture(struct picture *);
void sector_core_blur(struct picture *, int);
void row_blur(struct picture *);
void column_blur(struct picture *);