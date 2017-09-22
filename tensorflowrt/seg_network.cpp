/* ============================================================================
# [2017] - Robik AI Ltd - Paul Balanca
# All Rights Reserved.

# NOTICE: All information contained herein is, and remains
# the property of Robik AI Ltd, and its suppliers
# if any.  The intellectual and technical concepts contained
# herein are proprietary to Robik AI Ltd
# and its suppliers and may be covered by U.S., European and Foreign Patents,
# patents in process, and are protected by trade secret or copyright law.
# Dissemination of this information or reproduction of this material
# is strictly forbidden unless prior written permission is obtained
# from Robik AI Ltd.
# =========================================================================== */
#include <glog/logging.h>

#include "seg_network.h"

namespace tfrt
{
// ========================================================================== //
// Segmentation Network.
// ========================================================================== //
void seg_network::init_tensors_cached()
{
    const tfrt::cuda_tensor& cuda_output = m_cuda_outputs[0];
    const auto& oshape = cuda_output.shape;
    if (!m_rclasses_cached.is_allocated() ) {
        m_rclasses_cached = tfrt::cuda_tensor_u8(
            "classes", {oshape.n(), 1, oshape.h(), oshape.w()});
        m_rclasses_cached.allocate();
    }
    if (!m_rscores_cached.is_allocated() ) {
        m_rscores_cached = tfrt::cuda_tensor(
            "scores", {oshape.n(), 1, oshape.h(), oshape.w()});
        m_rscores_cached.allocate();
    }
}
void seg_network::post_processing()
{
    this->init_tensors_cached();
    // For God sake, used a fucking CUDA kernel for that!
    const auto& rtensor = m_cuda_outputs[0].tensor();
    const auto& oshape = m_cuda_outputs[0].shape;
    LOG(INFO) << "SEGNET: post-processing of output with shape: "
        << dims_str(m_cuda_outputs[0].shape);
    // CUDA(cudaDeviceSynchronize());
    for (long n = 0 ; n < rtensor.dimension(0) ; ++n) {
        for (long i = 0 ; i < rtensor.dimension(2) ; ++i) {
            for (long j = 0 ; j < rtensor.dimension(3) ; ++j) {
                uint8_t max_idx = 0;
                float max_score = 0.0f;
                // Channel loop.
                for (long k = 0 ; k < rtensor.dimension(1) ; ++k) {
                    float score = rtensor(n, k, i, j);
                    if (score > max_score) {
                        max_idx = uint8_t(k);
                        max_score = score;
                    }
                    // Save to cached tensors.
                    long idx = n * oshape.h() * oshape.w() + i * oshape.w() + j;
                    if (max_score > m_detection_threshold) {
                        m_rclasses_cached.cpu[idx] = max_idx + int(!m_empty_class);
                        m_rscores_cached.cpu[idx] = max_score;
                    }
                    else {
                        m_rclasses_cached.cpu[idx] = 0;
                        m_rscores_cached.cpu[idx] = max_score;
                    }
                }
            }
        }
    }
    // CUDA(cudaDeviceSynchronize());
    LOG(INFO) << "SEGNET: done with post-processing of output";
}

void seg_network::inference(vx_image image)
{
    LOG(INFO) << "SEGNET: inference with single input.";
    network::inference(image);
    // Post-processing of the output: computing classes and scores.
    this->post_processing();
}
void seg_network::inference(const nvx_image_inpatch& image)
{
    LOG(INFO) << "SEGNET: inference with single input.";
    network::inference(image);
    // Post-processing of the output: computing classes and scores.
    this->post_processing();
}

void seg_network::inference(vx_image img1, vx_image img2)
{
    LOG(INFO) << "SEGNET: inference with two inputs.";
    network::inference(img1, img2);
    // CUDA(cudaDeviceSynchronize());
    this->post_processing();
}
void seg_network::inference(const nvx_image_inpatch& img1, const nvx_image_inpatch& img2)
{
    LOG(INFO) << "SEGNET: inference with two inputs.";
    network::inference(img1, img2);
    // CUDA(cudaDeviceSynchronize());
    this->post_processing();
}

// ========================================================================== //
// Post-segmentation network.
// ========================================================================== //
seg_network_post::seg_network_post() :
    m_seg_outshape{0, 0, 0},
    m_empty_class{false},
    m_detection_threshold{0.0f}
{
}
seg_network_post::seg_network_post(nvinfer1::DimsCHW _seg_outshape, 
        bool _empty_class, float _detection_threshold) :
    m_seg_outshape{_seg_outshape},
    m_empty_class{_empty_class},
    m_detection_threshold{_detection_threshold}
{
    // Allocate temporary buffers.
    if (!m_rclasses_cached.is_allocated() ) {
        m_rclasses_cached = tfrt::cuda_tensor_u8(
            "classes", {1, 1, m_seg_outshape.h(), m_seg_outshape.w()});
        m_rclasses_cached.allocate();
    }
    if (!m_rscores_cached.is_allocated() ) {
        m_rscores_cached = tfrt::cuda_tensor(
            "scores", {1, 1, m_seg_outshape.h(), m_seg_outshape.w()});
        m_rscores_cached.allocate();
    }
}
void seg_network_post::apply(const tfrt::cuda_tensor& seg_output_raw, size_t batch_idx)
{
    // For God sake, used a fucking CUDA kernel for that!
    const auto& rtensor = seg_output_raw.tensor();
    const auto& oshape = seg_output_raw.shape;
    DLOG(INFO) << "SEGNET: post-processing of output with shape: "
        << dims_str(seg_output_raw.shape);
    // CUDA(cudaDeviceSynchronize());
    int n = batch_idx;
    for (long i = 0 ; i < rtensor.dimension(2) ; ++i) {
        for (long j = 0 ; j < rtensor.dimension(3) ; ++j) {
            uint8_t max_idx = 0;
            float max_score = 0.0f;
            // Channel loop.
            for (long k = 0 ; k < rtensor.dimension(1) ; ++k) {
                float score = rtensor(n, k, i, j);
                if (score > max_score) {
                    max_idx = uint8_t(k);
                    max_score = score;
                }
                // Save to cached tensors.
                long idx = n * oshape.h() * oshape.w() + i * oshape.w() + j;
                if (max_score > m_detection_threshold) {
                    m_rclasses_cached.cpu[idx] = max_idx + int(!m_empty_class);
                    m_rscores_cached.cpu[idx] = max_score;
                }
                else {
                    m_rclasses_cached.cpu[idx] = 0;
                    m_rscores_cached.cpu[idx] = max_score;
                }
            }
        }
    }
    // CUDA(cudaDeviceSynchronize());
    DLOG(INFO) << "SEGNET: done with post-processing of output";
}

}