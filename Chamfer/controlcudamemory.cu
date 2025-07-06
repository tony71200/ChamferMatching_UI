#include "chamfermatch.h"

template<typename T>
bool CudaChamfer::FreeCudaMemory(T** cuda_pointer_){
    try{
        cudaError_t cuda_error = cudaFree(*cuda_pointer_);
        if(cuda_error != cudaError::cudaSuccess){
            lw << "[ERROR] CUDA error: " << cudaGetErrorString(cuda_error)
               << ", File " <<  __FILE__ <<  ", line " << __LINE__
               << ", in " << __func__ << lw.endl;
            throw(0);
        }
    }
    catch(int n_){
        std::cout << "[WARNING] Trying to free CUDA memory which is NULL" << std::endl;
        return false;
    }
    *cuda_pointer_  = NULL;
    return true;
}

template<typename T>
void CudaChamfer::FreeMemory(T** pointer_){
    try{
        if(*pointer_ != NULL)
            delete[] *pointer_;
    }
    catch(...){
        std::cout << "[WARNING] Trying to free memory which is NULL" << std::endl;
    }
    *pointer_  = NULL;
}


bool CudaChamfer::FreeCudaMemory(int** cuda_pointer_){
    return FreeCudaMemory<int>(cuda_pointer_);
    try{
        cudaError_t cuda_error = cudaFree(*cuda_pointer_);
        if(cuda_error != cudaError::cudaSuccess){
            lw << "[ERROR] CUDA error: " << cudaGetErrorString(cuda_error)
               << ", File " <<  __FILE__ <<  ", line " << __LINE__
               << ", in " << __func__ << lw.endl;
            throw(0);
        }
    }
    catch(int n_){
        std::cout << "[WARNING] Trying to free CUDA memory which is NULL" << std::endl;
        return false;
    }
    *cuda_pointer_  = NULL;
    return true;
}

bool CudaChamfer::FreeCudaMemory(short** cuda_pointer_){
    return FreeCudaMemory<short>(cuda_pointer_);
    try{
        cudaError_t cuda_error = cudaFree(*cuda_pointer_);
        if(cuda_error != cudaError::cudaSuccess){
            lw << "[ERROR] CUDA error: " << cudaGetErrorString(cuda_error)
               << ", File " <<  __FILE__ <<  ", line " << __LINE__
               << ", in " << __func__ << lw.endl;
            throw(0);
        }
    }
    catch(int n_){
        std::cout << "[WARNING] Trying to free CUDA memory which is NULL" << std::endl;
        return false;
    }
    *cuda_pointer_  = NULL;
    return true;
}

bool CudaChamfer::FreeCudaMemory(float** cuda_pointer_){
    return FreeCudaMemory<float>(cuda_pointer_);
    try{
        cudaError_t cuda_error = cudaFree(*cuda_pointer_);
        if(cuda_error != cudaError::cudaSuccess){
            lw << "[ERROR] CUDA error: " << cudaGetErrorString(cuda_error)
               << ", File " <<  __FILE__ <<  ", line " << __LINE__
               << ", in " << __func__ << lw.endl;
            throw(0);
        }
    }
    catch(int n_){
        std::cout << "[WARNING] Trying to free CUDA memory which is NULL" << std::endl;
        return false;
    }
    *cuda_pointer_  = NULL;
    return true;
}

bool CudaChamfer::FreeCudaMemory(double** cuda_pointer_){
    return FreeCudaMemory<double>(cuda_pointer_);
    try{
        cudaError_t cuda_error = cudaFree(*cuda_pointer_);
        if(cuda_error != cudaError::cudaSuccess){
            lw << "[ERROR] CUDA error: " << cudaGetErrorString(cuda_error)
               << ", File " <<  __FILE__ <<  ", line " << __LINE__
               << ", in " << __func__ << lw.endl;
            throw(0);
        }
    }
    catch(int n_){
        std::cout << "[WARNING] Trying to free CUDA memory which is NULL" << std::endl;
        return false;
    }
    *cuda_pointer_  = NULL;
    return true;
}

bool CudaChamfer::FreeCudaMemory(uchar** cuda_pointer_){
    return FreeCudaMemory<uchar>(cuda_pointer_);
    try{
        cudaError_t cuda_error = cudaFree(*cuda_pointer_);
        if(cuda_error != cudaError::cudaSuccess){
            lw << "[ERROR] CUDA error: " << cudaGetErrorString(cuda_error)
               << ", File " <<  __FILE__ <<  ", line " << __LINE__
               << ", in " << __func__ << lw.endl;
            throw(0);
        }
    }
    catch(int n_){
        std::cout << "[WARNING] Trying to free CUDA memory which is NULL" << std::endl;
        return false;
    }
    *cuda_pointer_  = NULL;
    return true;
}

void CudaChamfer::FreeMemory(short** pointer_){
    FreeMemory<short>(pointer_); return;
    try{
        if(*pointer_ != NULL)
            delete[] *pointer_;
    }
    catch(...){
        std::cout << "[WARNING] Trying to free memory which is NULL" << std::endl;
    }
    *pointer_  = NULL;
}

void CudaChamfer::FreeMemory(int** pointer_){
    FreeMemory<int>(pointer_); return;
    try{
        if(*pointer_ != NULL)
            delete[] *pointer_;
    }
    catch(...){
        std::cout << "[WARNING] Trying to free memory which is NULL" << std::endl;
    }
    *pointer_  = NULL;
}

void CudaChamfer::FreeMemory(float** pointer_){
    FreeMemory<float>(pointer_); return;
    try{
        if(*pointer_ != NULL)
            delete[] *pointer_;
    }
    catch(...){
        std::cout << "[WARNING] Trying to free memory which is NULL" << std::endl;
    }
    *pointer_  = NULL;
}

void CudaChamfer::FreeMemory(double** pointer_){
    FreeMemory<double>(pointer_); return;
    try{
        if(*pointer_ != NULL)
            delete[] *pointer_;
    }
    catch(...){
        std::cout << "[WARNING] Trying to free memory which is NULL" << std::endl;
    }
    *pointer_  = NULL;
}

void CudaChamfer::FreeMemory(uchar** pointer_){
    FreeMemory<uchar>(pointer_); return;
    try{
        if(*pointer_ != NULL)
            delete[] *pointer_;
    }
    catch(...){
        std::cout << "[WARNING] Trying to free memory which is NULL" << std::endl;
    }
    *pointer_  = NULL;
}

