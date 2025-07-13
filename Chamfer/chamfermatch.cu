#include "chamfermatch.h"
#include <stdio.h>
#include <algorithm>

// Chamfer version 2 should include CUB library
//#define ENABLE_CHAMFER_VERSION2

#ifdef ENABLE_CHAMFER_VERSION2
#define CUB_IGNORE_DEPRECATED_COMPILER
#define CUB_STDERR
#include <cub/block/block_load.cuh>
#include <cub/block/block_store.cuh>
#include <cub/block/block_radix_sort.cuh>
#include <cub/cub.cuh>
///// Device-wise sorting
#include <cub/util_allocator.cuh>
#include <cub/device/device_radix_sort.cuh>
#include <cub/../test/test_util.h>
#endif

__global__ void WarmUpGpu(){
    int index_x = threadIdx.x + blockIdx.x * blockDim.x;
    float ia, ib;
    ia = ib = 0.0f;
    ib += ia + index_x;
}

// NOTE: CUDA cannot use reference pointer as sender -> int& (X) int (O)
// Because the pointers must be copied to GPU memory first.
__global__ void DilateAndMoveMat(uchar* input_mat_data_8uc1, uchar* output_mat_data_8uc1,
                                 short src_w, short src_h,
                                 short kernel_w, short kernel_h,
                                 short resize_scale,
                                 short padding_w, short padding_h)
{
    ///
    ///
    ///
    int index_x = threadIdx.x + blockIdx.x * blockDim.x; // blockDim: thread_size of one block
    int index_y = threadIdx.y + blockIdx.y * blockDim.y;
    int index_z = threadIdx.z + blockIdx.z * blockDim.z;

    // [OPTIMIZED] Commented out debug printfs for industrial code cleanliness
    // printf("recomputed_kernel_w: %d\n", (int)src_w);
    // printf("recomputed_kernel_h: %d\n", (int)src_h);
    // Out of range
    if(index_x >= src_w + padding_w ||
       index_y >= src_h + padding_h ){
        // [OPTIMIZED] Commented out debug printfs for industrial code cleanliness
        // printf("recomputed_kernel_w: %d\n", (int)src_w);
        // printf("recomputed_kernel_h: %d\n", (int)src_h);
        return;
    }
    // [OPTIMIZED] Commented out debug printfs for industrial code cleanliness
    // printf("----------------- DilateAndMoveMat -------------------\n");

    // index in the shared memory
    int current_input_img_index = (index_x - padding_w) + (index_y - padding_h) * src_w;

    // Target output data information
    int target_width = src_w * resize_scale + (padding_w * 2);
    int target_height = src_h * resize_scale + (padding_h * 2);
    int target_index_x = index_x;// * resize_scale + padding_w;
    int target_index_y = index_y;// * resize_scale + padding_h;
    int target_index = target_index_x + target_index_y * target_width;
    output_mat_data_8uc1[target_index] = 0;

    short recomputed_kernel_w = (src_w - index_x + padding_w < kernel_w) ? src_w - index_x + padding_w : kernel_w;
    short recomputed_kernel_h = (src_h - index_y + padding_h < kernel_h) ? src_h - index_y + padding_h : kernel_h;
    short recomputed_start_w = (index_x - padding_w < 0) ? padding_w - index_x : 0;
    short recomputed_start_h = (index_y - padding_h < 0) ? padding_h - index_y : 0;
    // [OPTIMIZED] Commented out debug printfs for industrial code cleanliness
    // printf("----------------- DilateAndMoveMat -------------------\n");
    // printf("recomputed_kernel_w: %d\n", (int)recomputed_kernel_w);
    // printf("recomputed_kernel_h: %d\n", (int)recomputed_kernel_h);

    int index_searching = 0;
    bool found = false;
    for (int i = recomputed_start_h; i < recomputed_kernel_h; ++i) {
        for (int j = recomputed_start_w; j < recomputed_kernel_w; ++j) {
            index_searching = current_input_img_index + j + (i * src_w);
            // If there is any contours in the area, draw a point
            if(input_mat_data_8uc1[index_searching] > 127){
                output_mat_data_8uc1[target_index] = 255;
                // [OPTIMIZED] Commented out debug printfs for industrial code cleanliness
                // printf("----------------- DilateAndMoveMat: a point is drawn -------------------\n");
                found = true;
                break;
            }
        }
        if(found) break;
    }
}

__global__ void ReduceContourMat(uchar* input_mat_data_8uc1, uchar* output_mat_data_8uc1,
                                 short src_w, short src_h,
                                 short skip_kernel_size)
{
    ///
    ///
    ///
    int index_x = threadIdx.x + blockIdx.x * blockDim.x; // blockDim: thread_size of one block
    int index_y = threadIdx.y + blockIdx.y * blockDim.y;
    int index_z = threadIdx.z + blockIdx.z * blockDim.z;


    // Target output data information
    int target_index_x = index_x * skip_kernel_size;
    int target_index_y = index_y * skip_kernel_size;
    int target_index = target_index_x + target_index_y * src_w;

    // Out of range
    if(target_index_x >= src_w ||
       target_index_y >= src_h ){
        return;
    }

    int index_searching = 0;
    bool found = false;
    for (int i = 0; i < skip_kernel_size; ++i) {
        if(target_index_y + i >= src_h) continue;
        for (int j = 0; j < skip_kernel_size; ++j) {
            if(target_index_x + j >= src_w) continue;
            index_searching = target_index + j + (i * src_w);

            if(found == false){
                if(input_mat_data_8uc1[index_searching] > 127){
                    output_mat_data_8uc1[index_searching] = 255;
                    found = true;
                }
                else{
                    output_mat_data_8uc1[index_searching] = 0;
                }
            }
            else{
                output_mat_data_8uc1[index_searching] = 0;
            }
        }
    }
}


__global__ void ChamferMatch(float* flat_source_dist_transfrom,
                             short* source_candidate_col, short* source_candidate_row,
                             int source_width, int source_height,
                             int num_templates,
                             short* flat_template_non_zero_col, short* flat_template_non_zero_row, int* template_num_non_zero,
                             float* flat_template_weight, short use_weight,
                             int length_source_candidate, int source_num_candidate_width, int source_num_candidate_height,
                             float* flat_score_map, int* flat_score_map_index)
{
    ///
    /// flat_template_non_zero_col = [scaling x rotation][num_non_zeros]
    ///
    /// num_templates: angles + scales
    ///
    /// (at the first stage) source_candidate_ is obtained by findNonZero function
    /// source_candidate_ is 1-d pointer. In order to use x and y threads at the same time, they should be separated to 2-d
    /// size of source_candidate_ =  max(index_x) * max(index_y)
    /// For example) size is 25 -> can be 5 x 5
    /// index_x: index of source_candidate_
    /// index_y: index of source_candidate_
    ///
    ///
    /// x: column, y: row, z: ?
    int index_x = threadIdx.x + blockIdx.x * blockDim.x; // blockDim: thread_size of one block
    int index_y = threadIdx.y + blockIdx.y * blockDim.y;
    int index_z = threadIdx.z + blockIdx.z * blockDim.z;

    // Out of range
    if(index_x >= source_num_candidate_width ||
       index_y >= source_num_candidate_height ||
       index_x + index_y * source_num_candidate_width >= length_source_candidate){
        // [OPTIMIZED] Commented out debug printfs for industrial code cleanliness
        // printf("Out of range - x: %d, y: %d\n", index_x, index_y);
        return;
    }

    // Get the thread index
    int candidate_index = index_x + index_y * source_num_candidate_width;
    // Get the target position on the source image
    int current_index = source_candidate_col[candidate_index] + source_candidate_row[candidate_index] * source_width;

    int template_index = 0;
    float tmp_val;

    for(short i = 0; i < num_templates; i ++){
        flat_score_map[candidate_index + i*length_source_candidate] = 0;
        flat_score_map_index[candidate_index + i*length_source_candidate] = candidate_index + i*length_source_candidate;
        for(short j= 0; j < template_num_non_zero[i]; j ++){
            int non_zero_index = flat_template_non_zero_col[template_index] + flat_template_non_zero_row[template_index] * source_width;
            // Jang 20220330
            if(use_weight == 1){
                float tmp_val_0 = flat_source_dist_transfrom[current_index + non_zero_index];
                float tmp_val_1 = flat_template_weight[template_index];
                tmp_val = tmp_val_0 * tmp_val_1;
                flat_score_map[candidate_index + i*length_source_candidate] += tmp_val;
                // [OPTIMIZED] Commented out debug printfs for industrial code cleanliness
                // if(flat_template_weight[template_index] > 1 || flat_template_weight[template_index] <= 0){
                //     printf("HERE:::::: flat_template_weight: %f \nflat_source_dist_transfrom: %f\n", flat_template_weight[template_index], flat_source_dist_transfrom[current_index + non_zero_index]);
                //     printf("HERE:::::: flat_template_weight: %f \n", flat_template_weight[template_index]);
                // }
            }
            else{
                flat_score_map[candidate_index + i*length_source_candidate] += flat_source_dist_transfrom[current_index + non_zero_index];
            }

            template_index++;
        }
        // average score
        flat_score_map[candidate_index + i*length_source_candidate] /= (float)template_num_non_zero[i];

    }
}


__global__ void ChamferMatch2(float* flat_source_dist_transfrom,
                             short* source_candidate_col, short* source_candidate_row,
                             int source_width, int source_height,
                             int num_templates,
                             short* flat_template_non_zero_col, short* flat_template_non_zero_row, int* template_num_non_zero,
                             float* flat_template_weight, short use_weight,
                             int length_source_candidate, int source_num_candidate_width, int source_num_candidate_height,
                             float* flat_score_map, int* flat_score_map_index)
{
    ///
    /// flat_template_non_zero_col = [scaling x rotation][num_non_zeros]
    ///
    /// num_templates: angles + scales
    ///
    /// (at the first stage) source_candidate_ is obtained by findNonZero function
    /// source_candidate_ is 1-d pointer. In order to use x and y threads at the same time, they should be separated to 2-d
    /// size of source_candidate_ =  max(index_x) * max(index_y)
    /// For example) size is 25 -> can be 5 x 5
    /// index_x: index of source_candidate_
    /// index_y: index of source_candidate_
    ///
    /// source_width: width of the image; including expanding(padding) option.
    ///
    /// x: column, y: row, z: ?
    ///
    ///
    int index_x = threadIdx.x + blockIdx.x * blockDim.x; // blockDim: thread_size of one block
    int index_y = threadIdx.y + blockIdx.y * blockDim.y;
    int index_z = threadIdx.z + blockIdx.z * blockDim.z;


    // Out of range
    if(index_x >= source_num_candidate_width ||
       index_y >= source_num_candidate_height ||
       index_x + index_y * source_num_candidate_width >= length_source_candidate){
//        printf("Out of range - x: %d, y: %d\n", index_x, index_y);
        return;
    }

    // Get the thread index
    int candidate_index = index_x + index_y * source_num_candidate_width;
    // Get the target position on the source image
    int current_index = source_candidate_col[candidate_index] + source_candidate_row[candidate_index] * source_width;

    int template_index = 0;
    int error_count_template_weight_0 = 0;
    int error_count_template_weight_1 = 0;
    float tmp_val;

    for(int i = 0; i < num_templates; i ++){
        flat_score_map[candidate_index + i*length_source_candidate] = 0;
        flat_score_map_index[candidate_index + i*length_source_candidate] = candidate_index + i*length_source_candidate;
        for(int j= 0; j < template_num_non_zero[i]; j ++){
            int non_zero_index = flat_template_non_zero_col[template_index] + flat_template_non_zero_row[template_index] * source_width;
            if(use_weight == 1){
                float tmp_val_0 = flat_source_dist_transfrom[current_index + non_zero_index];
                float tmp_val_1 = flat_template_weight[template_index];
                tmp_val = tmp_val_0 * tmp_val_1;
                flat_score_map[candidate_index + i*length_source_candidate] += tmp_val;
                // [OPTIMIZED] Commented out debug printfs and error counters for industrial code cleanliness
                if(flat_template_weight[template_index] > 1){
                    // error_count_template_weight_0++;
                    // printf("HERE:::::: flat_template_weight: %f \n", flat_template_weight[template_index]);
                }
                if(flat_template_weight[template_index] <= 0){
                    // error_count_template_weight_1++;
                    // printf("HERE:::::: flat_template_weight: %f \nflat_source_dist_transfrom: %f\n", flat_template_weight[template_index], flat_source_dist_transfrom[current_index + non_zero_index]);
                }
            }
            else{
                flat_score_map[candidate_index + i*length_source_candidate] += flat_source_dist_transfrom[current_index + non_zero_index];
            }
            template_index++;
        }
        // average score
        flat_score_map[candidate_index + i*length_source_candidate] /= (float)template_num_non_zero[i];

    }
    // [OPTIMIZED] Commented out debug printfs for industrial code cleanliness
    // printf("HERE:::::: error_count_template_weight_0: %d \n", error_count_template_weight_0);
    // printf("HERE:::::: error_count_template_weight_1: %d \n", error_count_template_weight_1);
}

__global__ void TestAccessMem(float* shared_mem){
    int index_x = threadIdx.x + blockIdx.x * blockDim.x;

    if(index_x > 100) return;
    for(int i = 0; i < 100; i ++){
        shared_mem[i + index_x] = 100;
    }


}

__global__ void SortChamferScores(float* score_candidates, int* score_candidates_index,
                             float* result_score_candidates, int* result_score_candidates_index,
                             int num_scores,
                             int target_num_scores, int divider,
                             float threshold_score, float max_score){
    ///
    /// num_scores: the number of score candidates
    /// target_num_scores: the number of result scores in one thread
    /// divider: divide num_scores by target_num_scores (the number of sorting blocks)
    ///
    /// (num_scores) / (divider) = a block size of the sorting
    /// (target_num_scores) * (num_scores) / (divider) = the result size
    ///
    /// threshold_score: threshold of scores
    /// max_score: if the score is higher than threshold_score, set the score as max_score
    ///


    int index_x = threadIdx.x + blockIdx.x * blockDim.x; // index_x: index of current sorting block
//    int index_y = threadIdx.y + blockIdx.y * blockDim.y;

    ///
    /// if num_scores = 150000
    /// sorting_block_size = 150000 / 150 = 1000
    ///
    int sorting_block_size = (num_scores / divider);    // At the last block, its size can be smaller 'sorting_block_size'
    if(num_scores % divider > 0) sorting_block_size++;  // ceil function

    // Out of range
    if(index_x >= divider){
        return;
    }

    int current_block_index = index_x * sorting_block_size;
    int current_result_index = index_x * target_num_scores;
    // init scores
    for(int i = 0; i < target_num_scores; i ++){
        result_score_candidates[i + current_result_index] = max_score;
    }


    for(int i = 0; i < sorting_block_size; i ++){
        if((i + current_block_index) >=  num_scores){
            break;
        }

        if(score_candidates[i + current_block_index] > threshold_score){
            continue;
        }

        if(score_candidates[i + current_block_index] == 0){
            continue;
        }


        for(int j = 0; j < target_num_scores; j ++){
            if(result_score_candidates[j + current_result_index] > score_candidates[i + current_block_index] ){
                // [OPTIMIZED] Commented out debug printfs for industrial code cleanliness
                // if(score_candidates[i + current_block_index] == 0){
                //     printf("HERE:::::: threadIdx.x: %d, threadIdx.y: %d \n ", threadIdx.x, threadIdx.y);
                // }
                // Make a space for new value
                for(int k = target_num_scores - 1; k > j; k--){
                    result_score_candidates[k + current_result_index] = result_score_candidates[k - 1 + current_result_index];
                    result_score_candidates_index[k + current_result_index] = result_score_candidates_index[k - 1 + current_result_index];
                }
                // Insert the new value
                result_score_candidates[j + current_result_index] = score_candidates[i + current_block_index] ;
                result_score_candidates_index[j + current_result_index] = score_candidates_index[i + current_block_index];

                // [OPTIMIZED] Commented out debug printfs for industrial code cleanliness
                // if(target_num_scores == 10){
                //     printf("HERE:::::: %f, %d, %d\n", result_score_candidates[j + current_result_index], i, j);
                // }
                // if(result_score_candidates[j + current_result_index] > 10000000){
                //     printf("HERE:::::: %f, %d, %d\n", result_score_candidates[j + current_result_index], i, j);
                //     // printf(" %f, %d, %d, %d\n", result_score_candidates[j + current_result_index+ 1], index_x, divider, num_scores);
                // }
                break;
            }
        }
    }

}


void CudaChamfer::SortScores(float* cuda_flat_score_map, int* cuda_flat_score_map_index,
                float* score_candidates_result, int* score_candidates_result_index,
                int num_scores, int target_num_scores, int steps,
                bool is_fine_detection){
    /// Input
    /// cuda_flat_score_map: scores
    /// cuda_flat_score_map_index: index of scores
    /// num_scores: the number of scores(length of array)
    /// target_num_scores: the target score lists. ex) Top 10 scores -> put 10
    /// steps: how many times sorting repeats.
    /// is_fine_detection: original matching(true) or downsized matching (false)
    ///
    /// Output
    /// score_candidates_result: the result scores of sorting
    /// score_candidates_result_index: ..



    // [OPTIMIZED] Add runtime parameter validation and error handling
    if (num_scores <= 1 || target_num_scores <= 0 || num_scores < target_num_scores || steps <= 0 || num_scores <= pow(10, steps) * target_num_scores) {
        fprintf(stderr, "[ERROR] Invalid parameters in SortScores.\n");
        return;
    }

    float threshold_score;
    float max_score;
    if(is_fine_detection){
        threshold_score = threshold_score_orig;
        max_score = max_score_orig;
    }
    else{
        threshold_score = threshold_score_py;
        max_score = max_score_py;
    }

    int num_scores_digits;
    int target_num_scores_digits = 0;
    int step_size;
    for(int i = 0; i < 1000; i++){
        double pow_val = pow(10, i);
        if(pow_val >= target_num_scores
           && target_num_scores_digits == 0){
            target_num_scores_digits = i - 1;
        }
        if(pow_val >= num_scores){
            num_scores_digits = i - 1;

            step_size = (float)(i - 1 - target_num_scores_digits) / (float)steps;
            break;
        }
    }
#ifdef DEBUG_MODE
    lw << "num_scores: " << num_scores << lw.endl;
    lw << "num_scores_digits: " << num_scores_digits << lw.endl;
    lw << "step_size: " << step_size << lw.endl;
#endif
    int num_score_blocks;
    int target_num_scores_for_each_step;
    int block_size = 32;    // max: 32

    dim3 threads_in_a_block;
    dim3 num_blocks;;

    float* cuda_score_candidates = cuda_flat_score_map;
    int* cuda_score_candidates_index = cuda_flat_score_map_index;

    int threads_digits;
    int num_threads;
    int target_num_per_thread;
    for(int i = 0; i < steps; i++){
        target_num_scores_for_each_step = num_scores / pow(10, step_size);    // 10,000,000 / 100 = 100,000 = divider * target_num_scores
        //  600,000 / 100 = 6,000 -> 1000  1st
        // 1000 / 100 = 10  2nd
//        lw << "target_num_scores_for_each_step: " << target_num_scores_for_each_step << lw.endl;
        if(target_num_scores_for_each_step > pow(10, step_size) * pow(10, target_num_scores_digits) ){
            num_scores_digits = num_scores_digits - step_size;  // 7 - 3
//            threads_digits = ceil((float)num_scores_digits / 2.0);       // 5 / 2 = 3
//            num_threads = pow(10, threads_digits);              // 1000
//            target_num_per_thread = pow(10, num_scores_digits - threads_digits);    // 10^(5-3) = 100

            target_num_per_thread = 10;    // 10^(5-3) = 100
            threads_digits = num_scores_digits - 1;       // 5 / 2 = 3
            num_threads = pow(10, threads_digits);              // 1000

        }
        else{
            num_threads = 1;
            target_num_per_thread = target_num_scores;
        }
        target_num_scores_for_each_step = target_num_per_thread * num_threads;
#ifdef DEBUG_MODE
        lw << "target_num_scores_for_each_step: " << target_num_scores_for_each_step << lw.endl;
        lw << "target_num_per_thread: " << target_num_per_thread << lw.endl;
        lw << "num_scores_digits: " << num_scores_digits << lw.endl;
        lw << "num_threads: " << num_threads << lw.endl;
#endif

        float* cuda_score_candidates_result = NULL;
        int* cuda_score_candidates_index_result = NULL;
        if (cudaMalloc(&cuda_score_candidates_result, target_num_scores_for_each_step * sizeof(float)) != cudaSuccess) {
            fprintf(stderr, "[ERROR] cudaMalloc failed for cuda_score_candidates_result.\n");
            return;
        }
        if (cudaMalloc(&cuda_score_candidates_index_result, target_num_scores_for_each_step * sizeof(int)) != cudaSuccess) {
            fprintf(stderr, "[ERROR] cudaMalloc failed for cuda_score_candidates_index_result.\n");
            cudaFree(cuda_score_candidates_result);
            return;
        }

        block_size = num_scores / num_threads;
        if(block_size > 32) block_size = 32;
        if(block_size < 1) block_size = 1;
        threads_in_a_block = dim3(block_size);
        num_score_blocks = (int)((num_threads + block_size - 1) / block_size);
        num_blocks = dim3(num_score_blocks);

//        cudaDeviceSynchronize();
        // [OPTIMIZED] Add CUDA error checking for kernel launch
        SortChamferScores<<<num_blocks, threads_in_a_block>>>(cuda_score_candidates, cuda_score_candidates_index,
                                                             cuda_score_candidates_result, cuda_score_candidates_index_result,
                                                             num_scores,
                                                             target_num_per_thread, num_threads,
                                                             threshold_score, max_score);
        cudaError_t err = cudaGetLastError();
        if (err != cudaSuccess) {
            fprintf(stderr, "[ERROR] SortChamferScores kernel launch failed: %s\n", cudaGetErrorString(err));
            cudaFree(cuda_score_candidates_result);
            cudaFree(cuda_score_candidates_index_result);
            return;
        }
//        cudaDeviceSynchronize();
        FreeCudaMemory(&cuda_score_candidates);
        FreeCudaMemory(&cuda_score_candidates_index);
        num_scores = target_num_scores_for_each_step;
        cuda_score_candidates = cuda_score_candidates_result;
        cuda_score_candidates_index = cuda_score_candidates_index_result;
    }
    cuda_flat_score_map = NULL;
    cuda_flat_score_map_index = NULL;

    // Copy to CPU
    // [OPTIMIZED] Add CUDA error checking for memory copy
    if (cudaMemcpy(score_candidates_result, cuda_score_candidates, num_scores * sizeof(float), cudaMemcpyDeviceToHost) != cudaSuccess) {
        fprintf(stderr, "[ERROR] cudaMemcpy failed for score_candidates_result.\n");
    }
    if (cudaMemcpy(score_candidates_result_index, cuda_score_candidates_index, num_scores * sizeof(int), cudaMemcpyDeviceToHost) != cudaSuccess) {
        fprintf(stderr, "[ERROR] cudaMemcpy failed for score_candidates_result_index.\n");
    }


    FreeCudaMemory(&cuda_score_candidates);
    FreeCudaMemory(&cuda_score_candidates_index);
}

void CudaChamfer::SortScores(float* cuda_flat_score_map, int* cuda_flat_score_map_index,
                float* score_candidates_result, int* score_candidates_result_index,
                int num_scores_copy, int steps,
                std::vector<int>& num_score_list, std::vector<int>& divider_list,
                bool is_fine_detection){
    /// Input
    /// cuda_flat_score_map: scores
    /// cuda_flat_score_map_index: index of scores
    /// num_scores_copy: the number of scores(length of array)
    /// steps: how many times sorting repeats.
    /// num_score_list
    /// divider_list
    /// is_fine_detection: original matching(true) or downsized matching (false)
    ///
    /// Output
    /// score_candidates_result: the result scores of sorting
    /// score_candidates_result_index: ..


    // [OPTIMIZED] Add runtime parameter validation and error handling
    if (steps <= 0 || num_score_list.size() != steps || divider_list.size() != steps || num_scores_copy <= 1) {
        fprintf(stderr, "[ERROR] Invalid parameters in SortScores (vector version).\n");
        return;
    }

    int num_threads;
    int target_num_per_thread;

    int num_score_blocks;
    int target_num_scores_for_each_step;
    int block_size = 32;    // max: 32
    int available_cores = 384; // Jetson Xavier NX

    dim3 threads_in_a_block;
    dim3 num_blocks;;

    float* cuda_score_candidates = cuda_flat_score_map;
    int* cuda_score_candidates_index = cuda_flat_score_map_index;


    for(int i = 0; i < steps; i++){
        target_num_per_thread = num_score_list.at(i);
        num_threads = divider_list.at(i);
        if(i > 0){
            assert(target_num_scores_for_each_step > target_num_per_thread * num_threads);
        }
        target_num_scores_for_each_step = target_num_per_thread * num_threads;
    }


    int num_scores = num_scores_copy;

    float threshold_score;
    float max_score;
    if(is_fine_detection){
        threshold_score = threshold_score_orig;
        max_score = max_score_orig;
    }
    else{
        threshold_score = threshold_score_py;
        max_score = max_score_py;
    }




    for(int i = 0; i < steps; i++){
        target_num_per_thread = num_score_list.at(i);
        num_threads = divider_list.at(i);
        target_num_scores_for_each_step = target_num_per_thread * num_threads;
#ifdef DEBUG_MODE
        lw << "target_num_scores_for_each_step: " << target_num_scores_for_each_step << lw.endl;
        lw << "target_num_per_thread: " << target_num_per_thread << lw.endl;
        lw << "num_threads: " << num_threads << lw.endl;
#endif

        float* cuda_score_candidates_result = nullptr;
        int* cuda_score_candidates_index_result = nullptr;
        if (cudaMalloc(&cuda_score_candidates_result, target_num_scores_for_each_step * sizeof(float)) != cudaSuccess) {
            fprintf(stderr, "[ERROR] cudaMalloc failed for cuda_score_candidates_result.\n");
            return;
        }
        if (cudaMalloc(&cuda_score_candidates_index_result, target_num_scores_for_each_step * sizeof(int)) != cudaSuccess) {
            fprintf(stderr, "[ERROR] cudaMalloc failed for cuda_score_candidates_index_result.\n");
            cudaFree(cuda_score_candidates_result);
            return;
        }


        block_size = num_scores / available_cores + 1;
        if(block_size > 32) block_size = 32;
        if(block_size < 1) block_size = 1;
        threads_in_a_block = dim3(block_size);
        num_score_blocks = (int)((num_threads + block_size - 1) / block_size);
        num_blocks = dim3(num_score_blocks);

//        cudaDeviceSynchronize();
        // [OPTIMIZED] Add CUDA error checking for kernel launch
        SortChamferScores<<<num_blocks, threads_in_a_block>>>(cuda_score_candidates, cuda_score_candidates_index,
                                                             cuda_score_candidates_result, cuda_score_candidates_index_result,
                                                             num_scores,
                                                             target_num_per_thread, num_threads,
                                                             threshold_score, max_score);
        cudaError_t err = cudaGetLastError();
        if (err != cudaSuccess) {
            fprintf(stderr, "[ERROR] SortChamferScores kernel launch failed: %s\n", cudaGetErrorString(err));
            cudaFree(cuda_score_candidates_result);
            cudaFree(cuda_score_candidates_index_result);
            return;
        }
//        cudaDeviceSynchronize();
        FreeCudaMemory(&cuda_score_candidates);
        FreeCudaMemory(&cuda_score_candidates_index);
        num_scores = target_num_scores_for_each_step;

        cuda_score_candidates = cuda_score_candidates_result;
        cuda_score_candidates_index = cuda_score_candidates_index_result;
    }
//    cuda_flat_score_map = nullptr;
//    cuda_flat_score_map_index = nullptr;

    // Copy to CPU
    // [OPTIMIZED] Add CUDA error checking for memory copy
    if (cudaMemcpy(score_candidates_result, cuda_score_candidates, num_scores * sizeof(float), cudaMemcpyDeviceToHost) != cudaSuccess) {
        fprintf(stderr, "[ERROR] cudaMemcpy failed for score_candidates_result.\n");
    }
    if (cudaMemcpy(score_candidates_result_index, cuda_score_candidates_index, num_scores * sizeof(int), cudaMemcpyDeviceToHost) != cudaSuccess) {
        fprintf(stderr, "[ERROR] cudaMemcpy failed for score_candidates_result_index.\n");
    }

    FreeCudaMemory(&cuda_score_candidates);
    FreeCudaMemory(&cuda_score_candidates_index);
}

#ifdef ENABLE_CHAMFER_VERSION2
using namespace cub;
CachingDeviceAllocator  g_allocator(true);  // Caching allocator for device memory
int device_wise_sort(float* input_value, int* input_index, int input_size, float* output_value, int* output_index, int target_size)
{
//    printf("cub::DeviceRadixSort::SortPairs() %d items (%d-byte keys %d-byte values)\n",
//        input_size, int(sizeof(float)), int(sizeof(int)));
//    fflush(stdout);

    // Allocate device arrays
    float* cu_output_val = nullptr;
    int* cu_output_index = nullptr;
    cudaMalloc(&cu_output_val, input_size * sizeof(float));
    cudaMalloc(&cu_output_index, input_size * sizeof(int));
    DoubleBuffer<float> d_keys(input_value, cu_output_val);
    DoubleBuffer<int>   d_values(input_index, cu_output_index);
    // Allocate temporary storage
    size_t  temp_storage_bytes  = 0;
    void    *d_temp_storage     = NULL;
    DeviceRadixSort::SortPairs(d_temp_storage, temp_storage_bytes, d_keys, d_values, input_size);
    cudaMalloc(&d_temp_storage, temp_storage_bytes);

    // Run
    DeviceRadixSort::SortPairs(d_temp_storage, temp_storage_bytes, d_keys, d_values, input_size);


    float   *h_keys_result              = new float[input_size];
    int     *h_values_result           = new int[input_size];
    cudaMemcpy(h_keys_result, d_keys.Current(), sizeof(float) * input_size, cudaMemcpyDeviceToHost);
    cudaMemcpy(h_values_result, d_values.Current(), sizeof(int) * input_size, cudaMemcpyDeviceToHost);
    // [OPTIMIZED] Commented out debug printfs for industrial code cleanliness
    // if(dm::log){
    //     for (int i = 0; i < 20 ; ++i) {
    //         printf("%d: %d, %f\n", i, h_values_result[i], h_keys_result[i]);
    //     }
    // }

    for (int i = 0; i < target_size; ++i) {
        output_value[i] = h_keys_result[i];
        output_index[i] = h_values_result[i];
    }
    if (h_values_result) delete[] h_values_result;
    if (h_keys_result) delete[] h_keys_result;

    // [OPTIMIZED] Commented out debug printfs and unused code for industrial code cleanliness
    // if(dm::log){
    //     printf("---------------------------------\n");
    //     h_keys_result              = new float[input_size];
    //     h_values_result           = new int[input_size];
    //     cudaMemcpy(h_keys_result, d_keys.d_buffers[0], sizeof(float) * input_size, cudaMemcpyDeviceToHost);
    //     cudaMemcpy(h_values_result, d_values.d_buffers[0], sizeof(int) * input_size, cudaMemcpyDeviceToHost);
    //     // Print results
    //     for (int i = 0; i < 30 ; ++i) {
    //         printf("%d: %d, %f\n", i, h_values_result[i], h_keys_result[i]);
    //     }
    //     printf("\n");
    //     // Cleanup
    //     if (h_values_result) delete[] h_values_result;
    //     if (h_keys_result) delete[] h_keys_result;
    // }
    // if (d_keys.d_buffers[0]) cudaFree(d_keys.d_buffers[0]);
    // if (d_values.d_buffers[0]) cudaFree(d_values.d_buffers[0]);
    if (d_temp_storage) cudaFree(d_temp_storage);
    if (cu_output_val) cudaFree(cu_output_val);
    if (cu_output_index) cudaFree(cu_output_index);
    return 0;
}
#endif

float CudaChamfer::ChamferInference(cv::Mat source_img, bool use_weight){
    // Jang 20220713
    const std::string pyr_path = data_path + "/pyr_source/";
    const std::string orig_path = data_path + "/orig_source/";
    const std::string result_path = data_path + "/results/";
    if(fs::exists(orig_path)){
       fs::remove_all(orig_path);
    }
    if(fs::exists(pyr_path)){
       fs::remove_all(pyr_path);
    }
    if(fs::exists(result_path)){
       fs::remove_all(result_path);
    }
    fs::create_directories(pyr_path);
    fs::create_directories(orig_path);
    fs::create_directories(result_path);

    // Remove result data
    if(fs::exists(data_path + result_score_path))
        fs::remove(data_path + result_score_path);

    // Jang 20220711
    // Should be modified
    lw << "[DEBUG] chamfer_method_: " << chamfer_method_
       << ", File " <<  __FILE__ <<  ", line " << __LINE__
       << ", in " << __func__ << lw.endl;

//    use_weight_ = 1;
    fail_msg = "";

    if(trained_template.non_zero_area_templates.size() < 1 ||
        trained_template_pyr.non_zero_area_templates.size() < 1){
        lw << "[FAIL] No template is detected"
           << ", File " <<  __FILE__ <<  ", line " << __LINE__
           << ", in " << __func__ << lw.endl;
        t_execute = 0;
        return 0;
    }
    if(source_img.empty()){
        lw << "[FAIL] No source image is detected"
           << ", File " <<  __FILE__ <<  ", line " << __LINE__
           << ", in " << __func__ << lw.endl;
        t_execute = 0;
        return 0;
    }

    if(dm::log){
        cudaDeviceSynchronize();
    }
    struct timespec start_all;//clock_t start_all = clock();
    clock_gettime(CLOCK_MONOTONIC, &start_all);
    struct timespec tp_start, tp_end;
    double host_elapsed;
    clock_gettime(CLOCK_MONOTONIC, &tp_start);
    if(dm::log){
        lw << "File " <<  __FILE__ <<  ", line " << __LINE__
            << ", in " << __func__ << lw.endl;
    }

    orig_source_img_mat_ = source_img.clone();

    if(dm::log){
    lw << "File " <<  __FILE__ <<  ", line " << __LINE__
       << ", in " << __func__ << lw.endl;
    }
    // Paras. setting #FIXME
    float angle_interval_fine_align = 1.0;
    float angle_interval_coarse_align = 5.0;
    float angle_min = -45.0;
    float angle_max = 45.0;
    int num_angles;
    int num_pyramid = 1;
    rslt_match.clear();

    std::string params_path = this->params_path_ + parameter_matching_file;
    if (!fio::exists(params_path)){
        if(dm::log){
            std::cerr << "File isn't opened." << std::endl;
        }
        CreateParamsIniFile(params_path);
    }else {
        // FileIO. 20220419. Jimmy. #fio14
        fio::FileIO inim(params_path, fio::FileioFormat::INI);
        inim.IniSetSection("ParameterChamfer");
        angle_min = inim.IniReadtoFloat("angle_min");
        angle_max = inim.IniReadtoFloat("angle_max");
        angle_interval_coarse_align = inim.IniReadtoFloat("angle_coarse_interval");
        angle_interval_fine_align = inim.IniReadtoFloat("angle_fine_interval");

        inim.close();
    }
    num_angles = (angle_max - angle_min ) / angle_interval_fine_align;


    assert(angle_min < angle_max);
    assert(angle_min >= -180);
    assert(angle_max <= 180);
    assert(angle_interval_fine_align <= angle_interval_coarse_align);
    assert(angle_interval_coarse_align > 0 && angle_interval_coarse_align <= 25);
    assert(angle_interval_fine_align > 0 && angle_interval_fine_align <= 10);

    // Original source image
    int orig_src_width = orig_source_img_mat_.cols;
    int orig_src_height = orig_source_img_mat_.rows;
    int orig_tmp_width = orig_template_img_mat_.cols;
    int orig_tmp_height = orig_template_img_mat_.rows;
    if(dm::log){
        lw << "File " <<  __FILE__ <<  ", line " << __LINE__
            << ", in " << __func__ << lw.endl;
    }
    // Down-sampling for source image
    cv::Mat pyr_src_img;
    cv::pyrDown(source_img, pyr_src_img, cv::Size(source_img.cols/2, source_img.rows/2));
    if(dm::log){
        lw << "File " <<  __FILE__ <<  ", line " << __LINE__
            << ", in " << __func__ << lw.endl;
    }
    pyr_source_img_mat_ = pyr_src_img;
    int pyr_src_width = pyr_source_img_mat_.cols;
    int pyr_src_height = pyr_source_img_mat_.rows;
    int pyr_tmp_width = pyr_template_img_mat_.cols;
    int pyr_tmp_height = pyr_template_img_mat_.rows;
    if(dm::log){
        checkCUDAandSysInfo();
        cudaDeviceSynchronize();
    }
    if(dm::log){
        lw << "File " <<  __FILE__ <<  ", line " << __LINE__
            << ", in " << __func__ << lw.endl;
    }
    image_proc->SkipBlurContour(false, false);
    orig_source_contours_mat_.release();


    image_proc->SkipBlurContour(false, false);
    createSrcNonZeroMat(pyr_src_img, preprocessed_source_pyr, pyr_tmp_width, pyr_tmp_height,
                        pyr_src_width, pyr_src_height, pyr_path);
    if(expand_img_){
        pyr_src_width = pyr_source_img_mat_.cols + pyr_tmp_width;
        pyr_src_height = pyr_source_img_mat_.rows + pyr_tmp_height;
    }
    if(dm::log){
        lw << "File " <<  __FILE__ <<  ", line " << __LINE__
            << ", in " << __func__ << lw.endl;
    }
    // Check if the nonzero parts are detected
    if(preprocessed_source_pyr.length_source_candidate < 1){
        t_execute = 0;
        return 0;
    }

    if(dm::log){
        cudaDeviceSynchronize();
        clock_gettime(CLOCK_MONOTONIC, &tp_end);
        host_elapsed = clock_diff (&tp_start, &tp_end);
        clock_gettime(CLOCK_MONOTONIC, &tp_start);

        lw << "[DEBUG] Time elapsed - createSrcNonZeroMat: " << host_elapsed << "s" << lw.endl;
    }
    ////////////////////////////////// Prepare Pointer Variables //////////////////////////////////
    int* template_num_non_zero = NULL;

    // CUDA memory
    short* cuda_flat_template_non_zero_col = NULL;
    short* cuda_flat_template_non_zero_row = NULL;
    short* cuda_source_candidate_col = NULL;
    short* cuda_source_candidate_row = NULL;
    // Jang 20220330
    float* cuda_flat_template_weight_pyr = NULL;

    float* cuda_flat_score_map = NULL;
    int* cuda_flat_score_map_index = NULL;
    float* cuda_flat_source_dist_transfrom = NULL;
    int* cuda_template_num_non_zero = NULL;

    // Used in the sorting process but old version.
    float* cuda_score_candidates = NULL;
    int* cuda_score_candidates_index = NULL;
    float* cuda_score_candidates_result_2nd = NULL;
    int* cuda_score_candidates_result_index_2nd = NULL;
    float* cuda_score_candidates_result_3rd = NULL;
    int* cuda_score_candidates_result_index_3rd = NULL;

    float* score_candidates_result = NULL;
    int* score_candidates_result_index = NULL;
    short* orig_src_cand_col = NULL;
    short* orig_src_cand_row = NULL;

    ////////////////////////////////// Memory Trans. {Host to Device} //////////////////////////////////
    // Copy memories into CUDA
    cudaMalloc(&cuda_source_candidate_col, preprocessed_source_pyr.length_source_candidate * sizeof(short));
    cudaMemcpy(cuda_source_candidate_col, preprocessed_source_pyr.source_candidate_col,
               preprocessed_source_pyr.length_source_candidate * sizeof(short), cudaMemcpyHostToDevice);
    cudaMalloc(&cuda_source_candidate_row, preprocessed_source_pyr.length_source_candidate * sizeof(short));
    cudaMemcpy(cuda_source_candidate_row, preprocessed_source_pyr.source_candidate_row,
               preprocessed_source_pyr.length_source_candidate * sizeof(short), cudaMemcpyHostToDevice);

    if(dm::log){
        lw << "File " <<  __FILE__ <<  ", line " << __LINE__
            << ", in " << __func__ << lw.endl;
    }
    // Score
    /// No need to make entire score map. Because the only non-zero positions will be computed.
    /// (Reducing the resource and process time.)
    int score_mem_size = trained_template_pyr.num_templates * preprocessed_source_pyr.length_source_candidate;

    cudaMalloc(&cuda_flat_score_map, score_mem_size * sizeof(float));
    cudaMalloc(&cuda_flat_score_map_index, score_mem_size * sizeof(int));

    // Set templates
    cudaMalloc(&cuda_flat_template_non_zero_col, trained_template_pyr.total_memory_size * sizeof(short));
    cudaMalloc(&cuda_flat_template_non_zero_row, trained_template_pyr.total_memory_size * sizeof(short));
    cudaMemcpy(cuda_flat_template_non_zero_col, trained_template_pyr.flat_template_non_zero_col_,
               trained_template_pyr.total_memory_size * sizeof(short), cudaMemcpyHostToDevice);
    cudaMemcpy(cuda_flat_template_non_zero_row, trained_template_pyr.flat_template_non_zero_row_,
               trained_template_pyr.total_memory_size * sizeof(short), cudaMemcpyHostToDevice);

    if(dm::log){
        lw << "File " <<  __FILE__ <<  ", line " << __LINE__
            << ", in " << __func__ << lw.endl;
    }
    // Jang 20220330
    cudaMalloc(&cuda_flat_template_weight_pyr, trained_template_pyr.total_memory_size * sizeof(float));
    cudaMemcpy(cuda_flat_template_weight_pyr, trained_template_pyr.flat_template_weight_,
               trained_template_pyr.total_memory_size * sizeof(float), cudaMemcpyHostToDevice);

    cudaMalloc(&cuda_flat_source_dist_transfrom, pyr_src_width * pyr_src_height * sizeof(float));
    cudaMemcpy(cuda_flat_source_dist_transfrom, preprocessed_source_pyr.flat_source_dist_transfrom,
               pyr_src_width * pyr_src_height * sizeof(float), cudaMemcpyHostToDevice);

    template_num_non_zero = trained_template_pyr.num_non_zeros;
    cudaMalloc(&cuda_template_num_non_zero, trained_template_pyr.num_templates * sizeof(int));
    cudaMemcpy(cuda_template_num_non_zero, template_num_non_zero,
               trained_template_pyr.num_templates * sizeof(int), cudaMemcpyHostToDevice);
    //////////////////////////////////////////////////////////////////////////////////////////////////////


    ////////////////////////////////// CUDA: Set the CUDA memory block //////////////////////////////////
    int block_size = 32;    // max: 32
    dim3 threads_in_a_block(block_size, block_size);    // 16x16 threads
    int num_blocks_w = (int)((preprocessed_source_pyr.source_num_candidate_width + block_size - 1) / block_size);
    int num_blocks_h = (int)((preprocessed_source_pyr.source_num_candidate_height + block_size - 1) / block_size);
    dim3 num_blocks(num_blocks_w, num_blocks_h);

    if(dm::log){
        lw << "File " <<  __FILE__ <<  ", line " << __LINE__
            << ", in " << __func__ << lw.endl;
    }
    // short length_source_candidate, short source_num_candidate_width, short source_num_candidate_height,
    // these should be computed for threads....
    // get total length
    // separate them

    // Jang 20220330
    ChamferMatch<<<num_blocks, threads_in_a_block>>>(cuda_flat_source_dist_transfrom,
                                 cuda_source_candidate_col, cuda_source_candidate_row,
                                 pyr_src_width, pyr_src_height,
                                 trained_template_pyr.num_templates,
                                 cuda_flat_template_non_zero_col, cuda_flat_template_non_zero_row, cuda_template_num_non_zero,
                                 cuda_flat_template_weight_pyr, use_weight,
                                 preprocessed_source_pyr.length_source_candidate, preprocessed_source_pyr.source_num_candidate_width,
                                 preprocessed_source_pyr.source_num_candidate_height,
                                 cuda_flat_score_map, cuda_flat_score_map_index);

    if(dm::log){
        cudaDeviceSynchronize();
        clock_gettime(CLOCK_MONOTONIC, &tp_end);
        host_elapsed = clock_diff (&tp_start, &tp_end);
        clock_gettime(CLOCK_MONOTONIC, &tp_start);

        lw << "[DEBUG] Time elapsed - ChamferMatch: " << host_elapsed << "s" << lw.endl;
    }


    // Do score sorting


    int divider;              // This is the number of threads.
    int num_score_blocks;

    int target_num_scores;     // each thread will collect top 10(target_num_scores) scores (which are lowest scores)
                                // // if a target_num_score is 10, one thread sort out 10 sorted values.
    int num_scores = score_mem_size;    // the number of templates * length_source_candidate

    int num_result_scores;

    lw << "[DEBUG] num_scores(num_templates x candidates): " << num_scores << lw.endl;
#ifdef DEBUG_MODE
    lw << "[DEBUG] trained_template_pyr.num_templates: " << trained_template_pyr.num_templates << lw.endl;
#endif

#ifdef DEBUG_MODE
    lw << "[DEBUG] preprocessed_source_pyr.length_source_candidate: " << preprocessed_source_pyr.length_source_candidate << lw.endl;
#endif

    bool is_fine_detection = false;
    bool old_sorting_method = false;
    int steps = 3;
    std::vector<int> target_num_score_list; // if a target_num_score is 10, one thread sort out 10 sorted values.
    std::vector<int> divider_list;
    target_num_score_list.resize(steps);
    divider_list.resize(steps);

    target_num_score_list.at(0) = 10;
    divider_list.at(0) = 1000;
    target_num_score_list.at(1) = 10;
    divider_list.at(1) = 100;
    target_num_score_list.at(2) = 10;
    divider_list.at(2) = 40;

    num_result_scores = target_num_score_list.at(target_num_score_list.size() - 1) * divider_list.at(divider_list.size() - 1);

    // Old one
    if(old_sorting_method){
        // 384 cores
        // 1000 threads / 384 = 2.xx
        // block size = 3
        target_num_scores = target_num_score_list.at(0);
        divider = divider_list.at(0);
        block_size = 32;
        num_result_scores = target_num_scores * divider;
        cudaMalloc(&cuda_score_candidates, num_result_scores * sizeof(float));
        cudaMalloc(&cuda_score_candidates_index, num_result_scores * sizeof(int));

        threads_in_a_block = dim3(block_size);
        num_score_blocks = (int)((divider + block_size - 1) / block_size);
        num_blocks = dim3(num_score_blocks);
        SortChamferScores<<<num_blocks, threads_in_a_block>>>(cuda_flat_score_map, cuda_flat_score_map_index,
                                     cuda_score_candidates, cuda_score_candidates_index,
                                     num_scores,
                                     target_num_scores, divider,
                                    threshold_score_py, max_score_py);

        // Second sorting
        num_scores  = num_result_scores;

        target_num_scores = target_num_score_list.at(1);
        divider = divider_list.at(1);
        num_result_scores = target_num_scores * divider;

        cudaMalloc(&cuda_score_candidates_result_2nd, num_result_scores * sizeof(float));
        cudaMalloc(&cuda_score_candidates_result_index_2nd, num_result_scores * sizeof(int));

        block_size = 2;
        threads_in_a_block = dim3(block_size);
        num_score_blocks = (int)((divider + block_size - 1) / block_size);
        num_blocks = dim3(num_score_blocks);
        SortChamferScores<<<num_blocks, threads_in_a_block>>>(cuda_score_candidates, cuda_score_candidates_index,
                                     cuda_score_candidates_result_2nd, cuda_score_candidates_result_index_2nd,
                                     num_scores,
                                     target_num_scores, divider,
                                     threshold_score_py, max_score_py);


        num_scores  = num_result_scores;

        target_num_scores = target_num_score_list.at(2);
        divider = divider_list.at(2);
        num_result_scores = target_num_scores * divider;

        cudaMalloc(&cuda_score_candidates_result_3rd, num_result_scores * sizeof(float));
        cudaMalloc(&cuda_score_candidates_result_index_3rd, num_result_scores * sizeof(int));

        block_size = 1;
        threads_in_a_block = dim3(block_size);
        num_score_blocks = (int)((divider + block_size - 1) / block_size);
        num_blocks = dim3(num_score_blocks);
        SortChamferScores<<<num_blocks, threads_in_a_block>>>(cuda_score_candidates_result_2nd, cuda_score_candidates_result_index_2nd,
                                     cuda_score_candidates_result_3rd, cuda_score_candidates_result_index_3rd,
                                     num_scores,
                                     target_num_scores, divider,
                                     threshold_score_py, max_score_py);


        score_candidates_result = new float[num_result_scores];
        score_candidates_result_index = new int[num_result_scores];
        cudaMemcpy(score_candidates_result, cuda_score_candidates_result_3rd, num_result_scores * sizeof(float), cudaMemcpyDeviceToHost);
        cudaMemcpy(score_candidates_result_index, cuda_score_candidates_result_index_3rd, num_result_scores * sizeof(int), cudaMemcpyDeviceToHost);

        FreeCudaMemory(&cuda_score_candidates);
        FreeCudaMemory(&cuda_score_candidates_index);
        FreeCudaMemory(&cuda_score_candidates_result_2nd);
        FreeCudaMemory(&cuda_score_candidates_result_index_2nd);
        FreeCudaMemory(&cuda_score_candidates_result_3rd);
        FreeCudaMemory(&cuda_score_candidates_result_index_3rd);

        FreeCudaMemory(&cuda_flat_score_map);
        FreeCudaMemory(&cuda_flat_score_map_index);
    }
    else{
        cudaMalloc(&cuda_score_candidates, num_result_scores * sizeof(float));
        cudaMalloc(&cuda_score_candidates_index, num_result_scores * sizeof(int));

        SortScores(cuda_flat_score_map, cuda_flat_score_map_index,
                    cuda_score_candidates, cuda_score_candidates_index,
                    num_scores, steps,
                   target_num_score_list, divider_list,
                    is_fine_detection);

        score_candidates_result = new float[num_result_scores];
        score_candidates_result_index = new int[num_result_scores];
        cudaMemcpy(score_candidates_result, cuda_score_candidates, num_result_scores * sizeof(float), cudaMemcpyDeviceToHost);
        cudaMemcpy(score_candidates_result_index, cuda_score_candidates_index, num_result_scores * sizeof(int), cudaMemcpyDeviceToHost);

        FreeCudaMemory(&cuda_score_candidates);
        FreeCudaMemory(&cuda_score_candidates_index);
    }

    if(dm::log){
        cudaDeviceSynchronize();
        clock_gettime(CLOCK_MONOTONIC, &tp_end);
        host_elapsed = clock_diff (&tp_start, &tp_end);
        clock_gettime(CLOCK_MONOTONIC, &tp_start);
        lw << "[DEBUG] Time elapsed - Sorting: " << host_elapsed << "s" << lw.endl;
    //    for (unsigned int i = 0; i < num_result_scores; i++){
    //        int source_target = score_candidates_result_index[i] % preprocessed_source_pyr.length_source_candidate;
    //        int source_target_rot = score_candidates_result_index[i] / preprocessed_source_pyr.length_source_candidate;
    //        short x__= preprocessed_source_pyr.source_candidate_col[source_target];
    //        short y__ = preprocessed_source_pyr.source_candidate_row[source_target];
    //        lw << "[DEBUG] Score: " << score_candidates_result[i] << lw.endl;
    //        lw << "[DEBUG] source_target: " << source_target << lw.endl;
    //        lw << "[DEBUG] source_target_rot: " << source_target_rot << lw.endl;
    //        lw << "[DEBUG] x__: " << x__ << lw.endl;
    //        lw << "[DEBUG] y__: " << y__ << lw.endl;
    //    }
    }

    template_num_non_zero = NULL;
    // Free memory
    FreeCudaMemory(&cuda_flat_template_non_zero_col);
    FreeCudaMemory(&cuda_flat_template_non_zero_row);
    FreeCudaMemory(&cuda_flat_template_weight_pyr);
    FreeCudaMemory(&cuda_source_candidate_col);
    FreeCudaMemory(&cuda_source_candidate_row);

    FreeCudaMemory(&cuda_flat_source_dist_transfrom);
    FreeCudaMemory(&cuda_template_num_non_zero);



    // ============================================================================================
    // ================================= Fineturn on Position =====================================
    /// From top 10 to do Chamfer matching in original domain
    // Create the non-zero mat for original source image
    orig_source_contours_mat_.release();
    createSrcNonZeroMat(orig_source_img_mat_, preprocessed_source, orig_tmp_width, orig_tmp_height,
                        orig_src_width, orig_src_height, orig_path);
    if(expand_img_){
        orig_src_width = orig_source_img_mat_.cols + orig_tmp_width;
        orig_src_height = orig_source_img_mat_.rows + orig_tmp_height;
    }
    if(dm::log){
        cudaDeviceSynchronize();
        clock_gettime(CLOCK_MONOTONIC, &tp_end);
        host_elapsed = clock_diff (&tp_start, &tp_end);
        clock_gettime(CLOCK_MONOTONIC, &tp_start);

        lw << "[DEBUG] Time elapsed - createSrcNonZeroMat: " << host_elapsed << "s" << lw.endl;
        lw << "[DEBUG] preprocessed_source_pyr.length_source_candidate: " << preprocessed_source_pyr.length_source_candidate << lw.endl;
        lw << "[DEBUG] num_result_scores: " << num_result_scores << lw.endl;
    }
    // Find the original poision of top 10 coordinates
    // target_num_scores = 10; Because top 10 from down-sampling domain
    int pyr2 = (int)pow(2, num_pyramid);
    unsigned int count = 0;
    int orig_tot_mem_size = num_result_scores * pyr2 * pyr2;
    int filtered_tot_mem_size = 0;

    // num_result_scores:100
    // * pyr2: 2 * pyr2: 2 *




    std::vector<cv::Point> filter_target;
    filter_target.resize(num_result_scores);
    bool do_pass = false;
    for (unsigned int i = 0; i < num_result_scores; i++){
        int source_target = score_candidates_result_index[i] % preprocessed_source_pyr.length_source_candidate;
        short fitx = preprocessed_source_pyr.source_candidate_col[source_target] * pyr2;
        short fity = preprocessed_source_pyr.source_candidate_row[source_target] * pyr2;
//        lw << "[DEBUG] i: " << (int)i << ", source_target: " << source_target << ", (" << preprocessed_source_pyr.source_candidate_col[source_target] << ", " << preprocessed_source_pyr.source_candidate_row[source_target] <<  ");  ";

        do_pass = false;
        for (int j = i - 1; j >= 0; j--){
            if(filter_target.at(j).x == fitx && filter_target.at(j).y == fity){
//                lw << "[DEBUG] dupl: " << (int)fitx << ", " << (int)fity << lw.endl;
                do_pass = true;
                break;
            }
        }
        if(!do_pass){
            filter_target.at(i).x = fitx;
            filter_target.at(i).y = fity;
            count++;
        }
        else{
            filter_target.at(i).x = 0;
            filter_target.at(i).y = 0;
        }

    }
    orig_tot_mem_size = count * pyr2 * pyr2;
    orig_src_cand_col = new short[orig_tot_mem_size];
    orig_src_cand_row = new short[orig_tot_mem_size];
#ifdef DEBUG_MODE
    lw << "[DEBUG] orig_tot_mem_size: " << orig_tot_mem_size << lw.endl;
#endif
    count = 0;
    for (unsigned int i = 0; i < num_result_scores; i++){
        short fitx = filter_target.at(i).x;
        short fity = filter_target.at(i).y;
        if(fitx == 0 && fity == 0) continue;

        for (unsigned int upx = 0; upx < pyr2; upx++){      // #FIXME: upx++ => upx+=pow(2, pyrNum-i)
            for (unsigned int upy = 0; upy < pyr2; upy++){

                orig_src_cand_col[count] = fitx + upx;
                orig_src_cand_row[count] = fity + upy;

//                lw << "[DEBUG]:(" << orig_src_cand_col[count] << ", " << orig_src_cand_row[count] << "); Score:" << score_candidates_result[i] << lw.endl;

                count++;
            }
        }
    }
#ifdef DEBUG_MODE
    lw << "[DEBUG] count: " << (int)count << lw.endl;
#endif
    filter_target.clear();

/*
    count = 0;
    for (unsigned int i = 0; i < num_result_scores; i++){
        int source_target = score_candidates_result_index[i] % preprocessed_source_pyr.length_source_candidate;
        short fitx = preprocessed_source_pyr.source_candidate_col[source_target] * pyr2;
        short fity = preprocessed_source_pyr.source_candidate_row[source_target] * pyr2;
        lw << "[DEBUG] i: " << (int)i << ", source_target: " << source_target << ", (" << preprocessed_source_pyr.source_candidate_col[source_target] << ", " << preprocessed_source_pyr.source_candidate_row[source_target] <<  ");  ";
        for (unsigned int upx = 0; upx < pyr2; upx++){      // #FIXME: upx++ => upx+=pow(2, pyrNum-i)
            for (unsigned int upy = 0; upy < pyr2; upy++){
                orig_src_cand_col[count] = fitx + upx;
                orig_src_cand_row[count] = fity + upy;
                lw << "[DEBUG]:(" << orig_src_cand_col[count] << ", " << orig_src_cand_row[count] << "); Score:" << score_candidates_result[i] << lw.endl;
                count++;
            }
        }
    }
 */


    FreeMemory(&preprocessed_source_pyr.flat_score_map);
//    FreeMemory(&preprocessed_source_pyr.flat_source_dist_transfrom);  // This is already released
    FreeMemory(&preprocessed_source_pyr.source_candidate_col);
    FreeMemory(&preprocessed_source_pyr.source_candidate_row);
    FreeMemory(&score_candidates_result);
    FreeMemory(&score_candidates_result_index);

    if(count == 0){
        lw << "[FAIL] No template is detected"
           << ", File " <<  __FILE__ <<  ", line " << __LINE__
           << ", in " << __func__ << lw.endl;

        FreeMemory(&orig_src_cand_col);
        FreeMemory(&orig_src_cand_row);
        FreeMemory(&preprocessed_source.flat_score_map);
        FreeMemory(&preprocessed_source.source_candidate_col);
        FreeMemory(&preprocessed_source.source_candidate_row);
        t_execute = 0;
        return 0;
    }

    // Declare the CUDA paras.
    int* template_num_non_zero_orig = NULL;

    // CUDA memory
    short* cuda_flat_template_non_zero_col_orig = NULL;
    short* cuda_flat_template_non_zero_row_orig = NULL;
    short* cuda_source_candidate_col_orig = NULL;
    short* cuda_source_candidate_row_orig = NULL;

    // Jang 20220330
    float* cuda_flat_template_weight_orig = NULL;

    float* cuda_flat_score_map_orig = NULL;
    int* cuda_flat_score_map_index_orig = NULL;
    float* cuda_flat_source_dist_transfrom_orig = NULL;
    int* cuda_template_num_non_zero_orig = NULL;

    float* cuda_score_candidates_orig = NULL;
    int* cuda_score_candidates_index_orig = NULL;

    float* score_candidates_result_orig = NULL;
    int* score_candidates_result_index_orig = NULL;

    ////////////////////////////////// Memory Trans. {Host to Device} //////////////////////////////////
    // Put to CUDA
    cudaMalloc(&cuda_source_candidate_col_orig, orig_tot_mem_size * sizeof(short));
    cudaMemcpy(cuda_source_candidate_col_orig, orig_src_cand_col,
               orig_tot_mem_size * sizeof(short), cudaMemcpyHostToDevice);
    cudaMalloc(&cuda_source_candidate_row_orig, orig_tot_mem_size * sizeof(short));
    cudaMemcpy(cuda_source_candidate_row_orig, orig_src_cand_row,
               orig_tot_mem_size * sizeof(short), cudaMemcpyHostToDevice);

    // Score
    int orig_score_mem_size = trained_template.num_templates * orig_tot_mem_size;

    cudaMalloc(&cuda_flat_score_map_orig, orig_score_mem_size * sizeof(float));
    cudaMalloc(&cuda_flat_score_map_index_orig, orig_score_mem_size * sizeof(int));

    // Set templates
    cudaMalloc(&cuda_flat_template_non_zero_col_orig, trained_template.total_memory_size * sizeof(short));
    cudaMalloc(&cuda_flat_template_non_zero_row_orig, trained_template.total_memory_size * sizeof(short));
    cudaMemcpy(cuda_flat_template_non_zero_col_orig, trained_template.flat_template_non_zero_col_,
               trained_template.total_memory_size * sizeof(short), cudaMemcpyHostToDevice);
    cudaMemcpy(cuda_flat_template_non_zero_row_orig, trained_template.flat_template_non_zero_row_,
               trained_template.total_memory_size * sizeof(short), cudaMemcpyHostToDevice);
    cudaMalloc(&cuda_flat_source_dist_transfrom_orig, orig_src_width * orig_src_height * sizeof(float));

    // Jang 20220330
    cudaMalloc(&cuda_flat_template_weight_orig, trained_template.total_memory_size * sizeof(float));
    cudaMemcpy(cuda_flat_template_weight_orig, trained_template.flat_template_weight_,
               trained_template.total_memory_size * sizeof(float), cudaMemcpyHostToDevice);

    cudaMemcpy(cuda_flat_source_dist_transfrom_orig, preprocessed_source.flat_source_dist_transfrom,
               orig_src_width * orig_src_height * sizeof(float), cudaMemcpyHostToDevice);

    template_num_non_zero_orig = trained_template.num_non_zeros;
    cudaMalloc(&cuda_template_num_non_zero_orig, trained_template.num_templates * sizeof(int));
    cudaMemcpy(cuda_template_num_non_zero_orig, template_num_non_zero_orig,
               trained_template.num_templates * sizeof(int), cudaMemcpyHostToDevice);
    //////////////////////////////////////////////////////////////////////////////////////////////////////


    ////////////////////////////////// CUDA: Set the CUDA memory block //////////////////////////////////
    block_size = 32;    // max: 32
    threads_in_a_block = dim3(block_size, block_size);    // 16x16 threads
    num_blocks_w = (int)((preprocessed_source.source_num_candidate_width + block_size - 1) / block_size);
    num_blocks_h = (int)((preprocessed_source.source_num_candidate_height + block_size - 1) / block_size);
    num_blocks = dim3(num_blocks_w, num_blocks_h);

    int cand_width = (int)(sqrt(orig_tot_mem_size) + 0.5);
    int cand_height = (int)((float)orig_tot_mem_size / (float)cand_width + 0.5);
#ifdef DEBUG_MODE
    lw << "[DEBUG] cand_width: " << cand_width<< lw.endl;
    lw << "[DEBUG] cand_height: " << cand_height<< lw.endl;
    lw << "[DEBUG] (int)sqrt(orig_tot_mem_size)+1: " << (int)sqrt(orig_tot_mem_size)+1<< lw.endl;
#endif

    // Jang 20220330
//    use_weight_ = 1;
    // Do Chamfer Matching in original domain
    ChamferMatch<<<num_blocks, threads_in_a_block>>>(cuda_flat_source_dist_transfrom_orig,
                                                        cuda_source_candidate_col_orig, cuda_source_candidate_row_orig,
                                                        orig_src_width, orig_src_height,
                                                        trained_template.num_templates,
                                                        cuda_flat_template_non_zero_col_orig, cuda_flat_template_non_zero_row_orig, cuda_template_num_non_zero_orig,
                                                        cuda_flat_template_weight_orig, use_weight,
                                                        orig_tot_mem_size, cand_width, cand_height,
                                                        cuda_flat_score_map_orig, cuda_flat_score_map_index_orig);

#ifdef DEBUG_MODE
    cudaDeviceSynchronize();
    clock_gettime(CLOCK_MONOTONIC, &tp_end);
    host_elapsed = clock_diff (&tp_start, &tp_end);
    clock_gettime(CLOCK_MONOTONIC, &tp_start);
    lw << "[DEBUG] Time elapsed - ChamferMatch: " << host_elapsed << "s" << lw.endl;
#endif
    FreeMemory(&preprocessed_source.flat_score_map);
//    FreeMemory(&preprocessed_source.flat_source_dist_transfrom);  // This is already released
    FreeMemory(&preprocessed_source.source_candidate_col);
    FreeMemory(&preprocessed_source.source_candidate_row);


    // Do Chamfer sorting
    // Fineturn position sorting
    num_scores = orig_score_mem_size;

    lw << "[DEBUG] num_scores[fine](num_templates x candidates): " << num_scores << lw.endl;

#ifdef DEBUG_MODE
//    float* TEST_score_candidates_result = NULL;
//    int* TEST_score_candidates_result_index = NULL;
//    // ex) orig_tot_mem_size = 40
//    TEST_score_candidates_result = new float[num_scores];
//    TEST_score_candidates_result_index = new int[num_scores];
//    cudaMemcpy(TEST_score_candidates_result, cuda_flat_score_map_orig, num_scores * sizeof(float), cudaMemcpyDeviceToHost);(num_templates x candidates)
//    cudaMemcpy(TEST_score_candidates_result_index, cuda_flat_score_map_index_orig, num_scores * sizeof(int), cudaMemcpyDeviceToHost);

//    for(int i = 0; i < num_scores; i ++){
//        if(TEST_score_candidates_result[i] < 0.1){
//            lw << "[DEBUG] Score: " << TEST_score_candidates_result[i] << ", " << TEST_score_candidates_result_index[i] << lw.endl;
//        }

//    }

//    FreeMemory(&TEST_score_candidates_result);
//    TEST_score_candidates_result = NULL;
//    FreeMemory(&TEST_score_candidates_result_index);
//    TEST_score_candidates_result_index = NULL;
#endif

    //
    is_fine_detection = true;
    old_sorting_method = false;
    steps = 3;

    target_num_score_list.resize(steps);
    divider_list.resize(steps);

    target_num_score_list.at(0) = 10;
    divider_list.at(0) = 1000;
    target_num_score_list.at(1) = 10;
    divider_list.at(1) = 100;
    target_num_score_list.at(2) = 30;
    divider_list.at(2) = 1;

    num_result_scores = target_num_score_list.at(target_num_score_list.size() - 1) * divider_list.at(divider_list.size() - 1);


    //
    if(old_sorting_method){
        target_num_scores = 10;
        divider =1;
        num_result_scores = target_num_scores * divider;

        cudaMalloc(&cuda_score_candidates_orig, num_result_scores * sizeof(float));
        cudaMalloc(&cuda_score_candidates_index_orig, num_result_scores * sizeof(int));

        block_size = 1;
        threads_in_a_block = dim3(block_size);
        num_score_blocks = (int)((divider + block_size - 1) / block_size);
        num_blocks = dim3(num_score_blocks);


        SortChamferScores<<<num_blocks, threads_in_a_block>>>(cuda_flat_score_map_orig, cuda_flat_score_map_index_orig,
                                     cuda_score_candidates_orig, cuda_score_candidates_index_orig,
                                     num_scores,
                                     target_num_scores, divider,
                                     threshold_score_orig, max_score_orig);
        score_candidates_result_orig = new float[num_result_scores];
        score_candidates_result_index_orig = new int[num_result_scores];
        cudaMemcpy(score_candidates_result_orig, cuda_score_candidates_orig, num_result_scores * sizeof(float), cudaMemcpyDeviceToHost);
        cudaMemcpy(score_candidates_result_index_orig, cuda_score_candidates_index_orig, num_result_scores * sizeof(int), cudaMemcpyDeviceToHost);

        FreeCudaMemory(&cuda_flat_score_map_orig);
        FreeCudaMemory(&cuda_flat_score_map_index_orig);
    }
    else{
        cudaMalloc(&cuda_score_candidates_orig, num_result_scores * sizeof(float));
        cudaMalloc(&cuda_score_candidates_index_orig, num_result_scores * sizeof(int));

        SortScores(cuda_flat_score_map_orig, cuda_flat_score_map_index_orig,
                    cuda_score_candidates_orig, cuda_score_candidates_index_orig,
                    num_scores, steps,
                   target_num_score_list, divider_list,
                    is_fine_detection);

        score_candidates_result_orig = new float[num_result_scores];
        score_candidates_result_index_orig = new int[num_result_scores];
        cudaMemcpy(score_candidates_result_orig, cuda_score_candidates_orig, num_result_scores * sizeof(float), cudaMemcpyDeviceToHost);
        cudaMemcpy(score_candidates_result_index_orig, cuda_score_candidates_index_orig, num_result_scores * sizeof(int), cudaMemcpyDeviceToHost);
    }


#ifdef DEBUG_MODE
    for(int i = 0; i < num_result_scores; i ++){
        lw << "[DEBUG] Score(fine): " << score_candidates_result_orig[i] << lw.endl;
    }
#endif

    bool cuda_free_mem_result;
    cuda_free_mem_result = FreeCudaMemory(&cuda_score_candidates_orig);

#ifdef DEBUG_MODE
    // To check whether the memory addresses are same or not.
    // If cuda memory is copied to the Host twice to difference host memory space, the addresses are different.
    if(!cuda_free_mem_result){
        float* TEST_cuda_score_candidates_orig_3 = NULL;
        TEST_cuda_score_candidates_orig_3 = new float[num_result_scores];
        cudaMemcpy(TEST_cuda_score_candidates_orig_3, cuda_score_candidates_orig, num_result_scores * sizeof(float), cudaMemcpyDeviceToHost);

        for(int i = 0; i < num_result_scores; i ++){
            lw << "[DEBUG] cuda_score_candidates_orig: " << TEST_cuda_score_candidates_orig_3[i] << lw.endl;
        }
        std::cout << "[DEBUG] mem addr: " << TEST_cuda_score_candidates_orig_3 << std::endl ;

        float* TEST_cuda_score_candidates_orig_2 = NULL;
        TEST_cuda_score_candidates_orig_2 = new float[num_result_scores];
        cudaMemcpy(TEST_cuda_score_candidates_orig_2, cuda_score_candidates_orig, num_result_scores * sizeof(float), cudaMemcpyDeviceToHost);

        for(int i = 0; i < num_result_scores; i ++){
            lw << "[DEBUG] cuda_score_candidates_orig: " << TEST_cuda_score_candidates_orig_2[i] << lw.endl;
        }
        std::cout << "[DEBUG] mem addr: " << TEST_cuda_score_candidates_orig_2 << std::endl ;

        FreeMemory(&TEST_cuda_score_candidates_orig_3);
        FreeMemory(&TEST_cuda_score_candidates_orig_2);
    }
    // BUT
    // If cuda memory is copied to the Host and "Free" the memory.
    // And then copy it again to another host memory space, the addresses are same.
    if(!cuda_free_mem_result){
        float* TEST_cuda_score_candidates_orig = NULL;
        TEST_cuda_score_candidates_orig = new float[num_result_scores];
        cudaMemcpy(TEST_cuda_score_candidates_orig, cuda_score_candidates_orig, num_result_scores * sizeof(float), cudaMemcpyDeviceToHost);

        for(int i = 0; i < num_result_scores; i ++){
            lw << "[DEBUG] cuda_score_candidates_orig: " << TEST_cuda_score_candidates_orig[i] << lw.endl;
        }
        std::cout << "[DEBUG] mem addr: " << TEST_cuda_score_candidates_orig << std::endl ;
        FreeMemory(&TEST_cuda_score_candidates_orig);


        float* TEST_cuda_score_candidates_orig_2 = NULL;
        TEST_cuda_score_candidates_orig_2 = new float[num_result_scores];
        cudaMemcpy(TEST_cuda_score_candidates_orig_2, cuda_score_candidates_orig, num_result_scores * sizeof(float), cudaMemcpyDeviceToHost);

        for(int i = 0; i < num_result_scores; i ++){
            lw << "[DEBUG] cuda_score_candidates_orig: " << TEST_cuda_score_candidates_orig_2[i] << lw.endl;
        }
        std::cout << "[DEBUG] mem addr: " << TEST_cuda_score_candidates_orig_2 << std::endl ;
        FreeMemory(&TEST_cuda_score_candidates_orig_2);
    }
#endif
    cuda_free_mem_result = FreeCudaMemory(&cuda_score_candidates_index_orig);

#ifdef DEBUG_MODE
    cudaDeviceSynchronize();
    clock_gettime(CLOCK_MONOTONIC, &tp_end);
    host_elapsed = clock_diff (&tp_start, &tp_end);
    clock_gettime(CLOCK_MONOTONIC, &tp_start);
    lw << "[DEBUG] Time elapsed - Sorting: " << host_elapsed << "s" << lw.endl;
#endif
    // For DEBUG
//    for (unsigned int i = 0; i<num_result_scores; i++){
//        lw << ">> [DEBUG]:(" << score_candidates_result_index_orig[i] <<
//              "; Score:" << score_candidates_result_orig[i] << lw.endl;
//    }

    // Free memory
    template_num_non_zero_orig = NULL;

    FreeCudaMemory(&cuda_flat_template_non_zero_col_orig);
    FreeCudaMemory(&cuda_flat_template_non_zero_row_orig);
    FreeCudaMemory(&cuda_source_candidate_col_orig);
    FreeCudaMemory(&cuda_source_candidate_row_orig);
    // Jang 20220330
    FreeCudaMemory(&cuda_flat_template_weight_orig);

    FreeCudaMemory(&cuda_flat_source_dist_transfrom_orig);
    FreeCudaMemory(&cuda_template_num_non_zero_orig);


    // $$ END of Fineturn on Position


#ifdef DEBUG_MODE
    // ex) orig_tot_mem_size = 40
    for(int i = 0; i < num_result_scores; i ++){
        int template_rot = score_candidates_result_index_orig[i] / orig_tot_mem_size;
        int source_target = score_candidates_result_index_orig[i] % orig_tot_mem_size;

        // Printout results
        int target_x = orig_src_cand_col[source_target] + orig_tmp_width / 2;
        int target_y = orig_src_cand_row[source_target] + orig_tmp_height / 2;


        // (45 + -45)
        // (5 + (-20 / 1)) * 1
        // 20
        float detected_angle = ((float)template_rot * angle_interval_fine_align) + angle_min;

        lw << "[DEBUG] Score: " << score_candidates_result_orig[i] << lw.endl;
        lw << "The result: " << "[ " << target_x << ", "<< target_y << " ]";
        lw << "| ang: " << detected_angle<< " deg." << lw.endl;
    }

#endif

//    // Save the result
//    cv::Rect crop_roi = cv::Rect(orig_src_cand_col[source_target],
//                                 orig_src_cand_row[source_target],
//                                 orig_tmp_width, orig_tmp_height);
//    cv::Mat mask_mat = trained_template.templates.at(template_rot);
//    cv::cvtColor(mask_mat, mask_mat, cv::COLOR_GRAY2RGB);
//    cv::Mat mask;
//    cv::inRange(mask_mat, cv::Scalar(250, 250, 250), cv::Scalar(255, 255, 255), mask);
//    mask_mat.setTo(cv::Scalar(0, 0, 255), mask);
//    mask_mat.copyTo(source_img(crop_roi), mask_mat);

//    cv::line(source_img, cv::Point(target_x-10, target_y), cv::Point(target_x+10, target_y), cv::Scalar(0, 255, 0), 5, cv::LINE_AA);
//    cv::line(source_img, cv::Point(target_x, target_y-10), cv::Point(target_x, target_y+10), cv::Scalar(0, 255, 0), 5, cv::LINE_AA);

//    cv::imwrite("./sss/copied_source_img.png", source_img);
//    cv::imwrite("./sss/copied_mask_mat.png", mask_mat);



    int best_case = 0;
    double best_ncc = 0;
    cv::Mat copied_source_img = source_img.clone();
    int length_source_candidate = orig_tot_mem_size;
    int template_width = orig_tmp_width;
    int template_height = orig_tmp_height;
    short* source_candidate_col = orig_src_cand_col;
    short* source_candidate_row = orig_src_cand_row;
    cv::Mat source_canny_img;
    cv::Mat source_gray_img = copied_source_img.clone();
    cv::Mat rot_template_mat;
    std::vector<float> ncc_results_vec;

    try{
        // NCC computation preparation
        if(copied_source_img.channels() > 1)
            cv::cvtColor(copied_source_img, source_gray_img, cv::COLOR_BGR2GRAY);
    //    if(orig_source_contours_mat_.empty() || (orig_source_contours_mat_.rows != copied_source_img.rows)){
    //        if(!use_image_proc){
    //            // Reduce the noise with kernel a 3x3 before the canny
    //            cv::blur(source_gray_img, source_gray_img, cv::Size(3, 3));
    //            //Canny edge
    //            cv::Canny(source_gray_img, source_canny_img, 100, 100, 3);
    //        }
    //        else{
    //            source_canny_img = this->image_proc->GetImageBySavedInfo(source_gray_img, use_cuda_for_improc);
    //        }
    //    }
    //    else{
    //        source_canny_img = orig_source_contours_mat_.clone();
    //    }


        if(expand_img_){
            std::vector<int> expanded_xy_index;
            bool pass_compute = false;
            for(int j = 0; j < num_result_scores; j++){
                int source_target = score_candidates_result_index_orig[j] % length_source_candidate;
                pass_compute = false;
                for(int k = 0; k < expanded_xy_index.size(); k ++){
                    if(source_target == expanded_xy_index.at(k)){
                        pass_compute = true;
                    }
                }
                if(pass_compute){
                    continue;
                }

                expanded_xy_index.push_back(source_target);

                if(dm::log){
                    std::cout << "=================orig===========================" << std::endl;
                    std::cout << "source_target: " << source_target << std::endl;
                    std::cout << "score_candidates_result_index_orig[j]: " << score_candidates_result_index_orig[j] << std::endl;
                    std::cout << "source_candidate_col[source_target]: " << source_candidate_col[source_target] << std::endl;
                    std::cout << "source_candidate_row[source_target]: " << source_candidate_row[source_target] << std::endl;
                    std::cout << "============================================" << std::endl;
                }
                source_candidate_col[source_target] -= (short)(template_width / 2);
                source_candidate_row[source_target] -= (short)(template_height / 2);
                if(dm::log){
                    std::cout << "source_candidate_col[source_target]: " << source_candidate_col[source_target] << std::endl;
                    std::cout << "source_candidate_row[source_target]: " << source_candidate_row[source_target] << std::endl;
                }
            }
        }

        // Get the best case
        fs::create_directories(data_path + "results/");
        for(int j = 0; j < num_result_scores; j++){
            int template_scale_rot = score_candidates_result_index_orig[j] / length_source_candidate;
            int template_scale = template_scale_rot / num_angles;
            int template_rot = template_scale_rot % num_angles; // + angle_min; // Jang 20211203    // This is the position of rotation from the beginning. (ex) from angle_min.
            int source_target = score_candidates_result_index_orig[j] % length_source_candidate;

//            lw << "Index: " << score_candidates_result_index_orig[j] << ", Score: " << score_candidates_result_orig[j] << lw.endl;
//            lw << "[ " << source_candidate_col[source_target] << ", " << source_candidate_row[source_target] << " ]" << lw.endl;
//            lw << "Scale: " << template_scale << ", Rot: " << template_rot << lw.endl;

            // Mask
            cv::Rect crop_roi;
            cv::Rect crop_temp_roi;

            int crop_x;
            int crop_y;
            int crop_w;
            int crop_h;
            int crop_tmp_x;
            int crop_tmp_y;
            crop_x = source_candidate_col[source_target];
            crop_y = source_candidate_row[source_target];
            crop_w = template_width;
            crop_h = template_height;
            crop_tmp_x = 0;
            crop_tmp_y = 0;
            if(source_candidate_col[source_target] < 0){
                crop_x = 0;
                crop_w = template_width + source_candidate_col[source_target];
                crop_tmp_x = -source_candidate_col[source_target];
            }
            if(source_candidate_row[source_target] < 0){
                crop_y = 0;
                crop_h = template_height + source_candidate_row[source_target];
                crop_tmp_y = -source_candidate_row[source_target];
            }
            if((source_candidate_col[source_target] + template_width) >  orig_source_img_mat_.cols){
                crop_w = (orig_source_img_mat_.cols - source_candidate_col[source_target]);
            }
            if((source_candidate_row[source_target] + template_height) >  orig_source_img_mat_.rows){
                crop_h = (orig_source_img_mat_.rows - source_candidate_row[source_target]);
            }
            crop_roi = cv::Rect(crop_x, crop_y, crop_w, crop_h);
            crop_temp_roi = cv::Rect(crop_tmp_x, crop_tmp_y, crop_w, crop_h);

            if(dm::log){
                std::cout << "crop_roi: " << crop_roi << std::endl;
                std::cout << "crop_temp_roi: " << crop_temp_roi << std::endl;

                lw << "File " <<  __FILE__ <<  ", line " << __LINE__
                   << ", in " << __func__ << lw.endl;
                lw << "template_scale_rot: " << template_scale_rot << lw.endl;
                lw << "score_candidates_result_index_orig[j]: " << score_candidates_result_index_orig[j] << lw.endl;

            }
            cv::Size temp_resized_size;
            temp_resized_size.width = crop_w;
            temp_resized_size.height = crop_h;
            if(trained_template.templates_gray.size() > template_scale_rot){
                // NCC computation
//                rot_template_mat = trained_template.templates_gray.at(template_scale_rot)(crop_temp_roi);
                rot_template_mat = rot_template_mat.zeros(temp_resized_size, trained_template.templates_gray.at(template_scale_rot).type());
                trained_template.templates_gray.at(template_scale_rot)(crop_temp_roi).copyTo(rot_template_mat);
                // Average of source
                double avg_source;
                avg_source = cv::mean(source_gray_img(crop_roi))[0];

                // Average of template
                double avg_template;
                avg_template = cv::mean(rot_template_mat)[0];
                if(dm::match_img){
                    cv::imwrite(data_path + "results/" + std::to_string(j) + "_template_contour.png", trained_template.templates.at(template_scale_rot));
                    cv::imwrite(data_path + "results/" + std::to_string(j) + "_cropped_source_gray_img.png", source_gray_img(crop_roi));
                    cv::imwrite(data_path + "results/" + std::to_string(j) + "_rot_template_mat.png", rot_template_mat);
                }
                // Sum of child values
                double sum_numerator = 0;
                double sum_denominator_0 = 0;
                double sum_denominator_1 = 0;
                for(int r = 0; r < rot_template_mat.rows; r ++){
                    for(int c = 0; c < rot_template_mat.cols; c++){
                        double compute_val_0 = rot_template_mat.at<uchar>(r, c) - avg_template;
                        double compute_val_1 = source_gray_img.at<uchar>(r + crop_roi.y, c + crop_roi.x) - avg_source;
                        sum_numerator += (compute_val_0 * compute_val_1);

                        sum_denominator_0 += std::pow(compute_val_0, 2);
                        sum_denominator_1 += std::pow(compute_val_1, 2);

                    }
                }

                sum_denominator_0 = std::sqrt(sum_denominator_0);
                sum_denominator_1 = std::sqrt(sum_denominator_1);



                double ncc_result = 0;
                ncc_result = sum_numerator / (sum_denominator_0 * sum_denominator_1);
//                ncc_result = std::fabs(ncc_result);

    #ifdef DEBUG_MODE
                lw << "avg_source: " << avg_source << lw.endl;
                lw << "avg_template: " <<  avg_template << lw.endl;
                lw << "sum_denominator_0: " << sum_denominator_0 << lw.endl;
                lw << "sum_denominator_1: " << sum_denominator_1 << lw.endl;
                lw << "Score(ncc_result): " << ncc_result << lw.endl;
    #endif

                ncc_results_vec.push_back(ncc_result);
                if(best_ncc < ncc_result){
                    best_ncc = ncc_result;
                    best_case = j;
                }

//                // NCC using OpenCV
//                cv::Mat ncc_result_mat = cv::Mat::zeros(1, 1, CV_32FC1);
//                cv::matchTemplate(source_gray_img(crop_roi), rot_template_mat, ncc_result_mat, cv::TM_SQDIFF_NORMED);
//                lw << "Score(opencv TM_SQDIFF_NORMED): " << ncc_result_mat << lw.endl;
//                cv::matchTemplate(source_gray_img(crop_roi), rot_template_mat, ncc_result_mat, cv::TM_CCORR_NORMED);
//                lw << "Score(opencv TM_CCORR_NORMED): " << ncc_result_mat << lw.endl;
//                cv::matchTemplate(source_gray_img(crop_roi), rot_template_mat, ncc_result_mat, cv::TM_CCOEFF_NORMED);
//                lw << "Score(opencv TM_CCOEFF_NORMED): " << ncc_result_mat << lw.endl;
            }
        }
        lw << "best_case(ncc_result): " << best_case << lw.endl;

    #ifdef DEBUG_MODE
        lw << "num_angles: " << num_angles << ", angle_min: " << angle_min << lw.endl;
    #endif
        {
            int j = best_case;
            int template_scale_rot = score_candidates_result_index_orig[j] / length_source_candidate;
            int template_scale = template_scale_rot / num_angles;
            int template_rot = template_scale_rot % num_angles; // + angle_min; // Jang 20211203    // This is the position of rotation from the beginning. (ex) from angle_min.
            int source_target = score_candidates_result_index_orig[j] % length_source_candidate;

            float detected_angle = ((float)template_rot * angle_interval_fine_align) + angle_min;

            float score_percentage;
            score_percentage = (4.0 - pow(score_candidates_result_orig[j], 2)) / 4.0;
            if(score_percentage < 0) score_percentage = 0;

            lw << "------ Result ------" << lw.endl;
            lw << "Index: " << score_candidates_result_index_orig[j] << ", Score: " << score_candidates_result_orig[j] << ", NCC: " << best_ncc << lw.endl;
            lw << "[ " << source_candidate_col[source_target] << ", " << source_candidate_row[source_target] << " ]" << lw.endl;
            lw << "Scale: " << template_scale << ", Rot: " << template_rot << lw.endl;

            if(dm::match_img){
                // Mask
                cv::Rect crop_roi = cv::Rect(source_candidate_col[source_target] , source_candidate_row[source_target], template_width, template_height);

                cv::Mat mask_mat = trained_template.templates.at(template_scale_rot);
                lw << "--- template_scale_rot: " << template_scale_rot << lw.endl;
                lw << "mask_mat: " << mask_mat.cols << ", " << mask_mat.rows << lw.endl;
                cv::Mat color_mask = cv::Mat::zeros(mask_mat.size(), CV_8UC3);
                std::vector<cv::Point> non_zero_;
                cv::findNonZero(mask_mat, non_zero_);

                // Change pixels' color
                for(int p = 0; p < non_zero_.size(); p++){
                    cv::Vec3b& vec3b = color_mask.at<cv::Vec3b>(non_zero_.at(p).y, non_zero_.at(p).x) ;
                    vec3b[0] = 0;
                    vec3b[1] = 0;
                    vec3b[2] = 255;
                }

                lw << "File " <<  __FILE__ <<  ", line " << __LINE__
                   << ", in " << __func__ << lw.endl;
                lw << "color_mask: " << color_mask.cols << ", " << color_mask.rows << lw.endl;
                lw << "copied_source_img: " << copied_source_img.cols << ", " << copied_source_img.rows << lw.endl;
                lw << "mask_mat: " << mask_mat.cols << ", " << mask_mat.rows << lw.endl;
                lw << "crop_roi: " << crop_roi.width << ", " << crop_roi.height << lw.endl;

                try{
                    cv::cvtColor(copied_source_img, copied_source_img, cv::COLOR_GRAY2BGR);
                }
                catch(cv::Exception e){
                    lw << "File " <<  __FILE__ <<  ", line " << __LINE__
                       << ", in " << __func__ << lw.endl;
                    std::string error_what = e.what();
                    lw << error_what << lw.endl;
                }catch(...){
                    lw << "Error. " << "File " <<  __FILE__ <<  ", line " << __LINE__
                       << ", in " << __func__ << lw.endl;
                }
                color_mask.copyTo(copied_source_img(crop_roi), mask_mat.clone());

                cv::imwrite(data_path + "results/" + std::to_string(j) + "_copied_source_img.png", copied_source_img);
                cv::imwrite(data_path + "results/" + std::to_string(j) + "_copied_mask_mat.png", color_mask);
            }

            MatchData match_data( 0, 0, 1.0f, .0f, .0f );
            match_data.x = source_candidate_col[source_target] + template_width / 2;   // center position
            match_data.y = source_candidate_row[source_target] + template_height / 2;  // center position
            match_data.angle = detected_angle; // unit: degree
            match_data.coef = best_ncc; // score_candidates_result_orig[j];
            match_data.scale = template_scale;

            // If the matched position is too close to the image boundaries, the matching failed.
            // if(match_data.x < (template_width / 4) || match_data.y < (template_height / 4) ||
//                    orig_source_img_mat_.cols - match_data.x < (template_width / 4) ||
//                    orig_source_img_mat_.rows - match_data.y < (template_height / 4) ){
//                 throw(__LINE__);
//             }
            // Jang 20221024
            // Disabled the function.
            if(match_data.x < (template_width / 2) || match_data.y < (template_height / 2) ||
                   orig_source_img_mat_.cols - match_data.x < (template_width / 2) ||
                   orig_source_img_mat_.rows - match_data.y < (template_height / 2) ){
                throw(__LINE__);
            }

            rslt_match.push_back(match_data);

            // Save the best candidates into a file. (the same file which is used in the RST matching)
            FILE *fp_result_p=fopen((data_path + result_matched_area_path).c_str(), "wb");
            {
                float angle_ = (detected_angle / 180 * PI) + (PI / 2);
                float radius_ = template_height / 2;
                // x1, y1: center position
                // x2, y2: template's boundary position to show angle
                int x1, x2, y1, y2;
                x1 = source_candidate_col[source_target] + template_width / 2;
                y1 = source_candidate_row[source_target] + template_height / 2;
                x2 = x1 + (std::cos(angle_)*radius_)+0.5;
                y2 = y1 - (sin(angle_)*radius_)+0.5;

                cv::Point pp0{x1, y1};
                cv::Point pp1{x2, y2};
                cv::Point p1{
                        x1+(cos(angle_)*((template_height-1)/2))+(cos(angle_+ (PI / 2))*((template_width-1)/2)),
                        y1-(sin(angle_)*((template_height-1)/2))-(sin(angle_+ (PI / 2))*((template_width-1)/2))
                };
                cv::Point p2{
                        (p1.x+(cos(angle_- (PI / 2))*template_width)),
                        (p1.y-(sin(angle_- (PI / 2))*template_width))
                };
                cv::Point p3{
                        (p2.x+(cos(angle_- PI)*template_height)),
                        p2.y-(sin(angle_- PI)*template_height)
                };
                cv::Point p4{
                        p1.x+(cos(angle_+ PI)*template_height),
                        p1.y-(sin(angle_+ PI)*template_height)
                };

//                fprintf(fp_result_p, "%u\n%u\n%u\n%u\n",pp0.x,pp0.y,pp1.x,pp1.y);
//                fprintf(fp_result_p, "%u\n%u\n%u\n%u\n",p1.x,p1.y,p2.x,p2.y);
//                fprintf(fp_result_p, "%u\n%u\n%u\n%u\n",p3.x,p3.y,p4.x,p4.y);

                fprintf(fp_result_p, "%d\n%d\n%d\n%d\n",pp0.x,pp0.y,pp1.x,pp1.y);
                fprintf(fp_result_p, "%d\n%d\n%d\n%d\n",p1.x,p1.y,p2.x,p2.y);
                fprintf(fp_result_p, "%d\n%d\n%d\n%d\n",p3.x,p3.y,p4.x,p4.y);
            }
            fclose(fp_result_p);    fp_result_p = NULL;



        }
    }
    catch(cv::Exception e){
        lw << "File " <<  __FILE__ <<  ", line " << __LINE__
           << ", in " << __func__ << lw.endl;
        std::string error_what = e.what();
        lw << error_what << lw.endl;
        rslt_match.resize(1);
        rslt_match.at(0).coef = 0;
    }
    catch(int line){
        rslt_match.resize(1);
        rslt_match.at(0).coef = 0;
    }
    catch(...){
        lw << "Error. " << "File " <<  __FILE__ <<  ", line " << __LINE__
           << ", in " << __func__ << lw.endl;
        rslt_match.resize(1);
        rslt_match.at(0).coef = 0;
    }


#ifdef DEBUG_MODE
    cudaDeviceSynchronize();
#endif
    clock_gettime(CLOCK_MONOTONIC, &tp_end);
    host_elapsed = clock_diff (&start_all, &tp_end);
    t_execute = host_elapsed;

    lw << "ChamferInference Done: " << host_elapsed << "s" << lw.endl;


    for(int j = 0; j < num_result_scores; j++){
        int template_scale_rot = score_candidates_result_index_orig[j] / length_source_candidate;
        int template_scale = template_scale_rot / num_angles;
        int template_rot = template_scale_rot % num_angles;
        int source_target = score_candidates_result_index_orig[j] % length_source_candidate;

        lw << "Index: " << score_candidates_result_index_orig[j] << ", Score: " << score_candidates_result_orig[j] << lw.endl;
        lw << "[ " << source_candidate_col[source_target] << ", " << source_candidate_row[source_target] << " ]" << lw.endl;
        lw << "Scale: " << template_scale << ", Rot: " << template_rot << lw.endl;

        if(ncc_results_vec.size() > j){
            lw << "Score(NCC): " << ncc_results_vec.at(j)<< lw.endl;
        }
    }

    // num of non zero
    short template_size = (template_width + template_width) / 2;
    float best_score = score_candidates_result_orig[best_case];
    lw << "template_size : " << template_size << lw.endl;
    lw << "best_score : " << best_score << lw.endl;
    lw << "best_ncc: " << best_ncc << lw.endl;  // rslt_match.at(0).coef
    if(0.5 > chamfer_score_threshold_ || chamfer_score_threshold_ > 10){
        chamfer_score_threshold_ = 3;
    }
    // if weight discount is small -> ncc threshold can be small
    if( 0.3 < best_ncc && best_ncc < 0.8
       && best_score < chamfer_score_threshold_){
        lw << "[WARNING] Chamfer matching score is high. But NCC score is low." << lw.endl;
    }
    else if(best_ncc < 0.8){
        fail_msg = "[FAILED]";
        fail_msg += "Failure 1: NCC matching score is lower than 0.8. ";

        if(score_candidates_result_orig[0] >= max_score_orig){
            fail_msg += "Failure 2: Chamfer matching score is too low. The current image may not have the shapes of the templates. Image processing parameters between templates and current images may be different. ";
        }

        lw << "[FAILED] Detection failed. Score: " << rslt_match.at(0).coef << lw.endl;
        lw << fail_msg << lw.endl;
        host_elapsed = 0;
        t_execute = host_elapsed;
        rslt_match.clear();
    }

    // Save result if matching is succeeded.
    if(host_elapsed > 0 && rslt_match.size() > 0){
        if(rslt_match.at(0).coef > 0.4){
            FILE *result_score=fopen((data_path + result_score_path).c_str(),"wb");
            for(int i = 0; i < 1; i++){
                MatchData& match_data = rslt_match.at(i);
                // Jang 20221020
                // Fixed wrong y-value problem.
                fprintf(result_score, "%2d:        (%4d,%4d);%12.1f%;%12.1f;%12.1f\n",
                        i+1, match_data.x, orig_src_height - match_data.y
                        , match_data.coef * 100.0
                        , _minScale + _scaleInterval * (float)match_data.scale, match_data.angle);
            }
            fclose(result_score); result_score = NULL;
        }
        else{
            // Jang 20221024
            host_elapsed = 0;
            t_execute = host_elapsed;
            rslt_match.clear();
            lw << "[FAILED] Detection Failed. " << "File " <<  __FILE__ <<  ", line " << __LINE__
               << ", in " << __func__ << lw.endl;
        }
    }

    FreeMemory(&score_candidates_result_orig);
    FreeMemory(&score_candidates_result_index_orig);
    FreeMemory(&orig_src_cand_col);
    FreeMemory(&orig_src_cand_row);

    if(dm::log) {
        cudaDeviceSynchronize();
        clock_gettime(CLOCK_MONOTONIC, &tp_end);
        host_elapsed = clock_diff (&tp_start, &tp_end);
        lw << "[DEBUG] Time elapsed - Saving results: " << host_elapsed << "s" << lw.endl;
    }
    return (float)(host_elapsed);
}

float CudaChamfer::ChamferInference2(cv::Mat source_img, bool use_weight){
#ifdef ENABLE_CHAMFER_VERSION2
//    // Jang 20220419
//    // Warming up function
//    int warmup_block_size =32;
//    dim3 warmup_threads_in_a_block(warmup_block_size, warmup_block_size);
//    int warmup_num_blocks_w = (int)((1000 + warmup_block_size - 1) / warmup_block_size);
//    int warmup_num_blocks_h = (int)((1000 + warmup_block_size - 1) / warmup_block_size);
//    dim3 warmup_num_blocks(warmup_num_blocks_w, warmup_num_blocks_h);
//    WarmUpGpu<<<warmup_num_blocks, warmup_threads_in_a_block>>>();

    // With Warming up function
    // Beginnig: 650 -> After 1 min: 360 // Normal: 240
    // Without Warming up function
    // Beginnig: 870 -> After 1 min: 355 // Normal: 230

//    use_weight_ = 1;
    // Jang 20220713
    const std::string pyr_path = data_path + "/pyr_source/";
    const std::string orig_path = data_path + "/orig_source/";
    const std::string result_path = data_path + "/results/";
    if(fs::exists(orig_path)){
       fs::remove_all(orig_path);
    }
    if(fs::exists(pyr_path)){
       fs::remove_all(pyr_path);
    }
    if(fs::exists(result_path)){
       fs::remove_all(result_path);
    }
    fs::create_directories(pyr_path);
    fs::create_directories(orig_path);
    fs::create_directories(result_path);

    // Remove result data
    if(fs::exists(data_path + result_score_path))
        fs::remove(data_path + result_score_path);

    // Jang 20220711
    // Should be modified
    lw << "[DEBUG] chamfer_method_: " << chamfer_method_
       << ", File " <<  __FILE__ <<  ", line " << __LINE__
       << ", in " << __func__ << lw.endl;

    fail_msg = "";
    if(trained_template_vec_.size() == 0){
        lw << "[FAIL] No template is detected"
           << ", File " <<  __FILE__ <<  ", line " << __LINE__
           << ", in " << __func__ << lw.endl;
        t_execute = 0;
        return 0;
    }
    for (int i = 0; i < trained_template_vec_.size(); ++i) {
        if(use_only_start_and_end_){
            if(i != 0 && i != (trained_template_vec_.size() - 1)){
                continue;
            }
        }
        if(trained_template_vec_.at(i).non_zero_area_templates.size() < 1){
            lw << "[FAIL] No template is detected"
               << ", File " <<  __FILE__ <<  ", line " << __LINE__
               << ", in " << __func__ << lw.endl;
            t_execute = 0;
            return 0;
        }
    }
    if(source_img.empty()){
        lw << "[FAIL] No source image is detected"
           << ", File " <<  __FILE__ <<  ", line " << __LINE__
           << ", in " << __func__ << lw.endl;
        t_execute = 0;
        return 0;
    }

    if(dm::log){
        cudaDeviceSynchronize();
    }
    struct timespec start_all;//clock_t start_all = clock();
    clock_gettime(CLOCK_MONOTONIC, &start_all);
    struct timespec tp_start, tp_end;
    double host_elapsed;
    clock_gettime(CLOCK_MONOTONIC, &tp_start);
    if(dm::log){
        lw << "File " <<  __FILE__ <<  ", line " << __LINE__
            << ", in " << __func__ << lw.endl;
    }

    orig_source_img_mat_ = source_img.clone();

    if(dm::log){
    lw << "File " <<  __FILE__ <<  ", line " << __LINE__
       << ", in " << __func__ << lw.endl;
    }
    // Paras. setting #FIXME
    float angle_interval_fine_align = 1.0;
    float angle_interval_coarse_align = 5.0;
    float angle_min = -45.0;
    float angle_max = 45.0;
    int num_angles;
    int num_pyramid = 1;
    rslt_match.clear();

    std::string params_path = this->params_path_ + parameter_matching_file;
    if (!fio::exists(params_path)){
        if(dm::log){
            std::cerr << "File isn't opened." << std::endl;
        }
        CreateParamsIniFile(params_path);
    }else {
        // FileIO. 20220419. Jimmy. #fio14
        fio::FileIO inim(params_path, fio::FileioFormat::INI);
        inim.IniSetSection("ParameterChamfer");
        angle_min = inim.IniReadtoFloat("angle_min");
        angle_max = inim.IniReadtoFloat("angle_max");
        angle_interval_coarse_align = inim.IniReadtoFloat("angle_coarse_interval");
        angle_interval_fine_align = inim.IniReadtoFloat("angle_fine_interval");

        inim.close();
    }
    num_angles = (angle_max - angle_min ) / angle_interval_fine_align;


    assert(angle_min < angle_max);
    assert(angle_min >= -180);
    assert(angle_max <= 180);
    assert(angle_interval_fine_align <= angle_interval_coarse_align);
    assert(angle_interval_coarse_align > 0 && angle_interval_coarse_align <= 25);
    assert(angle_interval_fine_align > 0 && angle_interval_fine_align <= 10);

    // Template
    short num_pyrmd = trained_template_vec_.size(); // 1, 2, 4, 8, ...
    pre_proc_src_vec_.resize(num_pyrmd);

    // Original source image
    int orig_src_width = orig_source_img_mat_.cols;
    int orig_src_height = orig_source_img_mat_.rows;
    int orig_tmp_width = orig_template_img_mat_.cols;
    int orig_tmp_height = orig_template_img_mat_.rows;
    short padding_w = orig_tmp_width / 2;
    short padding_h = orig_tmp_height / 2;

    if(dm::log){
        lw << "File " <<  __FILE__ <<  ", line " << __LINE__
            << ", in " << __func__ << lw.endl;
    }
    if(dm::log){
        checkCUDAandSysInfo();
        cudaDeviceSynchronize();
    }
    if(dm::log){
        lw << "File " <<  __FILE__ <<  ", line " << __LINE__
            << ", in " << __func__ << lw.endl;
    }
    image_proc->SkipBlurContour(false, false);
    orig_source_contours_mat_.release();

    CreateSrcNonZeroMat2(orig_source_img_mat_, pre_proc_src_vec_,
                        num_pyrmd,
                        orig_tmp_width, orig_tmp_height, orig_path);

    cudaDeviceSynchronize();
    clock_gettime(CLOCK_MONOTONIC, &tp_end);
    host_elapsed = clock_diff (&tp_start, &tp_end);
    clock_gettime(CLOCK_MONOTONIC, &tp_start);
    lw << "[DEBUG] Time elapsed - TOTAL (CreateSrcNonZeroMat): " << host_elapsed << "s" << lw.endl;


    int num_result_scores;
    int orig_tot_mem_size;
    std::shared_ptr<float[]> score_candidates_result;
    std::shared_ptr<int[]> score_candidates_result_index;

    std::vector<cv::Point> non_zero_filtered_cand;

    TrainTemplatePtr* trained_template = nullptr;
    bool is_last = false;
    for (int pyr = 0; pyr < num_pyrmd; ++pyr) {
        if(use_only_start_and_end_){
            if(pyr != 0 && pyr != (num_pyrmd - 1)){
                continue;
            }
        }

        // Index of pyramid scale
        // From the last
        int template_pyrmd_index = num_pyrmd - 1 - pyr;
        PreprocessedSourcePtr& pre_proc_src = pre_proc_src_vec_.at(template_pyrmd_index);
        trained_template = &trained_template_vec_.at(template_pyrmd_index);

        if(dm::log){
            lw << "[DEBUG] File " <<  __FILE__ <<  ", line " << __LINE__
               << ", in " << __func__ << lw.endl;
        }
        // Check if the nonzero parts are detected
        if(pre_proc_src.length_source_candidate < 1){
            t_execute = 0;
            return 0;
        }
        short resize_scale = std::pow(2, template_pyrmd_index);
        int re_src_width = pre_proc_src.width;
        int re_src_height = pre_proc_src.height;
        int re_tmp_width = orig_tmp_width / resize_scale;
        int re_tmp_height = orig_tmp_height / resize_scale;
        short padding_w = re_tmp_width / 2;
        short padding_h = re_tmp_height / 2;



        if(dm::log){
            lw << "[DEBUG] ====== template_pyrmd_index: " << template_pyrmd_index << " ======" << lw.endl;
            lw << "[DEBUG] length_source_candidate: " << pre_proc_src.length_source_candidate << lw.endl;
            lw << "[DEBUG] num_templates: " << trained_template->num_templates << lw.endl;
            lw << "[DEBUG] total_memory_size: " << trained_template->total_memory_size << lw.endl;
        }

        ////// CUDA memory
        float* cuda_flat_source_dist_transfrom = nullptr;
        // Candidate
        short* cuda_source_candidate_col = nullptr;
        short* cuda_source_candidate_row = nullptr;
        // Score
        float* cuda_flat_score_map = nullptr;
        int* cuda_flat_score_map_index = nullptr;
        // Template non-zero
        short* cuda_flat_template_non_zero_col = nullptr;
        short* cuda_flat_template_non_zero_row = nullptr;
        // Weight for scoring
        float* cuda_flat_template_weight = nullptr;

        // The number of non-zero in each template.
        int* cuda_template_num_non_zero = nullptr;
        // Used in the sorting process.
        float* cuda_score_candidates = nullptr;
        int* cuda_score_candidates_index = nullptr;

        ////// Memory
        // Candidate
        short* source_candidate_col = pre_proc_src.source_candidate_col.get();
        short* source_candidate_row = pre_proc_src.source_candidate_row.get();
        // Template non-zero
        short* flat_template_non_zero_col = trained_template->flat_template_non_zero_col_.get();
        short* flat_template_non_zero_row = trained_template->flat_template_non_zero_row_.get();
        // Weight for scoring
        float* flat_template_weight = trained_template->flat_template_weight_.get();
        // The number of non-zero in each template.
        int* template_num_non_zero = trained_template->num_non_zeros.get();


        ////////////////////////////////// Memory Trans. {Host to Device} //////////////////////////////////
        size_t re_src_size = re_src_width * re_src_height; // Including expanding(padding) case
        // Distance Transform (Original size)
        cudaMalloc(&cuda_flat_source_dist_transfrom, re_src_size * sizeof(float));
        cudaMemcpy(cuda_flat_source_dist_transfrom, pre_proc_src.flat_source_dist_transfrom,
                   re_src_size * sizeof(float), cudaMemcpyHostToDevice);

        size_t cand_size = pre_proc_src.length_source_candidate;
        // Candidate
        cudaMalloc(&cuda_source_candidate_col, cand_size * sizeof(short));
        cudaMemcpy(cuda_source_candidate_col, source_candidate_col,
                   cand_size * sizeof(short), cudaMemcpyHostToDevice);
        cudaMalloc(&cuda_source_candidate_row, cand_size * sizeof(short));
        cudaMemcpy(cuda_source_candidate_row, source_candidate_row,
                   cand_size * sizeof(short), cudaMemcpyHostToDevice);

        // Score
        /// No need to make entire score map. Because the only non-zero positions will be computed.
        /// (Reducing the resource and process time.)
        /// the score size, started from the minimum pyramid scale
        int score_mem_size = trained_template->num_templates
                                * cand_size;
        cudaMalloc(&cuda_flat_score_map, score_mem_size * sizeof(float));
        cudaMalloc(&cuda_flat_score_map_index, score_mem_size * sizeof(int));

        size_t temp_nonzero_size = trained_template->total_memory_size;  // Including rotated templates
        // Template non-zero
        cudaMalloc(&cuda_flat_template_non_zero_col, temp_nonzero_size * sizeof(short));
        cudaMalloc(&cuda_flat_template_non_zero_row, temp_nonzero_size * sizeof(short));
        cudaMemcpy(cuda_flat_template_non_zero_col, flat_template_non_zero_col,
                   temp_nonzero_size * sizeof(short), cudaMemcpyHostToDevice);
        cudaMemcpy(cuda_flat_template_non_zero_row, flat_template_non_zero_row,
                   temp_nonzero_size * sizeof(short), cudaMemcpyHostToDevice);

        // Weight for scoring
        cudaMalloc(&cuda_flat_template_weight, temp_nonzero_size * sizeof(float));
        cudaMemcpy(cuda_flat_template_weight, flat_template_weight,
                   temp_nonzero_size * sizeof(float), cudaMemcpyHostToDevice);



        // The number of non-zero in each template.
        cudaMalloc(&cuda_template_num_non_zero, trained_template->num_templates * sizeof(int));
        cudaMemcpy(cuda_template_num_non_zero, template_num_non_zero,
                   trained_template->num_templates * sizeof(int), cudaMemcpyHostToDevice);

        //////////////////////////////////////////////////////////////////////////////////////////////////////


        ////////////////////////////////// CUDA: Set the CUDA memory block //////////////////////////////////
        int block_size = 32;    // max: 32
        dim3 threads_in_a_block(block_size, block_size);    // 16x16 threads
        int num_blocks_w = (int)((pre_proc_src.source_num_candidate_width + block_size - 1) / block_size);
        int num_blocks_h = (int)((pre_proc_src.source_num_candidate_height + block_size - 1) / block_size);
        dim3 num_blocks(num_blocks_w, num_blocks_h);

        // short length_source_candidate, short source_num_candidate_width, short source_num_candidate_height,
        // these should be computed for threads....
        // get total length
        // separate them
        cudaError_t cuda_error = cudaGetLastError();
        if(cuda_error != cudaError::cudaSuccess){
            lw << "[ERROR] CUDA error: " << cudaGetErrorString(cuda_error)
               << ", File " <<  __FILE__ <<  ", line " << __LINE__
               << ", in " << __func__ << lw.endl;
        }

        lw << "[DEBUG] length_source_candidate: " << pre_proc_src.length_source_candidate << lw.endl;
        lw << "[DEBUG] source_num_candidate_width: " << pre_proc_src.source_num_candidate_width << lw.endl;
        lw << "[DEBUG] source_num_candidate_height: " << pre_proc_src.source_num_candidate_height << lw.endl;
        lw << "[DEBUG] source_candidate_col: " << source_candidate_col[100] << lw.endl;
        lw << "[DEBUG] source_candidate_row: " << source_candidate_row[100] << lw.endl;
        lw << "[DEBUG] flat_template_non_zero_col: " << flat_template_non_zero_col[100] << lw.endl;
        lw << "[DEBUG] flat_template_non_zero_row: " << flat_template_non_zero_row[100] << lw.endl;
        lw << "[DEBUG] temp_nonzero_size: " << temp_nonzero_size<< lw.endl;
        lw << "[DEBUG] flat_template_weight: " << flat_template_weight[100] << lw.endl;

        ChamferMatch2<<<num_blocks, threads_in_a_block>>>(cuda_flat_source_dist_transfrom,
                                     cuda_source_candidate_col, cuda_source_candidate_row,
                                     re_src_width, re_src_height,
                                     trained_template->num_templates,
                                     cuda_flat_template_non_zero_col, cuda_flat_template_non_zero_row, cuda_template_num_non_zero,
                                     cuda_flat_template_weight, (short)use_weight,
                                     pre_proc_src.length_source_candidate,
                                     pre_proc_src.source_num_candidate_width,
                                     pre_proc_src.source_num_candidate_height,
                                     cuda_flat_score_map, cuda_flat_score_map_index);
        if(dm::log){
            cudaDeviceSynchronize();
            checkCUDAandSysInfo();
        }
        cuda_error = cudaGetLastError();
        if(cuda_error != cudaError::cudaSuccess){
            lw << "[ERROR] CUDA error: " << cudaGetErrorString(cuda_error)
               << ", File " <<  __FILE__ <<  ", line " << __LINE__
               << ", in " << __func__ << lw.endl;
        }
        if(dm::log){
            cudaDeviceSynchronize();
            clock_gettime(CLOCK_MONOTONIC, &tp_end);
            host_elapsed = clock_diff (&tp_start, &tp_end);
            clock_gettime(CLOCK_MONOTONIC, &tp_start);
            lw << "[DEBUG] Time elapsed - ChamferMatch: " << host_elapsed << "s" << lw.endl;
        }
        lw << "[DEBUG] num_scores(num_templates x candidates): " << score_mem_size << lw.endl;

        //////// TEST ////////
        if(dm::log){
            // ex) orig_tot_mem_size = 40
            std::shared_ptr<float[]> TEST_score_candidates_result(new float[score_mem_size]());
            std::shared_ptr<int[]> TEST_score_candidates_result_index(new int[score_mem_size]());

            cudaMemcpy(TEST_score_candidates_result.get(), cuda_flat_score_map, score_mem_size * sizeof(float), cudaMemcpyDeviceToHost);
            cudaMemcpy(TEST_score_candidates_result_index.get(), cuda_flat_score_map_index, score_mem_size * sizeof(int), cudaMemcpyDeviceToHost);
            for(int i = 0; i < 100; i ++){
                if(TEST_score_candidates_result[i] < 0.1){
                    lw << "[DEBUG] Score: " << TEST_score_candidates_result[i] << ", " << TEST_score_candidates_result_index[i] << lw.endl;
                }

            }
            cudaDeviceSynchronize();
            cuda_error = cudaGetLastError();
            if(cuda_error != cudaError::cudaSuccess){
                lw << "[ERROR] CUDA error: " << cudaGetErrorString(cuda_error)
                   << ", File " <<  __FILE__ <<  ", line " << __LINE__
                   << ", in " << __func__ << lw.endl;
            }


            if(dm::match_img){
                try{
                    cv::Mat color_img;
                    if(source_img.channels() == 1){
                        cv::cvtColor(source_img, color_img, cv::COLOR_GRAY2BGR);
                    }
                    else {
                        color_img = source_img.clone();
                    }
                    cv::resize(color_img, color_img, cv::Size(source_img.cols / resize_scale, source_img.rows / resize_scale));
                    cv::Mat result_mat_img = cv::Mat::zeros(re_src_height, re_src_width, CV_8UC3);
                    cv::Rect crop_roi = cv::Rect(padding_w, padding_h, color_img.cols, color_img.rows);
                    color_img.copyTo(result_mat_img(crop_roi));

                    if(dm::log){
                        lw << "[DEBUG] resize_scale: " << resize_scale << lw.endl;
                        lw << "[DEBUG] padding_w: " << padding_w << lw.endl;
                        lw << "[DEBUG] padding_h: " << padding_h << lw.endl;
                        lw << "[DEBUG] re_src_height: " << re_src_height << lw.endl;
                        lw << "[DEBUG] re_src_width: " << re_src_width << lw.endl;
                        lw << "[DEBUG] color_img.cols: " << color_img.cols << lw.endl;
                        lw << "[DEBUG] color_img.rows: " << color_img.rows << lw.endl;
                    }

                    for(int i = 0; i < score_mem_size; i ++){
                        int source_target = TEST_score_candidates_result_index[i] % pre_proc_src.length_source_candidate;
                        int source_target_rot = TEST_score_candidates_result_index[i] / pre_proc_src.length_source_candidate;
                        short x__= pre_proc_src.source_candidate_col[source_target];
                        short y__ = pre_proc_src.source_candidate_row[source_target];

//                        result_mat_img.at<uchar>(y__, x__) = 255;

                        cv::Vec3b& vec3b = result_mat_img.at<cv::Vec3b>(y__, x__) ;
                        vec3b[0] = 0;
                        vec3b[1] = 0;
                        vec3b[2] = 255;
                    }
                    // To show original candidates before expanding
                    for(int i = 0; i < non_zero_filtered_cand.size(); i ++){
                        short x__= non_zero_filtered_cand.at(i).x;
                        short y__ = non_zero_filtered_cand.at(i).y;

                        cv::Vec3b& vec3b = result_mat_img.at<cv::Vec3b>(y__, x__) ;
                        vec3b[0] = 0;
                        vec3b[1] = 255;
                        vec3b[2] = 255;
                    }

                    cv::imwrite(data_path + "results/0_candidate_" + std::to_string(pyr) + ".png", result_mat_img);
                }catch(cv::Exception e){
                    lw << "File " <<  __FILE__ <<  ", line " << __LINE__
                       << ", in " << __func__ << lw.endl;
                    std::string error_what = e.what();
                    lw << error_what << lw.endl;
                }catch(...){
                    lw << "Error. " << "File " <<  __FILE__ <<  ", line " << __LINE__
                       << ", in " << __func__ << lw.endl;
                }
            }
            TEST_score_candidates_result.reset();
            TEST_score_candidates_result_index.reset();

            if(dm::log){
                cudaDeviceSynchronize();
                clock_gettime(CLOCK_MONOTONIC, &tp_end);
                host_elapsed = clock_diff (&tp_start, &tp_end);
                clock_gettime(CLOCK_MONOTONIC, &tp_start);
                lw << "[DEBUG] Time elapsed - Test (Visualization for candidates): " << host_elapsed << "s" << lw.endl;
            }

        }
        //////// END: TEST ////////

        // Do score sorting
        int num_scores = score_mem_size;    // the number of templates * length_source_candidate

        is_last = (pyr == (num_pyrmd - 1));
        bool is_fine_detection = false;
        int steps = 3;
        std::vector<int> target_num_score_list; // if a target_num_score is 10, one thread sort out 10 sorted values.
        std::vector<int> divider_list;
        target_num_score_list.resize(steps);
        divider_list.resize(steps);


        double init_pow; // 1/2, 2/3, 4/3, ...
        if(pyr < (num_pyrmd - 2)){
            init_pow = (double)num_pyrmd / (double)(2 + num_pyrmd);  // 1/2, 2/3, 4/3, ...
        }
        else{
            init_pow = (double)num_pyrmd / (double)(4 + num_pyrmd);  // 1/2, 2/3, 4/3, ...
        }
        double pow_step = (1.0 - init_pow) / (double)(steps);
//        int final_list_num = std::pow(num_scores, init_pow);
//        int step_unit = num_scores - final_list_num;
//        double step_unit_pow = 1.0 / (double)(steps);
//        step_unit = std::pow(step_unit, step_unit_pow);

        if(dm::log){
            lw << "[DEBUG] ======================================================================" << lw.endl;
            lw << "[DEBUG] init_pow: " << init_pow << lw.endl;
            lw << "[DEBUG] pow_step: " << pow_step << lw.endl;
//            lw << "[DEBUG] final_list_num: " << final_list_num << lw.endl;
//            lw << "[DEBUG] step_unit: " << step_unit << lw.endl;
//            lw << "[DEBUG] step_unit_pow: " << step_unit_pow << lw.endl;
        }

//        for (int s = 0; s < steps; ++s) {
//            target_num_score_list.at(s) = 10;
//            int total = std::pow(num_scores, 1.0 - (pow_step * ((double)s + 1.0)));
//            if(total <= 10){
//                target_num_score_list.at(s) = total;
//                if(total <= 0){
//                    total = std::pow(num_scores, 1.0 - ((pow_step / 2) * (double)s + 1.0));
//                }
//                divider_list.at(s) = 1;
//            }
//            else{
//                divider_list.at(s) = total / 10;
//            }

//            if(s == (steps - 1)){
//                if(total <= 10){
//                    target_num_score_list.at(s) = total;
//                }
//                divider_list.at(s) = 1;
//            }
//            if(is_last){
//                is_fine_detection = true;
//            }
//            if(dm::log){
//                lw << "[DEBUG] divider_list.at(s): " << divider_list.at(s) << lw.endl;
//                lw << "[DEBUG] target_num_score_list.at(s): " << target_num_score_list.at(s) << lw.endl;
//            }
//        }

//        num_result_scores = target_num_score_list.at(target_num_score_list.size() - 1)
//                            * divider_list.at(divider_list.size() - 1);

        num_result_scores = 20;

        if(dm::log){
            cudaDeviceSynchronize();
            clock_gettime(CLOCK_MONOTONIC, &tp_end);
            host_elapsed = clock_diff (&tp_start, &tp_end);
            clock_gettime(CLOCK_MONOTONIC, &tp_start);
            lw << "[DEBUG] Time elapsed - ETC: " << host_elapsed << "s" << lw.endl;
            lw << "[DEBUG] num_result_scores: " << num_result_scores << lw.endl;
        }
        /// Sort using CUB library
        std::shared_ptr<float[]> score_candidates(new float[num_result_scores]());
        std::shared_ptr<int[]> score_candidates_index(new int[num_result_scores]());
        device_wise_sort(cuda_flat_score_map, cuda_flat_score_map_index, score_mem_size,
                         score_candidates.get(), score_candidates_index.get(), num_result_scores);
        if(dm::log){
            cudaDeviceSynchronize();
            clock_gettime(CLOCK_MONOTONIC, &tp_end);
            host_elapsed = clock_diff (&tp_start, &tp_end);
            clock_gettime(CLOCK_MONOTONIC, &tp_start);
            lw << "[DEBUG] Time elapsed - Sorting (CUB_DEVICE_WISE): " << host_elapsed << "s" << lw.endl;
        }
        ///

//        cudaMalloc(&cuda_score_candidates, num_result_scores * sizeof(float));
//        cudaMalloc(&cuda_score_candidates_index, num_result_scores * sizeof(int));
//        SortScores(cuda_flat_score_map, cuda_flat_score_map_index,
//                    cuda_score_candidates, cuda_score_candidates_index,
//                    num_scores, steps,
//                   target_num_score_list, divider_list,
//                    is_fine_detection);

//        if(dm::log){
//            cudaDeviceSynchronize();
//            clock_gettime(CLOCK_MONOTONIC, &tp_end);
//            host_elapsed = clock_diff (&tp_start, &tp_end);
//            clock_gettime(CLOCK_MONOTONIC, &tp_start);
//            lw << "[DEBUG] Time elapsed - Sorting: " << host_elapsed << "s" << lw.endl;
//        }

//        cudaMemcpy(score_candidates.get(), cuda_score_candidates, num_result_scores * sizeof(float), cudaMemcpyDeviceToHost);
//        cudaMemcpy(score_candidates_index.get(), cuda_score_candidates_index, num_result_scores * sizeof(int), cudaMemcpyDeviceToHost);

//        FreeCudaMemory(&cuda_score_candidates);
//        FreeCudaMemory(&cuda_score_candidates_index);



        if(is_last){
            for (unsigned int i = 0; i < num_result_scores; i++){
                int source_target = score_candidates_index[i] % pre_proc_src.length_source_candidate;
                int source_target_rot = score_candidates_index[i] / pre_proc_src.length_source_candidate;
                short x__= pre_proc_src.source_candidate_col[source_target];
                short y__ = pre_proc_src.source_candidate_row[source_target];
                lw << "[DEBUG] Score: " << score_candidates[i] << lw.endl;
                lw << "[DEBUG] source_target: " << source_target << lw.endl;
                lw << "[DEBUG] source_target_rot: " << source_target_rot << lw.endl;
                lw << "[DEBUG] x__: " << x__ << lw.endl;
                lw << "[DEBUG] y__: " << y__ << lw.endl;
            }

            score_candidates_result = score_candidates;
            score_candidates_result_index = score_candidates_index;
        }


        ////////////////////////// Free CUDA memory //////////////////////////
        template_num_non_zero = NULL;
        // Free memory
        FreeCudaMemory(&cuda_flat_template_non_zero_col);
        FreeCudaMemory(&cuda_flat_template_non_zero_row);
        FreeCudaMemory(&cuda_flat_template_weight);
        FreeCudaMemory(&cuda_source_candidate_col);
        FreeCudaMemory(&cuda_source_candidate_row);
        FreeCudaMemory(&cuda_template_num_non_zero);
        ////////////////////////// END: Free CUDA memory //////////////////////////


        if(pyr != (num_pyrmd - 1)){
            ////////////////////////// Prepare for the next step //////////////////////////
            int next_pyrmd_index = template_pyrmd_index - 1;
            unsigned int count = 0;
            int pyrmd_unit = 2;
            if(use_only_start_and_end_){
                if(pyr != 0){
                    continue;
                }
                next_pyrmd_index = 0;
                pyrmd_unit = resize_scale;
            }

            PreprocessedSourcePtr& pre_proc_src_next = pre_proc_src_vec_.at(next_pyrmd_index);
            // Generate the next candidates
            std::vector<cv::Point> filter_target(num_result_scores);
            bool do_pass = false;

            if(dm::log){
                cudaDeviceSynchronize();
                clock_gettime(CLOCK_MONOTONIC, &tp_end);
                host_elapsed = clock_diff (&tp_start, &tp_end);
                clock_gettime(CLOCK_MONOTONIC, &tp_start);
                lw << "[DEBUG] Time elapsed - ETC: " << host_elapsed << "s" << lw.endl;
            }

            bool orig_dilate_method = false;
            if(orig_dilate_method){
                for (unsigned int i = 0; i < num_result_scores; i++){
                    int source_target = score_candidates_index[i] % pre_proc_src.length_source_candidate;
                    short fitx = pre_proc_src.source_candidate_col[source_target] * pyrmd_unit;
                    short fity = pre_proc_src.source_candidate_row[source_target] * pyrmd_unit;

                    do_pass = false;
                    for (int j = i - 1; j >= 0; j--){
                        if(filter_target.at(j).x == fitx && filter_target.at(j).y == fity){
            //                lw << "[DEBUG] dupl: " << (int)fitx << ", " << (int)fity << lw.endl;
                            do_pass = true;
                            break;
                        }
                    }
                    if(!do_pass){
                        filter_target.at(i).x = fitx;
                        filter_target.at(i).y = fity;
                        count++;
                    }
                    else{
                        filter_target.at(i).x = 0;
                        filter_target.at(i).y = 0;
                    }
                }
                if(count == 0){
                    lw << "[FAIL] No template is detected"
                       << ", File " <<  __FILE__ <<  ", line " << __LINE__
                       << ", in " << __func__ << lw.endl;
                    t_execute = 0;
                    return 0;
                }
                if(dm::log){
                    cudaDeviceSynchronize();
                    clock_gettime(CLOCK_MONOTONIC, &tp_end);
                    host_elapsed = clock_diff (&tp_start, &tp_end);
                    clock_gettime(CLOCK_MONOTONIC, &tp_start);
                    lw << "[DEBUG] Time elapsed - Filter Candidate: " << host_elapsed << "s" << lw.endl;
                }




                count = 0;
                std::vector<cv::Point> filter_expanded_target;
                for (int i = 0; i < num_result_scores; i++){
                    short fitx = filter_target.at(i).x;
                    short fity = filter_target.at(i).y;
                    if(fitx == 0 && fity == 0) continue;

                    for (short upx = 0; upx < pyrmd_unit*2; upx++){      // #FIXME: upx++ => upx+=pow(2, pyrNum-i)
                        for (short upy = 0; upy < pyrmd_unit*2; upy++){
                            short dilated_x = fitx + upx - pyrmd_unit;
                            short dilated_y = fity + upy - pyrmd_unit;
                            do_pass = false;
                            for (int j = count - 1; j >= 0; j--){
                                if(filter_expanded_target.at(j).x == dilated_x
                                        && filter_expanded_target.at(j).y == dilated_y){
                                    do_pass = true;
                                    break;
                                }
                            }
                            if(!do_pass){
                                if(dilated_x < 0 ) dilated_x = 0;
                                if(dilated_y < 0 ) dilated_y = 0;
                                filter_expanded_target.push_back(cv::Point(dilated_x, dilated_y));
                                count++;
                            }
                        }
                    }
                }
                orig_tot_mem_size = filter_expanded_target.size();
                MakeSharedPtr(pre_proc_src_next.source_candidate_col, orig_tot_mem_size);
                MakeSharedPtr(pre_proc_src_next.source_candidate_row, orig_tot_mem_size);
                for (int i = 0; i < orig_tot_mem_size; i++){
                    pre_proc_src_next.source_candidate_col[i] = filter_expanded_target.at(i).x;
                    pre_proc_src_next.source_candidate_row[i] = filter_expanded_target.at(i).y;
                }
                pre_proc_src.source_candidate_col.reset();
                pre_proc_src.source_candidate_row.reset();
                if(dm::log){
                    lw << "[DEBUG] orig_tot_mem_size: " << orig_tot_mem_size << lw.endl;
                    lw << "[DEBUG] Filtered count: " << (int)count << lw.endl;
                }

                filter_expanded_target.clear();
                if(dm::log){
                    cudaDeviceSynchronize();
                    clock_gettime(CLOCK_MONOTONIC, &tp_end);
                    host_elapsed = clock_diff (&tp_start, &tp_end);
                    clock_gettime(CLOCK_MONOTONIC, &tp_start);
                    lw << "[DEBUG] Time elapsed - Expand candidate using original method: " << host_elapsed << "s" << lw.endl;
                }



                filter_target.clear();
                if(dm::log){
                    lw << "[DEBUG] Generated count: " << (int)count << lw.endl;
                }
            }
            else{
                //// Dilating using OpenCV
                /// Slower than original method. Approx. 8ms, but original is about 2ms.
                cv::Mat filtered_candidate = cv::Mat::zeros(cv::Size(pre_proc_src_next.width, pre_proc_src_next.height), CV_8UC1);
                cv::Mat dilated_candidate;
                for (int i = 0; i < num_result_scores; i++){
                    int source_target = score_candidates_index[i] % pre_proc_src.length_source_candidate;
                    short fitx = pre_proc_src.source_candidate_col[source_target];
                    short fity = pre_proc_src.source_candidate_row[source_target];

                    // Jang 20220713
                    for (short upx = 0; upx < 3; upx++){
                        for (short upy = 0; upy < 3; upy++){
                            short dilated_fitx = (fitx + upx - 1) * pyrmd_unit;
                            short dilated_fity = (fity + upy - 1) * pyrmd_unit;
                            if(dilated_fitx < 0 || dilated_fity < 0) continue;
                            filtered_candidate.at<uchar>(dilated_fity, dilated_fitx) = 255;
                        }
                    }

                }
                if(dm::match_img){
                    cv::findNonZero(filtered_candidate, non_zero_filtered_cand);
                }
                cv::morphologyEx(filtered_candidate, dilated_candidate, cv::MORPH_DILATE,
                                cv::Mat::ones(cv::Size(pyrmd_unit * 3, pyrmd_unit * 3) , CV_8UC1));
                std::vector<cv::Point> non_zero_;
                cv::findNonZero(dilated_candidate, non_zero_);
                orig_tot_mem_size = non_zero_.size();
                MakeSharedPtr(pre_proc_src_next.source_candidate_col, orig_tot_mem_size);
                MakeSharedPtr(pre_proc_src_next.source_candidate_row, orig_tot_mem_size);
                pre_proc_src.source_candidate_col.reset();
                pre_proc_src.source_candidate_row.reset();
                for (int i = 0; i < orig_tot_mem_size; i++){
                    pre_proc_src_next.source_candidate_col[i] = non_zero_.at(i).x;
                    pre_proc_src_next.source_candidate_row[i] = non_zero_.at(i).y;
                }

                if(dm::log){
                    cudaDeviceSynchronize();
                    clock_gettime(CLOCK_MONOTONIC, &tp_end);
                    host_elapsed = clock_diff (&tp_start, &tp_end);
                    clock_gettime(CLOCK_MONOTONIC, &tp_start);
                    lw << "[DEBUG] Time elapsed - Expand candidate using OpenCV: " << host_elapsed << "s" << lw.endl;
                }
            }

            pre_proc_src_next.length_source_candidate = orig_tot_mem_size;
            pre_proc_src_next.source_num_candidate_width = (short)sqrt(pre_proc_src_next.length_source_candidate) + 1;
            pre_proc_src_next.source_num_candidate_height = pre_proc_src_next.source_num_candidate_width;

            score_candidates.reset();
            score_candidates_index.reset();

            if(dm::log){
                cudaDeviceSynchronize();
                clock_gettime(CLOCK_MONOTONIC, &tp_end);
                host_elapsed = clock_diff (&tp_start, &tp_end);
                clock_gettime(CLOCK_MONOTONIC, &tp_start);
                lw << "[DEBUG] Time elapsed - Prepare for the next step: " << host_elapsed << "s" << lw.endl;
            }
            ////////////////////////// END: Prepare for the next step //////////////////////////
        }
        ////////////////////////// Free CUDA memory //////////////////////////
        FreeCudaMemory(&cuda_flat_source_dist_transfrom);
        ////////////////////////// END: Free CUDA memory //////////////////////////
    }

    PreprocessedSourcePtr& pre_proc_src = pre_proc_src_vec_.at(0);

    int best_case = 0;
    double best_ncc = 0;
    cv::Mat copied_source_img = source_img.clone();
    int length_source_candidate = orig_tot_mem_size;
    int template_width = orig_tmp_width;
    int template_height = orig_tmp_height;
    short* source_candidate_col = pre_proc_src.source_candidate_col.get();
    short* source_candidate_row = pre_proc_src.source_candidate_row.get();
    cv::Mat source_canny_img;
    cv::Mat source_gray_img = copied_source_img.clone();
    cv::Mat rot_template_mat;
    std::vector<float> ncc_results_vec;

    ////////////////////////// NCC Score //////////////////////////
    try{
        // NCC computation preparation
        if(copied_source_img.channels() > 1)
            cv::cvtColor(copied_source_img, source_gray_img, cv::COLOR_BGR2GRAY);
        if(expand_img_){
            std::vector<int> expanded_xy_index;
            bool pass_compute = false;
            for(int j = 0; j < num_result_scores; j++){
                int source_target = score_candidates_result_index[j] % length_source_candidate;
                pass_compute = false;
                for(int k = 0; k < expanded_xy_index.size(); k ++){
                    if(source_target == expanded_xy_index.at(k)){
                        pass_compute = true;
                    }
                }
                if(pass_compute){
                    continue;
                }

                expanded_xy_index.push_back(source_target);

                if(dm::log){
                    std::cout << "=================orig===========================" << std::endl;
                    std::cout << "source_target: " << source_target << std::endl;
                    std::cout << "score_candidates_result_index[j]: " << score_candidates_result_index[j] << std::endl;
                    std::cout << "source_candidate_col[source_target]: " << source_candidate_col[source_target] << std::endl;
                    std::cout << "source_candidate_row[source_target]: " << source_candidate_row[source_target] << std::endl;
                    std::cout << "============================================" << std::endl;
                }
                source_candidate_col[source_target] -= (short)(template_width / 2);
                source_candidate_row[source_target] -= (short)(template_height / 2);
                if(dm::log){
                    std::cout << "source_candidate_col[source_target]: " << source_candidate_col[source_target] << std::endl;
                    std::cout << "source_candidate_row[source_target]: " << source_candidate_row[source_target] << std::endl;
                }
            }
        }

        // Get the best case
        fs::create_directories(data_path + "results/");
        for(int j = 0; j < num_result_scores; j++){
            int template_scale_rot = score_candidates_result_index[j] / length_source_candidate;
            int template_scale = template_scale_rot / num_angles;
            int template_rot = template_scale_rot % num_angles; // + angle_min; // Jang 20211203    // This is the position of rotation from the beginning. (ex) from angle_min.
            int source_target = score_candidates_result_index[j] % length_source_candidate;

//            lw << "Index: " << score_candidates_result_index_orig[j] << ", Score: " << score_candidates_result_orig[j] << lw.endl;
//            lw << "[ " << source_candidate_col[source_target] << ", " << source_candidate_row[source_target] << " ]" << lw.endl;
//            lw << "Scale: " << template_scale << ", Rot: " << template_rot << lw.endl;

            // Mask
            cv::Rect crop_roi;
            cv::Rect crop_temp_roi;

            int crop_x;
            int crop_y;
            int crop_w;
            int crop_h;
            int crop_tmp_x;
            int crop_tmp_y;
            crop_x = source_candidate_col[source_target];
            crop_y = source_candidate_row[source_target];
            crop_w = template_width;
            crop_h = template_height;
            crop_tmp_x = 0;
            crop_tmp_y = 0;
            if(source_candidate_col[source_target] < 0){
                crop_x = 0;
                crop_w = template_width + source_candidate_col[source_target];
                crop_tmp_x = -source_candidate_col[source_target];
            }
            if(source_candidate_row[source_target] < 0){
                crop_y = 0;
                crop_h = template_height + source_candidate_row[source_target];
                crop_tmp_y = -source_candidate_row[source_target];
            }
            if((source_candidate_col[source_target] + template_width) >  orig_source_img_mat_.cols){
                crop_w = (orig_source_img_mat_.cols - source_candidate_col[source_target]);
            }
            if((source_candidate_row[source_target] + template_height) >  orig_source_img_mat_.rows){
                crop_h = (orig_source_img_mat_.rows - source_candidate_row[source_target]);
            }
            crop_roi = cv::Rect(crop_x, crop_y, crop_w, crop_h);
            crop_temp_roi = cv::Rect(crop_tmp_x, crop_tmp_y, crop_w, crop_h);


            cv::Size temp_resized_size;
            temp_resized_size.width = crop_w;
            temp_resized_size.height = crop_h;
            if(trained_template->templates_gray.size() > template_scale_rot){
                // NCC computation
//                rot_template_mat = trained_template->templates_gray.at(template_scale_rot)(crop_temp_roi);
                rot_template_mat = rot_template_mat.zeros(temp_resized_size, trained_template->templates_gray.at(template_scale_rot).type());
                trained_template->templates_gray.at(template_scale_rot)(crop_temp_roi).copyTo(rot_template_mat);
                // Average of source
                double avg_source;
                avg_source = cv::mean(source_gray_img(crop_roi))[0];

                // Average of template
                double avg_template;
                avg_template = cv::mean(rot_template_mat)[0];
                if(dm::match_img){
                    cv::imwrite(data_path + "results/" + std::to_string(j) + "_cropped_source_gray_img.png", source_gray_img(crop_roi));
                    cv::imwrite(data_path + "results/" + std::to_string(j) + "_rot_template_mat.png", rot_template_mat);
                }
                // Sum of child values
                double sum_numerator = 0;
                double sum_denominator_0 = 0;
                double sum_denominator_1 = 0;
                for(int r = 0; r < rot_template_mat.rows; r ++){
                    for(int c = 0; c < rot_template_mat.cols; c++){
                        double compute_val_0 = rot_template_mat.at<uchar>(r, c) - avg_template;
                        double compute_val_1 = source_gray_img.at<uchar>(r + crop_roi.y, c + crop_roi.x) - avg_source;
                        sum_numerator += (compute_val_0 * compute_val_1);

                        sum_denominator_0 += std::pow(compute_val_0, 2);
                        sum_denominator_1 += std::pow(compute_val_1, 2);

                    }
                }

                sum_denominator_0 = std::sqrt(sum_denominator_0);
                sum_denominator_1 = std::sqrt(sum_denominator_1);



                double ncc_result = 0;
                ncc_result = sum_numerator / (sum_denominator_0 * sum_denominator_1);
//                ncc_result = std::fabs(ncc_result);

                if(dm::log){
                    lw << "avg_source: " << avg_source << lw.endl;
                    lw << "avg_template: " <<  avg_template << lw.endl;
                    lw << "sum_denominator_0: " << sum_denominator_0 << lw.endl;
                    lw << "sum_denominator_1: " << sum_denominator_1 << lw.endl;
                    lw << "Score(ncc_result): " << ncc_result << lw.endl;
                }

                ncc_results_vec.push_back(ncc_result);
                if(best_ncc < ncc_result){
                    best_ncc = ncc_result;
                    best_case = j;
                }

            }
        }
        lw << "best_case(ncc_result): " << best_case << lw.endl;

        if(dm::log){
            lw << "num_angles: " << num_angles << ", angle_min: " << angle_min << lw.endl;
        }
        {
            int j = best_case;
            int template_scale_rot = score_candidates_result_index[j] / length_source_candidate;
            int template_scale = template_scale_rot / num_angles;
            int template_rot = template_scale_rot % num_angles; // + angle_min; // Jang 20211203    // This is the position of rotation from the beginning. (ex) from angle_min.
            int source_target = score_candidates_result_index[j] % length_source_candidate;

            float detected_angle = ((float)template_rot * angle_interval_fine_align) + angle_min;

            float score_percentage;
            score_percentage = (4.0 - pow(score_candidates_result[j], 2)) / 4.0;
            if(score_percentage < 0) score_percentage = 0;

            lw << "------ Result ------" << lw.endl;
            lw << "Index: " << score_candidates_result_index[j] << ", Score: " << score_candidates_result[j] << ", NCC: " << best_ncc << lw.endl;
            lw << "[ " << source_candidate_col[source_target] << ", " << source_candidate_row[source_target] << " ]" << lw.endl;
            lw << "Scale: " << template_scale << ", Rot: " << template_rot << lw.endl;

            if(dm::match_img){
                // Mask
                cv::Rect crop_roi = cv::Rect(source_candidate_col[source_target] , source_candidate_row[source_target], template_width, template_height);

                cv::Mat mask_mat = trained_template->templates.at(template_scale_rot);
                lw << "--- template_scale_rot: " << template_scale_rot << lw.endl;
                lw << "mask_mat: " << mask_mat.cols << ", " << mask_mat.rows << lw.endl;
                cv::Mat color_mask = cv::Mat::zeros(mask_mat.size(), CV_8UC3);
                std::vector<cv::Point> non_zero_;
                cv::findNonZero(mask_mat, non_zero_);

                // Change pixels' color
                for(int p = 0; p < non_zero_.size(); p++){
                    cv::Vec3b& vec3b = color_mask.at<cv::Vec3b>(non_zero_.at(p).y, non_zero_.at(p).x) ;
                    vec3b[0] = 0;
                    vec3b[1] = 0;
                    vec3b[2] = 255;
                }

                lw << "File " <<  __FILE__ <<  ", line " << __LINE__
                   << ", in " << __func__ << lw.endl;
                lw << "color_mask: " << color_mask.cols << ", " << color_mask.rows << lw.endl;
                lw << "copied_source_img: " << copied_source_img.cols << ", " << copied_source_img.rows << lw.endl;
                lw << "mask_mat: " << mask_mat.cols << ", " << mask_mat.rows << lw.endl;
                lw << "crop_roi: " << crop_roi.width << ", " << crop_roi.height << lw.endl;

                try{
                    cv::cvtColor(copied_source_img, copied_source_img, cv::COLOR_GRAY2BGR);
                }
                catch(cv::Exception e){
                    lw << "File " <<  __FILE__ <<  ", line " << __LINE__
                       << ", in " << __func__ << lw.endl;
                    std::string error_what = e.what();
                    lw << error_what << lw.endl;
                }catch(...){
                    lw << "Error. " << "File " <<  __FILE__ <<  ", line " << __LINE__
                       << ", in " << __func__ << lw.endl;
                }
                color_mask.copyTo(copied_source_img(crop_roi), mask_mat.clone());

                cv::imwrite(data_path + "results/" + std::to_string(test_count) + "_copied_source_img.png", copied_source_img);
                cv::imwrite(data_path + "results/" + std::to_string(test_count) + "_copied_mask_mat.png", color_mask);
                test_count++;
            }

            MatchData match_data( 0, 0, 1.0f, .0f, .0f );
            match_data.x = source_candidate_col[source_target] + template_width / 2;   // center position
            match_data.y = source_candidate_row[source_target] + template_height / 2;  // center position
            match_data.angle = detected_angle; // unit: degree
            match_data.coef = best_ncc; // score_candidates_result_orig[j];
            match_data.scale = template_scale;

            // If the matched position is too close to the image boundaries, the matching failed.
//            if(match_data.x < (template_width / 4) || match_data.y < (template_height / 4) ||
//                    orig_source_img_mat_.cols - match_data.x < (template_width / 4) ||
//                    orig_source_img_mat_.rows - match_data.y < (template_height / 4) ){
//                 throw(__LINE__);
//             }
            // Jang 20221024
            // Disabled the function.
            if(match_data.x < (template_width / 2) || match_data.y < (template_height / 2) ||
                   orig_source_img_mat_.cols - match_data.x < (template_width / 2) ||
                   orig_source_img_mat_.rows - match_data.y < (template_height / 2) ){
                throw(__LINE__);
            }

            rslt_match.push_back(match_data);

            // Save the best candidates into a file. (the same file which is used in the RST matching)
            FILE *fp_result_p=fopen((data_path + result_matched_area_path).c_str(), "wb");
            {
                float angle_ = (detected_angle / 180 * PI) + (PI / 2);
                float radius_ = template_height / 2;
                // x1, y1: center position
                // x2, y2: template's boundary position to show angle
                int x1, x2, y1, y2;
                x1 = source_candidate_col[source_target] + template_width / 2;
                y1 = source_candidate_row[source_target] + template_height / 2;

                x2 = x1 + (std::cos(angle_)*radius_)+0.5;
                y2 = y1 - (sin(angle_)*radius_)+0.5;

                cv::Point pp0{x1, y1};
                cv::Point pp1{x2, y2};
                cv::Point p1{
                        x1+(cos(angle_)*((template_height-1)/2))+(cos(angle_+ (PI / 2))*((template_width-1)/2)),
                        y1-(sin(angle_)*((template_height-1)/2))-(sin(angle_+ (PI / 2))*((template_width-1)/2))
                };
                cv::Point p2{
                        (p1.x+(cos(angle_- (PI / 2))*template_width)),
                        (p1.y-(sin(angle_- (PI / 2))*template_width))
                };
                cv::Point p3{
                        (p2.x+(cos(angle_- PI)*template_height)),
                        p2.y-(sin(angle_- PI)*template_height)
                };
                cv::Point p4{
                        p1.x+(cos(angle_+ PI)*template_height),
                        p1.y-(sin(angle_+ PI)*template_height)
                };

                fprintf(fp_result_p, "%d\n%d\n%d\n%d\n",pp0.x,pp0.y,pp1.x,pp1.y);
                fprintf(fp_result_p, "%d\n%d\n%d\n%d\n",p1.x,p1.y,p2.x,p2.y);
                fprintf(fp_result_p, "%d\n%d\n%d\n%d\n",p3.x,p3.y,p4.x,p4.y);
            }
            fclose(fp_result_p);    fp_result_p = NULL;
        }
    }
    catch(cv::Exception e){
        lw << "File " <<  __FILE__ <<  ", line " << __LINE__
           << ", in " << __func__ << lw.endl;
        std::string error_what = e.what();
        lw << error_what << lw.endl;
        rslt_match.resize(1);
        rslt_match.at(0).coef = 0;
    }
    catch(int line){
        rslt_match.resize(1);
        rslt_match.at(0).coef = 0;
    }
    catch(...){
        lw << "Error. " << "File " <<  __FILE__ <<  ", line " << __LINE__
           << ", in " << __func__ << lw.endl;
        rslt_match.resize(1);
        rslt_match.at(0).coef = 0;
    }
    if(dm::log){
        cudaDeviceSynchronize();
        clock_gettime(CLOCK_MONOTONIC, &tp_end);
        host_elapsed = clock_diff (&tp_start, &tp_end);
        clock_gettime(CLOCK_MONOTONIC, &tp_start);
        lw << "[DEBUG] Time elapsed - Result Computation: " << host_elapsed << "s" << lw.endl;
    }
    ////////////////////////// END: NCC Score //////////////////////////

    if(dm::log){
        cudaDeviceSynchronize();
    }
    clock_gettime(CLOCK_MONOTONIC, &tp_end);
    host_elapsed = clock_diff (&start_all, &tp_end);
    t_execute = host_elapsed;

    lw << "ChamferInference Done: " << host_elapsed << "s" << lw.endl;
    // num of non zero
    short template_size = (template_width + template_width) / 2;
    float best_score = score_candidates_result[best_case];
    lw << "template_size : " << template_size << lw.endl;
    lw << "best_score : " << best_score << lw.endl;
    if(0.5 > chamfer_score_threshold_ || chamfer_score_threshold_ > 10){
        chamfer_score_threshold_ = 3;
    }
    if( 0.5 < rslt_match.at(0).coef && rslt_match.at(0).coef < 0.8
       && best_score < chamfer_score_threshold_){
        lw << "[WARNING] Chamfer matching score is high. But NCC score is low." << lw.endl;
    }
    else if(rslt_match.at(0).coef < 0.8){
        fail_msg = "[FAILED]";
        fail_msg += "Failure 1: NCC matching score is lower than 0.8. ";

        for(int j = 0; j < num_result_scores; j++){
            int template_scale_rot = score_candidates_result_index[j] / length_source_candidate;
            int template_scale = template_scale_rot / num_angles;
            int template_rot = template_scale_rot % num_angles;
            int source_target = score_candidates_result_index[j] % length_source_candidate;

            lw << "Index: " << score_candidates_result_index[j] << ", Score: " << score_candidates_result[j] << lw.endl;
            lw << "[ " << source_candidate_col[source_target] << ", " << source_candidate_row[source_target] << " ]" << lw.endl;
            lw << "Scale: " << template_scale << ", Rot: " << template_rot << lw.endl;

            if(ncc_results_vec.size() > j){
                lw << "Score(NCC): " << ncc_results_vec.at(j)<< lw.endl;
            }
        }

        if(score_candidates_result[0] >= max_score_orig){
            fail_msg += "Failure 2: Chamfer matching score is too low. The current image may not have the shapes of the templates. Image processing parameters between templates and current images may be different. ";
        }

        lw << "[FAILED] Detection failed. Score: " << rslt_match.at(0).coef << lw.endl;
        lw << fail_msg << lw.endl;
        host_elapsed = 0;
        t_execute = host_elapsed;
        rslt_match.clear();
    }

    // Save result if matching is succeeded.
    if(host_elapsed > 0 && rslt_match.size() > 0){
        // Jang 20221019
        // Handling failed case
        if(rslt_match.at(0).coef > 0.4){
            FILE *result_score=fopen((data_path + result_score_path).c_str(),"wb");
            for(int i = 0; i < 1; i++){
                MatchData& match_data = rslt_match.at(i);
                // Jang 20221020
                // Fixed wrong y-value problem.
                fprintf(result_score, "%2d:        (%4d,%4d);%12.1f%;%12.1f;%12.1f\n",
                        i+1, match_data.x, orig_src_height - match_data.y
                        , match_data.coef * 100.0, _minScale + _scaleInterval * (float)match_data.scale, match_data.angle);

            }
            fclose(result_score); result_score = NULL;
        }
        else {
            // Jang 20221024
            host_elapsed = 0;
            t_execute = host_elapsed;
            rslt_match.clear();
            lw << "[FAILED] Detection Failed. " << "File " <<  __FILE__ <<  ", line " << __LINE__
               << ", in " << __func__ << lw.endl;
        }
    }

    if(dm::log){
        cudaDeviceSynchronize();
        clock_gettime(CLOCK_MONOTONIC, &tp_end);
        host_elapsed = clock_diff (&tp_start, &tp_end);
        lw << "[DEBUG] Time elapsed - Saving results: " << host_elapsed << "s" << lw.endl;
    }
    return (float)(host_elapsed);
#endif
    return 0;
}

float CudaChamfer::LoadChamferData(){

    struct timespec start_all;//clock_t start_all = clock();
    clock_gettime(CLOCK_MONOTONIC, &start_all);
    struct timespec tp_end;
    double host_elapsed;

    const std::string path_temp = data_path + "/templates";
    const std::string path_temp_pyr = data_path + "/templates_pyr";
    lw << "data_path: " << data_path << lw.endl;

    orig_template_img_mat_.release();
    pyr_template_img_mat_.release();
    orig_template_img_mat_ = cv::imread(data_path + "0_orig_template_img_mat.png");
    pyr_template_img_mat_ = cv::imread(data_path + "0_pyr_template_img_mat.png");

    if(pyr_template_img_mat_.empty() || orig_template_img_mat_.empty()){
        lw << "[ERROR] Templates do not exist." << lw.endl;
        return 0;
    }

    // Get the matching parameters. Jimmy. 20220112
    std::string params_path = this->params_path_ + parameter_matching_file;
    if(!fs::exists(params_path_)){
        if(!fs::create_directories(params_path_)){
            lw << "[ERROR] The path is invalid. Path: " << params_path_ << lw.endl;
            return 0;
        }
    }
    // Jang 20220419
    LoadParamsIniFile(params_path);
    if(chamfer_method_ == 0){
        if(!LoadChamferTemplate(trained_template_pyr, angle_min_, angle_max_,
                                angle_interval_coarse_align_, path_temp_pyr)) return 0;
        if(!LoadChamferTemplate(trained_template, angle_min_, angle_max_,
                                angle_interval_fine_align_, path_temp)) return 0;
    }
    else{
        trained_template_vec_.resize(down_sampling_chamfer_);
        if(!LoadChamferTemplate2(trained_template_vec_, angle_min_, angle_max_,
                                 angle_interval_fine_align_, angle_interval_coarse_align_, path_temp)) return 0;

    }

//    return 0;

    clock_gettime(CLOCK_MONOTONIC, &tp_end);
    host_elapsed = clock_diff (&start_all, &tp_end);
    lw << "[Loading Templates] " << (double)(host_elapsed) << lw.endl;
    return (float)host_elapsed;
}

// Jang 20211207
bool CudaChamfer::IsChamferTemplateOk(){
    if(chamfer_template_score > 0) return true;
    else return false;
}

float CudaChamfer::ChamferTrain(cv::Mat template_img){
    chamfer_template_score = 0.0;
    lw << "[ChamferTrain start]" << lw.endl;

    struct timespec start_all;//clock_t start_all = clock();
    clock_gettime(CLOCK_MONOTONIC, &start_all);
    struct timespec tp_end;
    double host_elapsed;

    float execute_t = 0;

    const std::string path_temp = data_path + "/templates";
    const std::string path_temp_pyr = data_path + "/templates_pyr";
    if(fs::exists(path_temp)){
       fs::remove_all(path_temp);
    }
    if(fs::exists(path_temp_pyr)){
       fs::remove_all(path_temp_pyr);
    }

#ifdef DEBUG_MODE
    lw << "File " <<  __FILE__ <<  ", line " << __LINE__
       << ", in " << __func__ << lw.endl;
#endif

    if (!fs::exists(data_path)) fs::create_directories(data_path);

    std::string params_path = this->params_path_ + parameter_matching_file;
    if(!fs::exists(params_path_)){
        if(!fs::create_directories(params_path_)){
            lw << "[ERROR] The path is invalid. Path: " << params_path_ << lw.endl;
            return 0;
        }
    }
    // Jang 20220419
    LoadParamsIniFile(params_path);

    orig_template_img_mat_ = template_img.clone();
#ifdef DEBUG_MODE
    lw << "File " <<  __FILE__ <<  ", line " << __LINE__
       << ", in " << __func__ << lw.endl;
#endif
    // Down-sampling
    cv::Mat pyr_tmp_img;
    cv::pyrDown(template_img, pyr_tmp_img, cv::Size(template_img.cols/2, template_img.rows/2));    // become 1/2 size
    pyr_template_img_mat_ = pyr_tmp_img.clone();

    bool result = false;
    result = cv::imwrite(data_path + "0_orig_template_img_mat.png", orig_template_img_mat_);
    if (!result) {
        lw << "[ERROR] Failed to save templates." << lw.endl;
    }
    result = cv::imwrite(data_path + "0_pyr_template_img_mat.png", pyr_template_img_mat_);
    if (!result) {
        lw << "[ERROR] Failed to save pyr_templates." << lw.endl;
    }

    lw << data_path + "templates/0_orig_template_img_mat.png" << lw.endl;
    if (!orig_template_img_mat_.empty()) {
        lw << orig_template_img_mat_.rows << "-" << orig_template_img_mat_.cols<< lw.endl;
    } else {
        lw << "orig_template_img_mat_.empty()"<< lw.endl;
    }


#ifdef DEBUG_MODE
    lw << "File " <<  __FILE__ <<  ", line " << __LINE__
       << ", in " << __func__ << lw.endl;
#endif


    chamfer_template_score = 1;

    // Jang 20220711
    if(chamfer_method_ == 0){
        // Old method
        if(trained_template_pyr.templates.size() > 0){
            FreeMemory(&trained_template_pyr.flat_template_non_zero_col_);
            FreeMemory(&trained_template_pyr.flat_template_non_zero_row_);
            FreeMemory(&trained_template_pyr.num_non_zeros);
            trained_template_pyr.non_zero_area_templates.clear();
            trained_template_pyr.templates.clear();
            trained_template_pyr.templates_gray.clear();
            trained_template_pyr.template_angle.clear();
            // Jang 20220330
            FreeMemory(&trained_template_pyr.flat_template_weight_);
            trained_template_pyr.template_weights.clear();
        }
        if(trained_template.templates.size() > 0){
            FreeMemory(&trained_template.flat_template_non_zero_col_);
            FreeMemory(&trained_template.flat_template_non_zero_row_);
            FreeMemory(&trained_template.num_non_zeros);
            trained_template.non_zero_area_templates.clear();
            trained_template.templates.clear();
            trained_template.templates_gray.clear();
            trained_template.template_angle.clear();
            // Jang 20220330
            FreeMemory(&trained_template.flat_template_weight_);
            trained_template.template_weights.clear();
        }
        // Generate the Non-zero mat for template
        // Downsized image is already blurred, so that blurring is no need to be adapted.
        // Also removing contours is not needed.
        image_proc->SkipBlurContour(true, true);
        createTempNonZeroMat(pyr_tmp_img, trained_template_pyr, angle_min_, angle_max_,
                             angle_interval_coarse_align_, (int)(skip_pixel_ / 2), path_temp_pyr);
        if(!trained_template_pyr.is_template_ok){
            chamfer_template_score = 0;
            return 0 ;
        }
        // Original template needs blurring
        // But not removing contours
        image_proc->SkipBlurContour(false, true);
        createTempNonZeroMat(template_img, trained_template, angle_min_, angle_max_,
                             angle_interval_fine_align_, skip_pixel_, path_temp);
        if(!trained_template.is_template_ok){
            chamfer_template_score = 0;
            return 0 ;
        }
    }
    else{
        // Jang 20220530
        // Mem allocation
        short scale = std::pow(2, down_sampling_chamfer_ - 1);
        int min_size = 20;
        int min_w_h = (template_img.cols < template_img.rows) ? template_img.cols : template_img.rows;
        if(scale * min_size > min_w_h){
            int target_scale = min_w_h / min_size;
            int count = 0;
            while(target_scale > 0){
                target_scale /= 2;
                count++;
            }
            down_sampling_chamfer_ = count;
        }

        ImageParams iparam;
        this->image_proc->LoadParams(iparam);
        down_sampling_chamfer_ = iparam.downsample_step + 1;

        trained_template_vec_.resize(down_sampling_chamfer_);
        image_proc->SkipBlurContour(false, false);
        if(chamfer_method_ == 1){
            CreateTempNonZeroMat2(template_img, trained_template_vec_,
                                 angle_min_, angle_max_,
                                 angle_interval_fine_align_, angle_interval_coarse_align_,
                                 skip_pixel_, path_temp);
        }
    }



    clock_gettime(CLOCK_MONOTONIC, &tp_end);
    host_elapsed = clock_diff (&start_all, &tp_end);
    lw << "[Training] " << (double)(host_elapsed) << lw.endl;

    execute_t = (float)host_elapsed;
    return execute_t ;
}


// Jang 20220608
void CudaChamfer::CreateSrcNonZeroMat2(cv::Mat orig_src,
                               std::vector<PreprocessedSourcePtr> &preprocessed_source,
                               short num_pyrmd,
                               int temp_w, int temp_h,
                               std::string path)
{
    /// num_pyrmd: the number of pyramids for resizing image. e.g.) if num_pyrmd = 2, images will be original, original / 2
    ///             default = 1, which means original size.
    ///
    assert(num_pyrmd > 0);  // The number includes the original size.

    std::string orig_path = path;
    cv::Mat src_img = orig_src;//cv::Mat src_img = orig_src.clone();
    cv::Mat src_contours;
    cv::Mat src_contours_inv;
    struct timespec tp_total, tp_start, tp_end;
    double host_elapsed;
    if(dm::log) {
        cudaDeviceSynchronize();
        clock_gettime(CLOCK_MONOTONIC, &tp_total);
        clock_gettime(CLOCK_MONOTONIC, &tp_start);
        lw << "[DEBUG] Start - createSrcNonZeroMat" << lw.endl;
    }

    if(src_img.channels() > 1){
        cv::cvtColor(src_img, src_img, cv::COLOR_RGB2GRAY);
    }


    int src_w = orig_src.cols;
    int src_h = orig_src.rows;


    // Compute candidate with the smallest size
    // Detection will be precessed from the smallest image.

    for (int i = 0; i < preprocessed_source.size(); ++i) {
        if(use_only_start_and_end_){
            if(i != 0 && i != (preprocessed_source.size() - 1)){
                continue;
            }
        }

        path = orig_path + "/scale_" + std::to_string(i) + "/";
        fs::create_directories(path);
        PreprocessedSourcePtr& preproc_src = preprocessed_source.at(i);
        short resize_scale = std::pow(2, i); // 1, 2, 4, 8, ...
        int re_src_w = src_w / resize_scale;
        int re_src_h = src_h / resize_scale;
        int re_temp_w = temp_w / resize_scale;
        int re_temp_h = temp_h / resize_scale;
        cv::Size re_src_size = cv::Size(re_src_w, re_src_h);
        cv::Size re_temp_size = cv::Size(re_temp_w, re_temp_h);

        cv::Mat resized_img;
        cv::resize(src_img, resized_img, re_src_size);

        if(!use_image_proc) {
            cv::blur(resized_img, resized_img, cv::Size(3, 3));
            cv::Canny(resized_img, src_contours, 100, 200, 3);
        }
        else {
            ImageParams iparam;
            this->image_proc->LoadParams(iparam);
            if(i > 0){
                image_proc->AdjustParametersForDownsampling(iparam, iparam, i);
            }
            this->image_proc->GetImageBySavedInfo(resized_img, src_contours, iparam, use_cuda_for_improc);
        }
        if(dm::match_img)
        {
            cv::imwrite(path + std::to_string(src_contours.cols) + "_src_contours_0.png", src_contours);
        }

        if(dm::log) {
            cudaDeviceSynchronize();
            clock_gettime(CLOCK_MONOTONIC, &tp_end);
            host_elapsed = clock_diff (&tp_start, &tp_end);
            clock_gettime(CLOCK_MONOTONIC, &tp_start);
            lw << "[DEBUG] Time elapsed - Contour Detection: " << host_elapsed << "s" << lw.endl;
        }
        ////////////////////////////////// Mask //////////////////////////////////
        cv::Mat src_mask;
        if(src_mask_exists_){
            if(orig_source_img_mat_.rows == source_img_mask_.rows && orig_source_img_mat_.cols == source_img_mask_.cols){
                if(i > 0) {
                    if(!source_img_mask_.empty()){
                        cv::resize(source_img_mask_, pyr_source_img_mask_, re_src_size);
                    }
                    src_mask = pyr_source_img_mask_;
                }else{
                    src_mask = source_img_mask_;
                }
                src_contours = src_contours - src_mask;
            }
            else{
                src_mask_exists_ = false;
            }
        }
        ////////////////////////////////// END: Mask //////////////////////////////////

        short padding_w = re_temp_w / 2;
        short padding_h = re_temp_h / 2;
        int expand_w = re_src_w;
        int expand_h = re_src_h;
        if(expand_img_){
            expand_w = re_src_w + padding_w * 2; //temp_w;
            expand_h = re_src_h + padding_h * 2; //temp_h;
        }
        cv::Size expand_size;
        expand_size.width = expand_w;
        expand_size.height = expand_h;
        int img_mem_size = expand_w * expand_h;

        ////////////////////////////////// CUDA: Set the CUDA memory block //////////////////////////////////
        int block_size = 32;    // max: 32
        dim3 threads_in_a_block(block_size, block_size);    // 16x16 threads
        int num_blocks_w = (int)((expand_w + block_size - 1) / block_size);
        int num_blocks_h = (int)((expand_h + block_size - 1) / block_size);
        dim3 num_blocks(num_blocks_w, num_blocks_h);


        if(i == preprocessed_source.size() - 1) {
            uchar* contours_data = (uchar*)src_contours.data;

            uchar* cuda_input_mat_data_8uc1 = nullptr;
            uchar* cuda_output_mat_data_8uc1 = nullptr;
            cudaMalloc(&cuda_input_mat_data_8uc1, re_src_w * re_src_h * sizeof(uchar));
            cudaMemcpy(cuda_input_mat_data_8uc1, contours_data,
                       re_src_w * re_src_h  * sizeof(uchar), cudaMemcpyHostToDevice);
            cudaMalloc(&cuda_output_mat_data_8uc1, img_mem_size * sizeof(uchar));

            if(dm::log) {
                lw << ", File " <<  __FILE__ <<  ", line " << __LINE__
                   << ", in " << __func__ << lw.endl;
            }
            DilateAndMoveMat<<<num_blocks, threads_in_a_block>>>(cuda_input_mat_data_8uc1, cuda_output_mat_data_8uc1,
                                                                 re_src_w, re_src_h,
                                                                 re_temp_w / 2, re_temp_h / 2,
                                                                 1, // No scaling
                                                                 padding_w, padding_h);
            if(dm::log) {
                lw << ", File " <<  __FILE__ <<  ", line " << __LINE__
                   << ", in " << __func__ << lw.endl;
            }
            cudaError_t cuda_error = cudaGetLastError();
            if(cuda_error != cudaError::cudaSuccess){
                lw << "[ERROR] CUDA error: " << cudaGetErrorString(cuda_error)
                   << ", File " <<  __FILE__ <<  ", line " << __LINE__
                   << ", in " << __func__ << lw.endl;
            }
            cv::Mat cuda_dilated_contours = cv::Mat::zeros(expand_size, CV_8UC1);
            std::shared_ptr<uchar[]> dilated_contours_smt_ptr(new uchar[img_mem_size]());
            uchar* dilated_contours_data = dilated_contours_smt_ptr.get();
            cudaMemcpy(dilated_contours_data, cuda_output_mat_data_8uc1, (img_mem_size) * sizeof(uchar), cudaMemcpyDeviceToHost);
            cuda_dilated_contours = cv::Mat(expand_size, CV_8UC1, (uchar*)dilated_contours_data);

            /// Mask noise in boundary
            cv::Rect mask_out_of_range_w(re_src_w + re_temp_w / 2, 0, re_temp_w / 2, expand_h);
            cv::Rect mask_out_of_range_h(0, re_src_h + re_temp_h / 2, expand_w, re_temp_h / 2);
            cv::add(cuda_dilated_contours(mask_out_of_range_w), -255, cuda_dilated_contours(mask_out_of_range_w));
            cv::add(cuda_dilated_contours(mask_out_of_range_h), -255, cuda_dilated_contours(mask_out_of_range_h));
            ///
            if(dm::log){
                std::cout << "------------- TEST ---------------" << std::endl;
                std::cout << "[DEBUG] func: " << __func__ << ", line " <<  std::endl;
            }
            FreeCudaMemory(&cuda_input_mat_data_8uc1);
            FreeCudaMemory(&cuda_output_mat_data_8uc1);

            if(dm::log) {
                lw << ", File " <<  __FILE__ <<  ", line " << __LINE__
                   << ", in " << __func__ << lw.endl;
            }

            if(dm::match_img)
            {
                cv::imwrite(path + std::to_string(src_contours.cols) + "_resized_contours.png", src_contours);
                cv::imwrite(path + std::to_string(cuda_dilated_contours.cols) + "_cuda_dilated_contours.png", cuda_dilated_contours);
            }
            ////////////////////////////////// END: Set the CUDA memory block //////////////////////////////////

            // Visualisation of the candidates
            if(dm::match_img)
            {
                try{
                    cv::Mat color_mask = cv::Mat::zeros(src_contours.size(), CV_8UC3);
                    cv::Mat color_dilated_contours_mask;
                    std::vector<cv::Point> non_zero_;
                    cv::findNonZero(src_contours, non_zero_);

                    // Change pixels' color
                    for(int p = 0; p < non_zero_.size(); p++){
                        cv::Vec3b& vec3b = color_mask.at<cv::Vec3b>(non_zero_.at(p).y, non_zero_.at(p).x) ;
                        vec3b[0] = 0;
                        vec3b[1] = 0;
                        vec3b[2] = 255;
                    }
                    cv::Rect crop_roi = cv::Rect(padding_w, padding_h, color_mask.cols, color_mask.rows);
                    cv::cvtColor(cuda_dilated_contours, color_dilated_contours_mask, cv::COLOR_GRAY2BGR);
                    color_mask.copyTo(color_dilated_contours_mask(crop_roi), src_contours.clone());
                    cv::imwrite(path + std::to_string(expand_w) + "_cuda_dilated_contours_masked.png", color_dilated_contours_mask);
                }
                catch(cv::Exception e){
                    lw << "File " <<  __FILE__ <<  ", line " << __LINE__
                       << ", in " << __func__ << lw.endl;
                    std::string error_what = e.what();
                    lw << error_what << lw.endl;
                }catch(...){
                    lw << "Error. " << "File " <<  __FILE__ <<  ", line " << __LINE__
                       << ", in " << __func__ << lw.endl;
                }
            }

            if(dm::log) {
                lw << ", File " <<  __FILE__ <<  ", line " << __LINE__
                   << ", in " << __func__ << lw.endl;
            }



            cv::Mat candidate_mat = cuda_dilated_contours;// = cv::Mat::zeros(dilated_contours.size(), dilated_contours.type());



            if(dm::log) {
                cudaDeviceSynchronize();
                clock_gettime(CLOCK_MONOTONIC, &tp_end);
                host_elapsed = clock_diff (&tp_start, &tp_end);
                clock_gettime(CLOCK_MONOTONIC, &tp_start);
                lw << "[DEBUG] Time elapsed - morphologyEx: " << host_elapsed << "s" << lw.endl;
            }

            std::vector<cv::Point> non_zero_area;
            cv::findNonZero(candidate_mat, non_zero_area);  // non_zero_area: coordinate(idx) of non-zero value

            // Set candidate coordinate
            // Non zero area
            preproc_src.length_source_candidate = non_zero_area.size();     // The number of non-zero pixels
            // Process 1d to 2d
            if(preproc_src.length_source_candidate > 0){
                preproc_src.source_num_candidate_width = (short)sqrt(preproc_src.length_source_candidate) + 1;
                preproc_src.source_num_candidate_height = preproc_src.source_num_candidate_width;

                // Allocate memory
                MakeSharedPtr(preproc_src.source_candidate_col, preproc_src.length_source_candidate);
                MakeSharedPtr(preproc_src.source_candidate_row, preproc_src.length_source_candidate);

                for(int i = 0; i < preproc_src.length_source_candidate; i ++){
                    preproc_src.source_candidate_col[i] = non_zero_area.at(i).x;
                    preproc_src.source_candidate_row[i] = non_zero_area.at(i).y;
                }
                non_zero_area.clear();
            }
            preproc_src.num_pyrmd = num_pyrmd;

            if(dm::log) {
                cudaDeviceSynchronize();
                clock_gettime(CLOCK_MONOTONIC, &tp_end);
                host_elapsed = clock_diff (&tp_start, &tp_end);
                clock_gettime(CLOCK_MONOTONIC, &tp_start);
                lw << ", File " <<  __FILE__ <<  ", line " << __LINE__
                   << ", in " << __func__ << lw.endl;
                lw << "[DEBUG] Time elapsed - findNonZero: " << host_elapsed << "s" << lw.endl;
            }
        }

        // Resize the image to both up-left and down-right
        // Expanding size: template size
        cv::Mat expanded_candidate_mat;
        if(expand_img_){
            expanded_candidate_mat = cv::Mat::zeros(expand_size, src_contours.type());
            cv::Rect moving_contour_to(padding_w, padding_h, re_src_w, re_src_h);
            src_contours.copyTo(expanded_candidate_mat(moving_contour_to));
            src_contours.release();
            src_contours = expanded_candidate_mat.clone();
        }
        // # Distance Transform
        cv::threshold(src_contours, src_contours_inv, 127, 255, cv::THRESH_BINARY_INV);
        if(dm::match_img){
            cv::imwrite(path + std::to_string(resized_img.cols) + "_src.png", resized_img);
            cv::imwrite(path + std::to_string(src_contours.cols) + "_src_contours.png", src_contours);
            cv::imwrite(path + std::to_string(src_contours_inv.cols) + "_contour_source_img_inv.png", src_contours_inv);
        }
        if(dm::log) {
            cudaDeviceSynchronize();
            clock_gettime(CLOCK_MONOTONIC, &tp_end);
            host_elapsed = clock_diff (&tp_start, &tp_end);
            clock_gettime(CLOCK_MONOTONIC, &tp_start);
            lw << "[DEBUG] Time elapsed - Others: " << host_elapsed << "s" << lw.endl;
        }

        ////////////////////////////////// Create Chamfer Image for source image //////////////////////////////////
        // Calculates the distance to the closest zero pixel for each pixel of the source image
        cv::distanceTransform(src_contours_inv, preproc_src.chamfer_img, cv::DIST_L2, CV_16S);
        preproc_src.flat_source_dist_transfrom = (float*)preproc_src.chamfer_img.data;
        preproc_src.width = src_contours_inv.cols;
        preproc_src.height = src_contours_inv.rows;

        if(dm::log) {
            cudaDeviceSynchronize();
            clock_gettime(CLOCK_MONOTONIC, &tp_end);
            host_elapsed = clock_diff (&tp_start, &tp_end);
            clock_gettime(CLOCK_MONOTONIC, &tp_start);
            lw << "[DEBUG] Time elapsed - distanceTransform(CV_16S): " << host_elapsed << "s" << lw.endl;
        }

//        cv::Mat test_dist_trans;
//        cv::distanceTransform(src_contours_inv, test_dist_trans, cv::DIST_L2, CV_8U);
//        if(dm::log) {
//            cudaDeviceSynchronize();
//            clock_gettime(CLOCK_MONOTONIC, &tp_end);
//            host_elapsed = clock_diff (&tp_start, &tp_end);
//            clock_gettime(CLOCK_MONOTONIC, &tp_start);
//            lw << "[DEBUG] Time elapsed - distanceTransform(CV_8U): " << host_elapsed << "s" << lw.endl;
//        }
//        cv::Mat test_dist_trans3;
//        cv::distanceTransform(src_contours_inv, test_dist_trans3, cv::DIST_L2, CV_32F);
//        if(dm::log) {
//            cudaDeviceSynchronize();
//            clock_gettime(CLOCK_MONOTONIC, &tp_end);
//            host_elapsed = clock_diff (&tp_start, &tp_end);
//            clock_gettime(CLOCK_MONOTONIC, &tp_start);
//            lw << "[DEBUG] Time elapsed - distanceTransform(CV_32F): " << host_elapsed << "s" << lw.endl;
//        }
//        cv::Mat test_dist_trans2;
//        cv::distanceTransform(src_contours_inv, test_dist_trans2, cv::DIST_L2, CV_16S);
//        if(dm::log) {
//            cudaDeviceSynchronize();
//            clock_gettime(CLOCK_MONOTONIC, &tp_end);
//            host_elapsed = clock_diff (&tp_start, &tp_end);
//            clock_gettime(CLOCK_MONOTONIC, &tp_start);
//            lw << "[DEBUG] Time elapsed - distanceTransform(CV_16S again): " << host_elapsed << "s" << lw.endl;
//        }
//        if(dm::match_img){
//            cv::imwrite(path + std::to_string(test_dist_trans.cols) + "_chamfer_img.png", preproc_src.chamfer_img);
//            cv::imwrite(path + std::to_string(test_dist_trans.cols) + "_test_dist_trans.png", test_dist_trans);
//            cv::imwrite(path + std::to_string(test_dist_trans.cols) + "_test_dist_trans2.png", test_dist_trans2);
//            cv::imwrite(path + std::to_string(test_dist_trans.cols) + "_test_dist_trans3.png", test_dist_trans3);
//        }
    }

    if(dm::log) {
        cudaDeviceSynchronize();
        clock_gettime(CLOCK_MONOTONIC, &tp_end);
        host_elapsed = clock_diff (&tp_total, &tp_end);
        lw << "[DEBUG] Time elapsed - TOTAL (CreateSrcNonZeroMat) : " << host_elapsed << "s" << lw.endl;
    }
}
