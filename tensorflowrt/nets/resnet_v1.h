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

#ifndef TFRT_RESNET_V1
#define TFRT_RESNET_V1

#include <map>

#include <NvInfer.h>
#include "../tensorflowrt.h"

namespace resnet_v1
{
/** Arg scope for Inception v2: SAME padding + batch normalization + ReLU.
 */
typedef tfrt::separable_convolution2d<tfrt::ActivationType::RELU, tfrt::PaddingType::SAME, false> separable_conv2d;

typedef tfrt::convolution2d<tfrt::ActivationType::RELU, tfrt::PaddingType::SAME, true>  conv2d;
typedef tfrt::convolution2d<tfrt::ActivationType::NONE, tfrt::PaddingType::SAME, true>  conv2d_none;
// typedef tfrt::convolution2d<tfrt::ActivationType::NONE, tfrt::PaddingType::SAME, true>  conv2d;

typedef tfrt::max_pooling2d<tfrt::PaddingType::SAME>    max_pool2d;
typedef tfrt::avg_pooling2d<tfrt::PaddingType::SAME>    avg_pool2d;
typedef tfrt::concat_channels                           concat_channels;

// ========================================================================== //
// Main blocks defining Residual Networks.
// ========================================================================== //
/** Shortcut link in ResNet, subsampling if necessary. */
inline nvinfer1::ITensor* shortcut(nvinfer1::ITensor* input, int outdepth, int stride, tfrt::scope sc)
{
    typedef tfrt::convolution2d<tfrt::ActivationType::NONE, tfrt::PaddingType::SAME, false>  conv2d;

    auto ssc = sc.sub("shortcut");
    nvinfer1::ITensor* net{input};
    // Input shape.
    nvinfer1::DimsCHW inshape = static_cast<nvinfer1::DimsCHW&&>(input->getDimensions());
    if (inshape.c() == outdepth) {
        // Only sub-sample.
        if (stride > 1) {
            net = max_pool2d(ssc, "subsample").ksize(1).stride(stride)(net);
        }
    }
    else {
        // Change output size.
        net = conv2d(ssc, "conv_1x1").noutputs(outdepth).ksize(1).stride(stride)(net);
    }
    return net;
}
/** Residual bottleneck. */
inline nvinfer1::ITensor* bottleneck(nvinfer1::ITensor* input, int outdepth, int bndepth,
    int stride, tfrt::scope sc)
{
    nvinfer1::ITensor* res{nullptr}, *net{nullptr};
    auto ssc = sc.sub("bottleneck_v1");
    // Shortcut.
    net = shortcut(input, outdepth, stride, ssc);
    // Residual part.
    res = conv2d_none(ssc, "conv1").noutputs(bndepth).ksize(1).stride(1)(input);
    res = conv2d(ssc, "conv2").noutputs(bndepth).ksize(3).stride(stride)(res);
    res = conv2d(ssc, "conv3").noutputs(outdepth).ksize(1).stride(1)(res);
    // Add the final result!
    net = tfrt::add(ssc, "add")(net, res);
    return net;
}
/** Residual block, containing multiple layers and a last one of stride 2. */
inline nvinfer1::ITensor* block(nvinfer1::ITensor* input, size_t num_units,
    int outdepth, int bndepth, int stride, tfrt::scope sc)
{
    nvinfer1::ITensor* net{input};
    for (size_t i = 0 ; i < num_units ; ++i) {
        std::ostringstream name("unit_", std::ios_base::ate);
        name << i;
        // Custom stride for last layer.
        int lstride = (i == num_units-1) ? stride : 1;
        // Bottleneck layer.
        net = bottleneck(net, outdepth, bndepth, lstride, sc.sub(name.str()));
    }
    return net;
}
/** Root block.  */
inline nvinfer1::ITensor* root_block(nvinfer1::ITensor* input, int outdepth, tfrt::scope sc)
{
    nvinfer1::ITensor* net{input};
    net = conv2d(sc, "conv1").noutputs(outdepth).ksize(7).stride(2)(net);
    net = max_pool2d(sc, "pool1").ksize(3).stride(2)(net);
    return net;
}
/** Classification block. */
inline nvinfer1::ITensor* imagenet_block(nvinfer1::ITensor* input, int num_classes, tfrt::scope sc)
{
    typedef tfrt::avg_pooling2d<tfrt::PaddingType::VALID>    avg_pool2d;
    typedef tfrt::convolution2d<tfrt::ActivationType::NONE, tfrt::PaddingType::SAME, false>  conv2d;
    // Input shape.
    nvinfer1::ITensor* net{input};
    nvinfer1::DimsCHW inshape = static_cast<nvinfer1::DimsCHW&&>(input->getDimensions());
    // Average pooling + classification.
    net = avg_pool2d(sc, "pool5").ksize({inshape.h(), inshape.w()})(net);
    net = conv2d(sc, "logits").noutputs(num_classes).ksize({1, 1})(net);
    net = tfrt::softmax(sc, "Softmax")(net);
    return net;
}

}

// ========================================================================== //
// ResNet 50
// ========================================================================== //
namespace resnet_v1_50
{
class net : public tfrt::imagenet_network
{
public:
    net() : tfrt::imagenet_network("resnet_v1_50", 1000, true) {
    }
    virtual nvinfer1::ITensor* build(tfrt::scope sc) {
        auto net = tfrt::input(sc)();
        net = resnet_v1::root_block(net, 64, sc);
        // 4 main blocks.
        net = resnet_v1::block(net, 3, 064*4, 064, 2, sc.sub("block1"));
        net = resnet_v1::block(net, 4, 128*4, 128, 2, sc.sub("block2"));
        net = resnet_v1::block(net, 6, 256*4, 256, 2, sc.sub("block3"));
        net = resnet_v1::block(net, 3, 512*4, 512, 1, sc.sub("block4"));
        // Classification block.
        net = resnet_v1::imagenet_block(net, 1001, sc);
        return net;
    }
};
}
// ========================================================================== //
// ResNet 101
// ========================================================================== //
namespace resnet_v1_101
{
class net : public tfrt::imagenet_network
{
public:
    net() : tfrt::imagenet_network("resnet_v1_101", 1000, true) {
    }
    virtual nvinfer1::ITensor* build(tfrt::scope sc) {
        auto net = tfrt::input(sc)();
        net = resnet_v1::root_block(net, 64, sc);
        // 4 main blocks.
        net = resnet_v1::block(net, 3, 064*4, 064, 2, sc.sub("block1"));
        net = resnet_v1::block(net, 4, 128*4, 128, 2, sc.sub("block2"));
        net = resnet_v1::block(net, 23, 256*4, 256, 2, sc.sub("block3"));
        net = resnet_v1::block(net, 3, 512*4, 512, 1, sc.sub("block4"));
        // Classification block.
        net = resnet_v1::imagenet_block(net, 1001, sc);
        return net;
    }
};
}
// ========================================================================== //
// ResNet 152
// ========================================================================== //
namespace resnet_v1_152
{
class net : public tfrt::imagenet_network
{
public:
    net() : tfrt::imagenet_network("resnet_v1_152", 1000, true) {
    }
    virtual nvinfer1::ITensor* build(tfrt::scope sc) {
        auto net = tfrt::input(sc)();
        net = resnet_v1::root_block(net, 64, sc);
        // 4 main blocks.
        net = resnet_v1::block(net, 3, 064*4, 064, 2, sc.sub("block1"));
        net = resnet_v1::block(net, 8, 128*4, 128, 2, sc.sub("block2"));
        net = resnet_v1::block(net, 36, 256*4, 256, 2, sc.sub("block3"));
        net = resnet_v1::block(net, 3, 512*4, 512, 1, sc.sub("block4"));
        // Classification block.
        net = resnet_v1::imagenet_block(net, 1001, sc);
        return net;
    }
};
}

#endif
