#ifdef USE_CUDNN
#include <vector>

#include "caffe/layers/cudnn_lcn_layer.hpp"

namespace caffe {

template <typename Dtype>
void CuDNNLCNLayer<Dtype>::LayerSetUp(const vector<Blob<Dtype>*>& bottom,
    const vector<Blob<Dtype>*>& top) {
  LRNLayer<Dtype>::LayerSetUp(bottom, top);

  CUDNN_CHECK(cudnnCreateLRNDescriptor(&norm_desc_));
  cudnn::createTensor4dDesc<Dtype>(&bottom_desc_);
  cudnn::createTensor4dDesc<Dtype>(&top_desc_);

  // create a LRN handle
  handles_setup_ = true;

  size_ = this->layer_param().lrn_param().local_size();
  pre_pad_ = (size_ - 1) / 2;
  alpha_ = this->layer_param().lrn_param().alpha();
  beta_ = this->layer_param().lrn_param().beta();
  k_ = this->layer_param().lrn_param().k();
}

template <typename Dtype>
void CuDNNLCNLayer<Dtype>::Reshape(const vector<Blob<Dtype>*>& bottom,
    const vector<Blob<Dtype>*>& top) {
  LRNLayer<Dtype>::Reshape(bottom, top);
  cudnn::setTensor4dDesc<Dtype>(&bottom_desc_, bottom[0]->num(),
      this->channels_, this->height_, this->width_);
  cudnn::setTensor4dDesc<Dtype>(&top_desc_, bottom[0]->num(),
      this->channels_, this->height_, this->width_);
  CUDNN_CHECK(cudnnSetLRNDescriptor(norm_desc_, size_, alpha_, beta_, k_));

  // allocate / reallocate tempData buffers
  size_t totalSizeInBytes = sizeof(Dtype)*bottom[0]->num()* \
                            this->channels_*this->height_*this->width_;

  if (totalSizeInBytes > tempDataSize) {
    tempDataSize = totalSizeInBytes;

    cudaFree(tempData1);
    cudaFree(tempData2);

    // allocate new buffers
    CUDA_CHECK(cudaMalloc(&tempData1, totalSizeInBytes));
    CUDA_CHECK(cudaMalloc(&tempData2, totalSizeInBytes));
  }
}

template <typename Dtype>
CuDNNLCNLayer<Dtype>::~CuDNNLCNLayer() {
  // Check that handles have been setup before destroying.
  if (!handles_setup_) { return; }

  cudnnDestroyTensorDescriptor(bottom_desc_);
  cudnnDestroyTensorDescriptor(top_desc_);

  // free temp buffers
  cudaFree(tempData1);
  cudaFree(tempData2);
}

template <typename Dtype>
void CuDNNLCNLayer<Dtype>::Forward_gpu(const vector<Blob<Dtype>*>& bottom,
    const vector<Blob<Dtype>*>& top) {
  const Dtype* bottom_data = bottom[0]->gpu_data();
  Dtype* top_data = top[0]->mutable_gpu_data();

  CUDNN_CHECK(cudnnDivisiveNormalizationForward(
        Caffe::cudnn_handle(), norm_desc_, CUDNN_DIVNORM_PRECOMPUTED_MEANS,
        cudnn::dataType<Dtype>::one,
        bottom_desc_, bottom_data,
        NULL,  // srcMeansData
        this->tempData1, this->tempData2,
        cudnn::dataType<Dtype>::zero,
        top_desc_, top_data) );
}

template <typename Dtype>
void CuDNNLCNLayer<Dtype>::Backward_gpu(const vector<Blob<Dtype>*>& top,
    const vector<bool>& propagate_down, const vector<Blob<Dtype>*>& bottom) {
  const Dtype* top_diff = top[0]->gpu_diff();
  const Dtype* top_data = top[0]->gpu_data();
  const Dtype* bottom_data = bottom[0]->gpu_data();
  Dtype* bottom_diff = bottom[0]->mutable_gpu_diff();

  CUDNN_CHECK(cudnnDivisiveNormalizationBackward(
        Caffe::cudnn_handle(), norm_desc_, CUDNN_DIVNORM_PRECOMPUTED_MEANS,
        cudnn::dataType<Dtype>::one,
        bottom_desc_, bottom_data,
        NULL, top_diff,  // NULL - srcMeansData
        this->tempData1, this->tempData2,
        cudnn::dataType<Dtype>::zero,
        bottom_desc_, bottom_diff,
        NULL) );
}

INSTANTIATE_CLASS(CuDNNLCNLayer);

}   // namespace caffe
#endif
